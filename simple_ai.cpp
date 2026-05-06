#include "simple_ai.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>

namespace {

struct BoardMetrics {
  long long row_transitions = 0;
  long long column_transitions = 0;
  long long hole_score = 0;
  long long x0_hole = 0;
  long long x1_hole = 0;
  long long x2_hole = 0;
  int hole_depth = 0;
  int well_depth = 0;
  long long shape_adaptability = 0;
};

struct PlacementStats {
  int rows_cleared = 0;
  int eroded_piece_cells_metric = 0;
};

int bit_count(std::uint16_t bits);
std::uint16_t x0_hole_mask(int cols);
std::uint16_t x1_hole_mask(int cols);
std::uint16_t x2_hole_mask(int cols);
const std::vector<int>& well_scores_for_col(const WellScoreTables& tables, int cols, int c);


std::vector<int> piece_bottom_profile(const BitShape& shape) {
  std::vector<int> bottom;
  bottom.reserve(static_cast<std::size_t>(shape.max_x - shape.min_x + 1));
  for (int c = shape.min_x; c <= shape.max_x; ++c) {
    bottom.push_back(shape.bottom_by_col[static_cast<std::size_t>(c)]);
  }
  return bottom;
}

std::vector<int> profile_diff(const std::vector<int>& profile) {
  std::vector<int> out;
  if (profile.size() <= 1) {
    return out;
  }
  out.reserve(profile.size() - 1);
  for (std::size_t i = 1; i < profile.size(); ++i) {
    out.push_back(profile[i] - profile[i - 1]);
  }
  return out;
}

std::vector<int> negate_diff(const std::vector<int>& diff) {
  std::vector<int> out = diff;
  for (int& v : out) {
    v = -v;
  }
  return out;
}

const std::vector<std::vector<std::vector<int>>>& piece_diff_patterns() {
  static const std::vector<std::vector<std::vector<int>>> patterns = [] {
    std::vector<std::vector<std::vector<int>>> out(7);
    const auto& rotations = all_bit_rotations();
    for (int piece_type = 0; piece_type < static_cast<int>(rotations.size()); ++piece_type) {
      std::set<std::vector<int>> unique_patterns;
      const auto& piece_rotations = rotations[static_cast<std::size_t>(piece_type)];
      for (int shape_idx = 0; shape_idx < static_cast<int>(piece_rotations.size()); ++shape_idx) {
        const auto& shape = piece_rotations[static_cast<std::size_t>(shape_idx)];
        const int width = shape.max_x - shape.min_x + 1;
        if (width <= 1) {
          continue;
        }
        // Special case: do not consider vertical I.
        if (piece_type == 0 && width == 1) {
          continue;
        }

        const std::vector<int> bottom = piece_bottom_profile(shape);
        const std::vector<int> diff = profile_diff(bottom);
        if (diff.empty()) {
          continue;
        }
        unique_patterns.insert(diff);
        // Keep both slope directions so matching is robust to contour convention.
        unique_patterns.insert(negate_diff(diff));
      }
      out[static_cast<std::size_t>(piece_type)] =
          std::vector<std::vector<int>>(unique_patterns.begin(), unique_patterns.end());
    }
    return out;
  }();
  return patterns;
}

long long adaptability_score_from_match_count(int matches) {
  if (matches <= 0) {
    return 0;
  }
  if (matches == 1) {
    return 4;
  }
  return 6;
}

// Lookup tables for evaluate_shape_adaptability.
// diff values are in [-MAX_BOARD_ROWS, MAX_BOARD_ROWS]; index = diff + MAX_BOARD_ROWS.
constexpr int kDiffOffset = MAX_BOARD_ROWS;
constexpr int kDiffRange = 2 * MAX_BOARD_ROWS + 1;

struct AdaptLookupTables {
  // t1[diff_idx][piece_type] = number of len-1 patterns of piece_type matching diff
  std::array<std::array<int8_t, 7>, kDiffRange> t1{};
  // t2[diff_a_idx * kDiffRange + diff_b_idx][piece_type] = len-2 pattern match count
  std::array<std::array<int8_t, 7>, kDiffRange * kDiffRange> t2{};
};

const AdaptLookupTables& adapt_lookup_tables() {
  static const AdaptLookupTables tables = [] {
    AdaptLookupTables t;
    const auto& by_piece = piece_diff_patterns();
    for (int pt = 1; pt < static_cast<int>(by_piece.size()); ++pt) {
      for (const auto& pattern : by_piece[static_cast<std::size_t>(pt)]) {
        if (pattern.size() == 1) {
          const int idx = pattern[0] + kDiffOffset;
          if (idx >= 0 && idx < kDiffRange) {
            t.t1[static_cast<std::size_t>(idx)][static_cast<std::size_t>(pt)]++;
          }
        } else if (pattern.size() == 2) {
          const int ia = pattern[0] + kDiffOffset;
          const int ib = pattern[1] + kDiffOffset;
          if (ia >= 0 && ia < kDiffRange && ib >= 0 && ib < kDiffRange) {
            t.t2[static_cast<std::size_t>(ia * kDiffRange + ib)][static_cast<std::size_t>(pt)]++;
          }
        }
      }
    }
    return t;
  }();
  return tables;
}

long long evaluate_shape_adaptability(const std::vector<int>& col_heights) {
  const int n = static_cast<int>(col_heights.size());
  if (n <= 1) {
    return 0;
  }
  const int diff_size = n - 1;
  const AdaptLookupTables& tables = adapt_lookup_tables();
  std::array<int, 7> piece_matches{};

  for (int i = 0; i < diff_size; ++i) {
    const int d = col_heights[static_cast<std::size_t>(i + 1)] -
                  col_heights[static_cast<std::size_t>(i)];
    const int di = d + kDiffOffset;
    if (di >= 0 && di < kDiffRange) {
      const auto& row1 = tables.t1[static_cast<std::size_t>(di)];
      for (int pt = 1; pt < 7; ++pt) {
        piece_matches[static_cast<std::size_t>(pt)] += row1[static_cast<std::size_t>(pt)];
      }
      if (i + 1 < diff_size) {
        const int d2 = col_heights[static_cast<std::size_t>(i + 2)] -
                       col_heights[static_cast<std::size_t>(i + 1)];
        const int dj = d2 + kDiffOffset;
        if (dj >= 0 && dj < kDiffRange) {
          const auto& row2 = tables.t2[static_cast<std::size_t>(di * kDiffRange + dj)];
          for (int pt = 1; pt < 7; ++pt) {
            piece_matches[static_cast<std::size_t>(pt)] += row2[static_cast<std::size_t>(pt)];
          }
        }
      }
    }
  }

  long long total_score = 0;
  for (int pt = 1; pt < 7; ++pt) {
    total_score += adaptability_score_from_match_count(piece_matches[static_cast<std::size_t>(pt)]);
  }
  return total_score;
}

void evaluate_line_and_hole_metrics(const Grid& pool,
                                    const std::vector<int>& col_heights,
                                    BoardMetrics& m) {
  const int rows = pool.rows();
  const int cols = pool.cols();
  const std::uint16_t full_row_mask = pool.full_row_mask();
  const std::uint16_t x0_mask = x0_hole_mask(cols);
  const std::uint16_t x1_mask = x1_hole_mask(cols);
  const std::uint16_t x2_mask = x2_hole_mask(cols);
  std::uint16_t sky_mask = full_row_mask;
  int max_height = 0;
  for (const int h : col_heights) {
    max_height = std::max(max_height, h);
  }
  const int hole_start_row = rows - max_height;
  const std::uint16_t inner_row_transition_mask =
      static_cast<std::uint16_t>((cols > 1 ? (1u << (cols - 1)) - 1u : 0u));
  const std::uint16_t right_edge_bit = static_cast<std::uint16_t>(1u << (cols - 1));
  bool found_hole_depth = false;
  std::uint16_t prev_row = 0;
  for (int r = 0; r < rows; ++r) {
    const std::uint16_t row_bits = pool.row_bits(r);

    m.column_transitions +=
        bit_count(static_cast<std::uint16_t>((r == 0 ? row_bits : prev_row ^ row_bits)));
    prev_row = row_bits;

    if ((row_bits & 1u) == 0) {
      ++m.row_transitions;
    }
    m.row_transitions += bit_count(
        static_cast<std::uint16_t>((row_bits ^ (row_bits >> 1)) & inner_row_transition_mask));
    if ((row_bits & right_edge_bit) == 0) {
      ++m.row_transitions;
    }

    if (r >= hole_start_row) {
      const std::uint16_t hole_bits =
          static_cast<std::uint16_t>((~row_bits) & (~sky_mask) & full_row_mask);
      if (hole_bits != 0) {
        m.hole_score += bit_count(hole_bits);
        m.x0_hole += bit_count(static_cast<std::uint16_t>(hole_bits & x0_mask));
        m.x1_hole += bit_count(static_cast<std::uint16_t>(hole_bits & x1_mask));
        m.x2_hole += bit_count(static_cast<std::uint16_t>(hole_bits & x2_mask));
        if (!found_hole_depth) {
          for (int c = 0; c < cols; ++c) {
            if ((hole_bits & (1u << c)) != 0) {
              m.hole_depth =
                  std::max(m.hole_depth, col_heights[static_cast<std::size_t>(c)]);
            }
          }
          found_hole_depth = true;
        }
      }
      sky_mask = static_cast<std::uint16_t>(sky_mask & ~row_bits);
    }
  }
  m.column_transitions += bit_count(static_cast<std::uint16_t>((~prev_row) & full_row_mask));
}

int evaluate_well_depth(const std::vector<int>& col_heights,
                        int rows,
                        int cols,
                        const WellScoreTables& well_score_tables) {
  auto get_height = [&](int c) -> int {
    if (c < 0 || c >= cols) {
      return rows;
    }
    return col_heights[static_cast<std::size_t>(c)];
  };

  int well_depth_sum = 0;
  int well_depth_max = 0;
  for (int c = 0; c < cols; ++c) {
    const int center_h = get_height(c);
    const int left_h = get_height(c - 1);
    const int right_h = get_height(c + 1);
    if (left_h < center_h || right_h < center_h) {
      continue;
    }

    const int depth = std::min(left_h, right_h) - center_h;
    const int well_score =
        well_scores_for_col(well_score_tables, cols, c)[static_cast<std::size_t>(depth)];
    if (well_score <= 0) {
      continue;
    }
    well_depth_sum += well_score;
    well_depth_max = std::max(well_depth_max, well_score);
  }

  return 2 * well_depth_sum - well_depth_max;
}

// Table values are stored at 10x scale:
//   depth 1: 2*well2  (= well2/5 * 10, avoids /5)
//   depth 2: 10*well2
//   depth d>=3: 10*well3 + (d-2)*deep_score_x10
// deep_score_x10 for x0 col = 10*hole + 15*x1_hole (avoids *3/2).
// Caller must use coefficient 6 (= 60/10) when applying well_depth to the score.
std::vector<int> build_well_score_table(int rows,
                                        int well_depth_2,
                                        int well_depth_3,
                                        int deep_score_x10) {
  std::vector<int> table(static_cast<std::size_t>(rows + 1), 0);
  if (rows >= 1) {
    table[1] = 2 * well_depth_2;
  }
  if (rows >= 2) {
    table[2] = 10 * well_depth_2;
  }
  for (int depth = 3; depth <= rows; ++depth) {
    table[static_cast<std::size_t>(depth)] = 10 * well_depth_3 + (depth - 2) * deep_score_x10;
  }
  return table;
}

WellScoreTables build_well_score_tables(int rows, const AiWeights& weights) {
  return WellScoreTables{
      build_well_score_table(rows, weights.well_depth_2, weights.well_depth_3,
                             10 * weights.hole),
      build_well_score_table(rows, weights.well_depth_2, weights.well_depth_3,
                             10 * weights.hole + 15 * weights.x1_hole),
      build_well_score_table(rows, weights.well_depth_2, weights.well_depth_3,
                             10 * (weights.hole + weights.x1_hole)),
      build_well_score_table(rows, weights.well_depth_2, weights.well_depth_3,
                             10 * (weights.hole + weights.x2_hole)),
  };
}

bool same_well_score_weights(const AiWeights& a, const AiWeights& b) {
  return a.hole == b.hole && a.x1_hole == b.x1_hole && a.x2_hole == b.x2_hole &&
         a.well_depth_2 == b.well_depth_2 && a.well_depth_3 == b.well_depth_3;
}

int bit_count(std::uint16_t bits) {
  return __builtin_popcount(static_cast<unsigned>(bits));
}

std::uint16_t shifted_shape_row_bits(std::uint16_t row_bits, int x) {
  if (x >= 0) {
    return static_cast<std::uint16_t>(row_bits << x);
  }
  return static_cast<std::uint16_t>(row_bits >> -x);
}

std::uint16_t x0_hole_mask(int cols) {
  std::uint16_t mask = 1u;
  if (cols > 1) {
    mask |= static_cast<std::uint16_t>(1u << (cols - 1));
  }
  return mask;
}

std::uint16_t x1_hole_mask(int cols) {
  std::uint16_t mask = 0;
  if (cols > 2) {
    mask |= static_cast<std::uint16_t>(1u << 1);
    mask |= static_cast<std::uint16_t>(1u << (cols - 2));
  }
  return static_cast<std::uint16_t>(mask & ~x0_hole_mask(cols));
}

std::uint16_t x2_hole_mask(int cols) {
  std::uint16_t mask = 0;
  if (cols > 4) {
    mask |= static_cast<std::uint16_t>(1u << 2);
    mask |= static_cast<std::uint16_t>(1u << (cols - 3));
  }
  return static_cast<std::uint16_t>(mask & ~x0_hole_mask(cols) & ~x1_hole_mask(cols));
}

const std::vector<int>& well_scores_for_col(const WellScoreTables& tables, int cols, int c) {
  if (c == 0 || c == cols - 1) {
    return tables.x0;
  }
  if (c == 1 || c == cols - 2) {
    return tables.x1;
  }
  if (c == 2 || c == cols - 3) {
    return tables.x2;
  }
  return tables.normal;
}

BoardMetrics evaluate_board(const Grid& pool,
                            const std::vector<int>& col_heights,
                            const WellScoreTables& well_score_tables) {
  BoardMetrics m;
  const int rows = pool.rows();
  const int cols = pool.cols();
  if (col_heights.size() != static_cast<std::size_t>(cols)) {
    return m;
  }
  evaluate_line_and_hole_metrics(pool, col_heights, m);
  m.well_depth = evaluate_well_depth(col_heights, rows, cols, well_score_tables);
  m.shape_adaptability = evaluate_shape_adaptability(col_heights);

  return m;
}

constexpr int kSimpleRowTransitionsWeight = 100;

PlacementStats apply_lock_and_collect_stats(Grid& sim, const BitShape& shape, int x, int y) {
  PlacementStats stats;

  std::array<int, MAX_BOARD_ROWS> piece_cells_on_row{};
  for (int r = 0; r < SHAPE_SIZE; ++r) {
    const int board_r = y + r;
    const std::uint16_t board_bits = shifted_shape_row_bits(shape.rows[static_cast<std::size_t>(r)], x);
    if (board_bits == 0) {
      continue;
    }
    sim.set_row_bits_or(board_r, board_bits);
    std::uint16_t bits = board_bits;
    while (bits != 0) {
      ++piece_cells_on_row[static_cast<std::size_t>(board_r)];
      bits = static_cast<std::uint16_t>(bits & (bits - 1));
    }
  }

  int eroded_cells = 0;
  const std::uint16_t full_row_mask = sim.full_row_mask();
  for (int r = 0; r < sim.rows(); ++r) {
    if (sim.row_bits(r) == full_row_mask) {
      ++stats.rows_cleared;
      eroded_cells += piece_cells_on_row[static_cast<std::size_t>(r)];
    }
  }

  stats.eroded_piece_cells_metric = stats.rows_cleared * eroded_cells;
  clear_lines(sim);
  return stats;
}

}  // namespace

