#pragma
#include<cstdint>
#include<vector>
#include"hash.hpp"
#include"OrderBook.hpp"

// state serialization of OrderBook

namespace snapshot{
    inline void write_u8(std::vector<std::uint8_t>& out , std::uint8_t v){
        out.push_back(v);
    }
    inline void write_u64(std::vector<std::uint8_t>& out , std::uint64_t v){
        for(int i = 0;i<8;i++){
            out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
            v >>= 8;
        }
    }
    inline void write_i64(std::vector<std::uint8_t>&out , std::int64_t v){
        write_u64(out , static_cast<std::uint64_t>(v));
    }

    inline void serialize(const OrderBook& book , std::vector<std::uint8_t>&out){
        out.clear();
        out.reserve(1<<20);
        book.for_each_level([&](Price price , const PriceLevel& level){
          write_i64(out , price);
          write_u64(out , static_cast<std::uint64_t>(level.size()));
          const Order* ord = level.front();
          while(ord){
            write_u64(out , ord->id);
            write_i64(out , ord->quantity);
            write_u8(out ,static_cast<std::uint8_t>(ord->side));
            write_u64(out , ord->timestamp);
            ord = ord->next;
          }
        });
    }

    inline std::uint64_t hash(const OrderBook& book){
        std::vector<std::uint8_t>bytes;
        serialize(book , bytes);
        Fnv1a64 h;
        h.update(bytes.data() , bytes.size());
        return h.value();
    }
}