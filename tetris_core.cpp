#include "tetris_core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

using ShapeRows = std::array<std::uint16_t, SHAPE_SIZE>;

char piece_type_letter(int piece_type) {
  static constexpr char kLetters[7] = {'I', 'J', 'L', 'O', 'S', 'T', 'Z'};
  if (piece_type < 0 || piece_type >= 7) {
    return '?';
  }
  return kLetters[static_cast<std::size_t>(piece_type)];
}

BitShape make_bit_shape(const ShapeRows& rows) {
  BitShape bit_shape;
  bit_shape.bottom_by_col.fill(-1);
  for (int r = 0; r < SHAPE_SIZE; ++r) {
    const std::uint16_t row_bits = rows[static_cast<std::size_t>(r)];
    bit_shape.rows[static_cast<std::size_t>(r)] = row_bits;
    std::uint16_t bits = row_bits;
    while (bits != 0) {
      const int c = __builtin_ctz(static_cast<unsigned>(bits));
      bit_shape.min_x = std::min(bit_shape.min_x, c);
      bit_shape.max_x = std::max(bit_shape.max_x, c);
      bit_shape.bottom_by_col[static_cast<std::size_t>(c)] =
          std::max(bit_shape.bottom_by_col[static_cast<std::size_t>(c)], r);
      bit_shape.landing_height_base += r;
      bits = static_cast<std::uint16_t>(bits & ~(1u << c));
    }
  }
  return bit_shape;
}

std::vector<std::vector<BitShape>> build_bit_rotations() {
  // All pieces are ceiling-aligned (topmost row contains at least one block)
  const std::vector<ShapeRows> I = {
      {{0b1111, 0, 0, 0}},        // horizontal
      {{0b0100, 0b0100, 0b0100, 0b0100}},  // vertical
  };

  const std::vector<ShapeRows> J = {
      {{0b0001, 0b0111, 0, 0}},  // spawn
      {{0b0110, 0b0010, 0b0010, 0}},  // CW 90
      {{0b0111, 0b0100, 0, 0}},  // CW 180 (shifted up)
      {{0b0010, 0b0010, 0b0011, 0}},  // CW 270 (shifted up)
  };

  const std::vector<ShapeRows> L = {
      {{0b0100, 0b0111, 0, 0}},  // spawn
      {{0b0010, 0b0010, 0b0110, 0}},  // CW 90
      {{0b0111, 0b0001, 0, 0}},  // CW 180 (shifted up)
      {{0b0011, 0b0010, 0b0010, 0}},  // CW 270
  };

  const std::vector<ShapeRows> O = {
      {{0b0011, 0b0011, 0, 0}},  // square
  };

  const std::vector<ShapeRows> S = {
      {{0b0110, 0b0011, 0, 0}},  // spawn
      {{0b0010, 0b0110, 0b0100, 0}},  // CW 90 (shifted up)
  };

  const std::vector<ShapeRows> T = {
      {{0b0010, 0b0111, 0, 0}},  // spawn
      {{0b0010, 0b0110, 0b0010, 0}},  // CW 90
      {{0b0111, 0b0010, 0, 0}},  // CW 180 (shifted up)
      {{0b0010, 0b0011, 0b0010, 0}},  // CW 270
  };

  const std::vector<ShapeRows> Z = {
      {{0b0011, 0b0110, 0, 0}},  // spawn
      {{0b0100, 0b0110, 0b0010, 0}},  // CW 90 (shifted up)
  };

  const std::vector<std::vector<ShapeRows>> rows_by_piece = {
      I,
      J,
      L,
      O,
      S,
      T,
      Z,
  };

  std::vector<std::vector<BitShape>> out;
  out.reserve(rows_by_piece.size());
  for (const auto& piece_rows : rows_by_piece) {
    std::vector<BitShape> piece_rotations;
    piece_rotations.reserve(piece_rows.size());
    for (const ShapeRows& rows : piece_rows) {
      piece_rotations.push_back(make_bit_shape(rows));
    }
    out.push_back(std::move(piece_rotations));
  }
  return out;
}

std::uint16_t shifted_shape_row_bits(std::uint16_t row_bits, int x) {
  if (x >= 0) {
    return static_cast<std::uint16_t>(row_bits << x);
  }
  return static_cast<std::uint16_t>(row_bits >> -x);
}

}  // namespace

const std::vector<std::vector<BitShape>>& all_bit_rotations() {
  static const std::vector<std::vector<BitShape>> bit_rotations = build_bit_rotations();
  return bit_rotations;
}

const std::vector<std::vector<int>>& all_landing_height_bases() {
  static const std::vector<std::vector<int>> bases = [] {
    std::vector<std::vector<int>> out;
    const auto& rotations = all_bit_rotations();
    out.reserve(rotations.size());
    for (const auto& piece_rotations : rotations) {
      std::vector<int> piece_bases;
      piece_bases.reserve(piece_rotations.size());
      for (const auto& shape : piece_rotations) {
        piece_bases.push_back(shape.landing_height_base);
      }
      out.push_back(piece_bases);
    }
    return out;
  }();
  return bases;
}

