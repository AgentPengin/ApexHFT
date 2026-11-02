#pragma once
#include "Types.hpp"
#include <cstdint>

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