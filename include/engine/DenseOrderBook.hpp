#pragma once
#include <cstdint>
#include <memory>
#include <unordered_map>
#include "Types.hpp"
#include "DenseBook.hpp"
#include "TimeStamp.hpp"
#include "OrderBook.hpp"

static constexpr uint64_t INVALID_PRICE = 0; // Hoặc std::numeric_limits<uint64_t>::max()

class DenseOrderBook {
public:
    static constexpr size_t MAX_ORDERS = 1'000'000;
    static constexpr size_t MAX_TICKS = 4096;

    DenseOrderBook(uint64_t min_price = 1, uint64_t tick_size = 1, uint64_t max_price = 4000) 
        : pool_(std::make_unique<OrderPool<MAX_ORDERS>>()),
          bids_(min_price, tick_size), 
          asks_(min_price, tick_size),
          instrumentConfig_{1, tick_size, min_price, max_price, 1, 1'000'000, 0} {}

    void append_order(PriceLevel& level, uint32_t order_idx) {
        Order& order = pool_ -> get(order_idx);
        order.prev_order = level.tail_order;
        order.next_order = NULL_INDEX;

        if (level.tail_order != NULL_INDEX) {
            pool_ -> get(level.tail_order).next_order = order_idx;
        } else {
            level.head_order = order_idx;
        }
        level.tail_order = order_idx;
        level.total_volume += order.remainingQuantity;
        level.order_count++;
    }

    void remove_order(PriceLevel& level, uint32_t order_idx) {
        Order& order = pool_ ->get(order_idx);
        if (order.prev_order != NULL_INDEX) {
            pool_ -> get(order.prev_order).next_order = order.next_order;
        } else {
            level.head_order = order.next_order;
        }

        if (order.next_order != NULL_INDEX) {
            pool_ -> get(order.next_order).prev_order = order.prev_order;
        } else {
            level.tail_order = order.prev_order;
        }
        level.total_volume -= order.remainingQuantity;
        level.order_count--;
        pool_ -> release(order_idx);
    }

    Outcome2 cancel_id(uint64_t id) {
        Outcome2 outcome;
        outcome.assigned_id = id;
        outcome.reason = Reason::CANCEL_FAILED;
        
        auto it = orderLookup_.find(id);
        if (it == orderLookup_.end()) return outcome;

        uint32_t order_idx = it->second;
        Order& order = pool_->get(order_idx);
        uint64_t price = order.price;


        if (order.side == Side::BUY) {
            PriceLevel& level = bids_.get_level(order.price);
            remove_order(level, order_idx);
            if (level.order_count == 0) {
                bids_.deactivate_level(price);
            }
        } else {
            PriceLevel& level = asks_.get_level(order.price);
            remove_order(level, order_idx);
            if (level.order_count == 0) {
                asks_.deactivate_level(price);
            }
        }
        orderLookup_.erase(it);
        outcome.reason = Reason::CANCEL_ACCEPTED;
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
            limitPrice = (S == Side::SELL) ? 0 : UINT64_MAX;
        }

        auto match_and_rest = [&](auto& match_book, auto& rest_book) {
            auto get_best_match_price = [&]() -> int64_t {
                if constexpr (S == Side::BUY) {
                    return match_book.get_best_ask(); // BUY: tìm giá ask thấp nhất
                } else {
                    return match_book.get_best_bid(); // SELL: tìm giá bid cao nhất
                }
            };
            int64_t best_match = get_best_match_price();
            
            while (best_match != -1 && 
                SideTraits<S>::is_price_crossed(limitPrice, static_cast<uint64_t>(best_match)) && 
                incoming.remainingQuantity > 0) 
            {
                uint64_t bestPrice = static_cast<uint64_t>(best_match);
                PriceLevel& level = match_book.get_level(bestPrice);



                uint32_t maker_idx = level.head_order;
                Order& bestOrder = pool_->get(maker_idx);
                uint32_t matchedQuantity = std::min(incoming.remainingQuantity, bestOrder.remainingQuantity);
                incoming.executedQuantity += matchedQuantity;
                outcome.quantity_filled += matchedQuantity;
                Trade trade{tradeIdCounter_++, bestOrder.orderId, incoming.orderId, bestPrice, matchedQuantity, rdtsc()};
                incoming.remainingQuantity -= matchedQuantity;
                bestOrder.remainingQuantity -= matchedQuantity;
                level.total_volume -= matchedQuantity;
                bestOrder.executedQuantity += matchedQuantity;
                bestOrder.status = (bestOrder.remainingQuantity == 0) ? OrderStatus::FILLED : OrderStatus::PARTIALLY_FILLED;
                outcome.trades.push_back(trade);
                if (bestOrder.remainingQuantity == 0) {
                    orderLookup_.erase(bestOrder.orderId);
                    remove_order(level, maker_idx);
                    if (level.order_count == 0) {
                        match_book.deactivate_level(bestPrice);
                    }
                }
                best_match = get_best_match_price();
            }
            if (incoming.remainingQuantity == 0) {
                incoming.status = OrderStatus::FILLED;
            } else if (incoming.type == OrderType::MARKET) {
                incoming.status = OrderStatus::EXPIRED;
                outcome.reason = (incoming.executedQuantity > 0) ? Reason::EXPIRED : Reason::INSUFFICIENT_LIQUIDITY;
            } else if (incoming.executedQuantity > 0) {
                incoming.status = OrderStatus::PARTIALLY_FILLED;
            }
            if (incoming.type == OrderType::LIMIT && incoming.remainingQuantity > 0) {
                uint32_t new_idx = pool_->acquire();
                if (new_idx != NULL_INDEX) {
                    Order& rested = pool_->get(new_idx);
                    rested = incoming;
                    PriceLevel& level = rest_book.get_level(incoming.price);
                    append_order(level, new_idx);
                    rest_book.activate_level(incoming.price);
                    orderLookup_[incoming.orderId] = new_idx;
                }  else {
                    outcome.reason = Reason::INSUFFICIENT_MEMORY;
                    incoming.status = OrderStatus::REJECTED;
                }
            }
        };
        if constexpr (S == Side::BUY) {
            match_and_rest(asks_, bids_); 
        } else {
            match_and_rest(bids_, asks_); 
        }

        outcome.quantity_rested = incoming.remainingQuantity;
        if (incoming.type == OrderType::MARKET && incoming.remainingQuantity > 0) {
            outcome.quantity_rested = 0;
        }
        return outcome;
    }
    inline Outcome add_order(Order& incoming) {
        if (incoming.side == Side::BUY) return add_order<Side::BUY>(incoming);
        return add_order<Side::SELL>(incoming);
    }


    bool has_best_bid() const { return bids_.get_best_bid() != -1; }
    bool has_best_ask() const { return asks_.get_best_ask() != -1; }
    uint64_t get_best_bid() const { 
        if (!has_best_bid()) return INVALID_PRICE;
        return static_cast<uint64_t>(bids_.get_best_bid()); 
    }
    uint64_t get_best_ask() const { 
        if (!has_best_ask()) return INVALID_PRICE;
        return static_cast<uint64_t>(asks_.get_best_ask()); 
    }


private:
    uint64_t tradeIdCounter_{1};
    std::unique_ptr<OrderPool<MAX_ORDERS>> pool_;
    DenseBook<MAX_TICKS> bids_;
    DenseBook<MAX_TICKS> asks_;
    std::unordered_map<uint64_t, uint32_t> orderLookup_;
    InstrumentConfig instrumentConfig_;
};
