#include "LifeRunner.h"
#include "BitUtils.h"
#include <algorithm>
#include <utility>
#include <iostream>

void LifeRunner::init(PlacedGrid pg)
{
    cur_ = std::move(pg.grid);
    nxt_.reset(cur_.width(), cur_.height());
    nxt_.clear();
    offx_ = pg.offset_x;
    offy_ = pg.offset_y;
    gen_ = 0;
    recompute();
}

void LifeRunner::step(int margin)
{
    ensure_margin(margin);
    step_once();
}

void LifeRunner::run(int64_t gens, int margin)
{
    for (int64_t i = 0; i < gens; ++i)
    {
        step(margin);
        if (pop_ == 0)
            break;
    }
}

LifeRunner::LoopInfo LifeRunner::run_until_repeat(int margin, int64_t max_steps)
{
    if (pop_ == 0)
        return {gen_, 0, 1, 0, 0};
    LoopInfo info = find_cycle_floyd(margin, max_steps);
    return info;
}

LifeRunner::Stats LifeRunner::stats() const
{
    Stats s;
    s.gen = gen_;
    s.pop = pop_;
    s.bounds_grid = bounds_;
    s.offset_x = offx_;
    s.offset_y = offy_;
    return s;
}

CanonicalState LifeRunner::canonical_translation_invariant() const
{
    CanonicalState cs;
    if (bounds_.empty)
    {
        cs.w = 0;
        cs.h = 0;
        cs.words.clear();
        cs.hash = canonical_hash(cs.w, cs.h, cs.words);
        return cs;
    }

    int x0 = (int)bounds_.minx;
    int x1 = (int)bounds_.maxx;
    int y0 = (int)bounds_.miny;
    int y1 = (int)bounds_.maxy;

    cs.w = x1 - x0 + 1;
    cs.h = y1 - y0 + 1;

    int out_stride = (cs.w + 63) / 64;
    uint64_t out_last_mask = mask_for_width(cs.w);
    cs.words.assign((size_t)out_stride * (size_t)cs.h, 0);

    const int in_stride = cur_.words_per_row();
    const int base = x0 >> 6;
    const int shift = x0 & 63;

    for (int yy = 0; yy < cs.h; ++yy)
    {
        const uint64_t *src = cur_.row_ptr(y0 + yy);
        uint64_t *dst = cs.words.data() + (size_t)yy * (size_t)out_stride;

        for (int j = 0; j < out_stride; ++j)
        {
            int si = base + j;
            uint64_t a = (0 <= si && si < in_stride) ? src[si] : 0;
            uint64_t b = (0 <= si + 1 && si + 1 < in_stride) ? src[si + 1] : 0;
            uint64_t v;
            if (shift == 0)
                v = a;
            else
                v = (a >> shift) | (b << (64 - shift));
            dst[j] = v;
        }
        dst[out_stride - 1] &= out_last_mask;
    }

    cs.hash = canonical_hash(cs.w, cs.h, cs.words);
    return cs;
}

std::pair<int64_t, int64_t> LifeRunner::anchor_logical_min() const
{
    if (bounds_.empty)
        return {0, 0};
    return {offx_ + bounds_.minx, offy_ + bounds_.miny};
}

uint64_t LifeRunner::shift_left1(const uint64_t *row, int i)
{
    uint64_t v = row[i] << 1;
    if (i > 0)
        v |= (row[i - 1] >> 63);
    return v;
}

uint64_t LifeRunner::shift_right1(const uint64_t *row, int i, int stride)
{
    uint64_t v = row[i] >> 1;
    if (i + 1 < stride)
        v |= (row[i + 1] << 63);
    return v;
}

void LifeRunner::add_mask(uint64_t m, uint64_t &ones, uint64_t &twos, uint64_t &fours, uint64_t &eights)
{
    uint64_t c1 = ones & m;
    ones ^= m;
    uint64_t c2 = twos & c1;
    twos ^= c1;
    uint64_t c3 = fours & c2;
    fours ^= c2;
    eights ^= c3;
}

