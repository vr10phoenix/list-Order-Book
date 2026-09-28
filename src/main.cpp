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
#include "metrics.hpp"
#include "logger.hpp"

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

    // Metrics Handles
    auto& counter_processed = metrics().counter("engine.orders.processed_total");
    auto& counter_rejected = metrics().counter("engine.orders.rejected_orders");
    auto& gauge_depth = metrics().gauge("engine.queue.depth");
    auto& gauge_used = metrics().gauge("engine.pool.used");
    auto& histogram_add = metrics().histogram("engine.add_order.latency_ns");

    
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

    LOG_INFO("Engine starting. max_orders=" , config.max_orders, "max_levels=" , config.max_price_levels ,
               "queue_capacity" , config.spsc_queue_capacity);
    

    // consumer thread
    auto consumer = [&](){
        std::uint64_t local_processed = 0;
        std::uint64_t empty_spins = 0;

        while(g_running.load(std::memory_order_acquire) || !queue.empty()){
            auto maybe_req = queue.pop();
            if(!maybe_req){
                if(++empty_spins < 1000){
#if defined(_MSC_VER)
            _mm_pause();
#else 
            __builtin_ia_pause();
#endif
                }else{
                  std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
                continue;
            }
            empty_spins = 0;
            const auto& req = *maybe_req;
            
            // measure only book mutation
            auto t0 = std::chrono::steady_clock::now();
            // execute order add
            auto result = book.add_order(req.id , req.side , req.price , req.quantity , req.timestamp);
            auto t1 = std::chrono::steady_clock::now();
            const auto ns = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());

            histogram_add.record(ns);

            if(result.success){counter_processed.inc();}
            else{
                counter_rejected.inc();
                if(result.error != EngineErrorCode::PoolExhausted && result.error != EngineErrorCode::PriceLevelLimitReached){
                    LOG_WARN("Order" , req.id , "rejected:" , to_string(result.error));
                }
            }
            
            if(++local_processed % 100'000 == 0){
                // placeholder
                gauge_depth.set(static_cast<std::int64_t>(0));
                gauge_used.set(static_cast<std::int64_t>(book.used_orders()));
                LOG_INFO("Progress : Processed : " , local_processed , " pool used : " , book.used_orders());
            }
        }
        LOG_INFO("Consumer Drained. Local Total = " , local_processed);
    };

    // Producer Thread
    auto producer = [&queue , &config](){
       OrderId id_counter = 0;
       std::uint32_t full_spins = 0;

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
            if(++full_spins < 1000){
#if defined(_MSC_VER)
              _mm_pause();
#else        
            __builtin_ia32_pause();
#endif
            }else{
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
        full_spins = 0;
        if(id_counter >= config.max_orders * 2) break;
       }
    //    std::cout<<" PRODUCER : finished Generating Orders.\n";
    LOG_INFO("Producer finished ar id : " , id_counter);
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
    // auto used = book.used_orders();
    // std::cout << "\n=== Industry-Grade Shutdown Report ===\n";
    // std::cout << "Orders resting in book: " << used << "\n";
    // std::cout << "Successfully processed : " << total_processed.load() << "\n";
    // std::cout << "Rejected (Pool Full)   : " << total_rejected_pool.load() << "\n";
    // std::cout << "Rejected (Price Limit) : " << total_rejected_price.load() << "\n";
    // std::cout << "Book Best Bid/Ask      : " << *book.best_bid() << " / " << *book.best_ask() << "\n";

    LOG_INFO("=== SHUTDOWN REPORT ===");
    LOG_INFO("RESTING ORDERS : " , book.used_orders());
    LOG_INFO("PROCESSED ORDERS : " , counter_processed.value() , 
              "REJECTED ORDERS : " , counter_rejected.value());

    logger().flush();
    std::cout<<"\n ========== METRICS SNAPSHOT ==========\n";
    metrics().render_(std::cout);
    logger().flush();

    return 0;

}