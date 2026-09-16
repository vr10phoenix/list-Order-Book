#pragma once
#include <atomic>
#include <cstddef>
#include <vector>
#include <optional>
#include <cassert>

// Lock-free Single Producer / Single Consumer ring buffer.
template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t capacity)
        : capacity_(capacity), mask_(capacity - 1), buffer_(capacity) 
    {
        // Enforce power-of-two for fast modulo 
        assert((capacity & (capacity - 1)) == 0 && "Capacity must be power of 2");
    }

    // Producer side: push one item.
    // Returns false if the queue is full (spins/yield in practice).
    bool push(const T& item) noexcept {
        // Acquire the current head to check if we have space.
        size_t head = head_.load(std::memory_order_acquire);
        size_t tail = tail_.load(std::memory_order_relaxed);
        
        if (tail - head >= capacity_) {
            return false;   // full
        }

        // Write the item into the buffer.
        buffer_[tail & mask_] = item;

        // Release the new tail position to the consumer.
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Consumer side: pop one item.
    std::optional<T> pop() noexcept {
        // Acquire the current tail to see what's available.
        size_t tail = tail_.load(std::memory_order_acquire);
        size_t head = head_.load(std::memory_order_relaxed);

        if (head == tail) {
            return std::nullopt;   // empty
        }

        // Read the item from the buffer.
        T item = buffer_[head & mask_];

        // Release the new head position to the producer.
        head_.store(head + 1, std::memory_order_release);
        return item;
    }

    // For debugging / monitoring
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == 
               tail_.load(std::memory_order_acquire);
    }

private:
    size_t capacity_;
    size_t mask_;
    std::vector<T> buffer_;

    // Separate cache lines to avoid false sharing.
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};