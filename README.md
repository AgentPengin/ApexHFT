# ApexHFT

ApexHFT is a C++20 limit order book and matching engine for a future bot trading simulation arena. The current prototype matches orders in memory using price-time priority. It supports limit orders, market orders, partial fills, and cancellation by order ID.

## Engine design

- [`OrderBook`](include/engine/OrderBook.hpp) is the reference implementation. It uses `std::map` for price levels and `std::deque` to keep orders at each price in arrival order.
- [`DenseOrderBook`](include/engine/DenseOrderBook.hpp) uses a two-level bitmap to find the best bid and ask in a bounded price grid. Resting orders are held in a preallocated pool and linked in FIFO order at each price. A flat hash table maps order IDs to pool entries for cancellation.
- [`SPSCQueue`](include/core/RingBuffer.hpp) is a single-producer, single-consumer ring buffer with its own benchmark.

## Build and test

The commands below use Windows x86-64, CMake 3.20 or newer, and MinGW-w64 with a C++20-capable GCC. Run them from PowerShell with `g++` and `mingw32-make` on your `PATH`.

```powershell
git clone https://github.com/AgentPengin/ApexHFT.git
cd ApexHFT
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

[`test_orderbook`](tests/test_orderbook.cpp) covers matching, FIFO, cancellation, and order book invariants. [`test_differential`](tests/test_differential.cpp) compares the two implementations over a generated workload of 1 million operations. In engine revision `e52ac55`, the differential test fails on an order result code comparison.

## Benchmarks

From the repository root, run the Release executables:

```powershell
.\build\bin\benchmark_orderbook.exe
.\build\bin\benchmark_ringbuffer.exe
```

The order book benchmark compares throughput and latency percentiles on a synthetic mix of limit orders, market orders, and cancellations. See the [historical benchmark report](docs/BENCHMARK.md) for an earlier run and its environment. Use CTest separately to check correctness.

## Roadmap

- Add a bot interface for receiving market updates and submitting orders.
- Run multiple bots against the matching engine in a shared simulated market.
- Record fills and track bot positions and profit and loss so runs can be replayed and compared.
