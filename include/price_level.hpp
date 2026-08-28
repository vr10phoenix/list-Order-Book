#pragma once
#include "order.hpp"
#include<cassert>

class PriceLevel{
    private: 
    Order* head_{nullptr};
    Order* tail_{nullptr};
    std::size_t size_{0};

    public:
     PriceLevel() = default;

     bool empty() const noexcept {return head_ == nullptr;}

     void push_back(Order* order) noexcept {
        assert(order && order->prev == nullptr && order->next == nullptr);
        if(!head_) head_ = tail_ = order;
        else {
            tail_->next = order;
            tail_ = tail_->next;
            tail_ = order;
        }
        ++size_;
     }

     void remove(Order* order) noexcept{
        assert(order);
        if(order->prev) order->prev->next = order->next;
        else head_ = order->next;

        if(order->next) order->next->prev = order->prev;
        else tail_ = order->prev;

        order->prev = order->next;
        --size_;
     }

    Order* pop_front() noexcept {
        if(!head_) return nullptr;
        Order* ret = head_;
        head_ = head_ -> next;
        if(head_) head_->prev = nullptr;
        else tail_ = nullptr;
        ret->next = ret->prev = nullptr;
        --size_;
        return ret; 
    }

    Order* front() const noexcept{return head_;}
    Order* back() const noexcept{return tail_;}
    std::size_t size() const noexcept{return size_;}
};