int compute_drop_y(const Grid& pool, const BitShape& shape, int x) {
  if (shape.max_x < 0) {
    return -1;
  }
  if (x + shape.min_x < 0 || x + shape.max_x >= pool.cols()) {
    return -1;
  }

  int drop_y = pool.rows() + SHAPE_SIZE;
  const std::vector<int>& col_heights = pool.col_heights();
  for (int c = shape.min_x; c <= shape.max_x; ++c) {
    const int bottom_r = shape.bottom_by_col[static_cast<std::size_t>(c)];
    if (bottom_r < 0) {
      continue;
    }
    const int board_c = x + c;
    const int top_filled = pool.rows() - col_heights[static_cast<std::size_t>(board_c)];
    drop_y = std::min(drop_y, top_filled - bottom_r - 1);
  }

  return drop_y >= 0 ? drop_y : -1;
}

TetrisAiGame::TetrisAiGame(IAi& ai, int rows, int cols, int seed, std::int64_t max_lines)
    : TetrisAiGame(ai, rows, cols, GameOptions{seed, max_lines, nullptr}) {}

TetrisAiGame::TetrisAiGame(IAi& ai, int rows, int cols, const GameOptions& options)
    : ai_(ai),
      rng_(make_seeded_rng(options.seed)),
      pool_(rows, cols),
      max_lines_(options.max_lines),
      hook_(options.hook) {}

int TetrisAiGame::generate_piece_type() {
  const auto& rotations = all_bit_rotations();
  if (hook_ != nullptr) {
    const int hooked = hook_->next_piece_type(rng_);
    if (hooked >= 0 && hooked < static_cast<int>(rotations.size())) {
      return hooked;
    }
  }
  return piece_rng_.next();
}

double fitted_avg_height_decay_ratio(const std::vector<std::int64_t>& avg_height_histogram) {
  if (avg_height_histogram.size() <= 4) {
    return 1.0;
  }

  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<double> ws;
  xs.reserve(avg_height_histogram.size());
  ys.reserve(avg_height_histogram.size());
  ws.reserve(avg_height_histogram.size());

  for (std::size_t bucket = 4, x = 0; bucket < avg_height_histogram.size(); ++bucket, ++x) {
    const std::int64_t count = avg_height_histogram[bucket];
    if (count < 10) {
      break;
    }
    xs.push_back(static_cast<double>(x));
    ys.push_back(std::log(static_cast<double>(count)));
    ws.push_back(static_cast<double>(count));
  }

  const int n = static_cast<int>(xs.size());
  if (n <= 1) {
    return 1.0;
  }

  double w_sum = 0.0;
  double wx_sum = 0.0;
  double wy_sum = 0.0;
  for (int i = 0; i < n; ++i) {
    w_sum += ws[static_cast<std::size_t>(i)];
    wx_sum += ws[static_cast<std::size_t>(i)] * xs[static_cast<std::size_t>(i)];
    wy_sum += ws[static_cast<std::size_t>(i)] * ys[static_cast<std::size_t>(i)];
  }
  if (w_sum <= 0.0) {
    return 1.0;
  }

  const double mean_x = wx_sum / w_sum;
  const double mean_y = wy_sum / w_sum;

  double num = 0.0;
  double den = 0.0;
  for (int i = 0; i < n; ++i) {
    const double dx = xs[static_cast<std::size_t>(i)] - mean_x;
    const double dy = ys[static_cast<std::size_t>(i)] - mean_y;
    num += ws[static_cast<std::size_t>(i)] * dx * dy;
    den += ws[static_cast<std::size_t>(i)] * dx * dx;
  }
  if (std::abs(den) < 1e-12) {
    return 1.0;
  }

  const double slope = num / den;
  const double ratio = std::exp(slope);
  return std::isfinite(ratio) && ratio > 0.0 ? ratio : 1.0;
}

double weighted_avg_height_adjacent_ratio(const std::vector<std::int64_t>& avg_height_histogram) {
  if (avg_height_histogram.size() <= 5) {
    return 1.0;
  }

  double head = 0.0;
  for (std::size_t bucket = 0; bucket <= 4; ++bucket) {
    head += static_cast<double>(avg_height_histogram[bucket]);
  }
  if (head <= 0.0) {
    return 1.0;
  }

  std::vector<double> ratios;
  for (std::size_t bucket = 5; bucket < avg_height_histogram.size(); ++bucket) {
    const double tail = static_cast<double>(avg_height_histogram[bucket]);
    if (tail <= 0.0) {
      continue;
    }
    if (tail < 500.0) {
      break;
    }
    const double exponent = 1.0 / static_cast<double>(bucket - 1);
    const double ratio = std::pow(tail / head, exponent);
    if (std::isfinite(ratio) && ratio > 0.0) {
      ratios.push_back(ratio);
    }
  }

  if (ratios.empty()) {
    return 1.0;
  }

  double sum = 0.0;
  for (double ratio : ratios) {
    sum += ratio;
  }

  const double avg = sum / static_cast<double>(ratios.size());
  return std::isfinite(avg) && avg > 0.0 ? avg : 1.0;
}

