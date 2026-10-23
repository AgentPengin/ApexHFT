#pragma once
#include <cstdint>
#include <deque>
#include <map>
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
    Reason reason;
    uint64_t quantity_rested = 0;
    uint64_t quantity_filled = 0;
    uint64_t assigned_id = 0;    
    std::vector<Trade> trades;
};

struct Outcome2 {
    Reason reason;
    uint64_t assigned_id = 0;
};

struct OrderLocation {
    uint64_t price;
    Side side;
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

    inline Outcome add_order(Order incoming) {
        Outcome outcome;
        outcome.assigned_id = incoming.orderId;
        if (incoming.side == Side::BUY) {
            while (!asks.empty() && (incoming.price == OrderType::MARKET || asks.begin()->first <= incoming.price) && incoming.remainingQuantity > 0) {
                uint64_t bestAskPrice = asks.begin()->first;
                Order &bestAskOrder = asks[bestAskPrice].front();
                uint32_t matchedQuantity = std::min(incoming.remainingQuantity, bestAskOrder.remainingQuantity);
                outcome.quantity_filled += matchedQuantity;
                Trade trade;
                trade.tradeId = tradeIdCounter_++;
                trade.makerOrderId = bestAskOrder.orderId;
                trade.takerOrderId = incoming.orderId;
                trade.price = bestAskPrice;
                trade.quantity = matchedQuantity;
                trade.timestamp = rdtsc(); // Assuming you have a function to get the current timestamp
                incoming.remainingQuantity -= matchedQuantity;
                bestAskOrder.remainingQuantity -= matchedQuantity;
                outcome.trades.push_back(trade);
                if (bestAskOrder.remainingQuantity == 0) {
                    orderLookup_.erase(bestAskOrder.orderId);s
                    asks[bestAskPrice].pop_front();
                    if (asks[bestAskPrice].empty()) {
                        asks.erase(bestAskPrice);
                    }
                }
            }
            if (incoming.OrderType == OrderType::LIMIT && incoming.remainingQuantity > 0) {
                bids[incoming.price].push_back(incoming);
                orderLookup_[incoming.orderId] = {incoming.price, Side::BUY};
            } else if (incoming.remainingQuantity > 0 && incoming.type == OrderType::MARKET) {
                cancel_id(incoming.orderId);
            }
        } else {
            while (!bids.empty() && bids.begin()->first >= incoming.price && incoming.remainingQuantity > 0) {
                uint64_t bestBidPrice = bids.begin()->first;
                Order &bestBidOrder = bids[bestBidPrice].front();
                uint32_t matchedQuantity = std::min(incoming.remainingQuantity, bestBidOrder.remainingQuantity);
                outcome.quantity_filled += matchedQuantity;
                Trade trade;
                trade.tradeId = tradeIdCounter_++;
                trade.makerOrderId = bestBidOrder.orderId;
                trade.takerOrderId = incoming.orderId;
                trade.price = bestBidPrice;
                trade.quantity = matchedQuantity;
                trade.timestamp = rdtsc(); // Assuming you have a function to get the current timestamp
                incoming.remainingQuantity -= matchedQuantity;
                bestBidOrder.remainingQuantity -= matchedQuantity;
                outcome.trades.push_back(trade);
                if (bestBidOrder.remainingQuantity == 0) {
                    bids[bestBidPrice].pop_front();
                    if (bids[bestBidPrice].empty()) {
                        bids.erase(bestBidPrice);
                    }
                }
            }
            if (incoming.OrderType == OrderType::LIMIT && incoming.remainingQuantity > 0) {
                orderLookup_.erase(bestBidOrder.orderId);
                asks[incoming.price].push_back(incoming);
                orderLookup_[incoming.orderId] = {incoming.price, Side::SELL};
            } else if (incoming.remainingQuantity > 0 && incoming.type == OrderType::MARKET) {
                cancel_id(incoming.orderId);
            }
        }
        outcome.quantity_rested = incoming.remainingQuantity;
        outcome.reason = Reason::ACCEPTED;
        return outcome;
    }

private:
    uint64_t tradeIdCounter_{1};
    std::map<uint64_t, std::deque<Order>, std::greater<uint64_t>> bids;
    std::map<uint64_t, std::deque<Order>> asks;
    std::unordered_map<uint64_t, OrderLocation> orderLookup_;
};