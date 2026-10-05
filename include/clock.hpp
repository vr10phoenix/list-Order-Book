#pragma once
#include<chrono>
#include<cstdint>

class IClockSource{
    public:
     virtual ~IClockSource() = default;
     [[nodiscard]] virtual std::uint64_t now_ns() const noexcept = 0;
};

// production clock
class SystemClock final : public IClockSource {
    public:
     [[nodiscard]] std::uint64_t now_ns() const noexcept override{
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        }
};

// test clock : 
class TestClock final : public IClockSource{
    private:
     std::uint64_t now_ns_;
    public:
    [[nodiscard]] std::uint64_t now_ns() const noexcept override{
        return now_ns_;
    }
    void set(std::uint64_t t) noexcept {now_ns_ = t;}
    void advance(std::uint64_t delta_ns) noexcept {now_ns_ += delta_ns;}
};