SimpleAi::SimpleAi(int /*seed*/) {}

void SimpleAi::set_weights(const AiWeights& weights) {
  weights_ = weights;
  well_score_tables_cache_valid_ = false;
}

const AiWeights& SimpleAi::weights() const { return weights_; }

void SimpleAi::set_second_layer_search_enabled(bool enabled) {
  second_layer_search_enabled_ = enabled;
}

bool SimpleAi::second_layer_search_enabled() const { return second_layer_search_enabled_; }

int SimpleAi::drop_y(const Grid& pool, const BitShape& shape, int x) const {
  return compute_drop_y(pool, shape, x);
}

const WellScoreTables& SimpleAi::well_score_tables_for_rows(int rows) const {
  if (!well_score_tables_cache_valid_ || well_score_tables_cache_rows_ != rows ||
      !same_well_score_weights(well_score_tables_cache_weights_, weights_)) {
    well_score_tables_cache_ = build_well_score_tables(rows, weights_);
    well_score_tables_cache_weights_ = weights_;
    well_score_tables_cache_rows_ = rows;
    well_score_tables_cache_valid_ = true;
  }
  return well_score_tables_cache_;
}

long long SimpleAi::evaluate(const Grid& pool,
                             const std::vector<int>& col_heights,
                             int rows_cleared,
                             int landing_height_x4,
                             int eroded_piece_cells_metric,
                             const WellScoreTables& well_score_tables) const {
  (void)eroded_piece_cells_metric;
  const BoardMetrics m = evaluate_board(pool, col_heights, well_score_tables);

  return 15LL * static_cast<long long>(weights_.landing_height) * landing_height_x4 +
         60LL * static_cast<long long>(weights_.rows_cleared) * rows_cleared -
         60LL * static_cast<long long>(kSimpleRowTransitionsWeight) * m.row_transitions -
         60LL * static_cast<long long>(weights_.vertical) * m.column_transitions -
         60LL * static_cast<long long>(weights_.hole) * m.hole_score -
         90LL * static_cast<long long>(weights_.x1_hole) * m.x0_hole -
         60LL * static_cast<long long>(weights_.x1_hole) * m.x1_hole -
         60LL * static_cast<long long>(weights_.x2_hole) * m.x2_hole -
         60LL * static_cast<long long>(weights_.hole_depth) * m.hole_depth -
         6LL * static_cast<long long>(m.well_depth) +
         10LL * static_cast<long long>(weights_.shape_adaptability) * m.shape_adaptability;
}

