#include "SpiralGenerator.h"
#include "BitUtils.h"
#include "Bounds.h"
#include <algorithm>
#include <utility>
#include <vector>

PlacedGrid spiral_binary_bitmap(uint64_t index, int bits, int pad)
{
    bits = std::max(0, std::min(bits, 64));
    pad = std::max(0, pad);

    uint64_t used = (bits == 64) ? index : (index & ((uint64_t(1) << bits) - 1));

    std::vector<std::pair<int, int>> live;
    live.reserve(pop64(used));

    Bounds b;

    auto maybe_add = [&](int k, int x, int y)
    {
        if (((used >> k) & 1ULL) == 0)
            return;
        live.emplace_back(x, y);
        b.include(x, y);
    };

    int x = 0, y = 0;
    int dx = 1, dy = 0;
    int seg_len = 1;
    int seg_pass = 0;
    int steps = 0;

    if (bits > 0)
        maybe_add(0, 0, 0);

    for (int k = 1; k < bits; ++k)
    {
        x += dx;
        y += dy;
        maybe_add(k, x, y);

        if (++steps == seg_len)
        {
            steps = 0;
            int ndx = -dy;
            int ndy = dx;
            dx = ndx;
            dy = ndy;
            if (++seg_pass == 2)
            {
                seg_pass = 0;
                ++seg_len;
            }
        }
    }

    if (b.empty)
    {
        BitGrid g(std::max(1, 1 + 2 * pad), std::max(1, 1 + 2 * pad));
        return {std::move(g), int64_t(-pad), int64_t(-pad)};
    }

    int64_t minx = b.minx, maxx = b.maxx;
    int64_t miny = b.miny, maxy = b.maxy;

    int W = int((maxx - minx + 1) + 2 * pad);
    int H = int((maxy - miny + 1) + 2 * pad);

    BitGrid g(W, H);

    int ox = int(int64_t(pad) - minx);
    int oy = int(int64_t(pad) - miny);

    for (const auto& cell : live)
        g.set(cell.first + ox, cell.second + oy);
    g.mask_tail_bits();

    int64_t offx = minx - pad;
    int64_t offy = miny - pad;
    return {std::move(g), offx, offy};
}

SpiralBinaryGenerator::SpiralBinaryGenerator(int bits, int pad)
{
    bits_ = std::max(0, std::min(bits, 64));
    pad_ = std::max(0, pad);
    idx_ = 0;
}

void SpiralBinaryGenerator::set_index(uint64_t i)
{
    if (bits_ == 64)
        idx_ = i;
    else
        idx_ = i & ((uint64_t(1) << bits_) - 1);
}

PlacedGrid SpiralBinaryGenerator::from_index(uint64_t i) const
{
    return spiral_binary_bitmap(i, bits_, pad_);
}

PlacedGrid SpiralBinaryGenerator::next()
{
    PlacedGrid pg = from_index(idx_);
    advance();
    return pg;
}

void SpiralBinaryGenerator::advance()
{
    if (bits_ == 64)
    {
        idx_ += 1;
    }
    else
    {
        uint64_t mask = (uint64_t(1) << bits_) - 1;
        idx_ = (idx_ + 1) & mask;
    }
}
