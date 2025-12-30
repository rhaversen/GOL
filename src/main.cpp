#include "LifeRunner.h"
#include "CachedLifeAnalyzer.h"
#include "StateCache.h"
#include "SpiralGenerator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>
#include <chrono>
#include <iomanip>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <set>
#include <algorithm>

// Global state cache shared across all threads
StateCache g_state_cache;

struct AnalysisState
{
    uint64_t highest_idx = 0;
    std::set<uint64_t> processed;
    std::vector<uint64_t> missing;
};

AnalysisState scan_existing_file()
{
    AnalysisState state;
    std::ifstream infile("data/pattern_analysis.txt");
    
    if (!infile.is_open())
        return state;  // File doesn't exist
    
    std::string line;
    std::regex pattern("spiral 0x([0-9a-f]+):");
    
    while (std::getline(infile, line))
    {
        std::smatch match;
        if (std::regex_search(line, match, pattern))
        {
            uint64_t idx = std::stoull(match[1].str(), nullptr, 16);
            state.processed.insert(idx);
            if (idx > state.highest_idx)
                state.highest_idx = idx;
        }
    }
    
    // Find missing patterns below highest
    if (!state.processed.empty())
    {
        for (uint64_t i = 0; i <= state.highest_idx; ++i)
        {
            if (state.processed.find(i) == state.processed.end())
            {
                state.missing.push_back(i);
            }
        }
    }
    
    return state;
}

std::string format_duration(double seconds)
{
    // Handle unrealistic ETAs (overflow or extremely large values)
    if (seconds < 0 || seconds > 1e15)  // > ~31 million years
        return ">31M years";
    
    if (seconds < 60)
        return std::to_string((int)seconds) + "s";
    if (seconds < 3600)
        return std::to_string((int)(seconds / 60)) + "m " + std::to_string((int)seconds % 60) + "s";
    if (seconds < 86400)
        return std::to_string((int)(seconds / 3600)) + "h " + std::to_string((int)(seconds / 60) % 60) + "m";
    if (seconds < 31536000)  // < 1 year
    {
        int days = (int)(seconds / 86400);
        int hours = (int)(seconds / 3600) % 24;
        return std::to_string(days) + "d " + std::to_string(hours) + "h";
    }
    
    // For very large durations, show years
    double years = seconds / 31536000.0;
    if (years < 1000)
        return std::to_string((int)years) + " years";
    if (years < 1000000)
        return std::to_string((int)(years / 1000)) + "K years";
    if (years < 1000000000)
        return std::to_string((int)(years / 1000000)) + "M years";
    
    return std::to_string((int)(years / 1000000000)) + "B years";
}

