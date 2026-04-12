#include "tetris_core.hpp"

#include <algorithm>

namespace {

std::vector<std::vector<Shape>> build_rotations() {
  const std::vector<Shape> I = {
      {{{{1, 1, 1, 1}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 0, 1, 0}}, {{0, 0, 1, 0}}, {{0, 0, 1, 0}}, {{0, 0, 1, 0}}}},
  };
  const std::vector<Shape> J = {
      {{{{1, 0, 0, 0}}, {{1, 1, 1, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 1, 1, 0}}, {{0, 1, 0, 0}}, {{0, 1, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{1, 1, 1, 0}}, {{0, 0, 1, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 1, 0, 0}}, {{0, 1, 0, 0}}, {{1, 1, 0, 0}}, {{0, 0, 0, 0}}}},
  };
  const std::vector<Shape> L = {
      {{{{0, 0, 1, 0}}, {{1, 1, 1, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 1, 0, 0}}, {{0, 1, 0, 0}}, {{0, 1, 1, 0}}, {{0, 0, 0, 0}}}},
      {{{{1, 1, 1, 0}}, {{1, 0, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{1, 1, 0, 0}}, {{0, 1, 0, 0}}, {{0, 1, 0, 0}}, {{0, 0, 0, 0}}}},
  };
  const std::vector<Shape> O = {
      {{{{1, 1, 0, 0}}, {{1, 1, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
  };
  const std::vector<Shape> S = {
      {{{{0, 1, 1, 0}}, {{1, 1, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 1, 0, 0}}, {{0, 1, 1, 0}}, {{0, 0, 1, 0}}, {{0, 0, 0, 0}}}},
  };
  const std::vector<Shape> T = {
      {{{{0, 1, 0, 0}}, {{1, 1, 1, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 1, 0, 0}}, {{0, 1, 1, 0}}, {{0, 1, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{1, 1, 1, 0}}, {{0, 1, 0, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 1, 0, 0}}, {{1, 1, 0, 0}}, {{0, 1, 0, 0}}, {{0, 0, 0, 0}}}},
  };
  const std::vector<Shape> Z = {
      {{{{1, 1, 0, 0}}, {{0, 1, 1, 0}}, {{0, 0, 0, 0}}, {{0, 0, 0, 0}}}},
      {{{{0, 0, 1, 0}}, {{0, 1, 1, 0}}, {{0, 1, 0, 0}}, {{0, 0, 0, 0}}}},
  };
  return {I, J, L, O, S, T, Z};
}

}  // namespace

Grid::Grid(int rows, int cols) : rows_(rows), cols_(cols) {
  if (rows_ <= 0 || rows_ > MAX_BOARD_ROWS) {
    throw std::invalid_argument("rows must be in [1, 31]");
  }
  if (cols_ <= 0 || cols_ > MAX_BOARD_COLS) {
    throw std::invalid_argument("cols must be in [1, 14]");
  }
  full_row_mask_ = static_cast<std::uint16_t>((1u << cols_) - 1u);
  rows_data_.assign(static_cast<std::size_t>(rows_), 0);
  col_heights_.assign(static_cast<std::size_t>(cols_), 0);
}

bool Grid::test(int r, int c) const {
  return (rows_data_[static_cast<std::size_t>(r)] & (static_cast<std::uint16_t>(1u) << c)) != 0;
}

void Grid::set(int r, int c) {
  const std::uint16_t mask = static_cast<std::uint16_t>(1u) << c;
  rows_data_[static_cast<std::size_t>(r)] |= mask;
  col_heights_[static_cast<std::size_t>(c)] =
      std::max(col_heights_[static_cast<std::size_t>(c)], rows_ - r);
}

void Grid::recompute_col_heights() {
  std::fill(col_heights_.begin(), col_heights_.end(), 0);
  for (int c = 0; c < cols_; ++c) {
    for (int r = 0; r < rows_; ++r) {
      if (test(r, c)) {
        col_heights_[static_cast<std::size_t>(c)] = rows_ - r;
        break;
      }
    }
  }
}

const std::vector<std::vector<Shape>>& all_rotations() {
  static const std::vector<std::vector<Shape>> rotations = build_rotations();
  return rotations;
}

const std::vector<std::vector<int>>& all_landing_height_bases() {
  static const std::vector<std::vector<int>> bases = [] {
    std::vector<std::vector<int>> out;
    for (const auto& piece_rotations : all_rotations()) {
      std::vector<int> piece_bases;
      for (const auto& shape : piece_rotations) {
        int base = 0;
        for (int r = 0; r < SHAPE_SIZE; ++r) {
          for (int c = 0; c < SHAPE_SIZE; ++c) {
            if (shape[r][c] == 1) {
              base += r;
            }
          }
        }
        piece_bases.push_back(base);
      }
      out.push_back(piece_bases);
    }
    return out;
  }();
  return bases;
}

std::pair<int, int> x_bounds(const Shape& shape) {
  int min_x = SHAPE_SIZE;
  int max_x = -1;
  for (int r = 0; r < SHAPE_SIZE; ++r) {
    for (int c = 0; c < SHAPE_SIZE; ++c) {
      if (shape[r][c] == 1) {
        min_x = std::min(min_x, c);
        max_x = std::max(max_x, c);
      }
    }
  }
  return {min_x, max_x};
}

int compute_drop_y(const Grid& board, const Shape& shape, int x) {
  const auto [min_x, max_x] = x_bounds(shape);
  if (max_x < 0 || x + min_x < 0 || x + max_x >= board.cols()) {
    return -1;
  }
  for (int y = 0; y < board.rows() + SHAPE_SIZE; ++y) {
    for (int r = 0; r < SHAPE_SIZE; ++r) {
      for (int c = 0; c < SHAPE_SIZE; ++c) {
        if (shape[r][c] != 1) {
          continue;
        }
        const int board_r = y + r;
        const int board_c = x + c;
        if (board_r >= board.rows() || board.test(board_r, board_c)) {
          return y > 0 ? y - 1 : -1;
        }
      }
    }
  }
  return -1;
}

bool lock_shape(Grid& board, const Shape& shape, int x, int y) {
  if (y < 0) {
    return false;
  }
  for (int r = 0; r < SHAPE_SIZE; ++r) {
    for (int c = 0; c < SHAPE_SIZE; ++c) {
      if (shape[r][c] == 1) {
        board.set(y + r, x + c);
      }
    }
  }
  return true;
}

int clear_lines(Grid& board) {
  int lines = 0;
  int write_row = board.rows_ - 1;
  for (int read_row = board.rows_ - 1; read_row >= 0; --read_row) {
    if (board.rows_data_[static_cast<std::size_t>(read_row)] == board.full_row_mask_) {
      ++lines;
      continue;
    }
    board.rows_data_[static_cast<std::size_t>(write_row)] =
        board.rows_data_[static_cast<std::size_t>(read_row)];
    --write_row;
  }
  while (write_row >= 0) {
    board.rows_data_[static_cast<std::size_t>(write_row)] = 0;
    --write_row;
  }
  if (lines > 0) {
    board.recompute_col_heights();
  }
  return lines;
}

TetrisAiGame::TetrisAiGame(IAi& ai, int rows, int cols, int seed)
    : ai_(ai), rng_(make_seeded_rng(seed)), board_(rows, cols) {}

GameResult TetrisAiGame::play() {
  GameResult result;
  std::uniform_int_distribution<int> piece_dist(0, static_cast<int>(all_rotations().size()) - 1);
  while (true) {
    const int piece_type = piece_dist(rng_);
    const auto& shapes = all_rotations()[static_cast<std::size_t>(piece_type)];
    const AiMove move = ai_.choose(board_, piece_type, board_.col_heights());
    const int shape_idx = std::max(0, std::min(static_cast<int>(shapes.size()) - 1, move.shape_idx));
    const Shape& shape = shapes[static_cast<std::size_t>(shape_idx)];
    const int y = compute_drop_y(board_, shape, move.x);
    if (!lock_shape(board_, shape, move.x, y)) {
      break;
    }
    result.lines_cleared += clear_lines(board_);
    ++result.pieces_used;
  }
  result.score = result.lines_cleared > 0 ? static_cast<double>(result.lines_cleared)
                                          : static_cast<double>(result.pieces_used) / 1000.0;
  return result;
}
