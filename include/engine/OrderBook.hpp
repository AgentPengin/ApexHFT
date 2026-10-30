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
    PRICE_OUT_OF_BAND,
    INVALID_QUANTITY,
    EXPIRED,
    INSUFFICIENT_MEMORY
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
    FixedVector<Trade, 16> trades; // Giới hạn số lượng giao dịch được ghi lại cho mỗi lệnh
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
    OrderBook(uint64_t min_price = 1, uint64_t tick_size = 1, uint64_t max_price = 4000)
        : instrumentConfig_{1, tick_size, min_price, max_price, 1, 1'000'000, 0} {}

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
    bool check_invariants() const {
        if (!bids.empty() && !asks.empty()) {
            if (bids.begin()->first >= asks.begin()->first) {
                return false;
            }
        } 
        for (const auto& [price, orders] : bids) {
            if (orders.empty()) return false;
            for (const auto& order : orders) {
                if (order.quantity == 0 || order.price != price || (order.quantity != order.remainingQuantity + order.executedQuantity)) {
                    return false;
                }
            }
        }
        for (const auto& [price, orders] : asks) {
            if (orders.empty()) return false;
            for (const auto& order : orders) {
                if (order.quantity == 0 || order.price != price || (order.quantity != order.remainingQuantity + order.executedQuantity)) {
                    return false;
                }
            }
        }
        size_t total_orders_in_book = 0;
        for (const auto& [price, orders] : bids) total_orders_in_book += orders.size();
        for (const auto& [price, orders] : asks) total_orders_in_book += orders.size();

        if (total_orders_in_book != orderLookup_.size()) {
            return false;
        }
        return true;
    }

    Outcome2 cancel_id(uint64_t id) {
        Outcome2 outcome;
        outcome.assigned_id = id;
        outcome.reason = Reason::CANCEL_FAILED;
        auto it = orderLookup_.find(id);
        if (it == orderLookup_.end()) return outcome;
        const OrderLocation& loc = it->second;
        auto cancel_from_book = [&](auto& book) {
            auto price_it = book.find(loc.price);
            if (price_it == book.end()) return;
            auto& orders = price_it->second;
            for (auto order_it = orders.begin(); order_it != orders.end(); ++order_it) {
                if (order_it->orderId == id) {
                    orders.erase(order_it);
                    if (orders.empty()) {
                        book.erase(price_it);
                    }
                    orderLookup_.erase(it);
                    outcome.reason = Reason::CANCEL_ACCEPTED;
                    return;
                }
            }
        };
        if (loc.side == Side::BUY) {
            cancel_from_book(bids);
        } else {
            cancel_from_book(asks);
        }
        return outcome;
    }
    
    Reason validate_order(const Order& order, const InstrumentConfig& config) const {
        if (order.type != OrderType::MARKET) {
            if (order.price < config.min_price || order.price > config.max_price) return Reason::PRICE_OUT_OF_BAND;
            if (order.price % config.tick_size != 0) return Reason::PRICE_OUT_OF_BAND;
        }
        if (order.quantity < config.min_quantity || order.quantity > config.max_quantity) {
            return Reason::INVALID_QUANTITY;
        }
        return Reason::ACCEPTED;
    }

    template <Side S>
    inline Outcome add_order(Order& incoming) {
        Outcome outcome;
        outcome.assigned_id = incoming.orderId;
        Reason r = validate_order(incoming, instrumentConfig_);
        
        if (r != Reason::ACCEPTED) {
            outcome.reason = r;
            return outcome;
        }
        using Traits = SideTraits<S>;
        incoming.status = OrderStatus::NEW;
        uint64_t limitPrice = incoming.price;

        if (incoming.type == OrderType::MARKET) {
            if (S == Side::SELL) limitPrice = 0;
            else limitPrice = UINT64_MAX;
        }
        auto match_and_rest = [&](auto& match_book, auto& rest_book) {
            while (!match_book.empty() && 
                (incoming.type == OrderType::MARKET || Traits::is_price_crossed(limitPrice, match_book.begin()->first)) && 
                incoming.remainingQuantity > 0) 
            {
                uint64_t bestPrice = match_book.begin()->first;
                Order &bestOrder = match_book[bestPrice].front();
                uint32_t matchedQuantity = std::min(incoming.remainingQuantity, bestOrder.remainingQuantity);
                
                incoming.executedQuantity += matchedQuantity;
                outcome.quantity_filled += matchedQuantity;
                
                Trade trade;
                trade.tradeId = tradeIdCounter_++;
                trade.makerOrderId = bestOrder.orderId;
                trade.takerOrderId = incoming.orderId;
                trade.price = bestPrice;
                trade.quantity = matchedQuantity;
                trade.timestamp = rdtsc(); 
                
                incoming.remainingQuantity -= matchedQuantity;
                bestOrder.remainingQuantity -= matchedQuantity;
                bestOrder.executedQuantity += matchedQuantity;
                bestOrder.status = (bestOrder.remainingQuantity == 0) ? OrderStatus::FILLED : OrderStatus::PARTIALLY_FILLED;
                
                outcome.trades.push_back(trade);
                
                if (bestOrder.remainingQuantity == 0) {
                    orderLookup_.erase(bestOrder.orderId);
                    match_book[bestPrice].pop_front();
                    if (match_book[bestPrice].empty()) {
                        match_book.erase(bestPrice);
                    }
                }
            }
            if (incoming.remainingQuantity == 0) {
                incoming.status = OrderStatus::FILLED;
            } else if (incoming.executedQuantity > 0) {
                if (incoming.type == OrderType::LIMIT) 
                    incoming.status = OrderStatus::PARTIALLY_FILLED;
            }

            if (incoming.type == OrderType::LIMIT && incoming.remainingQuantity > 0) {
                rest_book[limitPrice].push_back(incoming);
                orderLookup_[incoming.orderId] = {limitPrice, S};
            } else if (incoming.remainingQuantity > 0 && incoming.type == OrderType::MARKET) {
                outcome.reason = Reason::INSUFFICIENT_LIQUIDITY;
            }
        };
        if constexpr (S == Side::BUY) {
            match_and_rest(asks, bids); 
        } else {
            match_and_rest(bids, asks); 
        }

        outcome.quantity_rested = incoming.remainingQuantity;
        if (incoming.type == OrderType::MARKET && incoming.remainingQuantity > 0) {
            outcome.quantity_rested = 0;
        }
        return outcome;
    }

    inline Outcome add_order(Order& incoming) {
        if (incoming.side == Side::BUY) {
            return add_order<Side::BUY>(incoming);
        } else {
            return add_order<Side::SELL>(incoming);
        }
    }

private:
    uint64_t tradeIdCounter_{1};
    std::map<uint64_t, std::deque<Order>, std::greater<uint64_t>> bids;
    std::map<uint64_t, std::deque<Order>> asks;
    std::unordered_map<uint64_t, OrderLocation> orderLookup_;
    InstrumentConfig instrumentConfig_;
};