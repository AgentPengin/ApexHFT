#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <random>
#include <iomanip>
#include <cassert>
#include "OrderBook.hpp"
#include "Types.hpp"
#include "TimeStamp.hpp"
#include "DenseOrderBook.hpp"

using namespace std;

constexpr size_t NUM_OPERATIONS = 1'000'000;
vector<uint64_t> order_ids;
vector<uint32_t> order_ids2;
mt19937_64 rng(42);

void process_order(DenseOrderBook& book, OrderBook& baseline_book, size_t i) {
    uint32_t op_type = rng() % 100;
    uint64_t price = 950 + (rng() % 100); // [950, 1050)
    uint64_t quantity = 10 + (rng() % 50); // [10, 50)
    Side side = (rng() % 2 == 0) ? Side::BUY : Side::SELL;

    Outcome dense_outcome;
    Outcome2 dense_outcome2;
    Outcome baseline_outcome;
    Outcome2 baseline_outcome2;

    if (op_type < 20 && !order_ids.empty()) { // 20% chance to cancel
        size_t idx = rng() % order_ids.size();
        uint64_t cancel_id = order_ids[idx];
        dense_outcome2 = book.cancel_id(cancel_id);
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
        dense_outcome = book.add_order(mkt); 
    } else {
        // 50% chance to add a limit order
        Order limit{};
        limit.orderId = i;
        limit.price = price;
        limit.quantity = quantity;
        limit.remainingQuantity = quantity;
        limit.side = side;
        limit.type = OrderType::LIMIT;
        dense_outcome = book.add_order(limit);
        if (dense_outcome.quantity_rested > 0) {
            order_ids.push_back(i);
        }
    }


    if (op_type < 20 && !order_ids2.empty()) { // 20% chance to cancel
        size_t idx = rng() % order_ids2.size();
        uint64_t cancel_id = order_ids2[idx];
        baseline_outcome2 = baseline_book.cancel_id(cancel_id);
        order_ids2[idx] = order_ids2.back();
        order_ids2.pop_back();
    } else if (op_type < 50) {
        // 30% chance to add a market order
        Order mkt{};
        mkt.orderId = i;
        mkt.price = 0;
        mkt.quantity = quantity;
        mkt.remainingQuantity = quantity;
        mkt.side = side;
        mkt.type = OrderType::MARKET;
        baseline_outcome = baseline_book.add_order(mkt); 
    } else {
        // 50% chance to add a limit order
        Order limit{};
        limit.orderId = i;
        limit.price = price;
        limit.quantity = quantity;
        limit.remainingQuantity = quantity;
        limit.side = side;
        limit.type = OrderType::LIMIT;
        baseline_outcome = baseline_book.add_order(limit);
        if (baseline_outcome.quantity_rested > 0) {
            order_ids2.push_back(i);
        }
    }
    assert(dense_outcome.reason == baseline_outcome.reason);
    assert(dense_outcome2.reason == baseline_outcome2.reason);
    if (op_type >= 20) {
        assert(dense_outcome.quantity_rested == baseline_outcome.quantity_rested);
        assert(dense_outcome.quantity_filled == baseline_outcome.quantity_filled);
        assert(dense_outcome.trades.size() == baseline_outcome.trades.size());
        for (size_t j = 0; j < dense_outcome.trades.size(); ++j) {
            const Trade& dense_trade = dense_outcome.trades[j];
            const Trade& baseline_trade = baseline_outcome.trades[j];
            assert(dense_trade.tradeId == baseline_trade.tradeId);
            assert(dense_trade.makerOrderId == baseline_trade.makerOrderId);
            assert(dense_trade.takerOrderId == baseline_trade.takerOrderId);
            assert(dense_trade.price == baseline_trade.price);
            assert(dense_trade.quantity == baseline_trade.quantity);
        }
    }
    assert(book.get_best_ask() == baseline_book.get_best_ask());
    assert(book.get_best_bid() == baseline_book.get_best_bid());
}

int main() {
    DenseOrderBook book;
    OrderBook baseline_book;
    order_ids.reserve(NUM_OPERATIONS);
    order_ids2.reserve(NUM_OPERATIONS);

    for (size_t i = 1; i <= NUM_OPERATIONS; ++i) {
        process_order(book, baseline_book, i);
    }

    return 0;
}