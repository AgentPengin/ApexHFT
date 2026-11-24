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

class WhaleSpooferBot {
public:
    WhaleSpooferBot(uint32_t bot_id, 
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
            uint64_t best_bid = state_.best_bid.load(std::memory_order_relaxed);
            uint64_t best_ask = state_.best_ask.load(std::memory_order_relaxed);

            Order order{};
            uint64_t spoof_id = ++order_counter;
            order.orderId = spoof_id;
            order.clientId = bot_id_;
            if (rng_() % 2 == 0) {
                order.side = Side::BUY;
                order.type = OrderType::LIMIT;
                order.price = best_bid > 0 ? best_bid : 1; // Đặt NGAY tại best bid
            } else {
                order.side = Side::SELL;
                order.type = OrderType::LIMIT;
                order.price = best_ask > 0 ? best_ask : 1; // Đặt NGAY tại best ask
            }
            
            
            order.quantity = rng_() % 301 + 200; // số lượng quanh 200 đến 500
            order.remainingQuantity = order.quantity;
            
            push_order(order);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));


            // Huỷ lệnh 
            Order cancel_order{};
            cancel_order.orderId = spoof_id;
            cancel_order.clientId = bot_id_;
            cancel_order.type = OrderType::CANCEL;
            cancel_order.price = 0; // Không cần giá cho lệnh huỷ
            cancel_order.quantity = 0; // Không cần số lượng cho lệnh huỷ
            cancel_order.remainingQuantity = 0; // Không cần số lượng còn lại cho lệnh huỷ
            push_order(cancel_order);
        
            // Nhịp thở của Noise Trader (50 microseconds = ~20.000 orders/giây)
            std::this_thread::sleep_for(std::chrono::microseconds(500000));
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
