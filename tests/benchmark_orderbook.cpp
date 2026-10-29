#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include "OrderBook.hpp"
#include "Types.hpp"
#include "TimeStamp.hpp"

using namespace std;

constexpr size_t NUM_OPERATIONS = 1'000'000;

int main() {
    cout << "apexhft engine benchmark: std::map + deque (baseline)" << endl;
    cout << "Genrating " << NUM_OPERATIONS << " random orders..." << endl;

    OrderBook book;
    mt19937_64 rng(42);

    vector<uint32_t> latencies;
    latencies.reserve(NUM_OPERATIONS);

    vector<uint64_t> order_ids;
    order_ids.reserve(NUM_OPERATIONS);

    cout << "Starting benchmark..." << endl;
    auto wall_start = chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_OPERATIONS; ++i) {
        uint32_t op_type = rng() % 100;
        uint64_t price = 950 + (rng() % 100); // [950, 1050)
        uint64_t quantity = 10 + (rng() % 50); // [10, 50)
        Side side = (rng() % 2 == 0) ? Side::BUY : Side::SELL;

        uint64_t t0 = rdtsc();

        if (op_type < 20 && !order_ids.empty()) { // 20% chance to cancel
            size_t idx = rng() % order_ids.size();
            uint64_t cancel_id = order_ids[idx];
            book.cancel_id(cancel_id);

            order_ids[idx] = order_ids.back();
            order_ids.pop_back();
        } else if (op_type < 50) {
            // 30% chance to add a market order
            Order mkt{};
            mkt.orderId = i;
            mkt.price = 0;
            mkt.quantity = quantity;
            mkt.remainingQuantity = quantity;
            mkt.side = side;
            mkt.type = OrderType::MARKET;
            book.add_order(mkt); 
        } else {
            // 50% chance to add a limit order
            Order limit{};
            limit.orderId = i;
            limit.price = price;
            limit.quantity = quantity;
            limit.remainingQuantity = quantity;
            limit.side = side;
            limit.type = OrderType::LIMIT;
            auto res = book.add_order(limit);

            if (res.quantity_rested > 0) {
                order_ids.push_back(i);
            }
        }
        uint64_t t1 = rdtsc();
        latencies.push_back(static_cast<uint32_t>(t1 - t0));
    }

    auto wall_end = chrono::high_resolution_clock::now();
    chrono::duration<double> wall_duration = wall_end - wall_start;

    sort(latencies.begin(), latencies.end());

    double mops = (NUM_OPERATIONS / wall_duration.count()) / 1'000'000.0;

    cout << "Benchmark completed." << endl;
    cout << "Total operations: " << NUM_OPERATIONS << endl;
    cout << "Total time (wall clock): " << wall_duration.count() << " seconds" << endl;
    cout << "Throughput: " << mops << " MOPS" << endl;
    cout << "------------------------------------------------------" << endl;
    cout << "Latency statistics (in CPU cycles):" << endl;
    cout << "Min: " << latencies.front() << " cycles" << endl;
    cout << "Median: " << latencies[NUM_OPERATIONS * 0.50] << " cycles" << endl;
    cout << "P90: " << latencies[NUM_OPERATIONS * 0.90] << " cycles" << endl;
    cout << "P99: " << latencies[NUM_OPERATIONS * 0.99] << " cycles" << endl;
    cout << "P99.9: " << latencies[NUM_OPERATIONS * 0.999] << " cycles" << endl;
    cout << "Max: " << latencies.back() << " cycles" << endl;

    return 0;
}
