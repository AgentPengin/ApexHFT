#include <iostream>
#include <random>
#include "DenseOrderBook.hpp"

using namespace std;

int main() {
    DenseOrderBook dense_book;
    mt19937_64 rng(42);
    vector<uint64_t> order_ids;

    cout << "Testing 1000 ops..." << endl;
    for (size_t i = 1; i <= 1000; ++i) {
        uint32_t op_type = rng() % 100;
        uint64_t price = 950 + (rng() % 100);
        uint32_t quantity = 10 + (rng() % 50);
        Side side = (rng() % 2 == 0) ? Side::BUY : Side::SELL;

        if (op_type < 20 && !order_ids.empty()) {
            size_t idx = rng() % order_ids.size();
            uint64_t cancel_id = order_ids[idx];
            dense_book.cancel_id(cancel_id);
            order_ids[idx] = order_ids.back();
            order_ids.pop_back();
        } else if (op_type < 50) {
            Order mkt{};
            mkt.orderId = i;
            mkt.price = 0;
            mkt.quantity = quantity;
            mkt.remainingQuantity = quantity;
            mkt.side = side;
            mkt.type = OrderType::MARKET;
            dense_book.add_order(mkt);
        } else {
            Order limit{};
            limit.orderId = i;
            limit.price = price;
            limit.quantity = quantity;
            limit.remainingQuantity = quantity;
            limit.side = side;
            limit.type = OrderType::LIMIT;
            auto res = dense_book.add_order(limit);
            if (res.quantity_rested > 0) {
                order_ids.push_back(i);
            }
        }
    }
    cout << "Finished 1000 ops successfully!" << endl;
    return 0;
}
