#include "BitGrid.h"
#include "BitUtils.h"
#include <algorithm>

BitGrid::BitGrid(int w, int h)
{
    reset(w, h);
}

void BitGrid::reset(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        W_ = H_ = stride_ = 0;
        last_mask_ = 0;
        bits_.clear();
        return;
    }
    W_ = w;
    H_ = h;
    stride_ = (W_ + 63) / 64;
    last_mask_ = mask_for_width(W_);
    bits_.assign((size_t)stride_ * (size_t)H_, 0);
}

void BitGrid::clear()
{
    std::fill(bits_.begin(), bits_.end(), 0);
}

void BitGrid::swap(BitGrid &other) noexcept
{
    std::swap(W_, other.W_);
    std::swap(H_, other.H_);
    std::swap(stride_, other.stride_);
    std::swap(last_mask_, other.last_mask_);
    bits_.swap(other.bits_);
}

uint64_t *BitGrid::row_ptr(int y)
{
    return bits_.data() + (size_t)y * (size_t)stride_;
}

const uint64_t *BitGrid::row_ptr(int y) const
{
    return bits_.data() + (size_t)y * (size_t)stride_;
}

void BitGrid::set(int x, int y)
{
    bits_[(size_t)y * (size_t)stride_ + (size_t)(x >> 6)] |= (uint64_t(1) << (x & 63));
}

bool BitGrid::get(int x, int y) const
{
    return (bits_[(size_t)y * (size_t)stride_ + (size_t)(x >> 6)] >> (x & 63)) & 1;
}

void BitGrid::mask_tail_bits()
{
    if (stride_ == 0)
        return;
    if (last_mask_ == ~uint64_t(0))
        return;
    for (int y = 0; y < H_; ++y)
        row_ptr(y)[stride_ - 1] &= last_mask_;
}

void BitGrid::blit_from(const BitGrid &src, int dx, int dy)
{
    if (src.W_ == 0 || src.H_ == 0)
        return;
    if (dx < 0 || dy < 0)
        return;

    const int word_shift = dx >> 6;
    const int bit_shift = dx & 63;

    for (int y = 0; y < src.H_; ++y)
    {
        const uint64_t *s = src.row_ptr(y);
        uint64_t *d = row_ptr(y + dy);

        if (bit_shift == 0)
        {
            for (int i = 0; i < src.stride_; ++i)
            {
                int di = i + word_shift;
                if (0 <= di && di < stride_)
                    d[di] |= s[i];
            }
        }
        else
        {
            const int inv = 64 - bit_shift;
            for (int i = 0; i < src.stride_; ++i)
            {
                uint64_t v = s[i];
                int di = i + word_shift;
                if (0 <= di && di < stride_)
                    d[di] |= (v << bit_shift);
                if (0 <= di + 1 && di + 1 < stride_)
                    d[di + 1] |= (v >> inv);
            }
        }
    }

    mask_tail_bits();
}