void LifeRunner::recompute()
{
    Bounds b;
    uint64_t pop = 0;
    const int H = cur_.height();
    const int S = cur_.words_per_row();
    const int last = S - 1;
    const uint64_t last_mask = cur_.last_word_mask();

    for (int y = 0; y < H; ++y)
    {
        const uint64_t *r = cur_.row_ptr(y);
        for (int i = 0; i < S; ++i)
        {
            uint64_t w = r[i];
            if (i == last)
                w &= last_mask;
            if (!w)
                continue;
            pop += (uint64_t)pop64(w);
            int64_t x0 = int64_t(i) * 64 + ctz64(w);
            int64_t x1 = int64_t(i) * 64 + (63 - clz64(w));
            b.include_span(x0, x1, y);
        }
    }

    pop_ = pop;
    bounds_ = b;
}

void LifeRunner::step_once()
{
    const int H = cur_.height();
    const int S = cur_.words_per_row();
    const int last = S - 1;
    const uint64_t last_mask = cur_.last_word_mask();

    nxt_.clear();

    Bounds b;
    uint64_t pop = 0;

    for (int y = 0; y < H; ++y)
    {
        const uint64_t *up = (y > 0) ? cur_.row_ptr(y - 1) : nullptr;
        const uint64_t *mid = cur_.row_ptr(y);
        const uint64_t *dn = (y + 1 < H) ? cur_.row_ptr(y + 1) : nullptr;
        uint64_t *out = nxt_.row_ptr(y);

        for (int i = 0; i < S; ++i)
        {
            const uint64_t maskW = (i == last) ? last_mask : ~uint64_t(0);

            const uint64_t um = up ? (up[i] & maskW) : 0;
            const uint64_t mm = (mid[i] & maskW);
            const uint64_t dm = dn ? (dn[i] & maskW) : 0;

            const uint64_t ul = up ? shift_left1(up, i) : 0;
            const uint64_t ur = up ? shift_right1(up, i, S) : 0;
            const uint64_t ml = shift_left1(mid, i);
            const uint64_t mr = shift_right1(mid, i, S);
            const uint64_t dl = dn ? shift_left1(dn, i) : 0;
            const uint64_t dr = dn ? shift_right1(dn, i, S) : 0;

            uint64_t ones = 0, twos = 0, fours = 0, eights = 0;
            add_mask(ul, ones, twos, fours, eights);
            add_mask(um, ones, twos, fours, eights);
            add_mask(ur, ones, twos, fours, eights);
            add_mask(ml, ones, twos, fours, eights);
            add_mask(mr, ones, twos, fours, eights);
            add_mask(dl, ones, twos, fours, eights);
            add_mask(dm, ones, twos, fours, eights);
            add_mask(dr, ones, twos, fours, eights);

            uint64_t eq2 = (~ones) & twos & (~fours) & (~eights);
            uint64_t eq3 = ones & twos & (~fours) & (~eights);

            uint64_t nextw = (eq3 | (mm & eq2)) & maskW;
            out[i] = nextw;

            if (nextw)
            {
                pop += (uint64_t)pop64(nextw);
                int64_t x0 = int64_t(i) * 64 + ctz64(nextw);
                int64_t x1 = int64_t(i) * 64 + (63 - clz64(nextw));
                b.include_span(x0, x1, y);
            }
        }
    }

    cur_.swap(nxt_);
    ++gen_;
    pop_ = pop;
    bounds_ = b;
}

void LifeRunner::ensure_margin(int margin)
{
    if (bounds_.empty)
        return;

    const int W = cur_.width();
    const int H = cur_.height();

    int needL = std::max(0, int(int64_t(margin) - bounds_.minx));
    int needT = std::max(0, int(int64_t(margin) - bounds_.miny));
    int needR = std::max(0, int((bounds_.maxx + margin) - (int64_t(W) - 1)));
    int needB = std::max(0, int((bounds_.maxy + margin) - (int64_t(H) - 1)));

    if (needL == 0 && needR == 0 && needT == 0 && needB == 0)
        return;

    const int extra = 32;
    if (needL)
        needL += extra;
    if (needR)
        needR += extra;
    if (needT)
        needT += extra;
    if (needB)
        needB += extra;

    grow(needL, needR, needT, needB);
}

