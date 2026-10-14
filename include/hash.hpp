#pragma once
#include<cstdint>
#include<cstddef>

// FNV-1a 64-bit. Fowler/Noll/Vo, public spec (draft-eastlake-fnv)
// why : zero dependencies, fully specified, same output on every
// platform forever, ~1 ns/byte.

class Fnv1a64{
    private : 
     std::uint64_t h_ ;
    public:
     static constexpr std::uint64_t kOffsetBasis = 14695981039346656037ULL;
     static constexpr std::uint64_t kPrime = 1099511628211ULL;
     
     Fnv1a64() noexcept : h_(kOffsetBasis) {}
     void reset() noexcept {h_ = kOffsetBasis;}
     void update(const void* data , std::size_t bytes) noexcept{
        const auto* p = static_cast<const std::uint8_t*>(data);
        for(std::size_t i = 0;i<bytes;i++){
            h_ ^= static_cast<std::uint64_t>(p[i]);
            h_ *= kPrime;
        }
     }
     [[nodiscard]] std::uint64_t value() const noexcept {return h_;}
};