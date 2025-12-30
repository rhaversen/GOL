#include "StateCache.h"
#include <algorithm>

bool StateCache::lookup(const CanonicalState& state, CachedCycleInfo& out_info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(state.hash);
    if (it == cache_.end())
    {
        misses_++;
        return false;
    }
    
    // Hash found - verify actual state equality (collision check)
    auto bucket_it = hash_buckets_.find(state.hash);
    if (bucket_it != hash_buckets_.end())
    {
        // Check if this exact state is in the bucket
        const auto& bucket = bucket_it->second;
        auto state_it = std::find_if(bucket.begin(), bucket.end(),
            [&state](const CanonicalState& s) {
                return canonical_equal(s, state);
            });
        
        if (state_it != bucket.end())
        {
            // Found exact match
            out_info = it->second;
            hits_++;
            return true;
        }
    }
    
    // Hash collision - different state
    misses_++;
    return false;
}

void StateCache::insert(const CanonicalState& state, const CachedCycleInfo& info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Insert or update cache entry
    cache_[state.hash] = info;
    
    // Store full state for collision resolution
    auto& bucket = hash_buckets_[state.hash];
    
    // Check if state already exists in bucket
    auto it = std::find_if(bucket.begin(), bucket.end(),
        [&state](const CanonicalState& s) {
            return canonical_equal(s, state);
        });
    
    if (it == bucket.end())
    {
        // New state - add to bucket
        bucket.push_back(state);
    }
    // If state already exists, no need to add again
}

size_t StateCache::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.size();
}
