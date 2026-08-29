#include<iostream>
#include "OrderBook.hpp"

int main(){
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

    OrderBook book(1'000'000, 100'000);
    std::cout<<"Adding 1000 orders : "<<'\n';
    for(int i = 0;i<1000;i++){
        bool ok = book.add_order(
         static_cast<OrderId>(i) ,
         (i % 2 == 0) ? Side::Buy : Side::Sell,
         10000 + (i % 100),
         100 + i % 50,
         static_cast<std::uint64_t>(i * 1000)
        );

        if(!ok){
            std::cerr<<"Failed at Order : "<<i<<'\n';
            return 1;
        }
    }

    book.dump();

    auto bid = book.best_bid();
    auto ask = book.best_ask();
    if(bid) std::cout<<"Best bid : "<<*bid<<'\n';
    if(ask) std::cout<<"Best Ask: "<<*ask<<'\n';

    std::cout<<"Used Orders : "<<book.used_orders()<<'\n';

    // testing initial cacellation
    std::cout<<"\n Cancelling order id = 42 (price 10042)..... \n";
    Quantity cancelled = book.cancel_order(42 , 10042);
    std::cout<<"Cancelled qty : "<<cancelled<<'\n';
    book.dump();


    return 0;

}