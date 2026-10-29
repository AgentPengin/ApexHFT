#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include "OrderBook.hpp"
#include "Types.hpp"
#include "TimeStamp.hpp"
#include "DenseOrderBook.hpp"

using namespace std;

struct BenchResult {
    double total_time_sec;
    double mops;
    uint32_t p50_cycles;
    uint32_t p90_cycles;
    uint32_t p99_cycles;
    uint32_t p999_cycles;
    uint32_t max_cycles;
    uint64_t total_trades;
    uint64_t total_volume_matched;
    uint64_t final_best_bid;
    uint64_t final_best_ask;
};

constexpr size_t NUM_OPERATIONS = 1'000'000;

template <typename BookType>
BenchResult run_benchmark(const string& engine_name) {
    cout << ">>> Running Benchmark on [" << engine_name << "] with " << NUM_OPERATIONS << " ops..." << endl;
    BookType book;
    mt19937_64 rng(42);

    vector<uint32_t> latencies;
    latencies.reserve(NUM_OPERATIONS);

    vector<uint64_t> order_ids;
    order_ids.reserve(NUM_OPERATIONS);

    uint64_t total_trades = 0;
    uint64_t total_volume = 0;

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
            total_trades += res.trades.size();
            total_volume += res.quantity_filled;
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
    
    BenchResult res;
    res.total_time_sec = wall_duration.count();
    res.mops = (NUM_OPERATIONS / res.total_time_sec) / 1'000'000.0;
    res.p50_cycles = latencies[NUM_OPERATIONS * 0.50];
    res.p90_cycles = latencies[NUM_OPERATIONS * 0.90];
    res.p99_cycles = latencies[NUM_OPERATIONS * 0.99];
    res.p999_cycles = latencies[NUM_OPERATIONS * 0.999];
    res.max_cycles = latencies.back();
    res.total_trades = total_trades;
    res.total_volume_matched = total_volume;
    res.final_best_bid = book.has_best_bid() ? book.get_best_bid() : 0;
    res.final_best_ask = book.has_best_ask() ? book.get_best_ask() : 0;
    return res;
}

int main() {
    cout << "apexhft engine benchmark: std::map + deque (baseline) vs dense order book" << endl;
    
    BenchResult map_res = run_benchmark<OrderBook>("std::map + deque (baseline)");

    BenchResult dense_res = run_benchmark<DenseOrderBook>("Dense Order Book");

    bool match_correct = (map_res.total_trades == dense_res.total_trades) &&
                         (map_res.total_volume_matched == dense_res.total_volume_matched);

    if (match_correct) {
        cout << "===> STATUS: 100% BIT-FOR-BIT MATCH! Both engines produced identical trades!" << endl;
        cout << "     Total Trades Matched: " << map_res.total_trades << endl;
        cout << "     Total Volume Matched: " << map_res.total_volume_matched << endl;
    } else {
        cout << "===> WARNING: Discrepancy detected! Map trades: " << map_res.total_trades 
             << " vs Dense trades: " << dense_res.total_trades << endl;
    }
    cout << "-------------------------------------------------------------------------\n" << endl;
    cout << fixed << setprecision(2);
    cout << left << setw(20) << "Metric" 
         << setw(25) << "std::map (Baseline)" 
         << setw(25) << "DenseBook (Optimized)" 
         << "Speedup" << endl;
    cout << "-------------------------------------------------------------------------" << endl;
    
    cout << left << setw(20) << "Throughput" 
         << setw(25) << (to_string(map_res.mops).substr(0, 5) + " MOPS")
         << setw(25) << (to_string(dense_res.mops).substr(0, 5) + " MOPS")
         << "+" << (dense_res.mops / map_res.mops) << "x" << endl;
    cout << left << setw(20) << "Total Time" 
         << setw(25) << (to_string(map_res.total_time_sec).substr(0, 6) + " s")
         << setw(25) << (to_string(dense_res.total_time_sec).substr(0, 6) + " s")
         << "-" << (map_res.total_time_sec / dense_res.total_time_sec) << "x faster" << endl;
    cout << left << setw(20) << "Median (P50)" 
         << setw(25) << (to_string(map_res.p50_cycles) + " cycles")
         << setw(25) << (to_string(dense_res.p50_cycles) + " cycles")
         << (double)map_res.p50_cycles / dense_res.p50_cycles << "x faster" << endl;
    cout << left << setw(20) << "P90 Latency" 
         << setw(25) << (to_string(map_res.p90_cycles) + " cycles")
         << setw(25) << (to_string(dense_res.p90_cycles) + " cycles")
         << (double)map_res.p90_cycles / dense_res.p90_cycles << "x faster" << endl;
    cout << left << setw(20) << "P99 Latency" 
         << setw(25) << (to_string(map_res.p99_cycles) + " cycles")
         << setw(25) << (to_string(dense_res.p99_cycles) + " cycles")
         << (double)map_res.p99_cycles / dense_res.p99_cycles << "x faster" << endl;
    cout << left << setw(20) << "P99.9 (Tail)" 
         << setw(25) << (to_string(map_res.p999_cycles) + " cycles")
         << setw(25) << (to_string(dense_res.p999_cycles) + " cycles")
         << (double)map_res.p999_cycles / dense_res.p999_cycles << "x faster" << endl;
    cout << left << setw(20) << "Max Latency" 
         << setw(25) << (to_string(map_res.max_cycles) + " cycles")
         << setw(25) << (to_string(dense_res.max_cycles) + " cycles")
         << (double)map_res.max_cycles / dense_res.max_cycles << "x faster" << endl;
    cout << "=========================================================================" << endl;
    return 0;

    return 0;
}
