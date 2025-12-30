#pragma once

#include <cstdint>
#include <vector>

struct CanonicalState
{
    int w = 0;
    int h = 0;
    std::vector<uint64_t> words;
    uint64_t hash = 0;
};

bool canonical_equal(const CanonicalState &a, const CanonicalState &b);
uint64_t canonical_hash(int w, int h, const std::vector<uint64_t> &words);
