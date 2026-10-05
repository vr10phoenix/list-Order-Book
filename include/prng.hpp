#pragma once
#include<cstdint>

// random number Generator function : 
// splitmix64. Steele, Lea, Flood, OOPSLA 2014. Public reference impl.

class SplitMix64 {
 private:
  std::uint64_t state_;
 public:
  explicit SplitMix64(std::uint64_t seed) noexcept : state_(seed) {}
  [[nodiscard]] std::uint64_t next() noexcept{
    std::uint64_t z = state_;
    state_ += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31); 
  }

  // Uniform in [0, bound). Modulo bias is O(bound / 2^64)
  [[nodiscard]] std::uint64_t next_bounded(std::uint64_t bound) noexcept {return next() % bound;}
};