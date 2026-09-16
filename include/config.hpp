#pragma once
#include<cstddef>
#include<cstdint>

struct EngineConfig{
    // Book and memory limits
    std::size_t max_orders = 1000000;
    std::size_t max_price_levels = 100000;

    //pipeline capacity:
    std::size_t spsc_queue_capacity = 1 << 16;

    // Operational limit
    std::uint64_t max_order_qty = 1000000; // 1 million
    std::uint64_t max_order_price = 100000000; // 100 million
}