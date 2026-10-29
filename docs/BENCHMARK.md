# ⚡ ApexHFT: Ground Truth vs. Hardware-Optimized Engine Benchmark

> **Author**: Bùi Quang Phúc  
> **Environment**: Windows x86_64 | MinGW GCC 14.2.0 | Flags: `-std=c++20 -O3 -march=native`  
> **Workload**: 1,000,000 Deterministic Operations (50% Limit Orders, 30% Market Orders, 20% Cancel Orders)  
> **Seed**: `mt19937_64(42)`

---

## 1. Executive Summary

In high-frequency trading (HFT), microseconds are an eternity and nanoseconds determine alpha. This benchmark evaluates the transition from a traditional STL-based matching engine to a custom, mechanical-sympathy engine designed for ultra-low latency and zero dynamic memory allocations on the hot path.

- **Baseline Engine**: `std::map<uint64_t, std::deque<Order>>` (Red-Black Tree with fragmented node allocations).
- **The Monster (`DenseOrderBook`)**: Two-level 64-bit Bitmap Ladder (`summary_` + `bitmap_[]`) with assembly intrinsics (`TZCNT`/`LZCNT`) combined with a fixed pre-allocated `OrderPool<1'000'000>` utilizing an intrusive doubly-linked list.

---

## 2. Differential Testing (Correctness Verification)

Before analyzing performance, both engines were subjected to the exact same stream of **1,000,000 operations** to guarantee functional equivalence:

```
[DIFFERENTIAL VERIFICATION REPORT]
✔ Total Trades Matched  : 252,926 (100% Identical)
✔ Total Volume Matched  : 4,640,409 (100% Identical)
✔ Status                : 100% BIT-FOR-BIT MATCH!
```

---

## 3. Head-to-Head Performance Results

```
========================================================================================
Metric                  std::map (Baseline)      DenseBook (Optimized)      Speedup
========================================================================================
Throughput              8.21 MOPS                12.79 MOPS                 +1.56x
Total Time (1M ops)     0.1218 s                 0.0781 s                   1.56x faster
Median Latency (P50)    380 cycles               189 cycles                 2.01x faster
P90 Latency             836 cycles               532 cycles                 1.57x faster
P99 Latency             1,520 cycles             1,139 cycles               1.33x faster
P99.9 (Tail Latency)    2,545 cycles             1,710 cycles               1.49x faster
Max Latency             392,273 cycles           126,805 cycles             3.09x faster
========================================================================================
```

---

## 4. Key Architectural Takeaways

### 1. Halving the Median Latency ($P50$: 380 $\rightarrow$ 189 cycles)
- `std::map` requires traversing tree pointers across disjoint heap pages, incurring frequent CPU L1/L2 cache misses.
- `DenseOrderBook` locates the best bid/ask in **exactly 2 CPU hardware instructions** (`__builtin_ctzll` and `__builtin_clzll`), which compile directly into x86 `TZCNT`/`LZCNT` executing in $\approx 3$ clock cycles ($< 1\text{ ns}$).

### 2. Zero-Allocation on Hot Path
- The baseline engine constantly invokes `malloc`/`free` when inserting/removing price levels and deque segments, causing allocator lock contention and memory fragmentation.
- `DenseOrderBook` pre-allocates an `OrderPool` of 1,000,000 orders once at startup. Order insertion and deletion execute in $O(1)$ time via simple array index updates (`free_top_` and intrusive list links).

### 3. Understanding Tail Latency Jitter (Why Max Latency varies)
- In a non-isolated consumer OS (Windows), `Max Latency` is dominated by OS kernel scheduling, DPC/ISR interrupts, and core migrations.
- In production trading servers running Linux, core pinning (`pthread_setaffinity_np`) and CPU isolation (`isolcpus`, `nohz_full`) are used to completely eliminate these OS context switches.
