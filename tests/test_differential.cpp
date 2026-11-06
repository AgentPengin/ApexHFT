#include <vector>
#include <random>
#include <cassert>
#include <iostream>
#include "OrderBook.hpp"
#include "Types.hpp"
#include "TimeStamp.hpp"
#include "DenseOrderBook.hpp"

using namespace std;

constexpr size_t NUM_OPERATIONS = 1'000'000;

enum class Kind {
    CANCEL,
    MARKET,
    LIMIT
};

struct Operation {
    Kind kind;
    uint64_t id;
    Side side;
    uint64_t price;
    uint32_t quantity;
    uint64_t cancel_id;
};

struct Step {
    Outcome outcome;
    OrderStatus status;
    uint64_t best_bid;
    uint64_t best_ask;
};

[[noreturn]] void fail(const char* field, size_t index,
                       const Operation& op, uint64_t seed,
                       long long a, long long b) {
    std::cerr << "Mismatch in field [" << field << "] at index " << index << "\n"
              << "  Expected: " << a << ", Got: " << b << "\n"
              << "  Seed: " << seed << "\n"
              << "  Operation: (kind=" << static_cast<int>(op.kind) 
              << ", id=" << op.id 
              << ", price=" << op.price 
              << ", quantity=" << op.quantity << ")\n";
    std::abort();
}

#define CHECK_EQ(field, index, op, seed, va, vb) \
    do { if (!((va) == (vb))) fail(#field, index, op, seed, (va), (vb)); } while (0)

vector<Operation> generate(uint64_t seed, size_t N) {
    vector<uint64_t> order_ids;
    mt19937_64 rng(seed);
    vector<Operation> ops;
    ops.reserve(N);
    
    for (size_t i = 1; i <= N; ++i) {
        uint32_t op_type = rng() % 100;
        uint64_t price = 950 + (rng() % 100); // [950, 1050)
        uint32_t quantity = 10 + (rng() % 50); // [10, 50)
        Side side = (rng() % 2 == 0) ? Side::BUY : Side::SELL;

        if (op_type < 20 && !order_ids.empty()) { // 20% chance to cancel
            size_t idx = rng() % order_ids.size();
            uint64_t cancel_id = order_ids[idx];
            ops.push_back({Kind::CANCEL, i, side, price, quantity, cancel_id});
            order_ids[idx] = order_ids.back();
            order_ids.pop_back();
        } else if (op_type < 50) {
            // 30% chance to add a market order
            ops.push_back({Kind::MARKET, i, side, price, quantity, 0});
        } else {
            // 50% chance to add a limit order
            ops.push_back({Kind::LIMIT, i, side, price, quantity, 0});
            order_ids.push_back(i);
        }
    }
    return ops;
}

Step apply_op(auto& book, const Operation& op) {
    Outcome outcome;
    Outcome2 outcome2;
    OrderStatus status = OrderStatus::NEW;

    if (op.kind == Kind::CANCEL) {
        outcome2 = book.cancel_id(op.cancel_id);
    } else if (op.kind == Kind::MARKET) {
        Order mkt{};
        mkt.orderId = op.id;
        mkt.price = 0;
        mkt.quantity = op.quantity;
        mkt.remainingQuantity = op.quantity;
        mkt.side = op.side;
        mkt.type = OrderType::MARKET;
        outcome = book.add_order(mkt); 
        status = mkt.status;
    } else if (op.kind == Kind::LIMIT) {
        // 50% chance to add a limit order
        Order limit{};
        limit.orderId = op.id;
        limit.price = op.price;
        limit.quantity = op.quantity;
        limit.remainingQuantity = op.quantity;
        limit.side = op.side;
        limit.type = OrderType::LIMIT;
        outcome = book.add_order(limit);
        status = limit.status;
    }
    if (op.kind == Kind::CANCEL) {
        outcome.reason = outcome2.reason;
    }
    Step step;
    step.outcome = outcome;
    step.status = status;
    step.best_bid = book.has_best_bid() ? book.get_best_bid() : 0;
    step.best_ask = book.has_best_ask() ? book.get_best_ask() : 0;
    return step;
}

void CHECK(const Step& a, const Step& b, size_t index, const Operation& op, uint64_t seed) {
    CHECK_EQ(reason, index, op, seed, 
             static_cast<long long>(a.outcome.reason), 
             static_cast<long long>(b.outcome.reason));
             
    CHECK_EQ(status, index, op, seed, 
             static_cast<long long>(a.status), 
             static_cast<long long>(b.status));

    CHECK_EQ(assigned_id, index, op, seed, a.outcome.assigned_id, b.outcome.assigned_id);
    CHECK_EQ(quantity_rested, index, op, seed, a.outcome.quantity_rested, b.outcome.quantity_rested);
    CHECK_EQ(quantity_filled, index, op, seed, a.outcome.quantity_filled, b.outcome.quantity_filled);
    
    CHECK_EQ(trades_count, index, op, seed, a.outcome.trades.size(), b.outcome.trades.size());
    
    for (size_t t = 0; t < a.outcome.trades.size(); ++t) {
        const auto& trade_a = a.outcome.trades[t];
        const auto& trade_b = b.outcome.trades[t];
        
        CHECK_EQ(trade_price, index, op, seed, trade_a.price, trade_b.price);
        CHECK_EQ(trade_quantity, index, op, seed, trade_a.quantity, trade_b.quantity);
        CHECK_EQ(trade_maker_id, index, op, seed, trade_a.makerOrderId, trade_b.makerOrderId);
        CHECK_EQ(trade_taker_id, index, op, seed, trade_a.takerOrderId, trade_b.takerOrderId);
        CHECK_EQ(trade_trade_id, index, op, seed, trade_a.tradeId, trade_b.tradeId);    
    }

    CHECK_EQ(best_bid, index, op, seed, a.best_bid, b.best_bid);
    CHECK_EQ(best_ask, index, op, seed, a.best_ask, b.best_ask);
}

int main() {
    DenseOrderBook dense;
    OrderBook base;
    size_t N = NUM_OPERATIONS;
    uint64_t seed = 42;
    vector<Operation> script = generate(seed, N);

    for (size_t i = 0; i < script.size(); ++i) {
        const auto& op = script[i];
        Step step_dense = apply_op(dense, op);
        Step step_base = apply_op(base, op);
    
        CHECK(step_dense, step_base, i, op, seed);
    }

    cerr << "PASS: " << N << " ops, seed=" << seed << endl;
    return 0;
}