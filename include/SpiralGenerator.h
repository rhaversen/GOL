#pragma once

#include "LifeTypes.h"
#include <cstdint>

PlacedGrid spiral_binary_bitmap(uint64_t index, int bits = 64, int pad = 2);

class SpiralBinaryGenerator
{
public:
    explicit SpiralBinaryGenerator(int bits = 64, int pad = 2);

    int bits() const { return bits_; }
    int pad() const { return pad_; }

    uint64_t index() const { return idx_; }
    void set_index(uint64_t i);

    PlacedGrid from_index(uint64_t i) const;
    PlacedGrid next();
    void advance();

private:
    int bits_ = 64;
    int pad_ = 2;
    uint64_t idx_ = 0;
};
