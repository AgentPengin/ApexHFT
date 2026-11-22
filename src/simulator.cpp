#include <atomic>
#include <array>
#include <thread>
#include <chrono>
#include <latch>
#include <csignal>
#include "RingBuffer.hpp"
#include "Types.hpp"
#include "DenseOrderBook.hpp"
#include "MarketState.hpp"
#include "MarketMakerBot.hpp"
#include "NoiseTraderBot.hpp"
#include "TerminalUI.hpp"

constexpr size_t NUM_BOTS = 2;

std::atomic<bool> running{true};
MarketState market_state;

void signal_handler(int) {
    running.store(false, std::memory_order_release);
}

void engine_worker(std::array<SPSCQueue<Order, 1024>, NUM_BOTS>& queues,
                   DenseOrderBook& book,
                   MarketState& market_state,
                   const std::atomic<bool>& running,
                   std::latch& start_gate) {
    start_gate.arrive_and_wait();
    Order order{};
    uint64_t processed = 0;
    uint64_t trades = 0;

    while (running.load(std::memory_order_relaxed)) {
        bool any_popped = false;
        for (size_t i = 0; i < NUM_BOTS; ++i) {
            if (queues[i].pop(order)) {
                any_popped = true;
                if (order.type == OrderType::CANCEL) {
                    book.cancel_id(order.orderId);
                } else {
                    auto outcome = book.add_order(order);
                    if (outcome.trades.size() > 0) {
                        market_state.last_price.store(outcome.trades[0].price, std::memory_order_relaxed);
                        for (size_t t = 0; t < outcome.trades.size(); ++t) {
                            const auto& tr = outcome.trades[t];
                            market_state.record_trade(tr.price, tr.quantity, (order.side == Side::BUY ? 0 : 1), tr.timestamp);
                        }
                        trades += outcome.trades.size();
                        market_state.total_trades.store(trades, std::memory_order_relaxed);
                    }
                }
                uint64_t bb = book.has_best_bid() ? book.get_best_bid() : 0;
                uint64_t ba = book.has_best_ask() ? book.get_best_ask() : 0;
                market_state.best_bid.store(bb, std::memory_order_relaxed);
                market_state.best_ask.store(ba, std::memory_order_relaxed);

                ++processed;
                market_state.total_orders.store(processed, std::memory_order_relaxed);
            }
        }
        if (!any_popped) {
            _mm_pause();
        }
    }

    // Drain remaining orders in queues before exiting
    bool draining = true;
    while (draining) {
        draining = false;
        for (size_t i = 0; i < NUM_BOTS; ++i) {
            if (queues[i].pop(order)) {
                draining = true;
                if (order.type == OrderType::CANCEL) {
                    book.cancel_id(order.orderId);
                } else {
                    auto outcome = book.add_order(order);
                    if (outcome.trades.size() > 0) {
                        market_state.last_price.store(outcome.trades[0].price, std::memory_order_relaxed);
                        for (size_t t = 0; t < outcome.trades.size(); ++t) {
                            const auto& tr = outcome.trades[t];
                            market_state.record_trade(tr.price, tr.quantity, (order.side == Side::BUY ? 0 : 1), tr.timestamp);
                        }
                        trades += outcome.trades.size();
                        market_state.total_trades.store(trades, std::memory_order_relaxed);
                    }
                }
                ++processed;
                market_state.total_orders.store(processed, std::memory_order_relaxed);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signal_handler);

    // Mặc định chạy demo 10 giây (hoặc truyền số giây từ CLI, 0 để chạy vô hạn)
    uint32_t run_seconds = 10;
    if (argc > 1) {
        run_seconds = static_cast<uint32_t>(std::atoi(argv[1]));
    }

    std::array<SPSCQueue<Order, 1024>, NUM_BOTS> queues;
    DenseOrderBook book;

    // 4 luồng đồng bộ xuất phát: engine + MM + noise + main
    std::latch start_gate(4);

    MarketMakerBot mm_bot(1, queues[0], market_state, running, start_gate);
    NoiseTraderBot noise_bot(2, queues[1], market_state, running, start_gate);

    std::thread engine_thread(engine_worker, std::ref(queues), std::ref(book), std::ref(market_state), std::ref(running), std::ref(start_gate));
    std::thread mm_thread(&MarketMakerBot::run, &mm_bot);
    std::thread noise_thread(&NoiseTraderBot::run, &noise_bot);

    // Cùng xuất phát
    start_gate.arrive_and_wait();

    // Bật luồng UI vẽ bảng điện terminal ở 20 FPS
    std::thread ui_thread(TerminalUI::render_loop, std::ref(market_state), std::ref(running), 20);

    // Đợi hết thời gian hoặc Ctrl+C
    if (run_seconds > 0) {
        for (uint32_t s = 0; s < run_seconds && running.load(std::memory_order_relaxed); ++s) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        running.store(false, std::memory_order_release);
    }

    // Thu dọn luồng sạch sẽ
    if (ui_thread.joinable()) ui_thread.join();
    if (mm_thread.joinable()) mm_thread.join();
    if (noise_thread.joinable()) noise_thread.join();
    if (engine_thread.joinable()) engine_thread.join();

    std::cout << "\n\033[1;32m[SIMULATION COMPLETED CLEANLY]\033[0m\n";
    std::cout << "Final Best Bid: " << book.get_best_bid() << " | Final Best Ask: " << book.get_best_ask() << "\n";
    std::cout << "Total Orders: " << market_state.total_orders.load() 
              << " | Total Trades: " << market_state.total_trades.load() << "\n";

    return 0;
}
