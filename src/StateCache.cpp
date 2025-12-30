#include "StateCache.h"
#include <algorithm>
#include <fstream>
#include <iostream>

bool StateCache::lookup(const CanonicalState& state, CachedCycleInfo& out_info)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = cache_.find(state.hash);
    if (it == cache_.end())
    {
        misses_++;
        checkpoint_misses_++;
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
            checkpoint_hits_++;
            return true;
        }
    }
    
    // Hash collision - different state
    misses_++;
    checkpoint_misses_++;
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

void StateCache::reset_checkpoint_stats()
{
    std::lock_guard<std::mutex> lock(mutex_);
    checkpoint_hits_ = 0;
    checkpoint_misses_ = 0;
}

bool StateCache::save(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open cache file for writing: " << filename << "\n";
        return false;
    }
    
    // Write magic number and version
    uint32_t magic = 0x474F4C43;  // "GOLC" in hex
    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Write number of entries
    uint64_t num_entries = cache_.size();
    file.write(reinterpret_cast<const char*>(&num_entries), sizeof(num_entries));
    
    // Write each entry
    for (const auto& bucket_pair : hash_buckets_)
    {
        uint64_t hash = bucket_pair.first;
        const auto& states = bucket_pair.second;
        
        // Get the cycle info for this hash
        auto cache_it = cache_.find(hash);
        if (cache_it == cache_.end())
            continue;
        
        const CachedCycleInfo& info = cache_it->second;
        
        // Write all states with this hash
        for (const auto& state : states)
        {
            // Write canonical state
            file.write(reinterpret_cast<const char*>(&state.w), sizeof(state.w));
            file.write(reinterpret_cast<const char*>(&state.h), sizeof(state.h));
            file.write(reinterpret_cast<const char*>(&state.hash), sizeof(state.hash));
            
            uint64_t num_words = state.words.size();
            file.write(reinterpret_cast<const char*>(&num_words), sizeof(num_words));
            file.write(reinterpret_cast<const char*>(state.words.data()), 
                      num_words * sizeof(uint64_t));
            
            // Write cycle info
            file.write(reinterpret_cast<const char*>(&info.steps_to_cycle), sizeof(info.steps_to_cycle));
            file.write(reinterpret_cast<const char*>(&info.cycle_start_gen), sizeof(info.cycle_start_gen));
            file.write(reinterpret_cast<const char*>(&info.period), sizeof(info.period));
            file.write(reinterpret_cast<const char*>(&info.dx), sizeof(info.dx));
            file.write(reinterpret_cast<const char*>(&info.dy), sizeof(info.dy));
            file.write(reinterpret_cast<const char*>(&info.cycle_state_hash), sizeof(info.cycle_state_hash));
        }
    }
    
    return true;
}

bool StateCache::load(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        // File doesn't exist yet - not an error
        return true;
    }
    
    // Read and verify magic number
    uint32_t magic;
    uint32_t version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    if (magic != 0x474F4C43)
    {
        std::cerr << "Invalid cache file format\n";
        return false;
    }
    
    if (version != 1)
    {
        std::cerr << "Unsupported cache file version: " << version << "\n";
        return false;
    }
    
    // Read number of entries
    uint64_t num_entries;
    file.read(reinterpret_cast<char*>(&num_entries), sizeof(num_entries));
    
    // Read each entry
    for (uint64_t i = 0; i < num_entries; ++i)
    {
        CanonicalState state;
        CachedCycleInfo info;
        
        // Read canonical state
        file.read(reinterpret_cast<char*>(&state.w), sizeof(state.w));
        file.read(reinterpret_cast<char*>(&state.h), sizeof(state.h));
        file.read(reinterpret_cast<char*>(&state.hash), sizeof(state.hash));
        
        uint64_t num_words;
        file.read(reinterpret_cast<char*>(&num_words), sizeof(num_words));
        state.words.resize(num_words);
        file.read(reinterpret_cast<char*>(state.words.data()), 
                 num_words * sizeof(uint64_t));
        
        // Read cycle info
        file.read(reinterpret_cast<char*>(&info.steps_to_cycle), sizeof(info.steps_to_cycle));
        file.read(reinterpret_cast<char*>(&info.cycle_start_gen), sizeof(info.cycle_start_gen));
        file.read(reinterpret_cast<char*>(&info.period), sizeof(info.period));
        file.read(reinterpret_cast<char*>(&info.dx), sizeof(info.dx));
        file.read(reinterpret_cast<char*>(&info.dy), sizeof(info.dy));
        file.read(reinterpret_cast<char*>(&info.cycle_state_hash), sizeof(info.cycle_state_hash));
        
        if (!file.good())
        {
            std::cerr << "Error reading cache file at entry " << i << "\n";
            return false;
        }
        
        // Insert into cache
        cache_[state.hash] = info;
        
        // Add to hash bucket if not already there
        auto& bucket = hash_buckets_[state.hash];
        auto it = std::find_if(bucket.begin(), bucket.end(),
            [&state](const CanonicalState& s) {
                return canonical_equal(s, state);
            });
        
        if (it == bucket.end())
        {
            bucket.push_back(state);
        }
    }
    
    std::cout << "Loaded " << num_entries << " states from cache file\n";
    return true;
}

