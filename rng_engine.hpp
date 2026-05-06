#pragma once

#include <cstdint>
#include <random>

#include "pcg_random.hpp"

using RngEngine = pcg32_k2_fast;

inline std::uint64_t seed_from_int(int seed) {
  return static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed));
}

inline RngEngine make_seeded_rng(int seed) {
  return RngEngine(seed_from_int(seed));
}

inline RngEngine make_entropy_rng() {
  std::random_device rd;
  const std::uint64_t hi = static_cast<std::uint64_t>(rd()) << 32;
  const std::uint64_t lo = static_cast<std::uint64_t>(rd());
  return RngEngine(hi ^ lo);
}

// Generates uniform random piece types in [0, 6] using 3-bit rejection
// sampling. Reuses leftover bits across calls; only rejects value == 7 (1/8).
class PieceTypeRng {
 public:
  explicit PieceTypeRng(RngEngine& rng) : rng_(rng) {}

  int next() {
    while (true) {
      if (bits_left_ < 3) {
        buf_ |= static_cast<std::uint64_t>(rng_()) << bits_left_;
        bits_left_ += 32;
      }
      const int val = static_cast<int>(buf_ & 0x7u);
      buf_ >>= 3;
      bits_left_ -= 3;
      if (val < 7) return val;
    }
  }

 private:
  RngEngine& rng_;
  std::uint64_t buf_ = 0;
  int bits_left_ = 0;
};
