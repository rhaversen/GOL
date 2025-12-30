#pragma once

#include "LifeRunner.h"
#include "StateCache.h"
#include "CanonicalState.h"
#include <vector>
#include <cstdint>

class CachedLifeAnalyzer
{
public:
    struct Result
    {
        int64_t first_repeat_step = 0;
        int64_t prefix_len = 0;
        int64_t period = 0;
        int64_t dx = 0;
        int64_t dy = 0;
        bool max_steps_reached = false;
    };
    
    CachedLifeAnalyzer(StateCache& cache) : cache_(cache) {}
    
    // Analyze a pattern, using cache when possible
    Result analyze(PlacedGrid pg, int margin = 2, int64_t max_steps = 1000);
    
private:
    StateCache& cache_;
    
    // Backfill cache with all states in a path
    void backfill_path(const std::vector<CanonicalState>& path,
                      int64_t start_gen,
                      const CachedCycleInfo& cycle_info);
};
