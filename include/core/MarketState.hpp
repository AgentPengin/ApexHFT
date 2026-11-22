#pragma once
#include <atomic>
#include <cstdint>

struct TradeEvent {
    uint64_t price{0};
    uint32_t quantity{0};
    uint8_t side{0}; // 0 = BUY, 1 = SELL
    uint64_t timestamp{0};
};

struct alignas(64) MarketState {
    std::atomic<uint64_t> best_bid{0};
    std::atomic<uint64_t> best_ask{0};
    std::atomic<uint64_t> last_price{1000};
    std::atomic<uint64_t> total_trades{0};
    std::atomic<uint64_t> total_orders{0};

    // Circular buffer of recent trades for UI
    static constexpr size_t TRADE_BUFFER_SIZE = 16;
    TradeEvent recent_trades[TRADE_BUFFER_SIZE]{};
    std::atomic<size_t> trade_head{0};

    inline void record_trade(uint64_t price, uint32_t qty, uint8_t side, uint64_t ts) {
        size_t idx = trade_head.fetch_add(1, std::memory_order_relaxed) % TRADE_BUFFER_SIZE;
        recent_trades[idx] = {price, qty, side, ts};
    }
};
