#pragma once

#include <cstdint>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static inline uint64_t mask_for_width(int w)
{
    int r = w & 63;
    if (r == 0)
        return ~uint64_t(0);
    return (uint64_t(1) << r) - 1;
}

static inline int ctz64(uint64_t x)
{
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanForward64(&idx, x);
    return (int)idx;
#else
    return __builtin_ctzll(x);
#endif
}

static inline int clz64(uint64_t x)
{
#if defined(_MSC_VER)
    unsigned long idx;
    _BitScanReverse64(&idx, x);
    return 63 - (int)idx;
#else
    return __builtin_clzll(x);
#endif
}

static inline int pop64(uint64_t x)
{
#if defined(_MSC_VER)
    return (int)__popcnt64(x);
#else
    return __builtin_popcountll(x);
#endif
}

static inline uint64_t splitmix64(uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