AiMove SimpleAi::choose(const Grid& pool, int piece_type, const std::vector<int>& col_heights) {
  const auto& rotations = all_bit_rotations();
  if (piece_type < 0 || piece_type >= static_cast<int>(rotations.size())) {
    return AiMove{};
  }
  if (col_heights.size() != static_cast<std::size_t>(pool.cols())) {
    return AiMove{};
  }
  const std::vector<BitShape>& shapes = rotations[static_cast<std::size_t>(piece_type)];
  const WellScoreTables& well_score_tables = well_score_tables_for_rows(pool.rows());

  struct CandidateMove {
    int shape_idx = 0;
    int x = 0;
    int center_bias = 0;
    long long coarse_score = 0;
    Grid board_after{1, 1};
  };

  constexpr long long kCoarsePruneGap = 200 * 60;
  constexpr std::array<int, 3> kHalfSearchTypes = {4, 6, 3};

  enum class LookaheadMode { None, Half, Full };

  LookaheadMode mode = LookaheadMode::None;
  if (second_layer_search_enabled_) {
    auto average_high = [](int rows, const std::vector<int>& heights) {
      long long total = 0;
      for (const int h : heights) {
        const int top_distance = rows - h;
        total += top_distance;
      }
      return static_cast<double>(total) / static_cast<double>(heights.size());
    };

    const double avg_high = average_high(pool.rows(), col_heights);
    if (avg_high < 8.0) {
      mode = LookaheadMode::Full;
    } else if (avg_high < 12.0) {
      mode = LookaheadMode::Half;
    }
  }

  auto best_coarse_for_piece =
      [this, &well_score_tables](const Grid& board, int piece_type) {
    const auto& piece_rotations = all_bit_rotations()[static_cast<std::size_t>(piece_type)];
    bool has_best = false;
    long long best = 0;
    Grid sim(board.rows(), board.cols());
    for (int shape_idx = 0; shape_idx < static_cast<int>(piece_rotations.size()); ++shape_idx) {
      const BitShape& shape = piece_rotations[static_cast<std::size_t>(shape_idx)];
      for (int x = -shape.min_x; x < board.cols() - shape.max_x; ++x) {
        const int y = drop_y(board, shape, x);
        if (y < 0) {
          continue;
        }
        sim = board;
        const PlacementStats stats = apply_lock_and_collect_stats(sim, shape, x, y);
        const long long score =
            evaluate(sim, sim.col_heights(), stats.rows_cleared,
                     4 * y + shape.landing_height_base,
                     stats.eroded_piece_cells_metric, well_score_tables);
        if (!has_best || score > best) {
          has_best = true;
          best = score;
        }
      }
    }
    if (!has_best) {
      return std::numeric_limits<long long>::min();
    }
    return best;
  };

  auto lookahead_score = [&](const Grid& board_after_first) {
    std::vector<int> piece_types;
    if (mode == LookaheadMode::Full) {
      piece_types = {1, 2, 3, 4, 5, 6};
    } else if (mode == LookaheadMode::Half) {
      piece_types.assign(kHalfSearchTypes.begin(), kHalfSearchTypes.end());
    } else {
        return std::numeric_limits<long long>::min();
    }

    long long total = 0;
    int valid_count = 0;
    for (const int piece_type : piece_types) {
      const long long best = best_coarse_for_piece(board_after_first, piece_type);
      if (best == std::numeric_limits<long long>::min()) {
        continue;
      }
      total += best;
      ++valid_count;
    }
    if (valid_count <= 0) {
      return std::numeric_limits<long long>::min();
    }
    return total / valid_count;
  };

  if (mode == LookaheadMode::None) {
    bool has_best = false;
    long long best_score = 0;
    int best_center_bias = 0;
    AiMove best_move{};
    Grid sim(pool.rows(), pool.cols());

    for (int shape_idx = 0; shape_idx < static_cast<int>(shapes.size()); ++shape_idx) {
      const BitShape& shape = shapes[static_cast<std::size_t>(shape_idx)];

      for (int x = -shape.min_x; x < pool.cols() - shape.max_x; ++x) {
        const int y = drop_y(pool, shape, x);
        if (y < 0) {
          continue;
        }

        sim = pool;
        const PlacementStats stats = apply_lock_and_collect_stats(sim, shape, x, y);
        const long long score =
            evaluate(sim, sim.col_heights(), stats.rows_cleared,
                     4 * y + shape.landing_height_base,
                     stats.eroded_piece_cells_metric, well_score_tables);
        const int landing_center_times_two = 2 * x + shape.min_x + shape.max_x;
        const int board_center_times_two = pool.cols() - 1;
        const int center_bias = std::abs(landing_center_times_two - board_center_times_two);
        if (!has_best || score > best_score ||
            (score == best_score &&
             (center_bias < best_center_bias ||
              (center_bias == best_center_bias && x < best_move.x)))) {
          has_best = true;
          best_score = score;
          best_center_bias = center_bias;
          best_move = AiMove{shape_idx, x};
        }
      }
    }

    return best_move;
  }

  std::vector<CandidateMove> candidates;
  candidates.reserve(64);
  Grid sim(pool.rows(), pool.cols());

  for (int shape_idx = 0; shape_idx < static_cast<int>(shapes.size()); ++shape_idx) {
    const BitShape& shape = shapes[static_cast<std::size_t>(shape_idx)];

    for (int x = -shape.min_x; x < pool.cols() - shape.max_x; ++x) {
      const int y = drop_y(pool, shape, x);
      if (y < 0) {
        continue;
      }

      sim = pool;
      const PlacementStats stats = apply_lock_and_collect_stats(sim, shape, x, y);
      const long long score =
          evaluate(sim, sim.col_heights(), stats.rows_cleared,
                   4 * y + shape.landing_height_base,
                   stats.eroded_piece_cells_metric, well_score_tables);
      const int landing_center_times_two = 2 * x + shape.min_x + shape.max_x;
      const int board_center_times_two = pool.cols() - 1;
      const int center_bias = std::abs(landing_center_times_two - board_center_times_two);
      candidates.push_back(CandidateMove{shape_idx, x, center_bias, score, sim});
    }
  }

  if (candidates.empty()) {
    return AiMove{};
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const CandidateMove& a, const CandidateMove& b) {
              if (a.coarse_score != b.coarse_score) {
                return a.coarse_score > b.coarse_score;
              }
              if (a.center_bias != b.center_bias) {
                return a.center_bias < b.center_bias;
              }
              return a.x < b.x;
            });

  constexpr int top_n = 8;
  const int limit = std::min(top_n, static_cast<int>(candidates.size()));
  bool has_best = false;
  long long best_score = 0;
  AiMove best_move{candidates.front().shape_idx, candidates.front().x};
  for (int i = 0; i < limit; ++i) {
    const CandidateMove& candidate = candidates[static_cast<std::size_t>(i)];
    if (has_best && candidate.coarse_score + kCoarsePruneGap < best_score) {
      break;
    }

    const long long refined_score = lookahead_score(candidate.board_after);
    const long long combined_score =
        (refined_score != std::numeric_limits<long long>::min())
            ? (candidate.coarse_score + refined_score + 50000LL)
            : candidate.coarse_score;
    if (!has_best || combined_score > best_score) {
      has_best = true;
      best_score = combined_score;
      best_move = AiMove{candidate.shape_idx, candidate.x};
    }
  }

  return best_move;
}
