#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include "Types.hpp"
#include "TimeStamp.hpp"

// ask là bán, bid là mua

enum class Reason : uint8_t {
    ACCEPTED,
    MODIFY_ACCEPTED,
    MODIFY_FAILED,
    CANCEL_ACCEPTED,
    CANCEL_FAILED,
    NOTHING_TO_MODIFY,
    INSUFFICIENT_LIQUIDITY,
    PRICE_OUT_OF_BAND
};

struct Trade {
    uint64_t tradeId;
    uint64_t makerOrderId;
    uint64_t takerOrderId;
    uint64_t price;
    uint32_t quantity;
    uint64_t timestamp;
};

struct Outcome {
    Reason reason = Reason::ACCEPTED;
    uint64_t quantity_rested = 0;
    uint64_t quantity_filled = 0;
    uint64_t assigned_id = 0;    
    std::vector<Trade> trades;
};

struct Outcome2 {
    Reason reason = Reason::ACCEPTED;
    uint64_t assigned_id = 0;
};

struct OrderLocation {
    uint64_t price;
    Side side;
};

template <Side S>
struct SideTraits;

template <>
struct SideTraits<Side::BUY> {
    static bool is_price_crossed(uint64_t incoming_price, uint64_t book_price) {
        return incoming_price >= book_price;
    }
};

template <>
struct SideTraits<Side::SELL> {
    static bool is_price_crossed(uint64_t incoming_price, uint64_t book_price) {
        return incoming_price <= book_price;
    }
};

class OrderBook {
public:
    OrderBook() = default;
    uint64_t get_order_count() const;
    Outcome2 cancel_id(uint64_t resting);
    Outcome2 modify_order(uint64_t id, uint64_t size);
    Outcome2 modify_price(uint64_t id, uint64_t price);

    bool has_best_bid() const {
        return !bids.empty();
    }
    uint64_t get_best_bid() const {
        return bids.begin()->first;
    }
    bool has_best_ask() const {
        return !asks.empty();
    }
    uint64_t get_best_ask() const {
        return asks.begin()->first;
    }

    bool validate_order(const Order& order, const InstrumentConfig& config) const {
        if (order.price < config.min_price || order.price > config.max_price) {
            return false;
        }
        if (order.quantity < config.min_quantity || order.quantity > config.max_quantity) {
            return false;
        }
        if (order.price % config.tick_size != 0) {
            return false;
        }
        return true;
    }

    template <Side S>
    inline Outcome add_order(Order& incoming) {
        if (!validate_order(incoming, instrumentConfig_)) {
            Outcome outcome;
            outcome.reason = Reason::PRICE_OUT_OF_BAND;
            return outcome;
        }
        using Traits = SideTraits<S>;

        Outcome outcome;
        incoming.status = OrderStatus::NEW;
        outcome.assigned_id = incoming.orderId;
        if (incoming.type == OrderType::MARKET) {
            if (S == Side::SELL) {
                incoming.price = 0; 
            } else {
                incoming.price = UINT64_MAX;
            }
        }
        auto& book = (S == Side::BUY) ? asks : bids;
        
        while (!book.empty() && Traits::is_price_crossed(incoming.price, book.begin()->first) && incoming.remainingQuantity > 0) {
            uint64_t bestPrice = book.begin()->first;
            Order &bestOrder = book[bestPrice].front();
            uint32_t matchedQuantity = std::min(incoming.remainingQuantity, bestOrder.remainingQuantity);
            incoming.executedQuantity += matchedQuantity;
            incoming.quantity_filled += matchedQuantity;
            Trade trade;
            trade.tradeId = tradeIdCounter_++;
            trade.makerOrderId = bestOrder.orderId;
            trade.takerOrderId = incoming.orderId;
            trade.price = bestPrice;
            trade.quantity = matchedQuantity;
            trade.timestamp = rdtsc(); 
            incoming.remainingQuantity -= matchedQuantity;
            bestOrder.remainingQuantity -= matchedQuantity;
            outcome.trades.push_back(trade);
            if (bestOrder.remainingQuantity == 0) {
                orderLookup_.erase(bestOrder.orderId);
                book[bestPrice].pop_front();
                if (book[bestPrice].empty()) {
                    book.erase(bestPrice);
                }
            }
        }
        if (incoming.remainingQuantity == 0) {
            incoming.status = OrderStatus::FILLED;
        } else if (incoming.executedQuantity > 0) {
            incoming.status = OrderStatus::PARTIALLY_FILLED;
        }
        if (incoming.type == OrderType::LIMIT && incoming.remainingQuantity > 0) {
            book[incoming.price].push_back(incoming);
            orderLookup_[incoming.orderId] = {incoming.price, S};
        } else if (incoming.remainingQuantity > 0 && incoming.type == OrderType::MARKET) {
            outcome.reason = Reason::INSUFFICIENT_LIQUIDITY;
        } 
        outcome.quantity_rested = incoming.remainingQuantity;
        if (incoming.type == OrderType::MARKET && incoming.remainingQuantity > 0) {
            outcome.quantity_rested = 0;
        }
        return outcome;
    }

private:
    uint64_t tradeIdCounter_{1};
    std::map<uint64_t, std::deque<Order>, std::greater<uint64_t>> bids;
    std::map<uint64_t, std::deque<Order>> asks;
    std::unordered_map<uint64_t, OrderLocation> orderLookup_;
    InstrumentConfig instrumentConfig_;
};