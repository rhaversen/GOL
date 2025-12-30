#include "CachedLifeAnalyzer.h"
#include <unordered_map>

CachedLifeAnalyzer::Result CachedLifeAnalyzer::analyze(PlacedGrid pg, int margin, int64_t max_steps)
{
    LifeRunner runner;
    runner.init(std::move(pg));
    
    int64_t current_gen = 0;
    std::vector<CanonicalState> path;
    path.reserve(max_steps / 10);  // Reasonable reserve to avoid reallocations
    
    while (current_gen < max_steps)
    {
        // Get canonical state (translation-invariant)
        CanonicalState canonical_state = runner.canonical_translation_invariant();
        
        // Check cache
        CachedCycleInfo cached_info;
        if (cache_.lookup(canonical_state, cached_info))
        {
            // Cache hit!
            Result result;
            result.prefix_len = current_gen + cached_info.steps_to_cycle;
            result.period = cached_info.period;
            result.first_repeat_step = result.prefix_len + result.period;
            result.dx = cached_info.dx;
            result.dy = cached_info.dy;
            result.max_steps_reached = false;
            
            // Backfill all states we visited on this path
            backfill_path(path, current_gen, cached_info);
            
            return result;
        }
        
        // No cache hit - add to path and continue
        path.push_back(canonical_state);
        
        // Step forward
        runner.step(margin);
        current_gen++;
        
        // Check for cycle in current path using Floyd's algorithm
        // We'll use the runner's built-in cycle detection
        // But first check if we've seen this state before in our current path
        CanonicalState new_state = runner.canonical_translation_invariant();
        
        for (size_t i = 0; i < path.size(); ++i)
        {
            if (canonical_equal(path[i], new_state))
            {
                // Found a cycle!
                int64_t cycle_start_gen = i;
                int64_t period = current_gen - cycle_start_gen;
                
                // Compute translation
                auto stats_cycle_start = runner.stats();
                
                // Need to get position at cycle start
                // We'll need to track positions - let's use the runner's approach
                LifeRunner temp_runner;
                temp_runner.init(runner.grid());
                temp_runner.run(period, margin);
                auto stats_after_period = temp_runner.stats();
                
                int64_t dx = stats_after_period.offset_x - stats_cycle_start.offset_x;
                int64_t dy = stats_after_period.offset_y - stats_cycle_start.offset_y;
                
                Result result;
                result.prefix_len = cycle_start_gen;
                result.period = period;
                result.first_repeat_step = cycle_start_gen + period;
                result.dx = dx;
                result.dy = dy;
                result.max_steps_reached = false;
                
                // Cache all states in the path
                CachedCycleInfo cycle_info;
                cycle_info.cycle_start_gen = cycle_start_gen;
                cycle_info.period = period;
                cycle_info.dx = dx;
                cycle_info.dy = dy;
                cycle_info.cycle_state_hash = path[cycle_start_gen].hash;
                
                // Backfill from current position back to start
                for (int64_t i = 0; i < (int64_t)path.size(); ++i)
                {
                    CachedCycleInfo info_for_state;
                    info_for_state.steps_to_cycle = cycle_start_gen - i;
                    info_for_state.cycle_start_gen = cycle_start_gen;
                    info_for_state.period = period;
                    info_for_state.dx = dx;
                    info_for_state.dy = dy;
                    info_for_state.cycle_state_hash = path[cycle_start_gen].hash;
                    
                    cache_.insert(path[i], info_for_state);
                }
                
                // Also cache the states in the cycle itself
                for (int64_t i = cycle_start_gen; i < current_gen; ++i)
                {
                    CachedCycleInfo info_for_state;
                    info_for_state.steps_to_cycle = 0;  // Already in cycle
                    info_for_state.cycle_start_gen = cycle_start_gen;
                    info_for_state.period = period;
                    info_for_state.dx = dx;
                    info_for_state.dy = dy;
                    info_for_state.cycle_state_hash = path[cycle_start_gen].hash;
                    
                    if (i < (int64_t)path.size())
                    {
                        cache_.insert(path[i], info_for_state);
                    }
                }
                
                return result;
            }
        }
    }
    
    // Max steps reached without finding cycle
    Result result;
    result.first_repeat_step = -1;
    result.max_steps_reached = true;
    return result;
}

void CachedLifeAnalyzer::backfill_path(const std::vector<CanonicalState>& path,
                                       int64_t start_gen,
                                       const CachedCycleInfo& cycle_info)
{
    // Fill cache for all states in the path
    for (size_t i = 0; i < path.size(); ++i)
    {
        CachedCycleInfo info_for_state;
        info_for_state.steps_to_cycle = cycle_info.steps_to_cycle + (path.size() - i);
        info_for_state.cycle_start_gen = cycle_info.cycle_start_gen;
        info_for_state.period = cycle_info.period;
        info_for_state.dx = cycle_info.dx;
        info_for_state.dy = cycle_info.dy;
        info_for_state.cycle_state_hash = cycle_info.cycle_state_hash;
        
        cache_.insert(path[i], info_for_state);
    }
}
