# ApexHFT — Trạng thái hiện tại

> Cập nhật: 2026-10-29
> Đọc file này 10 giây trước khi bắt đầu làm việc, để khỏi phải nhớ lại từ đầu.

---

## Cách build và chạy (lần đầu)

```bash
cmake -S . -B build -G "MinGW Makefiles"
```

```bash
cmake --build build -j 4
```

Chạy test:

```bash
ctest --test-dir build --output-on-failure
```

Chạy benchmark:

```bash
./build/bin/benchmark_ringbuffer.exe
./build/bin/benchmark_orderbook.exe
```

Từ lần sau chỉ cần `cmake --build build -j 4` (không cần configure lại,
trừ khi sửa `CMakeLists.txt`).

### Build Release vs Debug

| | Lệnh | Dùng khi nào |
|---|---|---|
| Release (mặc định) | `cmake -S . -B build -G "MinGW Makefiles"` | Chạy benchmark, đo latency |
| Debug | `cmake -S . -B build-debug -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug` | Đang sửa logic, cần debugger |

**Quan trọng:** test luôn được biên dịch với assert BẬT, kể cả ở Release
(`CMakeLists.txt` thêm `-UNDEBUG`). Nếu không, CMake tự thêm `-DNDEBUG` và
`assert()` biến thành no-op — test sẽ "PASS" ngay cả khi code sai hoàn toàn.

### Option

- `-DAPEX_NATIVE_ARCH=ON` — bật `-march=native`, cho CPU tối ưu cho đúng máy
  này. Chỉ bật khi đo benchmark trên máy mình; **không** dùng nếu muốn mang
  binary sang máy khác.
- `-DAPEX_BUILD_BENCHMARKS=OFF` — bỏ build benchmark (build nhanh hơn).

---

## ✅ Đã xong

### Tầng lõi (`include/core/`)

- **`RingBuffer.hpp` — `SPSCQueue`**
  - Producer `tail_`, consumer `head_`, mỗi cái `alignas(64)` → không false sharing
  - Memory ordering đúng: load `acquire`, store `release`
  - `static_assert` capacity là luỹ thừa 2
  - Đo được: **~48,8 MOPS, 77,8 cycles/op** (10 triệu order, 2 thread)

- **`Types.hpp`**
  - `Order` đúng **64 byte** (`static_assert`) — một cache line
  - Giá là `uint64_t` (tick index), không dùng `double`
  - `NULL_INDEX = 0xFFFFFFFF` cho intrusive list
  - `InstrumentConfig` (tick size, biên giá, biên quantity)
  - `OrderPool<MaxOrders>` + `PriceLevel` — **đang đóng băng**, xem mục sau

- **`TimeStamp.hpp`** — `rdtsc()` qua `__rdtsc()`

- **`DenseBook.hpp`** — Price Ladder bằng bitmap 2 tầng
  (`summary_` → `bitmap_[]` → level), tìm best bằng `ctz`/`clz`.
  Thuật toán **đúng** — đây là cách LMAX/sàn lớn làm.
  **Trạng thái: ĐÓNG BĂNG.** Chưa compile, chưa test, chưa nối vào `OrderBook`.
  Không sửa cho tới khi bản `std::map` có test đầy đủ.

### Tầng engine (`include/engine/OrderBook.hpp`)

`std::map` + `std::deque` — **bản cơ sở (baseline)**, chưa tối ưu nhưng phải đúng.

- `add_order<Side S>` — template hoá theo side, khớp FIFO price-time, khớp từng phần
  - `match_and_rest(match_book, rest_book)` → một nguồn code, hai bản sinh ra, không copy-paste
  - Market order chuẩn hoá giá: BUY → `UINT64_MAX`, SELL → `0`
  - Cập nhật `executedQuantity` + `status` cho **cả maker và taker**
- `validate_order` — kiểm tra biên giá, `tick_size`, biên quantity; MARKET bỏ qua kiểm tra giá
- `cancel_id` — tra `orderLookup_`, xoá khỏi deque, dọn level rỗng; trả `CANCEL_ACCEPTED` / `CANCEL_FAILED`
- `get_best_bid` / `get_best_ask` / `has_best_*`

### Test

`tests/test_orderbook.cpp` — 7 case (rest, full fill, partial, market buy, cancel 2 lần, validate): **PASS**

