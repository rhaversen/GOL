#include "CachedLifeAnalyzer.h"
#include <unordered_map>
#include <iostream>
#include <iomanip>

CachedLifeAnalyzer::Result CachedLifeAnalyzer::analyze(PlacedGrid pg, int margin, int64_t max_steps)
{
    LifeRunner runner;
    runner.init(std::move(pg));
    
    // Special case: empty pattern (population 0)
    // Floyd's algorithm returns {gen, 0, 1, 0, 0} for empty patterns
    if (runner.stats().pop == 0)
    {
        Result result;
        result.first_repeat_step = runner.stats().gen;  // Current generation
        result.prefix_len = 0;
        result.period = 1;
        result.dx = 0;
        result.dy = 0;
        result.max_steps_reached = false;
        return result;
    }
    
    int64_t current_gen = 0;
    std::vector<CanonicalState> path;
    path.reserve(max_steps / 10);
    
    while (current_gen < max_steps)
    {
        // Get canonical state (translation-invariant)
        CanonicalState canonical_state = runner.canonical_translation_invariant();
        
        // Check cache FIRST
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
            
            // Backfill all states we visited on this path (not including current)
            backfill_path(path, current_gen, cached_info);
            
            // Cache the current state separately
            cache_.insert(canonical_state, cached_info);
            
            return result;
        }
        
        // Check if we've seen this state in our current path (cycle detection)
        // Must check BEFORE adding to path to detect cycles properly
        for (size_t i = 0; i < path.size(); ++i)
        {
            if (canonical_equal(path[i], canonical_state))
            {
                // Found a cycle!
                int64_t cycle_start_gen = i;
                int64_t period = current_gen - cycle_start_gen;
                
                // Compute translation using anchor_logical_min (like Floyd's algorithm)
                auto anchor_before = runner.anchor_logical_min();
                
                // Create a PlacedGrid from current state
                PlacedGrid temp_pg;
                temp_pg.grid = runner.grid();
                auto stats = runner.stats();
                temp_pg.offset_x = stats.offset_x;
                temp_pg.offset_y = stats.offset_y;
                
                LifeRunner temp_runner;
                temp_runner.init(std::move(temp_pg));
                temp_runner.run(period, margin);
                auto anchor_after = temp_runner.anchor_logical_min();
                
                int64_t dx = anchor_after.first - anchor_before.first;
                int64_t dy = anchor_after.second - anchor_before.second;
                
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
                for (int64_t j = 0; j < (int64_t)path.size(); ++j)
                {
                    CachedCycleInfo info_for_state;
                    info_for_state.steps_to_cycle = (j < cycle_start_gen) ? (cycle_start_gen - j) : 0;
                    info_for_state.cycle_start_gen = cycle_start_gen;
                    info_for_state.period = period;
                    info_for_state.dx = dx;
                    info_for_state.dy = dy;
                    info_for_state.cycle_state_hash = path[cycle_start_gen].hash;
                    
                    cache_.insert(path[j], info_for_state);
                }
                
                // Also cache the current state (which completes the cycle)
                CachedCycleInfo info_for_current;
                info_for_current.steps_to_cycle = 0;  // Already in cycle
                info_for_current.cycle_start_gen = cycle_start_gen;
                info_for_current.period = period;
                info_for_current.dx = dx;
                info_for_current.dy = dy;
                info_for_current.cycle_state_hash = path[cycle_start_gen].hash;
                cache_.insert(canonical_state, info_for_current);
                
                return result;
            }
        }
        
        // No cache hit and no cycle yet - add to path and continue
        path.push_back(canonical_state);
        
        // Step forward
        runner.step(margin);
        current_gen++;
    }
    
    // Max steps reached without finding cycle
    Result result;
    result.first_repeat_step = -1;
    result.max_steps_reached = true;
    return result;
}

void CachedLifeAnalyzer::backfill_path(const std::vector<CanonicalState>& path,
                                       int64_t current_gen,
                                       const CachedCycleInfo& cycle_info)
{
    // Fill cache for all states in the path
    // path[i] is the state at generation i (path does NOT include current_gen)
    for (size_t i = 0; i < path.size(); ++i)
    {
        CachedCycleInfo info_for_state;
        // Steps from generation i to current_gen, then to cycle
        info_for_state.steps_to_cycle = (current_gen - (int64_t)i) + cycle_info.steps_to_cycle;
        info_for_state.cycle_start_gen = cycle_info.cycle_start_gen;
        info_for_state.period = cycle_info.period;
        info_for_state.dx = cycle_info.dx;
        info_for_state.dy = cycle_info.dy;
        info_for_state.cycle_state_hash = cycle_info.cycle_state_hash;
        
        cache_.insert(path[i], info_for_state);
    }
}
