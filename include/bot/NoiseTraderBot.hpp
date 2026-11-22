#pragma once
#include <atomic>
#include <latch>
#include <xmmintrin.h>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include "RingBuffer.hpp"
#include "Types.hpp"
#include "MarketState.hpp"

class NoiseTraderBot {
public:
    NoiseTraderBot(uint32_t bot_id, 
                   SPSCQueue<Order, 1024>& queue, 
                   const MarketState& state, 
                   const std::atomic<bool>& running, 
                   std::latch& start_gate,
                   uint64_t seed = 42)
        : bot_id_(bot_id), queue_(queue), state_(state), running_(running), start_gate_(start_gate), rng_(seed) {}

    void run() {
        start_gate_.arrive_and_wait();
        uint64_t order_counter = static_cast<uint64_t>(bot_id_) * 1'000'000;

        auto push_order = [&](const Order& o) {
            while (running_.load(std::memory_order_relaxed) && !queue_.push(o)) {
                _mm_pause();
            }
        };

        while (running_.load(std::memory_order_relaxed)) {
            uint64_t current_price = state_.last_price.load(std::memory_order_relaxed);
            Order order{};
            order.orderId = ++order_counter;
            order.clientId = bot_id_;
            order.side = (rng_() % 100 < 50) ? Side::BUY : Side::SELL;

            if (rng_() % 100 < 40) {
                // 40% Market Order: quét thẳng vào sổ lệnh
                order.type = OrderType::MARKET;
                order.price = 0;
            } else {
                // 60% Limit Order: đặt quanh giá hiện tại +- 3 ticks
                order.type = OrderType::LIMIT;
                int64_t offset = static_cast<int64_t>(rng_() % 7) - 3;
                int64_t target_price = static_cast<int64_t>(current_price) + offset;
                order.price = static_cast<uint64_t>(std::max<int64_t>(1, target_price));
            }
            order.quantity = 5;
            order.remainingQuantity = 5;

            push_order(order);

            // Nhịp thở của Noise Trader (50 microseconds = ~20.000 orders/giây)
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

private:
    uint32_t bot_id_;
    SPSCQueue<Order, 1024>& queue_;
    const MarketState& state_;
    const std::atomic<bool>& running_;
    std::latch& start_gate_;
    std::mt19937_64 rng_;
};