Kết quả kiểm tra ngoài (sandbox, chưa đưa vào repo):
- Fuzz 200.000 lệnh ngẫu nhiên → 0 vi phạm bất biến, 0 crossed book,
  `live orders 22.276 == lookup 22.276`

### Benchmark

`tests/benchmark_orderbook.cpp` — 1 triệu op, baseline `std::map`:

```
Throughput : 8,78 MOPS
Median     : 380 cycles
P90        : 760 cycles
P99        : 1216 cycles
P99.9      : 4484 cycles
Max        : 119016 cycles
```

Đây là **mốc so sánh**. Bản Price Ladder sau này phải đập được mấy con số này.

### Hạ tầng

- `CMakeLists.txt` — C++20, Release `-O3`, `-Wall -Wextra -Wpedantic`, có ctest
- `.gitignore` — bỏ `*.exe`, `build/`, `build-*`

---

## 🔨 Việc tiếp theo (theo thứ tự)

### 1. Hoàn thiện `OrderBook` baseline — 4 điểm còn hở

| # | Vấn đề | Chỗ sửa |
|---|---|---|
| 1 | Lệnh bị từ chối trả `assigned_id = 0` → client không biết lệnh nào bị chửi | Gán `assigned_id = incoming.orderId` **trước** validate |
| 2 | Lệnh `quantity = 0` bị từ chối với lý do `PRICE_OUT_OF_BAND` (sai chữ) | Thêm `Reason::INVALID_QUANTITY` |
| 3 | Market order khớp một phần rồi hết thanh khoản → `status = PARTIALLY_FILLED`, nhưng nó đã chết | Đổi thành `EXPIRED` (phần dư bị bỏ) |
| 4 | `add_order(Order&)` sửa giá của **object người gọi** (`price` thành `UINT64_MAX`) | Dùng biến local `limitPrice`; hoặc nhận `Order` by value |

### 2. Nâng test lên mức "đủ tin"

`tests/test_orderbook.cpp` hiện còn yếu:

- **Thiếu hàm `check_invariants()`** gọi sau **mỗi** thao tác:
  1. `best_bid < best_ask` (hoặc một bên rỗng)
  2. mọi order trong book: `quantity == executedQuantity + remainingQuantity`
  3. mọi order trong book: có đúng một entry trong `orderLookup_`
  4. `orderLookup_` không chứa order đã bị xoá
- **Thiếu test FIFO**: 2 maker cùng giá, taker khớp một phần → phải khớp maker vào trước
- **Thiếu test market ăn nhiều price level**
- Test 6 đang đặt tên "Market Sell" nhưng nội dung là cancel → đổi tên
- Test 7 dùng aggregate init 12/15 field, và vô tình fail vì **giá** chứ không phải
  vì quantity như ý định → viết rõ ràng struct rồi test từng nhánh validate

### 3. (Sau khi 1 + 2 xong) Chuyển sang Price Ladder

Mở băng `DenseBook.hpp` + `OrderPool` + `PriceLevel`, nối vào `OrderBook`,
rồi **chạy song song hai bản trên cùng chuỗi lệnh** và so kết quả
(*differential testing*). Lệch một đồng là biết ngay bản mới sai.

Trước khi mở băng, phải xử lý:
- `OrderPool<1'000'000>` = **64 MB** → không được khai báo trên stack
  (Windows stack chỉ 1 MB), phải `std::unique_ptr` cấp phát một lần
- `DenseBook::price_to_index` chưa có **bounds check** → giá ngoài dải sẽ ghi
  ra ngoài mảng
- `DenseBook` trộn `uint64_t` và `int64_t` trong phép trừ index → dễ wrap

### 4. Benchmark — nâng cấp dần

- `_mm_pause()` trong vòng spin của `benchmark_ringbuffer.cpp`
- Trong `benchmark_orderbook.cpp`: `uint32_t` chứa số cycles có thể tràn;
  `order_ids` giữ cả id đã bị khớp hết → cancel miss oan, làm sai tỉ lệ cancel
- Calibrate TSC → đổi cycles sang nanosecond

---

## 📌 Quy ước đang dùng

- Giá là **tick index `uint64_t`**, không dùng `double`
- Hot path: **không `new` / `malloc`** khi thị trường đang chạy
- Mọi input từ ngoài vào đều phải **validate**
- Không để hàm khai báo mà chưa định nghĩa (gây link error)
- Mỗi cấu trúc dữ liệu mới phải có test **trước** khi thay thế bản cũ