#pragma once
#include <cstdint>
#include<string_view>

using OrderId = std::uint64_t;

enum class EngineErrorCode : std::uint8_t{
    Ok = 0,
    PoolExhausted,           // memory pool full
    PriceLevelLimitReached , // unique price limit reached
    InvalidQuantity,
    InvalidPrice,
    OrderNotFound          // for cancellations
};

inline std::string_view to_string(EngineErrorCode code){
    using enum EngineErrorCode ;
    switch (code){
        case Ok: return "Success";
        case PoolExhausted: return "OrderPoolExhausted";
        case PriceLevelLimitReached: return "PriceLevelLimitReached";
        case InvalidQuantity: return "InvalidQuantity";
        case InvalidPrice: return "InvalidPrice";
        case OrderNotFound: return "OrderNotFound";
        default: return "UnknownError";
    }
}

struct AddOrderResult{
    bool success;
    EngineErrorCode error;
    OrderId order_id; // set on sucess , 0 on failure
};