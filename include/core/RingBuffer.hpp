#pragma once
#include<atomic>
#include<cstddef>

template <typename T, size_t capacity> 
class SPSCQueue {
private:
    alignas(64) T buffer[capacity];
    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::atomic<size_t> head_{0};

public:
    SPSCQueue() = default;

    bool push(const T& item) {
        size_t currentTail = tail_.load(std::memory_order_relaxed);
        size_t currentHead = head_.load(std::memory_order_acquire);

        if (currentTail - currentHead == capacity) {
            return false; // Queue is full
        }
        
        buffer[currentTail & (capacity - 1)] = item;
        tail_.store(currentTail + 1, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t currentHead = head_.load(std::memory_order_relaxed);

        if (currentHead == tail_.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }

        item = buffer[currentHead & (capacity - 1)];

        head_.store(currentHead + 1, std::memory_order_release);
        return true;
    }

static_assert((capacity > 1) && (capacity & (capacity - 1)) == 0, "Capacity must be a power of 2");    
};
