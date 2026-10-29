#include "OrderBook.hpp"
#include <cstdint>
#include <iostream>
#include <assert.h>
#include "Types.hpp"
#include <assert.h>

using namespace std;

inline Order make_order(uint64_t id, Side side, OrderType type, uint64_t price, uint32_t qty) {
    Order o{};
    o.orderId = id;
    o.side = side;
    o.type = type;
    o.price = price;
    o.quantity = qty;
    o.remainingQuantity = qty;
    o.status = OrderStatus::NEW;
    return o;
}

int main() {
    // Test 1: Limit Buy & Rest
    OrderBook orderBook;
    Order limitBuyOrder = make_order(1, Side::BUY, OrderType::LIMIT, 100, 5);
    orderBook.add_order(limitBuyOrder);
    cout << "Test 1: Limit Buy & Rest" << endl;
    assert(orderBook.has_best_bid());
    assert(orderBook.get_best_bid() == 100);

    cout << "Test 1: Limit Buy & Rest - PASSED" << endl;

    // Test 2: Limit Sell & Rest
    Order limitSellOrder = make_order(2, Side::SELL, OrderType::LIMIT, 105, 5);
    orderBook.add_order(limitSellOrder);
    cout << "Test 2: Limit Sell & Rest" << endl;
    assert(orderBook.has_best_ask());
    assert(orderBook.get_best_ask() == 105);

    cout << "Test 2: Limit Sell & Rest - PASSED" << endl;

    // Test 3: Full fill 
    Order limitSellOrder2 = make_order(3, Side::SELL, OrderType::LIMIT, 100, 5);
    orderBook.add_order(limitSellOrder2);
    cout << "Test 3: Full fill" << endl;
    assert(!orderBook.has_best_bid());

    cout << "Test 3: Full fill - PASSED" << endl;

    // Test 4: Partial fill
    Order limitBuyOrder2 = make_order(4, Side::BUY, OrderType::LIMIT, 100, 10);
    orderBook.add_order(limitBuyOrder2);
    Order limitSellOrder3 = make_order(5, Side::SELL, OrderType::LIMIT, 100, 5);
    orderBook.add_order(limitSellOrder3);
    cout << "Test 4: Partial fill" << endl;
    assert(orderBook.has_best_bid());
    assert(orderBook.get_best_bid() == 100);

    cout << "Test 4: Partial fill - PASSED" << endl;

    // Test 5: Market Buy
    Order marketBuyOrder = make_order(6, Side::BUY, OrderType::MARKET, 0, 5);
    orderBook.add_order(marketBuyOrder);
    cout << "Test 5: Market Buy" << endl;
    assert(!orderBook.has_best_ask());
    
    cout << "Test 5: Market Buy - PASSED" << endl;

    // Test 6: Cancel Order
    Outcome2 outcome = orderBook.cancel_id(4);
    assert(outcome.reason == Reason::CANCEL_ACCEPTED);

    Outcome2 outcome2 = orderBook.cancel_id(4);
    assert(outcome2.reason == Reason::CANCEL_FAILED);

    cout << "Test 6: Cancel Order - PASSED" << endl;
    
    // Test 7: Validation order
    InstrumentConfig config{1, 1, 10, 1000, 1, 100, 5};
    Order invalidOrder{7, 5, 70, 7, 7, 10, 0, 10, Side::BUY, OrderType::LIMIT, OrderStatus::NEW, 0};
    cout << "Test 7: Validation order" << endl;
    Reason r = orderBook.validate_order(invalidOrder, config);
    assert(r == Reason::PRICE_OUT_OF_BAND);
    cout << "Test 7: Validation order - PASSED" << endl;

    // Test 8: Check Invariants
    assert(orderBook.check_invariants());
    cout << "Test 8: Check Invariants - PASSED" << endl;

    // Test 9: FIFO Order Matching
    orderBook = OrderBook(); // Reset the order book
    Order limitSellOrder4 = make_order(8, Side::SELL, OrderType::LIMIT, 100, 5);
    orderBook.add_order(limitSellOrder4);
    Order limitSellOrder5 = make_order(9, Side::SELL, OrderType::LIMIT, 100, 5);
    orderBook.add_order(limitSellOrder5);
    Order marketBuyOrder2 = make_order(10, Side::BUY, OrderType::LIMIT, 100, 5);
    Outcome outcome9 = orderBook.add_order(marketBuyOrder2);
    for (const auto& trade : outcome9.trades) {
        assert(trade.makerOrderId == 8); 
    }

    cout << "Test 9: FIFO Order Matching - PASSED" << endl;

    // Test 10: Market Order
    orderBook = OrderBook(); // Reset the order book
    Order limitSellOrder6 = make_order(11, Side::SELL, OrderType::LIMIT, 101, 5);
    orderBook.add_order(limitSellOrder6);
    Order limitSellOrder7 = make_order(12, Side::SELL, OrderType::LIMIT, 102, 5);
    orderBook.add_order(limitSellOrder7);
    Order limitSellOrder8 = make_order(13, Side::SELL, OrderType::LIMIT, 103, 5);
    orderBook.add_order(limitSellOrder8);
    Order marketBuyOrder3 = make_order(14, Side::BUY, OrderType::MARKET, 0, 12);
    Outcome outcome10 = orderBook.add_order(marketBuyOrder3);
    bool trade101 = false, trade102 = false, trade103 = false;
    for (const auto& trade : outcome10.trades) {
        if (trade.price == 101) trade101 = true;
        if (trade.price == 102) trade102 = true;
        if (trade.price == 103) trade103 = true;
    }
    assert(trade101 && trade102 && trade103);
    cout << "Test 10: Market Order - PASSED" << endl;

    // Test 11: Market Order with Insufficient Liquidity
    orderBook = OrderBook(); // Reset the order book
    Order limitSellOrder9 = make_order(15, Side::SELL, OrderType::LIMIT, 101, 5);
    orderBook.add_order(limitSellOrder9);
    Order marketBuyOrder4 = make_order(16, Side::BUY, OrderType::MARKET, 0, 12);
    Outcome outcome11 = orderBook.add_order(marketBuyOrder4);
    assert(outcome11.trades.size() == 1);
    assert(outcome11.quantity_filled == 5);
    assert(outcome11.reason == Reason::INSUFFICIENT_LIQUIDITY);
    cout << "Test 11: Market Order with Insufficient Liquidity - PASSED" << endl;

}
