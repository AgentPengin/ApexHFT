#include <iostream>
#include <thread>
#include <chrono>
#include "RingBuffer.hpp"
#include "Types.hpp"
#include "TimeStamp.hpp"

using namespace std;

int main() {
    constexpr size_t TOTAL_ORDERS = 10'000'000;

    auto queue = std::make_unique<SPSCQueue<Order, 65536>>();

    cout << "Benchmarking SPSCQueue with " << TOTAL_ORDERS << " orders..." << endl;
    auto start_time = chrono::high_resolution_clock::now();
    uint64_t start_tsc = rdtsc();

    thread producer([&]() {
        for (size_t i = 0;i < TOTAL_ORDERS;i++) {
            Order order{};
            order.orderId = i;
            order.price = 100 + i;
            order.timestamp = rdtsc();
            order.clientOrderId = i;
            order.clientId = i % 1000;
            order.quantity = 10 + (i % 100);
            order.executedQuantity = 0;
            order.remainingQuantity = order.quantity;
            order.side = (i % 2 == 0) ? Side::BUY : Side::SELL;
            order.type = (i % 3 == 0) ? OrderType::MARKET : OrderType::LIMIT;
            order.status = OrderStatus::NEW;
            order.timeInForce = 0;

            while(!queue->push(order)) {

            }
        }
    });

    thread consumer([&]() {
        Order order{};
        for (size_t i = 0;i < TOTAL_ORDERS;i++) {
            Order order;
            while(!queue->pop(order)) {

            }
        }
    });

    producer.join();
    consumer.join();

    uint64_t end_tsc = rdtsc();
    auto end_time = chrono::high_resolution_clock::now();

    chrono::duration<double> elapsed = end_time - start_time;
    double mops = (TOTAL_ORDERS / elapsed.count()) / 1'000'000.0;
    double cycles_per_op = (double)(end_tsc - start_tsc) / TOTAL_ORDERS;

    cout << "Finished in: " << elapsed.count() << " seconds" << endl;
    cout << "Throughput: " << mops << " Million ops/sec (MOPS)" << endl;
    cout << "Avg Cycles: " << cycles_per_op << " cycles/op"<< endl;


    cout << "Ending time: " << rdtsc() << endl;

    return 0;
}