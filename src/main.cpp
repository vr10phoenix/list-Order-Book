#include<iostream>
#include<thread>
#include<chrono>
#include <vector>
#include<atomic> 
#include "OrderBook.hpp"
#include "spsc_queue.hpp"

// Global flag for shutdown 
std::atomic<bool> stop_producer{false};
std::atomic<bool> stop_consumer{false};

int main(){
    constexpr size_t QUEUE_CAPACITY = 1 << 16;
    constexpr size_t MAX_ORDERS = 1'000'000;
    constexpr size_t MAX_PRICE_LEVELS = 100'000;

    OrderBook book(MAX_ORDERS , MAX_PRICE_LEVELS);

    SPSCQueue<OrderRequest>queue(QUEUE_CAPACITY);
    auto consumer = [&book , &queue](){
        size_t processed = 0;
        while(!stop_consumer.load(std::memory_order_acquire) || !queue.empty()){
            auto maybe_req = queue.pop();
            if(maybe_req){
                const auto& req = *maybe_req;
                // add to the order book
                bool ok = book.add_order(req.id , req.side , req.price , req.quantity , req.timestamp);
                if(!ok){
                    std::cerr<<"Consumer : Failed to add the Order : "<<req.id<<"\n";
                }
                ++processed;
                if(processed % 100000 == 0){
                    std::cout<<"Conumer Processed "<<processed<<" orders.\n";
                }
            }
            else std::this_thread::yield();
        }
       std::cout<<"Consumer Finished. Total Processed : "<<processed<<"\n";
    };

    // Producer Thread
    auto producer = [&queue](){
        size_t produced = 0;
        while(!stop_producer.load(std::memory_order_acquire)){
            // generating determinstic order
            OrderRequest req;
            req.id = static_cast<OrderId>(produced);
            req.price = 10000 + (produced % 100);
            req.quantity = 100 + (produced % 50);
            req.side = (produced % 2 == 0) ? Side::Buy : Side::Sell;
            req.timestamp = static_cast<std::uint64_t>(produced * 1000);
            
            // pushing : if full -> spin breify
            while(!queue.push(req)) std::this_thread::yield();
            ++produced;
            if(produced >= 1000000) break;
        }
        std::cout<<"Producer finished. Produced : "<<produced<<"\n";
    };

    // Launch threads
    std::thread t_consumer(consumer);
    std::thread t_producer(producer);

    // Wait for producer to finish
    t_producer.join();
    stop_producer.store(true , std::memory_order_release);
    
    // consumer drain remaining items , then stop
    // wait till queue is empty before stopping consumer
    while(!queue.empty()) std::this_thread::yield();
    stop_consumer.store(true , std::memory_order_release);
    t_consumer.join();

    // Report 
    auto best_bid = book.best_bid();
    auto best_ask = book.best_ask();
    std::cout<<"___________ FINAL BOOK STATE ________"<<"\n";
    if(best_bid) std::cout<<"Best Bid : "<<*best_bid<<"\n";
    if(best_ask) std::cout<<"Best Ask : "<<*best_ask<<"\n";
    std::cout<<"Total orders still in book (approx) : "<<book.used_orders()<<"\n";

    book.dump();

    return 0;

}