#pragma once
#include <atomic>
#include <latch>
#include <xmmintrin.h>
#include <chrono>
#include <thread>
#include "RingBuffer.hpp"
#include "Types.hpp"
#include "MarketState.hpp"

class MarketMakerBot {
public:
    MarketMakerBot(uint32_t bot_id, 
                   SPSCQueue<Order, 1024>& queue, 
                   const MarketState& state, 
                   const std::atomic<bool>& running, 
                   std::latch& start_gate)
        : bot_id_(bot_id), queue_(queue), state_(state), running_(running), start_gate_(start_gate) {}

    void run() {
        start_gate_.arrive_and_wait();
        uint64_t prev_bid_id = 0;
        uint64_t prev_ask_id = 0;
        uint64_t order_counter = static_cast<uint64_t>(bot_id_) * 1'000'000;

        auto push_order = [&](const Order& o) {
            while (running_.load(std::memory_order_relaxed) && !queue_.push(o)) {
                _mm_pause();
            }
        };

        while (running_.load(std::memory_order_relaxed)) {
            uint64_t bb = state_.best_bid.load(std::memory_order_relaxed);
            uint64_t ba = state_.best_ask.load(std::memory_order_relaxed);
            uint64_t mid = (bb > 0 && ba > 0) ? (bb + ba) / 2 : state_.last_price.load(std::memory_order_relaxed);

            // 1. Rút cặp lệnh cũ nếu có
            if (prev_bid_id > 0) {
                Order cancel_bid{};
                cancel_bid.orderId = prev_bid_id;
                cancel_bid.type = OrderType::CANCEL;
                push_order(cancel_bid);
            }
            if (prev_ask_id > 0) {
                Order cancel_ask{};
                cancel_ask.orderId = prev_ask_id;
                cancel_ask.type = OrderType::CANCEL;
                push_order(cancel_ask);
            }

            // 2. Đặt cặp lệnh mới quanh mid_price
            uint64_t new_bid_id = ++order_counter;
            Order bid{};
            bid.orderId = new_bid_id;
            bid.clientId = bot_id_;
            bid.type = OrderType::LIMIT;
            bid.side = Side::BUY;
            bid.price = (mid > 1) ? mid - 1 : 1;
            bid.quantity = 10;
            bid.remainingQuantity = 10;

            
            push_order(bid);
            prev_bid_id = new_bid_id;

            uint64_t new_ask_id = ++order_counter;
            Order ask{};
            ask.orderId = new_ask_id;
            ask.clientId = bot_id_;
            ask.type = OrderType::LIMIT;
            ask.side = Side::SELL;
            ask.price = mid + 1;
            ask.quantity = 10;
            ask.remainingQuantity = 10;
            push_order(ask);
            prev_ask_id = new_ask_id;

            // Nhịp thở của Market Maker (100 microseconds = ~10.000 quotes/giây)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

private:
    uint32_t bot_id_;
    SPSCQueue<Order, 1024>& queue_;
    const MarketState& state_;
    const std::atomic<bool>& running_;
    std::latch& start_gate_;
};
