#include "simple_ai.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

namespace {

constexpr double kRowTransitionsWeight = 100.0;

struct BoardMetrics {
  long long row_transitions = 0;
  long long column_transitions = 0;
  long long hole_score = 0;
  long long x1_hole = 0;
  double hole_depth = 0.0;
  double well_depth = 0.0;
  long long shape_adaptability = 0;
};

std::vector<int> piece_bottom_profile(const Shape& shape) {
  const auto [min_c, max_c] = x_bounds(shape);
  std::vector<int> bottom;
  for (int c = min_c; c <= max_c; ++c) {
    int bottom_r = -1;
    for (int r = SHAPE_SIZE - 1; r >= 0; --r) {
      if (shape[r][c] == 1) {
        bottom_r = r;
        break;
      }
    }
    bottom.push_back(bottom_r);
  }
  return bottom;
}

std::vector<int> profile_diff(const std::vector<int>& profile) {
  std::vector<int> out;
  for (std::size_t i = 1; i < profile.size(); ++i) {
    out.push_back(profile[i] - profile[i - 1]);
  }
  return out;
}

std::vector<int> negate_diff(std::vector<int> diff) {
  for (int& v : diff) {
    v = -v;
  }
  return diff;
}

const std::vector<std::vector<std::vector<int>>>& piece_diff_patterns() {
  static const std::vector<std::vector<std::vector<int>>> patterns = [] {
    std::vector<std::vector<std::vector<int>>> out(7);
    const auto& rotations = all_rotations();
    for (int piece_type = 0; piece_type < static_cast<int>(rotations.size()); ++piece_type) {
      std::set<std::vector<int>> unique;
      for (const auto& shape : rotations[static_cast<std::size_t>(piece_type)]) {
        const auto [min_c, max_c] = x_bounds(shape);
        if (max_c <= min_c) {
          continue;
        }
        const std::vector<int> diff = profile_diff(piece_bottom_profile(shape));
        if (!diff.empty()) {
          unique.insert(diff);
          unique.insert(negate_diff(diff));
        }
      }
      out[static_cast<std::size_t>(piece_type)] =
          std::vector<std::vector<int>>(unique.begin(), unique.end());
    }
    return out;
  }();
  return patterns;
}

long long adaptability_score(int matches) {
  if (matches <= 0) {
    return 0;
  }
  if (matches == 1) {
    return 4;
  }
  if (matches == 2) {
    return 6;
  }
  return 7;
}

long long evaluate_shape_adaptability(const std::vector<int>& col_heights) {
  if (col_heights.size() <= 1) {
    return 0;
  }
  std::vector<int> board_diff;
  for (std::size_t i = 1; i < col_heights.size(); ++i) {
    board_diff.push_back(col_heights[i] - col_heights[i - 1]);
  }

  long long total = 0;
  const auto& patterns = piece_diff_patterns();
  for (std::size_t piece_type = 1; piece_type < patterns.size(); ++piece_type) {
    int matches = 0;
    for (const auto& pattern : patterns[piece_type]) {
      if (pattern.size() > board_diff.size()) {
        continue;
      }
      for (std::size_t start = 0; start + pattern.size() <= board_diff.size(); ++start) {
        bool ok = true;
        for (std::size_t i = 0; i < pattern.size(); ++i) {
          if (board_diff[start + i] != pattern[i]) {
            ok = false;
            break;
          }
        }
        if (ok) {
          ++matches;
        }
      }
    }
    total += adaptability_score(matches);
  }
  return total;
}

BoardMetrics evaluate_board(const Grid& board, const std::vector<int>& col_heights) {
  BoardMetrics m;
  const int rows = board.rows();
  const int cols = board.cols();
  std::vector<int> highest_hole_y_by_col(static_cast<std::size_t>(cols), rows);

  for (int c = 0; c < cols; ++c) {
    const int h = col_heights[static_cast<std::size_t>(c)];
    for (int r = rows - h; r < rows; ++r) {
      if (!board.test(r, c)) {
        ++m.hole_score;
        if (c <= 1 || c >= cols - 2) {
          ++m.x1_hole;
        }
        highest_hole_y_by_col[static_cast<std::size_t>(c)] =
            std::min(highest_hole_y_by_col[static_cast<std::size_t>(c)], r);
      }
    }
  }

  for (int c = 0; c < cols; ++c) {
    int prev = 0;
    for (int r = 0; r < rows; ++r) {
      const int cur = board.test(r, c) ? 1 : 0;
      if (cur != prev) {
        ++m.column_transitions;
        prev = cur;
      }
    }
    if (prev == 0) {
      ++m.column_transitions;
    }
  }

  int highest_hole_y = rows;
  for (int c = 0; c < cols; ++c) {
    const int y = highest_hole_y_by_col[static_cast<std::size_t>(c)];
    if (y >= rows) {
      continue;
    }
    if (y < highest_hole_y) {
      highest_hole_y = y;
      m.hole_depth = col_heights[static_cast<std::size_t>(c)];
    } else if (y == highest_hole_y) {
      m.hole_depth = std::max<double>(m.hole_depth, col_heights[static_cast<std::size_t>(c)]);
    }
  }

  for (int r = 0; r < rows; ++r) {
    int prev = 1;
    for (int c = 0; c < cols; ++c) {
      const int cur = board.test(r, c) ? 1 : 0;
      if (cur != prev) {
        ++m.row_transitions;
        prev = cur;
      }
    }
    if (prev != 1) {
      ++m.row_transitions;
    }
  }

  auto get_height = [&](int c) {
    return (c < 0 || c >= cols) ? rows : col_heights[static_cast<std::size_t>(c)];
  };
  double well_sum = 0.0;
  double well_max = 0.0;
  for (int c = 0; c < cols; ++c) {
    const int depth = std::min(get_height(c - 1), get_height(c + 1)) - get_height(c);
    if (depth > 0) {
      const double score = static_cast<double>(depth) * static_cast<double>(depth + 1) / 2.0;
      well_sum += score;
      well_max = std::max(well_max, score);
    }
  }
  m.well_depth = 2.0 * well_sum - well_max;
  m.shape_adaptability = evaluate_shape_adaptability(col_heights);
  return m;
}

int lock_shape_and_clear(Grid& sim, const Shape& shape, int x, int y) {
  for (int r = 0; r < SHAPE_SIZE; ++r) {
    for (int c = 0; c < SHAPE_SIZE; ++c) {
      if (shape[r][c] == 1) {
        sim.set(y + r, x + c);
      }
    }
  }
  return clear_lines(sim);
}

}  // namespace

