#include "CanonicalState.h"
#include "BitUtils.h"

bool canonical_equal(const CanonicalState &a, const CanonicalState &b)
{
    return a.w == b.w && a.h == b.h && a.words == b.words;
}

uint64_t canonical_hash(int w, int h, const std::vector<uint64_t> &words)
{
    uint64_t h0 = 0;
    h0 = splitmix64(h0 ^ (uint64_t)w);
    h0 = splitmix64(h0 ^ (uint64_t)h);
    for (uint64_t v : words)
        h0 = splitmix64(h0 ^ v);
    return h0;
}
