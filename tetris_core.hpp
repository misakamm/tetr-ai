#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include "rng_engine.hpp"

constexpr int SHAPE_SIZE = 4;
constexpr int MAX_BOARD_ROWS = 31;
constexpr int MAX_BOARD_COLS = 14;

using Shape = std::array<std::array<std::int8_t, SHAPE_SIZE>, SHAPE_SIZE>;

struct AiMove {
  int shape_idx = 0;
  int x = 0;
};

class Grid {
 public:
  Grid(int rows, int cols);

  int rows() const { return rows_; }
  int cols() const { return cols_; }
  std::uint16_t full_row_mask() const { return full_row_mask_; }
  std::uint16_t row_bits(int r) const { return rows_data_[static_cast<std::size_t>(r)]; }
  const std::vector<int>& col_heights() const { return col_heights_; }

  bool test(int r, int c) const;
  void set(int r, int c);

 private:
  friend int clear_lines(Grid& board);

  void recompute_col_heights();

  int rows_ = 0;
  int cols_ = 0;
  std::uint16_t full_row_mask_ = 0;
  std::vector<std::uint16_t> rows_data_;
  std::vector<int> col_heights_;
};

class IAi {
 public:
  virtual ~IAi() = default;
  virtual AiMove choose(const Grid& board, int piece_type, const std::vector<int>& col_heights) = 0;
};

struct GameResult {
  std::int64_t pieces_used = 0;
  std::int64_t lines_cleared = 0;
  double score = 0.0;
};

class TetrisAiGame {
 public:
  TetrisAiGame(IAi& ai, int rows, int cols, int seed);
  GameResult play();

 private:
  IAi& ai_;
  RngEngine rng_;
  Grid board_;
};

const std::vector<std::vector<Shape>>& all_rotations();
const std::vector<std::vector<int>>& all_landing_height_bases();
std::pair<int, int> x_bounds(const Shape& shape);
int compute_drop_y(const Grid& board, const Shape& shape, int x);
bool lock_shape(Grid& board, const Shape& shape, int x, int y);
int clear_lines(Grid& board);