void LifeRunner::grow(int addL, int addR, int addT, int addB)
{
    const int newW = cur_.width() + addL + addR;
    const int newH = cur_.height() + addT + addB;

    BitGrid newCur(newW, newH);
    BitGrid newNxt(newW, newH);
    newCur.clear();
    newNxt.clear();

    newCur.blit_from(cur_, addL, addT);

    cur_ = std::move(newCur);
    nxt_ = std::move(newNxt);

    offx_ -= addL;
    offy_ -= addT;

    bounds_.minx += addL;
    bounds_.maxx += addL;
    bounds_.miny += addT;
    bounds_.maxy += addT;
}

LifeRunner::LoopInfo LifeRunner::find_cycle_floyd(int margin, int64_t max_steps) const
{
    LifeRunner tort = *this;
    LifeRunner hare = *this;

    tort.step(margin);
    hare.step(margin);
    hare.step(margin);

    int64_t steps = 0;
    while (steps < max_steps)
    {
        // Diagnostic logging disabled for performance
        // if (steps % 1000 == 0)
        // {
        //     std::cout << " [phase1:" << steps << "]" << std::flush;
        // }
        CanonicalState ct = tort.canonical_translation_invariant();
        CanonicalState ch = hare.canonical_translation_invariant();
        if (ct.hash == ch.hash && canonical_equal(ct, ch))
            break;
        tort.step(margin);
        hare.step(margin);
        hare.step(margin);
        ++steps;
    }

    if (steps >= max_steps)
    {
        // Timeout - return sentinel values
        return {-1, -1, -1, 0, 0};
    }

    int64_t mu = 0;
    LifeRunner tort2 = *this;
    while (mu < max_steps)
    {
        // Diagnostic logging disabled for performance
        // if (mu % 1000 == 0)
        // {
        //     std::cout << " [phase2:" << mu << "]" << std::flush;
        // }
        CanonicalState c1 = tort2.canonical_translation_invariant();
        CanonicalState c2 = tort.canonical_translation_invariant();
        if (c1.hash == c2.hash && canonical_equal(c1, c2))
            break;
        tort2.step(margin);
        tort.step(margin);
        ++mu;
    }

    if (mu >= max_steps)
    {
        // Timeout - return sentinel values
        return {-1, -1, -1, 0, 0};
    }

    int64_t lambda = 1;
    LifeRunner hare2 = tort;
    hare2.step(margin);
    while (lambda < max_steps)
    {
        // Diagnostic logging disabled for performance
        // if (lambda % 1000 == 0)
        // {
        //     std::cout << " [phase3:" << lambda << "]" << std::flush;
        // }
        CanonicalState c1 = tort.canonical_translation_invariant();
        CanonicalState c2 = hare2.canonical_translation_invariant();
        if (c1.hash == c2.hash && canonical_equal(c1, c2))
            break;
        hare2.step(margin);
        ++lambda;
    }

    if (lambda >= max_steps)
    {
        // Timeout - return sentinel values
        return {-1, -1, -1, 0, 0};
    }

    int64_t first_repeat = mu + lambda;

    LifeRunner base = *this;
    for (int64_t i = 0; i < mu; ++i)
        base.step(margin);
    auto a0 = base.anchor_logical_min();

    LifeRunner moved = base;
    for (int64_t i = 0; i < lambda; ++i)
        moved.step(margin);
    auto a1 = moved.anchor_logical_min();

    int64_t dx = a1.first - a0.first;
    int64_t dy = a1.second - a0.second;

    return {first_repeat, mu, lambda, dx, dy};
}
