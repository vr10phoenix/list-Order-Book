#include <cstdint>
#include <cstdio>

#include "book_snapshot.hpp"
#include "config.hpp"
#include "error_codes.hpp"
#include "OrderBook.hpp"
#include "prng.hpp"

namespace {

constexpr std::uint64_t kSeed       = 0x5EED'1234'5678'9ABCULL;
constexpr std::size_t   kMaxOrders  = 1'000'000;
constexpr std::size_t   kMaxLevels  = 100'000;
constexpr std::size_t   kNumOrders  = 500'000;   // half the pool, on purpose:
                                                 // we want headroom, not saturation
constexpr Price         kPriceMin   = 9'900;
constexpr Price         kPriceSpan  = 200;       // [9900, 10100)
constexpr Quantity      kQtyMin     = 1;
constexpr Quantity      kQtySpan    = 500;

// Regenerate ONLY via the workflow above.
constexpr std::uint64_t kGoldenHash = 0ULL;

struct DecisionTally {
    std::uint64_t accepted          = 0;
    std::uint64_t rejected_pool     = 0;
    std::uint64_t rejected_level    = 0;
    std::uint64_t rejected_invalid  = 0;
};

} // namespace

int main() {
    EngineConfig cfg;
    cfg.max_orders          = kMaxOrders;
    cfg.max_price_levels    = kMaxLevels;
    cfg.spsc_queue_capacity = 1u << 16;

    OrderBook book(cfg);
    SplitMix64 rng(kSeed);
    DecisionTally tally;

    // NO threads. NO wall-clock. Single-threaded, in-order, reproducible
    // from the seed alone.
    std::uint64_t ts = 0;
    for (std::size_t i = 0; i < kNumOrders; ++i) {
        const OrderId  id    = static_cast<OrderId>(i);
        const Side     side  = (rng.next() & 1u) ? Side::Buy : Side::Sell;
        const Price    price = kPriceMin + static_cast<Price>(rng.next_bounded(kPriceSpan));
        const Quantity qty   = kQtyMin + static_cast<Quantity>(rng.next_bounded(kQtySpan));
        ts += 1000;

        auto r = book.add_order(id, side, price, qty, ts);
        if (r.success) {
            ++tally.accepted;
        } else {
            switch (r.error) {
                case EngineErrorCode::PoolExhausted:          ++tally.rejected_pool;    break;
                case EngineErrorCode::PriceLevelLimitReached: ++tally.rejected_level;   break;
                case EngineErrorCode::InvalidQuantity:
                case EngineErrorCode::InvalidPrice:           ++tally.rejected_invalid; break;
                default:                                      ++tally.rejected_invalid; break;
            }
        }
    }

    const std::uint64_t computed = snapshot::hash(book);

    std::printf("=== Determinism Test ===\n");
    std::printf("seed          : 0x%016llX\n", static_cast<unsigned long long>(kSeed));
    std::printf("orders fed    : %zu\n",     kNumOrders);
    std::printf("accepted      : %llu\n",    static_cast<unsigned long long>(tally.accepted));
    std::printf("rej (pool)    : %llu\n",    static_cast<unsigned long long>(tally.rejected_pool));
    std::printf("rej (level)   : %llu\n",    static_cast<unsigned long long>(tally.rejected_level));
    std::printf("rej (invalid) : %llu\n",    static_cast<unsigned long long>(tally.rejected_invalid));
    std::printf("resting       : %zu\n",     book.used_orders());
    std::printf("computed hash : 0x%016llX\n", static_cast<unsigned long long>(computed));
    std::printf("golden   hash : 0x%016llX\n", static_cast<unsigned long long>(kGoldenHash));

    if (kGoldenHash == 0ULL) {
        std::printf("\n[SETUP] Paste the computed hash into kGoldenHash, "
                    "rebuild, rerun.\n");
        return 2;
    }
    if (computed != kGoldenHash) {
        std::printf("\n[FAIL] Determinism drift detected.\n");
        return 1;
    }
    std::printf("\n[PASS] Book state matches golden hash.\n");
    return 0;
}