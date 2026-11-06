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

void engine_worker(SPSCQueue<Order, 1024>& queue, DenseOrderBook& book, size_t total_orders, std::latch& start_gate) {
    start_gate.arrive_and_wait();
    size_t processed = 0;
    Order order{};
    while (processed < total_orders) {
        if (queue.pop(order)) {
            if (order.type == OrderType::CANCEL) {
                book.cancel_id(order.orderId);
            } else {
                book.add_order(order);
            }
            ++processed;
        } else {
            _mm_pause(); // Wait until there's an order in the queue
        }
    }
}

int main() {
    constexpr size_t TOTAL_ORDERS = 100'000;

    SPSCQueue<Order, 1024> queue;
    DenseOrderBook book;
    
    std::latch start_gate(3);

    std::thread bot_thread(bot_worker, std::ref(queue), TOTAL_ORDERS, std::ref(start_gate));
    std::thread engine_thread(engine_worker, std::ref(queue), std::ref(book), TOTAL_ORDERS, std::ref(start_gate));

    using Clock = std::chrono::high_resolution_clock;

    auto start = Clock::now();
    start_gate.arrive_and_wait(); // Start both threads simultaneously

    bot_thread.join();
    engine_thread.join();

    auto end = Clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    double mops = static_cast<double>(TOTAL_ORDERS) / seconds / 1'000'000.0;

    std::cout << "MOPS: " << mops << '\n';

    return 0;
}