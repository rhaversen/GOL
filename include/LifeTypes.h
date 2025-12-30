#pragma once

#include "BitGrid.h"
#include <cstdint>

struct PlacedGrid
{
    BitGrid grid;
    int64_t offset_x = 0;
    int64_t offset_y = 0;
};
