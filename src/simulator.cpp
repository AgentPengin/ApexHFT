#include <atomic>
#include "RingBuffer.hpp"
#include "Types.hpp"
#include <xmmintrin.h>
#include "DenseOrderBook.hpp"
#include <iostream>
#include <latch>
#include <thread>
#include <chrono>
#include <functional>
#include <iomanip>

struct alignas(64) MarketState {
    std::atomic<uint64_t> best_bid{0};
    std::atomic<uint64_t> best_ask{0};
    std::atomic<uint64_t> last_price{1000};
};

void bot_worker(SPSCQueue<Order, 1024>& queue, size_t total_orders, std::latch& start_gate) {
    start_gate.arrive_and_wait();
    for (size_t i = 1;i <= total_orders;i++) {
        Order order{};
        order.orderId = i;
        order.clientId = 1; // Bot ID = 1
        order.type = OrderType::LIMIT;
        order.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        order.price = 1000 + (i % 20);
        order.quantity = 10;
        order.remainingQuantity = 10;
        while(!queue.push(order)) {
            _mm_pause(); // Wait until there's space in the queue
        }
    }
}

MarketState market_state;
std::atomic<bool> bot_done{false};

void engine_worker(SPSCQueue<Order, 1024>& queue, 
                  DenseOrderBook& book, 
                  MarketState& market_state,
                  const std::atomic<bool>& bot_done,
                  std::latch& start_gate) {
    start_gate.arrive_and_wait();
    size_t processed = 0;
    Order order{};
    while (true) {
        if (queue.pop(order)) {
            if (order.type == OrderType::CANCEL) {
                book.cancel_id(order.orderId);
            } else {
                auto outcome = book.add_order(order);
                if (outcome.trades.size() > 0) {
                    market_state.last_price.store(outcome.trades[0].price, std::memory_order_relaxed);
                }
            }
            uint64_t bb = book.has_best_bid() ? book.get_best_bid() : 0;
            uint64_t ba = book.has_best_ask() ? book.get_best_ask() : 0;
            market_state.best_bid.store(bb, std::memory_order_relaxed);
            market_state.best_ask.store(ba, std::memory_order_relaxed);
            
            ++processed;
        } else if (bot_done.load(std::memory_order_acquire)) {
            break;
        } else {
            _mm_pause(); // Wait until there's an order in the queue
        }
    }
}


void market_maker_worker(SPSCQueue<Order, 1024>& queue,
                        const MarketState& market_state,
                        size_t rounds,
                        std::latch& start_gate) {
    start_gate.arrive_and_wait();

    uint64_t prev_bid_id = 0;
    uint64_t prev_ask_id = 0;
    uint64_t order_counter = 100;

    auto push_order = [&](const Order& order) {
        while (!queue.push(order)) {
            _mm_pause(); // Wait until there's space in the queue
        }
    };

    for (size_t i = 0; i < rounds; ++i) {
        uint64_t bb = market_state.best_bid.load(std::memory_order_relaxed);
        uint64_t ba = market_state.best_ask.load(std::memory_order_relaxed);
        uint64_t mid = (bb > 0 && ba > 0) ? (bb + ba) / 2 : market_state.last_price.load(std::memory_order_relaxed);

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

        uint64_t new_bid_id = ++order_counter;
        Order bid{};
        bid.orderId = new_bid_id;
        bid.clientId = 1; // Market Maker ID = 1
        bid.type = OrderType::LIMIT;
        bid.side = Side::BUY;
        bid.price = (mid > 1) ? mid - 1 : 1; // Place bid below best bid
        bid.quantity = 10;
        bid.remainingQuantity = 10;
        push_order(bid);
        prev_bid_id = new_bid_id;

        uint64_t new_ask_id = ++order_counter;
        Order ask{};
        ask.orderId = new_ask_id;
        ask.clientId = 1; // Market Maker ID = 1
        ask.type = OrderType::LIMIT;
        ask.side = Side::SELL;
        ask.price = mid + 1; // Place ask above best ask
        ask.quantity = 10;
        ask.remainingQuantity = 10;
        push_order(ask);
        prev_ask_id = new_ask_id;
    }
    bot_done.store(true, std::memory_order_release);
}

int main() {
    constexpr size_t TOTAL_ORDERS = 100'000;
    constexpr size_t ROUNDS = 25'000;

    SPSCQueue<Order, 1024> queue;
    DenseOrderBook book;
    
    std::latch start_gate(3);

    // std::thread bot_thread(bot_worker, std::ref(queue), TOTAL_ORDERS, std::ref(start_gate));
    std::thread engine_thread(engine_worker, std::ref(queue), std::ref(book), std::ref(market_state), std::ref(bot_done), std::ref(start_gate));
    std::thread market_maker_thread(market_maker_worker, std::ref(queue), std::ref(market_state), ROUNDS, std::ref(start_gate));

    using Clock = std::chrono::high_resolution_clock;

    auto start = Clock::now();
    start_gate.arrive_and_wait(); // Start both threads simultaneously

    market_maker_thread.join();
    engine_thread.join();

    auto end = Clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double mops = static_cast<double>(TOTAL_ORDERS) / seconds / 1'000'000.0;

    std::cout << book.get_best_ask() << " " << book.get_best_bid() << '\n';

    std::cout << "MOPS: " << mops << '\n';

    return 0;
}