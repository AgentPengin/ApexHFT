#pragma once
#include <cstdint>
#include <vector>

constexpr uint32_t NULL_INDEX = 0xFFFFFFFF;

enum class Side : uint8_t { BUY, SELL };

enum class OrderType : uint8_t { 
    LIMIT,
    MARKET, 
    CANCEL
};

enum class OrderStatus : uint8_t { 
    NEW, 
    PARTIALLY_FILLED, 
    FILLED, 
    CANCELED, 
    REJECTED,
    EXPIRED
};

struct Order {
    uint64_t orderId;
    uint64_t price;
    uint64_t timestamp; 
    uint64_t clientOrderId;

    uint32_t clientId;
    uint32_t quantity;
    uint32_t executedQuantity;
    uint32_t remainingQuantity;

    Side side;
    OrderType type;
    OrderStatus status;
    uint8_t timeInForce; 

    uint32_t prev_order{NULL_INDEX};
    uint32_t next_order{NULL_INDEX};

    uint32_t level_idx{NULL_INDEX};
};

struct InstrumentConfig {
    uint32_t symbolId;
    uint64_t tick_size;
    uint64_t min_price;
    uint64_t max_price;
    uint32_t min_quantity;
    uint32_t max_quantity;
    uint64_t collar_pct;
};

struct PriceLevel {
    uint64_t price{0};
    uint64_t total_volume{0};
    uint64_t order_count{0};

    uint32_t head_order{NULL_INDEX};
    uint32_t tail_order{NULL_INDEX};
};

template <size_t MaxOrders> 
class OrderPool {
private:
    Order pool_[MaxOrders];
    uint32_t free_list_[MaxOrders];
    uint32_t free_top_{0};
public:
    OrderPool() {
        for (uint32_t i = 0;i < MaxOrders;i++) {
            free_list_[i] = i;
        }
        free_top_ = MaxOrders;
    }

    uint32_t acquire() {
        if(free_top_ == 0) return NULL_INDEX;
        return free_list_[--free_top_];
    }

    void release(uint32_t idx) {
        pool_[idx] = Order{};
        free_list_[free_top_++] = idx;
    }

    Order& get (uint32_t idx) {
        return pool_[idx];
    }

    const Order& get (uint32_t idx) const {
        return pool_[idx];
    }

};

template <typename T, size_t Capacity>
class FixedVector {
private:
    T data_[Capacity];
    size_t size_{0};
public:
    FixedVector() = default;
    void push_back(const T& item) {
        if (size_ < Capacity) {
            data_[size_++] = item;
        }
    }
    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    void clear() { size_ = 0; }
    T& operator[](size_t index) { return data_[index];}

    const T& operator[](size_t index) const { return data_[index]; }

    T* begin() { return &data_[0]; }
    T* end() { return &data_[size_]; }
    const T* begin() const { return &data_[0]; }
    const T* end() const { return &data_[size_]; }
};

static_assert(sizeof(Order) == 64);
static_assert(alignof(Order) == 8);
