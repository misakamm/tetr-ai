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
