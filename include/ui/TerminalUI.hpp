#pragma once
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include "MarketState.hpp"

class TerminalUI {
public:
    static void render_loop(const MarketState& state, 
                            const std::atomic<bool>& running, 
                            uint32_t fps = 20) {
        auto frame_duration = std::chrono::milliseconds(1000 / fps);

        while (running.load(std::memory_order_relaxed)) {
            render(state);
            export_json(state, "web/market_data.json");
            std::this_thread::sleep_for(frame_duration);
        }
        render(state);
        export_json(state, "web/market_data.json");
    }

    static void export_json(const MarketState& state, const std::string& filepath) {
        uint64_t bid = state.best_bid.load(std::memory_order_relaxed);
        uint64_t ask = state.best_ask.load(std::memory_order_relaxed);
        uint64_t last = state.last_price.load(std::memory_order_relaxed);
        uint64_t trades = state.total_trades.load(std::memory_order_relaxed);
        uint64_t orders = state.total_orders.load(std::memory_order_relaxed);

        std::ofstream f(filepath, std::ios::trunc);
        if (!f.is_open()) return;

        f << "{\n";
        f << "  \"engine_status\": \"LIVE\",\n";
        f << "  \"last_price\": " << last << ",\n";
        f << "  \"best_bid\": " << bid << ",\n";
        f << "  \"best_ask\": " << ask << ",\n";
        f << "  \"spread\": " << (ask >= bid && bid > 0 ? (ask - bid) : 0) << ",\n";
        f << "  \"total_orders\": " << orders << ",\n";
        f << "  \"total_trades\": " << trades << ",\n";
        f << "  \"recent_trades\": [\n";

        size_t head = state.trade_head.load(std::memory_order_relaxed);
        size_t count = std::min<size_t>(head, MarketState::TRADE_BUFFER_SIZE);
        for (size_t i = 0; i < count; ++i) {
            size_t idx = (head + MarketState::TRADE_BUFFER_SIZE - 1 - i) % MarketState::TRADE_BUFFER_SIZE;
            const auto& t = state.recent_trades[idx];
            if (t.price == 0) continue;
            f << "    {\"price\": " << t.price << ", \"qty\": " << t.quantity 
              << ", \"side\": \"" << (t.side == 0 ? "BUY" : "SELL") << "\"}";
            if (i + 1 < count) f << ",";
            f << "\n";
        }
        f << "  ]\n";
        f << "}\n";
    }

    static void render(const MarketState& state) {
        uint64_t bid = state.best_bid.load(std::memory_order_relaxed);
        uint64_t ask = state.best_ask.load(std::memory_order_relaxed);
        uint64_t last = state.last_price.load(std::memory_order_relaxed);
        uint64_t trades = state.total_trades.load(std::memory_order_relaxed);
        uint64_t orders = state.total_orders.load(std::memory_order_relaxed);

        std::cout << "\033[2J\033[H";
        std::cout << "\033[1;36m======================================================================\033[0m\n";
        std::cout << "\033[1;37m                 APEX HFT - LIVE MARKET SIMULATOR                     \033[0m\n";
        std::cout << "\033[1;36m======================================================================\033[0m\n";
        std::cout << " Engine Mode: \033[1;32mLIVE STREAMING\033[0m | Web Bridge: \033[1;33mweb/market_data.json\033[0m\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << " [MARKET SUMMARY]\n";
        std::cout << "   Last Traded Price: \033[1;33m" << std::setw(6) << last << "\033[0m\n";
        if (bid > 0 && ask > 0 && ask >= bid) {
            std::cout << "   Spread:            \033[1;37m" << std::setw(6) << (ask - bid) << " ticks\033[0m\n";
        } else {
            std::cout << "   Spread:            \033[1;31m   N/A (No 2-sided liquidity)\033[0m\n";
        }
        std::cout << "   Total Ingested:    \033[1;37m" << std::setw(8) << orders << " orders\033[0m\n";
        std::cout << "   Total Executed:    \033[1;32m" << std::setw(8) << trades << " trades\033[0m\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << " [ORDER BOOK LADDER]\n";
        std::cout << "   SIDE      PRICE       INDICATOR\n";
        std::cout << "   -------------------------------------------------------------\n";
        
        if (ask > 0) {
            std::cout << "   \033[1;31mASK       " << std::setw(6) << ask << "      [ BEST ASK ] ##########\033[0m\n";
        } else {
            std::cout << "   \033[1;31mASK       [EMPTY]     (No sellers available)\033[0m\n";
        }

        std::cout << "   -------------------------------------------------------------\n";

        if (bid > 0) {
            std::cout << "   \033[1;32mBID       " << std::setw(6) << bid << "      [ BEST BID ] ##########\033[0m\n";
        } else {
            std::cout << "   \033[1;32mBID       [EMPTY]     (No buyers available)\033[0m\n";
        }

        std::cout << "----------------------------------------------------------------------\n";
        std::cout << " Web UI: Open http://localhost:8080 in your browser to view Binance UI\n";
        std::cout << " Press Ctrl+C in terminal to stop simulation.\n" << std::flush;
    }
};
