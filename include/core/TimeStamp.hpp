#pragma once
#include <cstdint>
#include <x86intrin.h>

inline uint64_t rdtsc() {
    return __rdtsc();
}