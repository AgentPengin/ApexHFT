#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include "Types.hpp"

template <size_t MaxTicks>
class DenseBook {
private:
    static constexpr size_t NUM_WORDS = (MaxTicks + 63) / 64;

    PriceLevel levels_[MaxTicks];

    uint64_t bitmap_[NUM_WORDS]{0};

    uint64_t summary_{0};

    int64_t min_price_;
    int64_t tick_size_;

    inline size_t price_to_index(uint64_t price) const {
        return static_cast<size_t>((price - min_price_) / tick_size_);
    }

    inline int64_t index_to_price(size_t idx) const {
        return min_price_ + static_cast<int64_t>(idx) * tick_size_;
    }

    inline void set_bit(size_t idx) {
        size_t word_idx = idx / 64;
        size_t bit_offset = idx % 64;
        bitmap_[word_idx] |= (1ULL << bit_offset);
        summary_ |= (1ULL << word_idx);
    }

    inline void clear_bit(size_t idx) {
        size_t word_idx = idx / 64;
        size_t bit_offset = idx % 64;
        bitmap_[word_idx] &= ~(1ULL << bit_offset);
        if (bitmap_[word_idx] == 0) {
            summary_ &= ~(1ULL << word_idx);
        }
    }

public:
    DenseBook(int64_t min_price, int64_t tick_size) : min_price_(min_price), tick_size_(tick_size) {}

    inline PriceLevel& get_level(uint64_t price) {
        return levels_[price_to_index(price)];
    }

    inline void activate_level(int64_t price) {
        set_bit(price_to_index(price));
    }

    inline void deactivate_level(int64_t price) {
        clear_bit(price_to_index(price));
    }

    inline int64_t get_best_ask() const {
        if (summary_ == 0) return -1; 

        unsigned long word_idx = __builtin_ctzll(summary_);
        
        unsigned long bit_offset = __builtin_ctzll(bitmap_[word_idx]);

        size_t idx = word_idx * 64 + bit_offset;
        return index_to_price(idx);
    }

    inline int64_t get_best_bid() const {
        if (summary_ == 0) return -1; 

        unsigned long word_idx = 63 - __builtin_clzll(summary_);
        
        unsigned long bit_offset = 63 - __builtin_clzll(bitmap_[word_idx]);

        size_t idx = word_idx * 64 + bit_offset;
        return index_to_price(idx);
    }
};