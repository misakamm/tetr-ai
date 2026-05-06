#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rng_engine.hpp"

constexpr int SHAPE_SIZE = 4;
constexpr int MAX_BOARD_ROWS = 31;
constexpr int MAX_BOARD_COLS = 14;

struct BitShape {
  std::array<std::uint16_t, SHAPE_SIZE> rows{};
  std::array<int, SHAPE_SIZE> bottom_by_col{};
  int min_x = SHAPE_SIZE;
  int max_x = -1;
  int landing_height_base = 0;
};

struct Grid {
  Grid(int rows, int cols) : rows_(rows), cols_(cols) {
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

  int rows() const { return rows_; }
  int cols() const { return cols_; }
  std::uint16_t full_row_mask() const { return full_row_mask_; }
  const std::vector<int>& col_heights() const { return col_heights_; }

  void clear() {
    std::fill(rows_data_.begin(), rows_data_.end(), 0);
    std::fill(col_heights_.begin(), col_heights_.end(), 0);
  }

  bool test(int r, int c) const {
    return (rows_data_[static_cast<std::size_t>(r)] & (static_cast<std::uint16_t>(1u) << c)) != 0;
  }

  void set(int r, int c) {
    const std::uint16_t mask = static_cast<std::uint16_t>(1u) << c;
    std::uint16_t& row = rows_data_[static_cast<std::size_t>(r)];
    if ((row & mask) != 0) {
      return;
    }
    row |= mask;
    const int h = rows_ - r;
    int& col_h = col_heights_[static_cast<std::size_t>(c)];
    if (h > col_h) {
      col_h = h;
    }
  }

  void set_row_bits_or(int r, std::uint16_t bits) {
    std::uint16_t new_bits =
        static_cast<std::uint16_t>(bits & full_row_mask_ & ~rows_data_[static_cast<std::size_t>(r)]);
    if (new_bits == 0) {
      return;
    }
    rows_data_[static_cast<std::size_t>(r)] =
        static_cast<std::uint16_t>(rows_data_[static_cast<std::size_t>(r)] | new_bits);
    while (new_bits != 0) {
      const int c = __builtin_ctz(static_cast<unsigned>(new_bits));
      const int h = rows_ - r;
      int& col_h = col_heights_[static_cast<std::size_t>(c)];
      if (h > col_h) {
        col_h = h;
      }
      new_bits = static_cast<std::uint16_t>(new_bits & ~(1u << c));
    }
  }

  std::uint16_t row_bits(int r) const { return rows_data_[static_cast<std::size_t>(r)]; }
  bool push_up_and_insert_bottom(std::uint16_t bits) {
    const bool overflow = rows_data_.empty() ? false : (rows_data_.front() != 0);
    for (int r = 0; r + 1 < rows_; ++r) {
      rows_data_[static_cast<std::size_t>(r)] = rows_data_[static_cast<std::size_t>(r + 1)];
    }
    if (!rows_data_.empty()) {
      rows_data_.back() = static_cast<std::uint16_t>(bits & full_row_mask_);
    }
    recompute_col_heights();
    return !overflow;
  }

 private:
  void set_row_bits(int r, std::uint16_t bits) {
    rows_data_[static_cast<std::size_t>(r)] = static_cast<std::uint16_t>(bits & full_row_mask_);
  }

  void apply_cleared_lines(int cleared, std::uint32_t /*cleared_rows_mask*/) {
    if (cleared <= 0) {
      return;
    }
    std::fill(col_heights_.begin(), col_heights_.end(), 0);
    std::uint16_t unresolved = full_row_mask_;
    for (int r = 0; r < rows_ && unresolved != 0; ++r) {
      std::uint16_t bits = static_cast<std::uint16_t>(rows_data_[static_cast<std::size_t>(r)] & unresolved);
      while (bits != 0) {
        const int c = __builtin_ctz(static_cast<unsigned>(bits));
        col_heights_[static_cast<std::size_t>(c)] = rows_ - r;
        const std::uint16_t bit = static_cast<std::uint16_t>(1u << c);
        unresolved = static_cast<std::uint16_t>(unresolved & ~bit);
        bits = static_cast<std::uint16_t>(bits & ~bit);
      }
    }
  }

  void recompute_col_heights() {
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

  friend int clear_lines(Grid& pool);

  int rows_;
  int cols_;
  std::uint16_t full_row_mask_ = 0;
  std::vector<std::uint16_t> rows_data_;
  std::vector<int> col_heights_;
};

struct AiMove {
  int shape_idx = 0;
  int x = 0;
};

class IAi {
 public:
  virtual ~IAi() = default;
  virtual AiMove choose(const Grid& pool, int piece_type, const std::vector<int>& col_heights) = 0;
};

const std::vector<std::vector<BitShape>>& all_bit_rotations();
const std::vector<std::vector<int>>& all_landing_height_bases();
int compute_drop_y(const Grid& pool, const BitShape& shape, int x);
bool lock_shape(Grid& pool, const BitShape& shape, int x, int y);
int clear_lines(Grid& pool);
double fitted_avg_height_decay_ratio(const std::vector<std::int64_t>& avg_height_histogram);
double weighted_avg_height_adjacent_ratio(const std::vector<std::int64_t>& avg_height_histogram);
int count_holes(const Grid& pool);

struct GameResult {
  std::int64_t pieces_used = 0;
  std::int64_t lines_cleared = 0;
  double score = 0.0;
  std::vector<std::int64_t> avg_height_histogram;
  std::string top_out_last_pieces;
  bool topped_out = false;
  bool stopped_by_max_lines = false;
  bool stopped_by_max_pieces = false;
  bool stopped_by_no_hole = false;
  int holes_at_end = 0;
  int min_holes_seen = 0;
};

class ITetrisGameHook {
 public:
  virtual ~ITetrisGameHook() = default;
  virtual bool on_game_start(Grid&, GameResult&) { return true; }
  virtual bool on_after_piece(Grid&, GameResult&) { return true; }
  virtual double final_score(const Grid&, const GameResult&) const { return 0.0 / 0.0; }
  virtual int next_piece_type(RngEngine&) { return -1; }
};

struct GameOptions {
  int seed = 0;
  std::int64_t max_lines = -1;
  ITetrisGameHook* hook = nullptr;
};

class TetrisAiGame {
 public:
  explicit TetrisAiGame(IAi& ai, int rows, int cols, int seed = 0, std::int64_t max_lines = -1);
  TetrisAiGame(IAi& ai, int rows, int cols, const GameOptions& options);
  GameResult play();

 private:
  IAi& ai_;
  RngEngine rng_;
  Grid pool_;
  bool game_over_ = false;
  std::int64_t max_lines_ = -1;
  ITetrisGameHook* hook_ = nullptr;

  int generate_piece_type();

  PieceTypeRng piece_rng_{rng_};
};
