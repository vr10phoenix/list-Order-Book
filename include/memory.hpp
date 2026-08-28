#pragma once
#include<vector>
#include<array>
#include<cassert>
#include<cstdlib> 
#include "order.hpp"

class Memory{
 private : 
  Order* storage_;
  Order* free_head_;
  std::size_t capacity_;

  public : 
    explicit Memory(std::size_t capacity) : capacity_(capacity) , free_head_(nullptr){
        storage_ = static_cast<Order*>(_aligned_malloc(capacity * sizeof(Order), alignof(Order)));
        if(!storage_) throw std::bad_alloc();

        for(std::size_t i = 0;i<capacity_;i++){
           Order*cur = &storage_[i];
           cur->pool_next = (i + 1 < capacity_) ? &storage_[i+1] : nullptr;
           cur->reset();
        }
        free_head_ = &storage_[0];
    }

    ~Memory(){_aligned_free(storage_);}

    Memory(const Memory&) = delete;
    Memory& operator = (const Memory&) = delete; 

    Order* allocate() noexcept{
        if(!free_head_) return nullptr;
        Order* ret = free_head_;
        free_head_= free_head_->pool_next;
        ret->pool_next = nullptr;
        ret->reset();
        return ret;
    }

    void deallocate(Order* order) noexcept{
        assert(order >= storage_ && order < storage_ + capacity_);
        order->reset();
        order->pool_next = free_head_;
        free_head_ = order;
    }

    [[nodiscard]] std::size_t capacity() const noexcept {return capacity_;}
    [[nodiscard]] std::size_t used_count() const noexcept{
        std::size_t free = 0;
        Order* cur = free_head_;
        while(cur){free++;cur =  cur->pool_next;}
        return capacity_ - free;
    }

};