int main()
{
    // Load persistent cache
    std::cout << "Loading cache from disk..." << std::flush;
    if (g_state_cache.load("data/state_cache.bin"))
    {
        std::cout << " " << g_state_cache.size() << " states loaded\n" << std::flush;
    }
    else
    {
        std::cout << " failed or not found\n" << std::flush;
    }
    
    // Scan existing file
    std::cout << "Scanning existing file...\n" << std::flush;
    AnalysisState state = scan_existing_file();
    
    if (state.processed.empty())
    {
        std::cout << "Starting fresh from 0x0\n\n" << std::flush;
    }
    else
    {
        std::cout << "Found " << state.processed.size() << " patterns, highest: 0x" 
                  << std::hex << state.highest_idx << std::dec << "\n";
        
        if (!state.missing.empty())
        {
            std::cout << "Found " << state.missing.size() << " missing patterns below highest\n";
            std::cout << "Will fill gaps first, then continue from 0x" 
                      << std::hex << (state.highest_idx + 1) << std::dec << "\n\n" << std::flush;
        }
        else
        {
            std::cout << "No gaps found. Continuing from 0x" 
                      << std::hex << (state.highest_idx + 1) << std::dec << "\n\n" << std::flush;
        }
    }
    
    // Multithreading setup
    const int NUM_THREADS = std::thread::hardware_concurrency();
    std::cout << "Using " << NUM_THREADS << " threads\n\n" << std::flush;
    
    // Shared state
    std::atomic<size_t> missing_idx(0);  // Current position in missing array
    std::atomic<uint64_t> next_idx(state.highest_idx + 1);  // For new patterns after gaps filled
    std::atomic<bool> filling_gaps(state.missing.size() > 0);
    
    std::mutex output_mutex;
    std::vector<std::string> pending_output;
    pending_output.reserve(10000);
    
    std::atomic<uint64_t> patterns_processed(0);
    std::atomic<uint64_t> last_checkpoint(0);
    std::atomic<size_t> last_cache_size(0);
    const uint64_t CHECKPOINT_INTERVAL = 10000;  // Save cache every 10,000 patterns
    auto start_time = std::chrono::steady_clock::now();
    
    // Progress reporting thread
    std::atomic<bool> done(false);
    std::thread progress_thread([&]() {
        const int PROGRESS_INTERVAL = 5;  // Report every 5 seconds
        while (!done)
        {
            std::this_thread::sleep_for(std::chrono::seconds(PROGRESS_INTERVAL));
            
            uint64_t processed = patterns_processed.load();
            
            if (processed > 0)
            {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - start_time).count();
                double rate = processed / elapsed;
                
                std::ostringstream oss;
                
                if (filling_gaps.load())
                {
                    size_t current_missing = missing_idx.load();
                    size_t total_missing = state.missing.size();
                    oss << "[Filling gaps: " << current_missing << "/" << total_missing << "]";
                }
                else
                {
                    uint64_t current_idx = next_idx.load();
                    oss << "0x" << std::hex << current_idx << std::dec;
                    
                    if (processed >= 5000)
                    {
                        uint64_t remaining = UINT64_MAX - current_idx;
                        double eta_seconds = remaining / rate;
                        oss << " | ETA to 2^64: " << format_duration(eta_seconds);
                    }
                    
                    oss << " | Progress: " << std::fixed << std::setprecision(10)
                        << (100.0 * current_idx / (double)UINT64_MAX) << "%";
                }
                
                oss << " | " << (int)rate << " patterns/s\n";
                std::cout << oss.str() << std::flush;
            }
        }
    });
    
    // Worker threads
    std::vector<std::thread> workers;
    const int BATCH_SIZE = 100;
    
    for (int t = 0; t < NUM_THREADS; ++t)
    {
        workers.emplace_back([&, missing_patterns = state.missing]() {
            SpiralBinaryGenerator gen(64, 3);
            CachedLifeAnalyzer analyzer(g_state_cache);
            std::vector<std::string> local_batch;
            local_batch.reserve(BATCH_SIZE);
            
            while (!done.load())
            {
                uint64_t idx;
                
                // First fill gaps
                if (filling_gaps.load())
                {
                    size_t pos = missing_idx.fetch_add(1);
                    if (pos >= missing_patterns.size())
                    {
                        // Done filling gaps, switch to sequential mode
                        filling_gaps.store(false);
                        if (pos == missing_patterns.size())  // Only first thread prints
                        {
                            std::cout << "\nGaps filled. Continuing from 0x" << std::hex 
                                      << next_idx.load() << std::dec << "\n\n" << std::flush;
                        }
                        idx = next_idx.fetch_add(1);
                    }
                    else
                    {
                        idx = missing_patterns[pos];
                    }
                }
                else
                {
                    // Normal sequential processing
                    idx = next_idx.fetch_add(1);
                }
                
                PlacedGrid pg = gen.from_index(idx);
                auto info = analyzer.analyze(std::move(pg), 2);
                
                // Build result string
                std::ostringstream oss;
                oss << "spiral 0x" << std::hex << idx << std::dec << ": ";
                
                if (info.first_repeat_step == -1)
                {
                    oss << "MAX_STEPS_REACHED\n";
                }
                else
                {
                    oss << "prefix=" << info.prefix_len
                        << " period=" << info.period
                        << " first_repeat=" << info.first_repeat_step
                        << " dx=" << info.dx
                        << " dy=" << info.dy << "\n";
                }
                
                local_batch.push_back(oss.str());
                patterns_processed++;
                
                // Periodic cache checkpoint
                uint64_t current_processed = patterns_processed.load();
                uint64_t last_cp = last_checkpoint.load();
                if (current_processed - last_cp >= CHECKPOINT_INTERVAL)
                {
                    if (last_checkpoint.compare_exchange_strong(last_cp, current_processed))
                    {
                        // Only one thread saves
                        size_t current_cache_size = g_state_cache.size();
                        size_t prev_cache_size = last_cache_size.load();
                        size_t new_states = current_cache_size - prev_cache_size;
                        
                        uint64_t hits = g_state_cache.hits();
                        uint64_t misses = g_state_cache.misses();
                        uint64_t total_lookups = hits + misses;
                        double hit_rate = total_lookups > 0 ? (100.0 * hits / total_lookups) : 0.0;
                        
                        uint64_t cp_hits = g_state_cache.checkpoint_hits();
                        uint64_t cp_misses = g_state_cache.checkpoint_misses();
                        uint64_t cp_total = cp_hits + cp_misses;
                        double cp_hit_rate = cp_total > 0 ? (100.0 * cp_hits / cp_total) : 0.0;
                        
                        std::cout << "\n[Checkpoint: " << current_cache_size << " states (+" 
                                  << new_states << " new) | Hit: " << std::fixed 
                                  << std::setprecision(1) << cp_hit_rate << "% (overall " 
                                  << hit_rate << "%) | Saving...]";
                        
                        if (g_state_cache.save("data/state_cache.bin"))
                        {
                            std::cout << " Done\n" << std::flush;
                            last_cache_size.store(current_cache_size);
                            g_state_cache.reset_checkpoint_stats();
                        }
                        else
                        {
                            std::cout << " Failed\n" << std::flush;
                        }
                    }
                }
                
                // Flush local batch to shared output
                if (local_batch.size() >= BATCH_SIZE)
                {
                    std::lock_guard<std::mutex> lock(output_mutex);
                    pending_output.insert(pending_output.end(), 
                                         local_batch.begin(), local_batch.end());
                    local_batch.clear();
                }
            }
        });
    }
    
    // Writer thread
    std::thread writer_thread([&]() {
        std::ofstream outfile("pattern_analysis.txt", std::ios::app);
        
        while (!done)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            std::vector<std::string> to_write;
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                to_write.swap(pending_output);
            }
            
            for (const auto& line : to_write)
            {
                outfile << line;
            }
            outfile.flush();
        }
    });
    
    // Wait indefinitely (Ctrl+C to stop)
    std::cout << "Processing patterns...\n" << std::flush;
    std::cout << "Press Ctrl+C to stop and display statistics\n\n" << std::flush;
    
    // In a real scenario, you'd handle signals properly
    // For now, threads run indefinitely
    for (auto& w : workers)
    {
        w.join();
    }
    
    done.store(true);
    progress_thread.join();
    writer_thread.join();
    
    // Save final cache state
    std::cout << "\nSaving cache..." << std::flush;
    if (g_state_cache.save("data/state_cache.bin"))
    {
        std::cout << " saved " << g_state_cache.size() << " states\n";
    }
    else
    {
        std::cout << " failed\n";
    }
    
    // Display cache statistics
    std::cout << "\n=== Cache Statistics ===\n";
    std::cout << "Cache size: " << g_state_cache.size() << " states\n";
    std::cout << "Cache hits: " << g_state_cache.hits() << "\n";
    std::cout << "Cache misses: " << g_state_cache.misses() << "\n";
    uint64_t total_lookups = g_state_cache.hits() + g_state_cache.misses();
    if (total_lookups > 0)
    {
        double hit_rate = 100.0 * g_state_cache.hits() / total_lookups;
        std::cout << "Hit rate: " << std::fixed << std::setprecision(2) << hit_rate << "%\n";
    }
    std::cout << "========================\n";
    
    return 0;
}
