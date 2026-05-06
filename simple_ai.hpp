#pragma once

#include "tetris_core.hpp"

#include <array>
#include <vector>

struct AiWeights {
  int vertical = 95;
  int hole = 401;
  int x1_hole = 88;
  int x2_hole = 88;
  int landing_height = 67;
  int rows_cleared = 86;
  int hole_depth = 75;
  int well_depth_2 = 176;
  int well_depth_3 = 99;
  int shape_adaptability = 117;
};

struct WellScoreTables {
  std::vector<int> normal;
  std::vector<int> x0;
  std::vector<int> x1;
  std::vector<int> x2;
};

class SimpleAi : public IAi {
 public:
  explicit SimpleAi(int seed = 0);

  AiMove choose(const Grid& pool, int piece_type, const std::vector<int>& col_heights) override;

  void set_weights(const AiWeights& weights);
  const AiWeights& weights() const;
  void set_second_layer_search_enabled(bool enabled);
  bool second_layer_search_enabled() const;

 private:
  AiWeights weights_{};
  bool second_layer_search_enabled_ = false;
  mutable WellScoreTables well_score_tables_cache_{};
  mutable AiWeights well_score_tables_cache_weights_{};
  mutable int well_score_tables_cache_rows_ = -1;
  mutable bool well_score_tables_cache_valid_ = false;

  int drop_y(const Grid& pool, const BitShape& shape, int x) const;
  const WellScoreTables& well_score_tables_for_rows(int rows) const;
  long long evaluate(const Grid& pool,
                     const std::vector<int>& col_heights,
                     int rows_cleared,
                     int landing_height_x4,
                     int eroded_piece_cells_metric,
                     const WellScoreTables& well_score_tables) const;
};
