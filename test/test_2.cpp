#include<cstdio>
#include<cstdint>
#include<vector>
#include "order.hpp"
#include "config.hpp"
#include "OrderBook.hpp"
#include "spsc_queue.hpp"

namespace{
 struct ExpectedTrade{
    OrderId taker_id;
    OrderId maker_id;
    Price price;
    Quantity quantity;
 };
 struct TestContext{
    EngineConfig cfg;
    SPSCQueue<Trade> outbound;
    OrderBook book;
    int failures{0};
    const char* name;

    explicit TestContext(const char* n) : cfg{} , outbound(cfg.outbound_queue_capacity) , book(cfg , outbound) , 
                                          name(n) {}
    void add(OrderId id , Side s , Price p , Quantity q , std::uint64_t ts){
        auto r = book.add_order(id , s , p , q , ts);
        if(!r.success){
            std::printf("[%s] FAIL : add_order(%llu) rejected : %s\n" , name , 
                                                     static_cast<unsigned long long>(id),
                                                    std::string(to_string(r.error)).c_str());
            ++failures;
        }
    }

    std::vector<Trade>drain(){
        std::vector<Trade>out;
        while(auto t = outbound.pop()) out.push_back(*t);
        return out;
    }

    void expect_trade_count(std::size_t expected){
        const auto trades = drain();
        if(trades.size() != expected){
            std::printf("[%s] FAIL : expected %zu trades , got %zu \n" , name , expected , trades.size());
            for(const auto& t : trades){
                std::printf("trade taker = %llu , trade maker = %llu , price = %llu , qty = %llu \n" , 
                                    static_cast<unsigned long long>(t.taker_id) , 
                                    static_cast<unsigned long long>(t.maker_id) , 
                                    static_cast<long long>(t.price) , 
                                    static_cast<long long>(t.quantity));
            }
            ++failures;
        }
    }

    void expect_book_empty(){
        if(book.used_orders() != 0){
            std::printf("[%s] FAIL : expected %zu resting , got %zu\n" , name , 0 , book.used_orders());
            ++failures;
        }
    }

    void expect_resting(std::size_t n){
        if(book.used_orders() != n){
            std::printf("[%s] FAIL : expected %zu resting , got %zu \n" , name ,n , book.used_orders());
            ++failures;
        }
    }

    void expect_not_crossed(){
        const auto bb = book.best_bid();
        const auto ba = book.best_ask();
        if(bb && ba && *bb >= *ba){
            std::printf("[%s] FAIL : book crossed bid = %llu ask = %llu \n" ,name ,  static_cast<long long>(*bb) , static_cast<long long>(*ba));
            ++failures;
        }
    }

    int result(){
        if(failures == 0) {
            std::printf("test : [%s] --> PASS \n" , name);
            return 0;
        }
        std::printf("test : [%s] --> FAILED : %d failures" , name , failures);
        return 1;
    }
 };  

 void scenario_full_match() {
    TestContext t("S1:full_match");
    t.add(1, Side::Sell, 100, 100, 1000);
    (void)t.drain();                       // resting, no trades yet
    t.add(2, Side::Buy,  100, 100, 2000);
    auto trades = t.drain();
    if (trades.size() != 1) {
        std::printf("[S1] FAIL: expected 1 trade, got %zu\n", trades.size());
        ++t.failures;
    } else {
        const auto& tr = trades[0];
        if (tr.taker_id != 2 || tr.maker_id != 1 ||
            tr.price != 100 || tr.quantity != 100) {
            std::printf("[S1] FAIL: trade mismatch\n");
            ++t.failures;
        }
    }
    t.expect_book_empty();
    t.expect_not_crossed();
    t.result();
}

void scenario_partial_taker() {
    TestContext t("S2:partial_taker");
    t.add(1, Side::Sell, 100, 100, 1000);
    (void)t.drain();
    t.add(2, Side::Buy,  100,  50, 2000);
    auto trades = t.drain();
    if (trades.size() != 1 || trades[0].quantity != 50) {
        std::printf("[S2] FAIL: expected 1 trade qty 50\n");
        ++t.failures;
    }
    t.expect_resting(1);
    t.expect_not_crossed();
    t.result();
}
}

int main(){
    scenario_full_match();
    scenario_partial_taker();

    return 0;
}
