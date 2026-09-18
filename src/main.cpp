#include<iostream>
#include<thread>
#include<chrono>
#include<vector>
#include<csignal>
#include<atomic> 
#include "OrderBook.hpp"
#include "spsc_queue.hpp"
#include "config.hpp"
#include "error_codes.hpp"

// Global flag for shutdown 
std::atomic<bool> g_running{true};

void signal_handler(int){
    std::cout<<"\n SHUTDOWN SIGNAL RECEIVED. DRAINING QUEUES...\n";
    g_running.store(false , std::memory_order_release);
}

int main(){
    // setup signal handling
    std::signal(SIGINT , signal_handler);
    std::signal(SIGTERM , signal_handler);
    
    EngineConfig config;
    config.max_orders = 1000000;
    config.max_order_price = 100000;
    config.spsc_queue_capacity = 1 << 16;

    // Instatntize Book and Queue
    OrderBook book(config);
    SPSCQueue<OrderRequest>queue(config.spsc_queue_capacity);
    
    // mertics
    std::atomic<std::uint64_t> total_processed{0};
    std::atomic<std::uint64_t> total_rejected_pool{0};
    std::atomic<std::uint64_t> total_rejected_price{0};
    

    // consumer thread
    auto consumer = [&book ,&queue , &total_processed , &total_rejected_pool , &total_rejected_price](){
        while(g_running.load(std::memory_order_acquire) || !queue.empty()){
            auto maybe_req = queue.pop();
            if(!maybe_req){
                std::this_thread::yield();
                continue;
            }
            const auto& req = *maybe_req;
            // execute order add
            auto result = book.add_order(req.id , req.side , req.price , req.quantity , req.timestamp);

            if(result.success){
                total_processed.fetch_add(1 , std::memory_order_relaxed);
            }else{
                if(result.error == EngineErrorCode::PoolExhausted){
                    total_rejected_pool.fetch_add(1 , std::memory_order_relaxed);
                }else if(result.error == EngineErrorCode::PriceLevelLimitReached){
                    total_rejected_price.fetch_add(1 , std::memory_order_relaxed);
                }else{
                    std::cerr<<"CONSUMER ORDER"<< req.id <<" Rejected : "<< to_string(result.error)<<"\n";
                }
            }
        }
    };

    // Producer Thread
    auto producer = [&queue , &config](){
       OrderId id_counter = 0;
       while(g_running.load(std::memory_order_acquire)){
        OrderRequest req;
        req.id = id_counter++;
        req.price = 10000 + (req.id % 100);
        req.quantity = 100 + (req.id % 50);
        req.side = (req.id % 2 == 0) ? Side::Buy : Side::Sell;
        req.timestamp = static_cast<std::uint64_t>(req.id * 1000);

        //push with backpressure
        while(!queue.push(req)){
            if(!g_running.load(std::memory_order_acquire)) break;
            std::this_thread::yield();
        }
        if(id_counter >= config.max_orders * 2) break;
       }
       std::cout<<" PRODUCER : finished Generating Orders.\n";
    };

    // Launch threads
    std::thread t_consumer(consumer);
    std::thread t_producer(producer);

    // Wait for producer to finish
    t_producer.join();
    g_running.store(true , std::memory_order_release);

    // wait for consumer to drain rem. queues
    t_consumer.join();
    
    // Report :
    auto used = book.used_orders();
    std::cout << "\n=== Industry-Grade Shutdown Report ===\n";
    std::cout << "Orders resting in book: " << used << "\n";
    std::cout << "Successfully processed : " << total_processed.load() << "\n";
    std::cout << "Rejected (Pool Full)   : " << total_rejected_pool.load() << "\n";
    std::cout << "Rejected (Price Limit) : " << total_rejected_price.load() << "\n";
    std::cout << "Book Best Bid/Ask      : " << *book.best_bid() << " / " << *book.best_ask() << "\n";
    

    return 0;

}