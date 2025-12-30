#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

struct Bounds
{
    int64_t minx = std::numeric_limits<int64_t>::max();
    int64_t miny = std::numeric_limits<int64_t>::max();
    int64_t maxx = std::numeric_limits<int64_t>::min();
    int64_t maxy = std::numeric_limits<int64_t>::min();
    bool empty = true;

    void include(int64_t x, int64_t y)
    {
        empty = false;
        minx = std::min(minx, x);
        miny = std::min(miny, y);
        maxx = std::max(maxx, x);
        maxy = std::max(maxy, y);
    }

    void include_span(int64_t x0, int64_t x1, int64_t y)
    {
        empty = false;
        minx = std::min(minx, x0);
        maxx = std::max(maxx, x1);
        miny = std::min(miny, y);
        maxy = std::max(maxy, y);
    }
};
