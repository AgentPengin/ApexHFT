#pragma once

#include <cstdint>

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