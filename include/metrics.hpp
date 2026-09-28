#pragma once 
#include<array>
#include<ostream>
#include<atomic>
#include<bit>
#include<cmath>
#include<cstdint>
#include<memory>
#include<mutex>
#include<string>
#include<unordered_map>

#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable : 4324) // structure padded due to alignas
# endif

// Counter
class Counter{
    public:
     void inc(std::uint64_t n = 1) noexcept {
        v_.fetch_add(n , std::memory_order_relaxed);
     }
     std::uint64_t value() const noexcept{
       return v_.load(std::memory_order_relaxed);
     }
    
     private:
      std::atomic<uint64_t> v_{0};
     
};

//Gauge : movement id up and down
class Gauge{
  public:
    void set(std::uint64_t x) noexcept {v_.store(x , std::memory_order_relaxed);}
    void add(std::uint64_t x) noexcept {v_.fetch_add(x , std::memory_order_relaxed);}
    std::uint64_t value() noexcept {return v_.load(std::memory_order_relaxed);}
  private:
    std::atomic<std::uint64_t> v_{0};
};

// Latency Histogram -> log scale buckets
class LatencyHistogram{
    public:
      static constexpr std::size_t NUM_BUCKETS = 64;

      void record(std::uint64_t ns) noexcept{
        count_.fetch_add(1 , std::memory_order_relaxed);
        sum_.fetch_add(ns , std::memory_order_relaxed);
        update_min(ns);
        update_max(ns);
        std::size_t idx = 0;
        if (ns != 0) {
            idx = 63u - static_cast<std::size_t>(std::countl_zero(ns));
        }
        if(idx >= NUM_BUCKETS) idx = NUM_BUCKETS - 1;
        buckets_[idx].fetch_add(1 , std::memory_order_relaxed);
      }

      std::uint64_t count() const noexcept{return count_.load(std::memory_order_relaxed);}
      std::uint64_t sum() const noexcept {return sum_.load(std::memory_order_relaxed);}
      std::uint64_t min() const noexcept {
         auto m = min_.load(std::memory_order_relaxed);
         return (m == UINT64_MAX) ? 0 : m;
      }
      std::uint64_t max() const noexcept{
        return max_.load(std::memory_order_relaxed);
      }

      std::uint64_t percentile(double p) const noexcept {
        const std::uint64_t total = count();
        if(total == 0) return 0;
        const auto target = static_cast<std::uint64_t>(std::ceil(p * static_cast<double>(total)));
        
        std::uint64_t running = 0;
        for(std::size_t i = 0;i< NUM_BUCKETS;++i){
            running += buckets_[i].load(std::memory_order_relaxed);
            if(running >= target) {
                return (i == 0) ? 1ULL : (1ULL << i);
            }
        }
        return max();
      }

    private:

      void update_min(std::uint64_t ns) noexcept{
        auto cur = min_.load(std::memory_order_relaxed);
        while(ns < cur && !min_.compare_exchange_weak(cur , ns , std::memory_order_relaxed)){}
      }

      void update_max(std::uint64_t ns) noexcept{
        auto cur = max_.load(std::memory_order_relaxed);
        while(ns > cur && !max_.compare_exchange_weak(cur , ns , std::memory_order_relaxed)){}
      }
      
      // private variables
      std::atomic<std::uint64_t>count_{0};
      std::atomic<std::uint64_t>sum_{0};
      std::atomic<std::uint64_t> min_{UINT64_MAX};
      std::atomic<std::uint64_t> max_{0};
      std::array<std::atomic<uint64_t> , NUM_BUCKETS> buckets_{};

};

class MetricsRegistry{
  private: 
   mutable std::mutex mu_;
   std::unordered_map<std::string , std::unique_ptr<Counter>> counters_;
   std::unordered_map<std::string , std::unique_ptr<Gauge>> gauges_;
   std::unordered_map<std::string , std::unique_ptr<LatencyHistogram>> histograms_;

  public: 
   Counter& counter(const std::string& name){
    std::lock_guard<std::mutex> lk(mu_);
    auto i = counters_.find(name);
    if(i == counters_.end()){
      auto ptr = std::make_unique<Counter>();
      Counter* raw = ptr.get();
      counters_.emplace(name , std::move(ptr));
      return *raw;
    }
    return *i->second;
   }

   Gauge& gauge(const std::string& name){
    std::lock_guard<std::mutex> lk(mu_);
    auto i = gauges_.find(name);
    if(i == gauges_.end()){
      auto ptr = std::make_unique<Gauge>();
      Gauge* raw = ptr.get();
      gauges_.emplace(name , std::move(ptr));
      return *raw;
    }
    return *i->second;
   }

   LatencyHistogram& histogram(const std::string& name){
    std::lock_guard<std::mutex> lk(mu_);
    auto i = histograms_.find(name);
    if(i == histograms_.end()){
      auto ptr = std::make_unique<LatencyHistogram>();
      LatencyHistogram* raw = ptr.get();
      histograms_.emplace(name , std::move(ptr));
      return *raw;
    }
    return *i->second;
   }

   void render_(std::ostream& os) const{
    std::lock_guard<std::mutex>lk(mu_);
    for(const auto& [name , c] : counters_){
      os<<"# TYPE "<<name<<" counter\n" <<name<< " "<<c->value()<<"\n";
    }
    for(const auto& [name , g] : gauges_){
      os<<"# TYPE "<<name<<" gauge\n"<<name<<" "<<g->value()<<"\n"; 
    }
    for(const auto& [name , h] : histograms_){
      os<<"# TYPE "<<name<<" summary\n"
        <<name<< "_count "<<h->count()<<"\n"
        <<name<<"_sum "<<h->sum()<<"\n"
        <<name<<"_min"<<h->min()<<"\n"
        <<name<<"_max"<<h->max()<<"\n"
        <<name<<"{quantile=\"0.5\"}"<<h->percentile(0.5)<<"\n"
        <<name<<"{quantile=\"0.99\"}"<<h->percentile(0.99)<<"\n"
        <<name<<"{quantile=\"0.999\"}"<<h->percentile(0.999)<<"\n";
    }
   }
};

inline MetricsRegistry& metrics(){
  static MetricsRegistry inst;
  return inst;
}

#if defined(_MSC_VER)
# pragma warning(pop)
#endif
