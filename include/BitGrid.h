#pragma once

#include <cstdint>
#include <vector>

class BitGrid
{
public:
    BitGrid() = default;
    BitGrid(int w, int h);

    void reset(int w, int h);
    void clear();
    void swap(BitGrid &other) noexcept;

    int width() const { return W_; }
    int height() const { return H_; }
    int words_per_row() const { return stride_; }
    uint64_t last_word_mask() const { return last_mask_; }

    uint64_t *row_ptr(int y);
    const uint64_t *row_ptr(int y) const;

    void set(int x, int y);
    bool get(int x, int y) const;

    void mask_tail_bits();
    void blit_from(const BitGrid &src, int dx, int dy);

private:
    int W_ = 0, H_ = 0;
    int stride_ = 0;
    uint64_t last_mask_ = 0;
    std::vector<uint64_t> bits_;
};
