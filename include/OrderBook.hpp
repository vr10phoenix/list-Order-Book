#pragma once
#include<vector>
#include<algorithm>
#include<optional>
#include<iostream>
#include<cstddef>
#include<cassert>
#include "memory.hpp"
#include "price_level.hpp"
#include "config.hpp"
#include "error_codes.hpp"
#include "spsc_queue.hpp"

struct LevelEntry{
    Price price;
    PriceLevel list;
};

class OrderBook{
    private : 
    Memory pool_;
    std::vector<LevelEntry>levels_;
    EngineConfig cfg_;
    SPSCQueue<Trade>&outbound_;
    std::uint64_t match_seq_{0};


    void match_buy(Order* taker){
        while(taker->quantity > 0){
          PriceLevel* level = find_matchable_sell(taker->price);
          if(!level) break;
          if(match_against_level(taker , level))break;   
        }
    }

    void match_sell(Order* taker){
        while(taker->quantity > 0){
            PriceLevel* level = find_matchable_buy(taker->price);
            if(!level) break;
            if(!match_against_level(taker , level)) break;
        }
    }
    
    // level with price <= limit --> sold first
    PriceLevel* find_matchable_sell(Price lim){
        for(auto& e : levels_){
            if(e.list.empty()) continue;
            if(e.list.front()->side != Side::Sell) continue;
            if(e.price > lim) return nullptr;
            return &e.list;
        }
        return nullptr;
    }

    // hightest buy with price >= limit
    PriceLevel* find_matchable_buy(Price lim){
        for(auto it = levels_.rbegin();it != levels_.rend();++it){
            if(it->list.empty()) continue;
            if(it->list.front()->side != Side::Buy) continue;
            if(it->price < lim) return nullptr;

            return &it->list;
        }
        return nullptr;
    }

    // Match taker
    bool match_against_level(Order* taker , PriceLevel* level){
        assert(!level->empty());
        Order* maker = level->front();
        Quantity match_qty = std::min(taker->quantity , maker->quantity);

        Trade tr{
            taker->id,
            maker->id,
            maker->price,
            match_qty,
            taker->side,
            taker->timestamp,
            match_seq_ + 1
        };

        if(!outbound_.push(tr)) return false;
        ++match_seq_;

        taker->quantity -= match_qty;
        maker->quantity -= match_qty;

        if(maker->quantity == 0){
            const Price maker_price = maker->price;
            level->pop_front();
            pool_.deallocate(maker);
            if(level->empty()) remove_empty_level(maker_price);
        }
        return true;
    }

    PriceLevel* find_level(Price price){
        auto it = std::lower_bound(levels_.begin() , levels_.end() , price , 
        [](const LevelEntry& e, Price p){return e.price < p;});
        if(it != levels_.end() && it->price == price) return &it->list;

        return nullptr;
    }

    const PriceLevel* find_level(Price price) const {
        auto it = std::lower_bound(levels_.begin() , levels_.end() , price , 
    [](const LevelEntry& e , Price p){return e.price < p;});
    if(it != levels_.end() && it->price == price) return &it->list;

    return nullptr;
    }

    PriceLevel* find_or_create_level(Price price){
       auto it = std::lower_bound(levels_.begin() , levels_.end() , price , 
                                 [](const LevelEntry& e , Price p){return e.price < p;});
       if(it != levels_.end() && it->price == price) {return &it->list;}
       if(levels_.size() >= levels_.capacity()) return nullptr;
       LevelEntry new_entry{price , PriceLevel{}};
       auto pos = levels_.insert(it , new_entry);

       return &pos->list;
    }

    void remove_empty_level(Price price){
        auto it = std::lower_bound(levels_.begin() , levels_.end() , price , 
                              [](const LevelEntry& e , Price p){return e.price < p;});
        if(it != levels_.end() && it->price == price && it->list.empty()){
            levels_.erase(it);
        }
    }

    public:
       OrderBook(const EngineConfig& cfg , SPSCQueue<Trade>& outbound) : cfg_(cfg) , pool_(cfg.max_orders) , levels_(), outbound_(outbound){
        levels_.reserve(cfg.max_price_levels);
       }

       AddOrderResult add_order(OrderId id , Side side , Price price , Quantity qty , std::uint64_t ts){
        // intital checks
         if(qty <= 0 || static_cast<std::uint64_t>(qty) > cfg_.max_order_qty) return {false , EngineErrorCode::InvalidQuantity , 0};
         if(price <= 0 || static_cast<std::uint64_t>(price) > cfg_.max_order_price) return {false , EngineErrorCode::InvalidPrice , 0};
         if(price % cfg_.tick_size != 0) return {false , EngineErrorCode::InvalidPrice , 0};

         // pool allocation
         Order* ord = pool_.allocate();
         if(!ord) {return {false , EngineErrorCode::PoolExhausted , 0};}

         // fill the order
         ord->id = id;
         ord->price = price;
         ord->side = side;
         ord->quantity = qty;
         ord->timestamp = ts;
         ord->prev = nullptr;
         ord->next = nullptr;

         if(side == Side::Buy) match_buy(ord);
         else match_sell(ord);

         if(ord->quantity == 0){
            pool_.deallocate(ord);
            return {true , EngineErrorCode::Ok , id};
         }

         // find or create price level : 
         PriceLevel* level = find_or_create_level(price);
         if(!level){
            pool_.deallocate(ord);
            return {false , EngineErrorCode::PriceLevelLimitReached , 0};
         }

         level->push_back(ord);
         return {true , EngineErrorCode::Ok , id};
       }

       Quantity cancel_order(OrderId id , Price price){
        PriceLevel* level = find_level(price);
        if(!level || level->empty()) return 0;

        Order* cur = level->front();
        while(cur){
            Order* next = cur->next;
            if(cur->id == id){
                Quantity qty = cur->quantity;
                level->remove(cur);
                pool_.deallocate(cur);

                if(level->empty()){
                  remove_empty_level(price);
                }
                return qty;
            }
            cur = next;
        }
        return 0;
       }

       std::optional<Price> best_bid() const{
        for(auto it = levels_.rbegin();it != levels_.rend();it++){
            if(!it->list.empty() && it->list.front()->side == Side::Buy){
                return it->price;
            }
        }
        return std::nullopt;
       }

       std::optional<Price> best_ask() const{
         for(const auto& entry : levels_){
            if(!entry.list.empty() && entry.list.front()->side == Side::Sell){
                return entry.price;
            }
         }
          return std::nullopt;
       }

       void dump() const{
        for(const auto& entry : levels_){
            if(entry.list.empty()) continue;
            std::cout<<"Price : "<<entry.price<<" : ";
            Order* cur = entry.list.front();
            while(cur){
                std::cout<<"["<<(cur->side == Side::Buy ? "B" : "S")<<" id = "<<cur->id<<" q = "<<cur->quantity<<"]\n";
                cur = cur->next;
            }
            std::cout<<"\n";
        }
       }

       std::size_t used_orders() const {return pool_.used_count();}

       template <typename Fn>
       void for_each_level(Fn&& fn) const{
        for(const auto& entry : levels_){
            if(entry.list.empty()) continue;
            fn(entry.price , entry.list);
        }
       }
};
