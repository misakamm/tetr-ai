#pragma once

#include "tetris_core.hpp"

struct AiWeights {
  int vertical = 156;
  int hole = 431;
  int x1_hole = 88;
  int landing_height = 76;
  int rows_cleared = 114;
  int hole_depth = 92;
  int well_depth = 99;
  int shape_adaptability = 59;
};

class SimpleAi final : public IAi {
 public:
  AiMove choose(const Grid& board, int piece_type, const std::vector<int>& col_heights) override;

 private:
  AiWeights weights_{};

  double evaluate(const Grid& board,
                  const std::vector<int>& col_heights,
                  int rows_cleared,
                  double landing_height) const;
};
