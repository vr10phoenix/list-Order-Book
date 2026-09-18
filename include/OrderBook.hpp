#pragma once
#include<vector>
#include<algorithm>
#include<optional>
#include<iostream>
#include "memory.hpp"
#include "price_level.hpp"
#include "config.hpp"
#include "error_codes.hpp"

struct LevelEntry{
    Price price;
    PriceLevel list;
};

class OrderBook{
    private : 
    Memory pool_;
    std::vector<LevelEntry>levels_;

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
       OrderBook(const EngineConfig& cfg) : pool_(cfg.max_orders) , levels_(cfg.max_price_levels){
        levels_.reserve(cfg.max_price_levels);
       }

       AddOrderResult add_order(OrderId id , Side side , Price price , Quantity qty , std::uint64_t ts){
        // intital checks
         if(qty <= 0 ||qty > 1000000) return {false , EngineErrorCode::InvalidQuantity , 0};
         if(price < 0) return {false , EngineErrorCode::InvalidPrice , 0};

         // pool allocation
         Order* ord = pool_.allocate();
         if(!ord) {return {false , EngineErrorCode::PoolExhausted , 0};}

         // fill the order
         ord->id = id;
         ord->price = price;
         ord->side = side;
         ord->quantity = qty;
         ord->timestamp = ts;

         // find or create price level : 
         PriceLevel* level = find_or_create_level(price);
         if(!level){
            pool_.deallocate(ord);
            return {false , EngineErrorCode::PriceLevelLimitReached , 0};
         }

         level->push_back(ord);
         return {true , EngineErrorCode::Ok};
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

       void match(Order* incoming){
        (void) incoming;
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
};