double SimpleAi::evaluate(const Grid& board,
                          const std::vector<int>& col_heights,
                          int rows_cleared,
                          double landing_height) const {
  const BoardMetrics m = evaluate_board(board, col_heights);
  return static_cast<double>(weights_.landing_height) * landing_height +
         static_cast<double>(weights_.rows_cleared) * static_cast<double>(rows_cleared) -
         kRowTransitionsWeight * static_cast<double>(m.row_transitions) -
         static_cast<double>(weights_.vertical) * static_cast<double>(m.column_transitions) -
         static_cast<double>(weights_.hole) * static_cast<double>(m.hole_score) -
         static_cast<double>(weights_.x1_hole) * static_cast<double>(m.x1_hole) -
         static_cast<double>(weights_.hole_depth) * static_cast<double>(m.hole_depth) -
         static_cast<double>(weights_.well_depth) * static_cast<double>(m.well_depth) +
         static_cast<double>(weights_.shape_adaptability) *
             static_cast<double>(m.shape_adaptability) / 6.0;
}

AiMove SimpleAi::choose(const Grid& board, int piece_type, const std::vector<int>& col_heights) {
  (void)col_heights;
  const auto& shapes = all_rotations()[static_cast<std::size_t>(piece_type)];
  const auto& landing_bases = all_landing_height_bases()[static_cast<std::size_t>(piece_type)];
  bool has_best = false;
  double best_score = -std::numeric_limits<double>::infinity();
  int best_center_bias = 0;
  AiMove best_move{};

  for (int shape_idx = 0; shape_idx < static_cast<int>(shapes.size()); ++shape_idx) {
    const Shape& shape = shapes[static_cast<std::size_t>(shape_idx)];
    const auto [min_c, max_c] = x_bounds(shape);
    for (int x = -min_c; x < board.cols() - max_c; ++x) {
      const int y = compute_drop_y(board, shape, x);
      if (y < 0) {
        continue;
      }
      Grid sim = board;
      const int rows_cleared = lock_shape_and_clear(sim, shape, x, y);
      const double landing_height =
          static_cast<double>(y) + static_cast<double>(landing_bases[static_cast<std::size_t>(shape_idx)]) / 4.0;
      const double score = evaluate(sim, sim.col_heights(), rows_cleared, landing_height);
      const int landing_center_times_two = 2 * x + min_c + max_c;
      const int board_center_times_two = board.cols() - 1;
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
