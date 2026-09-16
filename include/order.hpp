#pragma once
#include<cstdint>
#include<cstddef>

using Price = std::int64_t;
using Quantity = std::int64_t;
using OrderId = std::uint64_t;

enum class Side : std::uint8_t {Buy , Sell};

struct Order{
    OrderId id;
    Price price;
    Quantity quantity;
    std::uint64_t timestamp;
    Order* prev{nullptr};
    Order* next{nullptr};
    Order* pool_next{nullptr};
    Side side;

    void reset() noexcept{
        id = 0; price = 0;quantity = 0;
        side = Side::Buy;timestamp = 0;
        prev = next = nullptr;
    }
};

struct OrderRequest{
    OrderId id;
    Price price;
    Quantity quantity;
    Side side;
    std::uint64_t timestamp;
};

static_assert(offsetof(Order , prev) % alignof(Order*) == 0);