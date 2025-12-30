#pragma once

#include "BitGrid.h"
#include "Bounds.h"
#include "CanonicalState.h"
#include "LifeTypes.h"
#include <cstdint>

class LifeRunner
{
public:
    struct Stats
    {
        int64_t gen = 0;
        uint64_t pop = 0;
        Bounds bounds_grid;
        int64_t offset_x = 0;
        int64_t offset_y = 0;
    };

    struct LoopInfo
    {
        int64_t first_repeat_step = 0;
        int64_t prefix_len = 0;
        int64_t period = 0;
        int64_t dx = 0;
        int64_t dy = 0;
    };

    void init(PlacedGrid pg);
    void step(int margin = 2);
    void run(int64_t gens, int margin = 2);
    LoopInfo run_until_repeat(int margin = 2, int64_t max_steps = 1000);

    const BitGrid &grid() const { return cur_; }
    Stats stats() const;
    CanonicalState canonical_translation_invariant() const;

private:
    BitGrid cur_;
    BitGrid nxt_;
    int64_t offx_ = 0;
    int64_t offy_ = 0;
    int64_t gen_ = 0;
    uint64_t pop_ = 0;
    Bounds bounds_;

    std::pair<int64_t, int64_t> anchor_logical_min() const;
    void recompute();
    void step_once();
    void ensure_margin(int margin);
    void grow(int addL, int addR, int addT, int addB);
    LoopInfo find_cycle_floyd(int margin, int64_t max_steps) const;

    static inline uint64_t shift_left1(const uint64_t *row, int i);
    static inline uint64_t shift_right1(const uint64_t *row, int i, int stride);
    static inline void add_mask(uint64_t m, uint64_t &ones, uint64_t &twos, uint64_t &fours, uint64_t &eights);
};
