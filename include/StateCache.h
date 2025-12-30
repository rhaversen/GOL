#pragma once

#include "CanonicalState.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <mutex>

struct CachedCycleInfo
{
    int64_t steps_to_cycle;    // How many steps from this state to enter cycle
    int64_t cycle_start_gen;   // Absolute generation where cycle starts
    int64_t period;            // Cycle period
    int64_t dx;                // Translation per cycle
    int64_t dy;
    uint64_t cycle_state_hash; // Hash of the state where cycle starts
};

class StateCache
{
public:
    StateCache() = default;
    
    // Lookup cache entry for a given state
    // Returns true if found, false otherwise
    bool lookup(const CanonicalState& state, CachedCycleInfo& out_info);
    
    // Insert a cache entry
    void insert(const CanonicalState& state, const CachedCycleInfo& info);
    
    // Get statistics
    size_t size() const;
    uint64_t hits() const { return hits_; }
    uint64_t misses() const { return misses_; }
    
    // Thread-safe operations
    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }
    
private:
    // Main cache: hash -> cycle info
    std::unordered_map<uint64_t, CachedCycleInfo> cache_;
    
    // Collision resolution: hash -> list of full states
    std::unordered_map<uint64_t, std::vector<CanonicalState>> hash_buckets_;
    
    // Statistics
    uint64_t hits_ = 0;
    uint64_t misses_ = 0;
    
    // Thread safety
    mutable std::mutex mutex_;
};
