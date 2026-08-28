#pragma once
#include<vector>
#include<algorithm>
#include<optional>
#include<iostream>
#include "memory.hpp"
#include "price_level.hpp"

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

    }

    void remove_empty_level(){
        
    }

    public:
       OrderBook(std::size_t max_orders , std::size_t max_price_levels) : pool_(max_orders){
        levels_.reserve(max_price_levels);
       }

       bool add_order(OrderId id , Side side , Price price , Quantity qty , std::uint64_t ts){
          Order* ord = pool_.allocate();
          if(!ord) return false;

          ord->id = id;
          ord->price = price;
          ord->quantity = qty;
          ord->side = side;
          ord->timestamp = ts;

          PriceLevel* level = find_or_create_level(price);
          if(!level){
            pool_.deallocate(ord);
            return false;
          }
          level->push_back(ord);
          return true;
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
};
