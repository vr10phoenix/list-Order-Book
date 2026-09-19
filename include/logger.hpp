#pragma once 
#include<atomic>
#include<chrono>
#include<condition_variable>
#include<cstdint>
#include<cstdio>
#include<ctime>
#include<iostream>
#include<queue>
#include<mutex>
#include<sstream>
#include<string>
#include<thread>
#include<utility>

enum class LogLevel : std::uint8_t {Trace = 0 , Debug , Info , Warn , Error , Fatal};

// LogLevel enum to text 
inline const char* level_name(LogLevel lvl) noexcept {
    switch(lvl){
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }

    return "?";
}

// function to capture time
namespace detail{
    inline std::uint64_t wall_ns() noexcept{
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
            );
    }
}

// Entry datastructure
struct LogEntry{
    LogLevel level;
    std::uint64_t timestamp_ns;
    std::size_t thread_hash;
    std::string message;
};

// Logger class 
class AsyncLogger{
    public: 
    AsyncLogger(){
        worker_ = std::thread([this]{run();});
    }

    ~AsyncLogger() { shutdown(); }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    void set_min_level(LogLevel lvl) noexcept {
        min_level_.store(lvl, std::memory_order_relaxed);
    }

    template <typename... Args>
    void log(LogLevel lvl, Args&&... args) {
        if (lvl < min_level_.load(std::memory_order_relaxed)) return;

        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));

        LogEntry entry{
            lvl,
            detail::wall_ns(),
            std::hash<std::thread::id>{}(std::this_thread::get_id()),
            oss.str()
        };

        {
            std::lock_guard<std::mutex> lk(mutex_);
            queue_.push(std::move(entry));
        }
        cv_.notify_one();
    }

    // Block until all queued messages are flushed. Use before shutdown only.
    void flush() {
        std::unique_lock<std::mutex> lk(mutex_);
        drained_cv_.wait(lk, [this] { return queue_.empty(); });
    }

    void shutdown() {
        {
            std::lock_guard<std::mutex> lk(mutex_);
            if (!running_.exchange(false, std::memory_order_acq_rel)) return;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    
    private: 

    void run(){
       while(true){
        std::unique_lock<std::mutex>lk(mutex_);
        cv_.wait(lk , [this]{
           return !queue_.empty() || !running_.load(std::memory_order_acquire);
        });

        // commands the thread to drop the lock and immediately go to sleep.
        while(!queue_.empty()){
            LogEntry entry = std::move(queue_.front());
            queue_.pop();
            lk.unlock();
            emit(entry);
            lk.lock();
        }

        if(!running_.load(std::memory_order_acquire))break;
        drained_cv_.notify_all();
       }
    }

    static void emit(const LogEntry& entry){
        const auto secs = entry.timestamp_ns / 1'000'000'000;
        const auto nanos = entry.timestamp_ns % 1'000'000'000;
        std::time_t t = static_cast<std::time_t>(secs);
        std::tm tm_buf{};
#if defined(_WIN32)
       gmtime_s(&tm_buf , &t);
#else 
       gmtime_r(&t , &tm_buf);
#endif
       char tbuf[32];
       std::strftime(tbuf , sizeof(tbuf) , "%Y-%m-%dT%H:%M:%S" , &tm_buf);
       std::ostream& out = (entry.level >= LogLevel::Warn) ? std::cerr : std::cout;

       char line[1024];
       std::snprintf(line , sizeof(line) , "%s.%09lluZ [%s] (tid=%zx) %s\n",
                     tbuf,static_cast<unsigned long long>(nanos),level_name(entry.level),
                     entry.thread_hash, entry.message.c_str());
        
        out<<line;
        out.flush();
        if (entry.level == LogLevel::Fatal) std::abort();
    }

    std::mutex mutex_;
    std::condition_variable drained_cv_;
    std::condition_variable cv_;
    std::queue<LogEntry>queue_;
    std::thread worker_;
    std::atomic<bool> running_{true};
    std::atomic<LogLevel> min_level_{LogLevel::Info};
};

// Global Singleton accessor
inline AsyncLogger& logger(){
   static AsyncLogger inst;
   return inst;
}


#define LOG_TRACE(...) ::logger().log(LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(...) ::logger().log(LogLevel::Debug ,__VA_ARGS__)
#define LOG_INFO(...) ::logger().log(LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) ::logger().log(LogLevel::Warn, __VA_ARGS__)
#define LOG_FATAL(...) ::logger().log(LogLevel::Fatal, __VA_ARGS__)