int count_holes(const Grid& pool) {
  int holes = 0;
  for (int c = 0; c < pool.cols(); ++c) {
    bool seen_filled = false;
    for (int r = 0; r < pool.rows(); ++r) {
      if (pool.test(r, c)) {
        seen_filled = true;
      } else if (seen_filled) {
        ++holes;
      }
    }
  }
  return holes;
}

bool lock_shape(Grid& pool, const BitShape& shape, int x, int y) {
  if (y < 0) {
    return false;
  }
  for (int r = 0; r < SHAPE_SIZE; ++r) {
    const std::uint16_t row_bits = shape.rows[static_cast<std::size_t>(r)];
    if (row_bits == 0) {
      continue;
    }
    pool.set_row_bits_or(y + r, shifted_shape_row_bits(row_bits, x));
  }
  return true;
}

GameResult TetrisAiGame::play() {
  std::int64_t lines_cleared = 0;
  std::int64_t placed = 0;
  std::vector<std::int64_t> avg_height_histogram(static_cast<std::size_t>(pool_.rows()), 0);
  std::string recent_piece_types;
  bool topped_out = false;
  bool stopped_by_max_lines = false;
  GameResult result;
  result.min_holes_seen = count_holes(pool_);
  if (hook_ != nullptr && !hook_->on_game_start(pool_, result)) {
    result.holes_at_end = count_holes(pool_);
    const double hook_score = hook_->final_score(pool_, result);
    if (std::isfinite(hook_score)) {
      result.score = hook_score;
    } else if (lines_cleared == 0) {
      result.score = static_cast<double>(placed) / 1000.0;
    } else {
      result.score = static_cast<double>(lines_cleared);
    }
    return result;
  }

  while (!game_over_) {
    const int piece_type = generate_piece_type();
    recent_piece_types.push_back(piece_type_letter(piece_type));
    if (recent_piece_types.size() > 100) {
      recent_piece_types.erase(0, recent_piece_types.size() - 100);
    }
    const std::vector<BitShape>& bit_shapes = all_bit_rotations()[piece_type];
    if (bit_shapes.empty()) {
      game_over_ = true;
      break;
    }

    const AiMove move = ai_.choose(pool_, piece_type, pool_.col_heights());
    const int shape_idx = std::max(0, std::min(static_cast<int>(bit_shapes.size()) - 1, move.shape_idx));
    const BitShape& shape = bit_shapes[static_cast<std::size_t>(shape_idx)];

    const int y = compute_drop_y(pool_, shape, move.x);

    if (!lock_shape(pool_, shape, move.x, y)) {
      game_over_ = true;
      topped_out = true;
      break;
    }

    lines_cleared += clear_lines(pool_);
    ++placed;
    result.pieces_used = placed;
    result.lines_cleared = lines_cleared;

    if (!avg_height_histogram.empty()) {
      std::int64_t total_height = 0;
      for (const int h : pool_.col_heights()) {
        total_height += h;
      }
      const double avg_height =
          static_cast<double>(total_height) / static_cast<double>(pool_.col_heights().size());
      int bucket = static_cast<int>(avg_height);
      bucket = std::max(0, std::min(static_cast<int>(avg_height_histogram.size()) - 1, bucket));
      ++avg_height_histogram[static_cast<std::size_t>(bucket)];
    }

    if (max_lines_ > 0 && lines_cleared >= max_lines_) {
      stopped_by_max_lines = true;
      break;
    }

    if (hook_ != nullptr && !hook_->on_after_piece(pool_, result)) {
      break;
    }

  }

  result.pieces_used = placed;
  result.lines_cleared = lines_cleared;
  result.avg_height_histogram = std::move(avg_height_histogram);
  result.top_out_last_pieces = topped_out ? recent_piece_types : std::string{};
  result.topped_out = topped_out;
  result.stopped_by_max_lines = stopped_by_max_lines;
  result.holes_at_end = count_holes(pool_);
  const double hook_score = hook_ != nullptr ? hook_->final_score(pool_, result)
                                             : std::numeric_limits<double>::quiet_NaN();
  if (std::isfinite(hook_score)) {
    result.score = hook_score;
  } else if (lines_cleared == 0) {
    result.score = static_cast<double>(placed) / 1000.0;
  } else {
    result.score = static_cast<double>(lines_cleared);
  }
  return result;
}

int clear_lines(Grid& pool) {
  int lines = 0;
  int write_row = pool.rows() - 1;
  std::uint32_t cleared_rows_mask = 0;

  for (int read_row = pool.rows() - 1; read_row >= 0; --read_row) {
    const bool full = pool.row_bits(read_row) == pool.full_row_mask();

    if (!full) {
      if (read_row != write_row) {
        pool.set_row_bits(write_row, pool.row_bits(read_row));
      }
      --write_row;
    } else {
      ++lines;
      cleared_rows_mask |= (1u << static_cast<std::uint32_t>(read_row));
    }
  }

  for (int row = 0; row <= write_row; ++row) {
    pool.set_row_bits(row, 0);
  }

  pool.apply_cleared_lines(lines, cleared_rows_mask);
  return lines;
}
