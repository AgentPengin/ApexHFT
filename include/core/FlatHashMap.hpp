#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <algorithm>

template <size_t Capacity = 131'072> // 128K slots = 2MB (Nằm trọn trong L3 Cache!)
class FlatHashMap {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two.");
    static constexpr uint64_t EMPTY_KEY = 0;

    struct Entry {
        uint64_t key{EMPTY_KEY};
        uint32_t val{0};
    };

private:
    std::unique_ptr<Entry[]> table_;
    size_t size_{0};

    static inline size_t hash_fn(uint64_t key) {
        return static_cast<size_t>((key * 11400714819323198485ULL) >> 47); // 64 - 17 = 47
    }

public:
    FlatHashMap() : table_(std::make_unique<Entry[]>(Capacity)) {
        clear();
    }

    void clear() {
        for (size_t i = 0; i < Capacity; ++i) table_[i].key = EMPTY_KEY;
        size_ = 0;
    }

    inline void put(uint64_t key, uint32_t val) {
        if (key == EMPTY_KEY) return;
        size_t idx = hash_fn(key) & (Capacity - 1);
        while (table_[idx].key != EMPTY_KEY && table_[idx].key != key) {
            idx = (idx + 1) & (Capacity - 1);
        }
        if (table_[idx].key == EMPTY_KEY) ++size_;
        table_[idx].key = key;
        table_[idx].val = val;
    }

    inline bool find(uint64_t key, uint32_t& out_val) const {
        if (key == EMPTY_KEY) return false;
        size_t idx = hash_fn(key) & (Capacity - 1);
        while (table_[idx].key != EMPTY_KEY) {
            if (table_[idx].key == key) {
                out_val = table_[idx].val;
                return true;
            }
            idx = (idx + 1) & (Capacity - 1);
        }
        return false;
    }

    inline bool erase(uint64_t key, uint32_t& out_val) {
        if (key == EMPTY_KEY) return false;
        size_t i = hash_fn(key) & (Capacity - 1);
        while (table_[i].key != EMPTY_KEY) {
            if (table_[i].key == key) {
                out_val = table_[i].val;
                size_t j = i;
                while (true) {
                    j = (j + 1) & (Capacity - 1);
                    if (table_[j].key == EMPTY_KEY) break;
                    size_t k = hash_fn(table_[j].key) & (Capacity - 1);
                    if ((i <= j) ? (k <= i || k > j) : (k <= i && k > j)) {
                        table_[i] = table_[j];
                        i = j;
                    }
                }
                table_[i].key = EMPTY_KEY;
                --size_;
                return true;
            }
            i = (i + 1) & (Capacity - 1);
        }
        return false;
    }

    inline bool erase(uint64_t key) {
        uint32_t dummy = 0;
        return erase(key, dummy);
    }

    size_t size() const { return size_; }
};