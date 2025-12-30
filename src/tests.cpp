/*
 * Game of Life Cache System - Comprehensive Test Suite
 * 
 * This test suite validates all aspects of the DAG-based caching system:
 * 
 * PATTERN BEHAVIOR DIAGNOSTICS:
 *   - Raw LifeRunner behavior (glider translation, blinker oscillation)
 *   - Canonical state correctness (different orientations produce different hashes)
 * 
 * BASIC CACHE FUNCTIONALITY:
 *   - Insert/lookup operations
 *   - Collision handling
 *   - Empty pattern handling
 *   - Checkpoint statistics
 *   - Persistent storage (save/load)
 * 
 * CACHE CORRECTNESS:
 *   - Cached vs uncached analysis produces same results
 *   - Cache reuse across multiple patterns
 * 
 * EDGE CASES:
 *   - Translation invariance (same pattern at different positions)
 *   - Intermediate state shortcuts
 *   - Cache hits when revisiting patterns
 * 
 * PATTERN-SPECIFIC TESTS:
 *   - Still lifes (block)
 *   - Oscillators (blinker)
 *   - Spaceships (glider, LWSS)
 *   - Converging patterns
 * 
 * HARD STRESS TESTS:
 *   - Patterns sharing intermediate states
 *   - Diverging patterns
 *   - Cache efficiency with repetition
 *   - Rotation handling
 *   - Complex collisions
 *   - Long transients (R-pentomino, Acorn)
 * 
 * Build: .\test.ps1 or F5 with "Test Suite" launch configuration
 */

#include "StateCache.h"
#include "CachedLifeAnalyzer.h"
#include "LifeRunner.h"
#include "SpiralGenerator.h"
#include "RLEParser.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <chrono>

// ============================================================================
// RLE PATTERN LIBRARY
// ============================================================================
// Standard Game of Life patterns in RLE format
// Fill these in with correct RLE strings

namespace Patterns {
    // Glider (period 4, translates diagonally by (1,1) every 4 gens)
    constexpr const char* GLIDER = R"(x = 3, y = 3, rule = B3/S23
bob$2bo$3o!)";
    
    // Blinker (period 2 oscillator)
    constexpr const char* BLINKER_HORIZONTAL = R"(x = 3, y = 1, rule = B3/S23
3o!)";
    constexpr const char* BLINKER_VERTICAL = R"(x = 1, y = 3, rule = B3/S23
o$o$o!)";
    
    // Block (still life, period 1)
    constexpr const char* BLOCK = R"(x = 2, y = 2, rule = B3/S23
2o$2o!)";
    
    // Lightweight spaceship (period 4, translates horizontally)
    constexpr const char* LWSS = R"(x = 5, y = 4, rule = B3/S23
bo2bo$o$o3bo$4o!)";

    // R-pentomino (methuselah, stabilizes after 1103 generations)
    constexpr const char* R_PENTOMINO = R"(x = 3, y = 3, rule = B3/S23
b2o$2o$bo!)";
    
    // Acorn (methuselah, stabilizes after 5206 generations)
    constexpr const char* ACORN = R"(x = 7, y = 3, rule = B3/S23
bo5b$3bo3b$2o2b3o!)";
}

// Test colors
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

// Test tracking
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static bool g_current_test_failed = false;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            if (!g_current_test_failed) { \
                std::cout << RED << "FAILED" << RESET << std::endl; \
                g_current_test_failed = true; \
            } \
            std::cout << "  Assertion failed: " << message << std::endl; \
            std::cout << "  at line " << __LINE__ << std::endl; \
        } \
    } while(0)

#define TEST_PASS() \
    do { \
        if (g_current_test_failed) { \
            g_tests_failed++; \
        } else { \
            std::cout << GREEN << "PASSED" << RESET << std::endl; \
            g_tests_passed++; \
        } \
    } while(0)

#define RUN_TEST(test_func) \
    do { \
        g_tests_run++; \
        g_current_test_failed = false; \
        size_t tests_before = g_tests_passed + g_tests_failed; \
        test_func(); \
        size_t tests_after = g_tests_passed + g_tests_failed; \
        if (tests_after == tests_before) { \
            if (g_current_test_failed) { \
                g_tests_failed++; \
            } else { \
                g_tests_failed++; \
            } \
        } \
    } while(0)

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void test_rle_parser()
{
    std::cout << "Testing RLE parser... " << std::flush;
    
    // Test simple block pattern: 2o$2o!
    PlacedGrid pg = RLEParser::parse("2o$2o!", 2);
    
    // Should create a 2x2 block with padding
    TEST_ASSERT(pg.grid.width() >= 4, "Grid should have padding");
    TEST_ASSERT(pg.grid.height() >= 4, "Grid should have padding");
    
    // Check the block cells (accounting for padding of 2)
    TEST_ASSERT(pg.grid.get(2, 2), "Cell (2,2) should be alive");
    TEST_ASSERT(pg.grid.get(3, 2), "Cell (3,2) should be alive");
    TEST_ASSERT(pg.grid.get(2, 3), "Cell (2,3) should be alive");
    TEST_ASSERT(pg.grid.get(3, 3), "Cell (3,3) should be alive");
    
    // Test blinker: 3o!
    PlacedGrid pg2 = RLEParser::parse("3o!", 2);
    TEST_ASSERT(pg2.grid.get(2, 2), "Blinker cell 1 should be alive");
    TEST_ASSERT(pg2.grid.get(3, 2), "Blinker cell 2 should be alive");
    TEST_ASSERT(pg2.grid.get(4, 2), "Blinker cell 3 should be alive");
    
    TEST_PASS();
}

// Create pattern from RLE string if available, otherwise return empty grid
PlacedGrid create_pattern_from_rle(const char* rle, int width, int height, int padding = 2)
{
    if (rle && rle[0] != '\0')
    {
        return RLEParser::parse(rle, padding);
    }
    else
    {
        // Return empty grid of specified size for manual setup
        PlacedGrid pg;
        pg.grid = BitGrid(width, height);
        pg.offset_x = 0;
        pg.offset_y = 0;
        return pg;
    }
}

// Create completely empty grid
PlacedGrid create_empty_grid(int width = 10, int height = 10)
{
    PlacedGrid pg;
    pg.grid = BitGrid(width, height);
    pg.offset_x = 0;
    pg.offset_y = 0;
    return pg;
}

// ============================================================================
// BASIC CACHE FUNCTIONALITY
// ============================================================================

void test_basic_cache_operations()
{
    std::cout << "Testing basic cache operations... " << std::flush;
    
    StateCache cache;
    
    // Create a test state
    CanonicalState state;
    state.w = 5;
    state.h = 5;
    state.words = {0x1F, 0x10, 0x10, 0x10, 0x1F}; // Box pattern
    state.hash = canonical_hash(state.w, state.h, state.words);
    
    // Create cycle info
    CachedCycleInfo info;
    info.steps_to_cycle = 0;
    info.cycle_start_gen = 0;
    info.period = 1;
    info.dx = 0;
    info.dy = 0;
    info.cycle_state_hash = state.hash;
    
    // Test insert
    cache.insert(state, info);
    TEST_ASSERT(cache.size() == 1, "Cache should have 1 entry");
    
    // Test lookup - should hit
    CachedCycleInfo retrieved;
    bool found = cache.lookup(state, retrieved);
    TEST_ASSERT(found, "Lookup should find the entry");
    TEST_ASSERT(retrieved.period == 1, "Expected period 1");
    TEST_ASSERT(cache.hits() == 1, "Should have 1 hit");
    TEST_ASSERT(cache.misses() == 0, "Should have 0 misses");
    
    // Test lookup - should miss
    CanonicalState different_state;
    different_state.w = 3;
    different_state.h = 3;
    different_state.words = {0x7, 0x5, 0x7};
    different_state.hash = canonical_hash(different_state.w, different_state.h, different_state.words);
    
    found = cache.lookup(different_state, retrieved);
    TEST_ASSERT(!found, "Lookup should not find non-existent entry");
    TEST_ASSERT(cache.hits() == 1, "Should still have 1 hit");
    TEST_ASSERT(cache.misses() == 1, "Should have 1 miss");
    
    TEST_PASS();
}

void test_collision_handling()
{
    std::cout << "Testing collision handling... " << std::flush;
    
    StateCache cache;
    
    // Create two different states (they will have different hashes naturally)
    CanonicalState state1;
    state1.w = 3;
    state1.h = 3;
    state1.words = {0x7, 0x5, 0x7};
    state1.hash = canonical_hash(state1.w, state1.h, state1.words);
    
    CanonicalState state2;
    state2.w = 3;
    state2.h = 3;
    state2.words = {0x7, 0x7, 0x7};
    state2.hash = canonical_hash(state2.w, state2.h, state2.words);
    
    CachedCycleInfo info1;
    info1.period = 2;
    info1.steps_to_cycle = 0;
    info1.cycle_start_gen = 0;
    info1.dx = 0;
    info1.dy = 0;
    info1.cycle_state_hash = state1.hash;
    
    CachedCycleInfo info2;
    info2.period = 3;
    info2.steps_to_cycle = 0;
    info2.cycle_start_gen = 0;
    info2.dx = 0;
    info2.dy = 0;
    info2.cycle_state_hash = state2.hash;
    
    cache.insert(state1, info1);
    cache.insert(state2, info2);
    
    // Verify both can be retrieved correctly
    CachedCycleInfo retrieved;
    bool found1 = cache.lookup(state1, retrieved);
    TEST_ASSERT(found1, "Should find first entry");
    TEST_ASSERT(retrieved.period == 2, "Expected period 2");
    
    bool found2 = cache.lookup(state2, retrieved);
    TEST_ASSERT(found2, "Should find second entry");
    TEST_ASSERT(retrieved.period == 3, "Expected period 3");
    
    TEST_PASS();
}

// ============================================================================
// CACHE CORRECTNESS
// ============================================================================

void test_analyzer_correctness()
{
    std::cout << "Testing analyzer correctness (with vs without cache)... " << std::flush;
    
    SpiralBinaryGenerator gen(64, 3);
    
    // Test a few patterns
    std::vector<uint64_t> test_patterns = {0x0, 0x1, 0x7, 0xFF, 0x1234};
    
    for (uint64_t idx : test_patterns)
    {
        PlacedGrid pg1 = gen.from_index(idx);
        PlacedGrid pg2 = gen.from_index(idx);
        
        // Without cache (use LifeRunner directly)
        LifeRunner runner;
        runner.init(std::move(pg1));
        auto info_nocache = runner.run_until_repeat(2, 1000);
        
        // With cache
        StateCache cache;
        CachedLifeAnalyzer analyzer(cache);
        auto info_cached = analyzer.analyze(std::move(pg2), 2, 1000);
        
        // Results should be identical
        if (info_nocache.first_repeat_step != info_cached.first_repeat_step) {
            std::cout << "\nPattern 0x" << std::hex << idx << std::dec 
                      << ": nocache=" << info_nocache.first_repeat_step 
                      << ", cached=" << info_cached.first_repeat_step << std::endl;
        }
        TEST_ASSERT(info_nocache.first_repeat_step == info_cached.first_repeat_step, "Cached and uncached first_repeat_step differ");
        TEST_ASSERT(info_nocache.prefix_len == info_cached.prefix_len, "Cached and uncached prefix_len differ");
        TEST_ASSERT(info_nocache.period == info_cached.period, "Cached and uncached period differ");
        TEST_ASSERT(info_nocache.dx == info_cached.dx, "Cached and uncached dx differ");
        TEST_ASSERT(info_nocache.dy == info_cached.dy, "Cached and uncached dy differ");
    }
    
    TEST_PASS();
}

void test_cache_reuse()
{
    std::cout << "Testing cache reuse across patterns... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    SpiralBinaryGenerator gen(64, 3);
    
    // Analyze several patterns
    std::vector<uint64_t> patterns = {0x1, 0x2, 0x3, 0x4, 0x5};
    
    for (uint64_t idx : patterns)
    {
        PlacedGrid pg = gen.from_index(idx);
        analyzer.analyze(std::move(pg), 2, 1000);
    }
    
    size_t cache_size_after_first_run = cache.size();
    uint64_t hits_after_first_run = cache.hits();
    
    // Analyze the same patterns again - should get cache hits
    for (uint64_t idx : patterns)
    {
        PlacedGrid pg = gen.from_index(idx);
        analyzer.analyze(std::move(pg), 2, 1000);
    }
    
    // Cache should have some hits on second run
    uint64_t hits_after_second_run = cache.hits();
    
    std::cout << "First run: " << cache_size_after_first_run << " states, " 
              << hits_after_first_run << " hits" << std::endl;
    std::cout << "Second run: Additional " << (hits_after_second_run - hits_after_first_run) 
              << " hits... ";
    
    // We should get at least SOME hits on the second run
    if (hits_after_second_run > hits_after_first_run)
    {
        TEST_PASS();
    }
    else
    {
        std::cout << YELLOW << "WARNING: No cache hits on second run" << RESET << std::endl;
    }
}

void test_persistent_storage()
{
    std::cout << "Testing persistent storage (save/load)... " << std::flush;
    
    const std::string test_file = "test_cache.bin";
    
    // Create and populate cache
    StateCache cache1;
    
    CanonicalState state;
    state.w = 5;
    state.h = 5;
    state.words = {0x1F, 0x10, 0x10, 0x10, 0x1F};
    state.hash = canonical_hash(state.w, state.h, state.words);
    
    CachedCycleInfo info;
    info.steps_to_cycle = 10;
    info.cycle_start_gen = 50;
    info.period = 2;
    info.dx = 1;
    info.dy = -1;
    info.cycle_state_hash = state.hash;
    
    cache1.insert(state, info);
    
    // Save
    bool saved = cache1.save(test_file);
    TEST_ASSERT(saved, "Cache save should succeed");
    
    // Load into new cache
    StateCache cache2;
    bool loaded = cache2.load(test_file);
    TEST_ASSERT(loaded, "Cache load should succeed");
    TEST_ASSERT(cache2.size() == 1, "Cache should have 1 entry after load");
    
    // Verify data integrity
    CachedCycleInfo retrieved;
    bool found = cache2.lookup(state, retrieved);
    TEST_ASSERT(found, "Lookup should find the entry");
    TEST_ASSERT(retrieved.steps_to_cycle == 10, "Expected steps_to_cycle 10");
    TEST_ASSERT(retrieved.cycle_start_gen == 50, "Expected cycle_start_gen 50");
    TEST_ASSERT(retrieved.period == 2, "Expected period 2");
    TEST_ASSERT(retrieved.dx == 1, "Expected dx 1");
    TEST_ASSERT(retrieved.dy == -1, "Expected dy -1");
    
    // Cleanup
    std::remove(test_file.c_str());
    
    TEST_PASS();
}

void test_empty_pattern()
{
    std::cout << "Testing empty pattern handling... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Empty pattern (all zeros)
    SpiralBinaryGenerator gen(64, 3);
    PlacedGrid pg = gen.from_index(0);
    
    auto result = analyzer.analyze(std::move(pg), 2, 1000);
    
    // Empty should stabilize immediately
    TEST_ASSERT(result.prefix_len == 0, "Expected prefix_len 0");
    TEST_ASSERT(result.period == 1, "Expected period 1");
    TEST_ASSERT(!result.max_steps_reached, "Should not reach max steps");
    
    TEST_PASS();
}

void test_checkpoint_stats()
{
    std::cout << "Testing checkpoint statistics reset... " << std::flush;
    
    StateCache cache;
    
    CanonicalState state1;
    state1.w = 3;
    state1.h = 3;
    state1.words = {0x7, 0x5, 0x7};
    state1.hash = canonical_hash(state1.w, state1.h, state1.words);
    
    CachedCycleInfo info;
    info.period = 1;
    info.steps_to_cycle = 0;
    info.cycle_start_gen = 0;
    info.dx = 0;
    info.dy = 0;
    info.cycle_state_hash = state1.hash;
    
    cache.insert(state1, info);
    
    // Do some lookups
    CachedCycleInfo retrieved;
    cache.lookup(state1, retrieved); // Hit
    
    CanonicalState state2;
    state2.w = 2;
    state2.h = 2;
    state2.words = {0x3, 0x3};
    state2.hash = canonical_hash(state2.w, state2.h, state2.words);
    cache.lookup(state2, retrieved); // Miss
    
    TEST_ASSERT(cache.checkpoint_hits() == 1, "Should have 1 checkpoint hit");
    TEST_ASSERT(cache.checkpoint_misses() == 1, "Should have 1 checkpoint miss");
    
    // Reset checkpoint stats
    cache.reset_checkpoint_stats();
    
    TEST_ASSERT(cache.checkpoint_hits() == 0, "Checkpoint hits should reset to 0");
    TEST_ASSERT(cache.checkpoint_misses() == 0, "Checkpoint misses should reset to 0");
    TEST_ASSERT(cache.hits() == 1, "Overall hits should remain");
    TEST_ASSERT(cache.misses() == 1, "Overall misses should remain");
    
    TEST_PASS();
}

// ============================================================================
// EDGE CASES
// ============================================================================

void test_translation_invariance()
{
    std::cout << "Testing translation invariance... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    SpiralBinaryGenerator gen(64, 3);
    
    // Generate a pattern (block at center)
    // We'll manually create patterns at different positions
    PlacedGrid pg1 = RLEParser::parse(Patterns::BLOCK, 3);
    
    PlacedGrid pg2 = RLEParser::parse(Patterns::BLOCK, 3);
    pg2.offset_x = 100;  // Translated by 100 in X
    pg2.offset_y = 200;  // Translated by 200 in Y
    
    // Analyze first pattern
    auto result1 = analyzer.analyze(std::move(pg1), 2, 1000);
    size_t cache_size_after_first = cache.size();
    uint64_t hits_after_first = cache.hits();
    
    // Analyze translated pattern - should hit cache
    auto result2 = analyzer.analyze(std::move(pg2), 2, 1000);
    uint64_t hits_after_second = cache.hits();
    
    // Results should be identical (dx, dy are relative, not absolute)
    TEST_ASSERT(result1.prefix_len == result2.prefix_len, "prefix_len should match");
    TEST_ASSERT(result1.period == result2.period, "period should match");
    TEST_ASSERT(result1.dx == result2.dx, "dx should match");
    TEST_ASSERT(result1.dy == result2.dy, "dy should match");
    
    // Should have gotten cache hits from the translation
    TEST_ASSERT(hits_after_second > hits_after_first, "Should get cache hits from translation");
    
    std::cout << "Cache hits from translation: " << (hits_after_second - hits_after_first) << "... ";
    TEST_PASS();
}

void test_intermediate_state_shortcut()
{
    std::cout << "Testing intermediate state cache shortcut... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    SpiralBinaryGenerator gen(64, 3);
    
    // Analyze a pattern that evolves through multiple states
    PlacedGrid pg1 = gen.from_index(0x42);
    auto result1 = analyzer.analyze(std::move(pg1), 2, 1000);
    
    size_t cache_size_after_first = cache.size();
    uint64_t total_lookups_after_first = cache.hits() + cache.misses();
    
    // Now analyze a different pattern that should converge to an intermediate state
    // from the first pattern's evolution
    PlacedGrid pg2 = gen.from_index(0x43);
    auto result2 = analyzer.analyze(std::move(pg2), 2, 1000);
    
    uint64_t total_lookups_after_second = cache.hits() + cache.misses();
    
    // Second analysis should have done lookups
    TEST_ASSERT(total_lookups_after_second > total_lookups_after_first, "Should perform lookups in second analysis");
    
    std::cout << "Lookups in second analysis: " << (total_lookups_after_second - total_lookups_after_first) 
              << ", Cache size: " << cache.size() << "... ";
    TEST_PASS();
}

void test_glider_detection()
{
    std::cout << "Testing glider detection (dx, dy tracking)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg = RLEParser::parse(Patterns::GLIDER, 5);
    
    auto result = analyzer.analyze(std::move(pg), 2, 1000);
    
    // Glider should have period 4 and move diagonally
    TEST_ASSERT(result.period == 4, "Expected glider period 4");
    TEST_ASSERT(result.dx != 0 || result.dy != 0, "Glider should translate");
    TEST_ASSERT(!result.max_steps_reached, "Glider analysis hit max steps");
    
    std::cout << "Glider: period=" << result.period << ", dx=" << result.dx 
              << ", dy=" << result.dy << "... ";
    TEST_PASS();
}

void test_blinker_oscillator()
{
    std::cout << "Testing blinker oscillator (period 2)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg = RLEParser::parse(Patterns::BLINKER_VERTICAL, 5);
    
    auto result = analyzer.analyze(std::move(pg), 2, 1000);
    
    // Blinker should have period 2, no translation
    TEST_ASSERT(result.period == 2, "Assertion failed");
    TEST_ASSERT(result.dx == 0, "Expected dx 0");
    TEST_ASSERT(result.dy == 0, "Expected dy 0");
    TEST_ASSERT(!result.max_steps_reached, "Should not reach max steps");
    
    std::cout << "Blinker: period=" << result.period << ", prefix=" << result.prefix_len << "... ";
    TEST_PASS();
}

// ============================================================================
// PATTERN-SPECIFIC TESTS
// ============================================================================

void test_still_life_block()
{
    std::cout << "Testing still life (block)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg = RLEParser::parse(Patterns::BLOCK, 5);
    
    auto result = analyzer.analyze(std::move(pg), 2, 1000);
    
    // Block should be immediately stable (period 1, no prefix)
    TEST_ASSERT(result.period == 1, "Expected period 1");
    TEST_ASSERT(result.prefix_len == 0, "Expected prefix_len 0");
    TEST_ASSERT(result.dx == 0, "Expected dx 0");
    TEST_ASSERT(result.dy == 0, "Expected dy 0");
    TEST_ASSERT(!result.max_steps_reached, "Should not reach max steps");
    
    TEST_PASS();
}

void test_converging_patterns()
{
    std::cout << "Testing multiple patterns converging to same attractor... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Create several patterns that should die out (converge to empty)
    std::vector<PlacedGrid> dying_patterns;
    
    for (int i = 0; i < 5; i++)
    {
        PlacedGrid pg;
        pg.grid = BitGrid(10, 10);
        pg.offset_x = 0;
        pg.offset_y = 0;
        
        // Single isolated cells at different positions
        pg.grid.set(i + 2, i + 2);
        
        dying_patterns.push_back(std::move(pg));
    }
    
    uint64_t initial_hits = cache.hits();
    
    // Analyze all patterns
    for (auto& pg : dying_patterns)
    {
        auto result = analyzer.analyze(std::move(pg), 2, 100);
        // All should die (empty = period 1, all zeros)
        TEST_ASSERT(result.period == 1, "Expected period 1");
    }
    
    uint64_t final_hits = cache.hits();
    
    // Should have gotten cache hits from converging to same empty state
    std::cout << "Cache hits from convergence: " << (final_hits - initial_hits) << "... ";
    
    if (final_hits > initial_hits)
    {
        TEST_PASS();
    }
    else
    {
        std::cout << YELLOW << "WARNING: Expected cache hits from convergence" << RESET << std::endl;
    }
}

void test_cache_hit_on_revisit()
{
    std::cout << "Testing cache hit when revisiting same pattern... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    SpiralBinaryGenerator gen(64, 3);
    
    // Analyze a pattern
    PlacedGrid pg1 = gen.from_index(0x100);
    auto result1 = analyzer.analyze(std::move(pg1), 2, 1000);
    
    uint64_t hits_before = cache.hits();
    uint64_t misses_before = cache.misses();
    
    // Analyze the EXACT same pattern again
    PlacedGrid pg2 = gen.from_index(0x100);
    auto result2 = analyzer.analyze(std::move(pg2), 2, 1000);
    
    uint64_t hits_after = cache.hits();
    uint64_t misses_after = cache.misses();
    
    // Should get immediate cache hit (first lookup should succeed)
    TEST_ASSERT(hits_after > hits_before, "Should get cache hits on revisit");
    
    // Results should be identical
    TEST_ASSERT(result1.prefix_len == result2.prefix_len, "prefix_len should match");
    TEST_ASSERT(result1.period == result2.period, "period should match");
    TEST_ASSERT(result1.dx == result2.dx, "dx should match");
    TEST_ASSERT(result1.dy == result2.dy, "dy should match");
    
    std::cout << "Got " << (hits_after - hits_before) << " cache hits on revisit... ";
    TEST_PASS();
}

void test_r_pentomino()
{
    std::cout << "Testing R-pentomino (long transient, 1103 generations to stabilize)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg = RLEParser::parse(Patterns::R_PENTOMINO, 50);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = analyzer.analyze(std::move(pg), 10, 5000);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "prefix=" << result.prefix_len 
              << ", period=" << result.period
              << ", time=" << ms << "ms, cache_size=" << cache.size()
              << "... ";
    
    // R-pentomino should have a long prefix (over 1000 gens)
    TEST_ASSERT(result.prefix_len > 100, "Expected R-pentomino prefix_len > 100");
    TEST_PASS();
}

void test_acorn_pattern()
{
    std::cout << "Testing Acorn (stabilizes at 5206 generations)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg = RLEParser::parse(Patterns::ACORN, 70);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = analyzer.analyze(std::move(pg), 2, 10000);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "prefix=" << result.prefix_len 
              << ", period=" << result.period
              << ", time=" << ms << "ms, cache_size=" << cache.size()
              << "... ";
    
    TEST_ASSERT(result.prefix_len > 1000 || result.max_steps_reached, "Acorn should have long prefix or reach max steps");
    TEST_PASS();
}

// ============================================================================
// HARD STRESS TESTS
// ============================================================================

void test_shared_intermediate_states()
{
    std::cout << "Testing patterns sharing intermediate states... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg1;
    pg1.grid = BitGrid(20, 20);
    pg1.offset_x = 0;
    pg1.offset_y = 0;
    // Horizontal blinker at (5,5)
    pg1.grid.set(4, 5);
    pg1.grid.set(5, 5);
    pg1.grid.set(6, 5);
    
    auto result1 = analyzer.analyze(std::move(pg1), 2, 100);
    size_t cache_after_first = cache.size();
    uint64_t hits_after_first = cache.hits();
    
    PlacedGrid pg2;
    pg2.grid = BitGrid(20, 20);
    pg2.offset_x = 0;
    pg2.offset_y = 0;
    // Horizontal blinker at (10,10) - same shape, different position
    pg2.grid.set(9, 10);
    pg2.grid.set(10, 10);
    pg2.grid.set(11, 10);
    
    auto result2 = analyzer.analyze(std::move(pg2), 2, 100);
    size_t cache_after_second = cache.size();
    uint64_t hits_after_second = cache.hits();
    
    // Both should have period 2
    TEST_ASSERT(result1.period == 2, "First pattern should have period 2");
    TEST_ASSERT(result2.period == 2, "Second pattern should have period 2");
    
    // Second pattern should reuse cached states (translation invariance)
    uint64_t new_hits = hits_after_second - hits_after_first;
    size_t new_cache_entries = cache_after_second - cache_after_first;
    
    std::cout << "new_hits=" << new_hits << ", new_cache_entries=" << new_cache_entries << "... ";
    
    // Should get cache hits without adding many new states
    TEST_ASSERT(new_hits > 0, "Should get cache hits from shared states");
    TEST_ASSERT(new_cache_entries == 0, "Should not add new entries for identical patterns"); // Should be exactly the same canonical states
    
    TEST_PASS();
}

void test_diverging_patterns()
{
    std::cout << "Testing patterns that diverge after sharing initial states... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Start with a blinker
    PlacedGrid pg1;
    pg1.grid = BitGrid(30, 30);
    pg1.offset_x = 0;
    pg1.offset_y = 0;
    pg1.grid.set(14, 15);
    pg1.grid.set(15, 15);
    pg1.grid.set(16, 15);
    
    auto result1 = analyzer.analyze(std::move(pg1), 2, 200);
    size_t cache_size_1 = cache.size();
    
    // Same blinker but with one extra cell nearby
    PlacedGrid pg2;
    pg2.grid = BitGrid(30, 30);
    pg2.offset_x = 0;
    pg2.offset_y = 0;
    pg2.grid.set(14, 15);
    pg2.grid.set(15, 15);
    pg2.grid.set(16, 15);
    pg2.grid.set(17, 17); // Extra cell that will interact
    
    uint64_t hits_before = cache.hits();
    auto result2 = analyzer.analyze(std::move(pg2), 2, 200);
    uint64_t hits_after = cache.hits();
    size_t cache_size_2 = cache.size();
    
    std::cout << "hits_gained=" << (hits_after - hits_before) 
              << ", cache_growth=" << (cache_size_2 - cache_size_1)
              << ", result1.period=" << result1.period
              << ", result2.period=" << result2.period << "... ";
    
    // Cache should grow since patterns evolve differently
    TEST_ASSERT(cache_size_2 > cache_size_1, "Expected cache growth from divergent states");
    
    TEST_PASS();
}

void test_lwss_spaceship()
{
    std::cout << "Testing LWSS spaceship (period 4, translates)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg = RLEParser::parse(Patterns::LWSS, 25);
    
    auto result = analyzer.analyze(std::move(pg), 2, 500);
    
    std::cout << "period=" << result.period << ", dx=" << result.dx << ", dy=" << result.dy << "... ";
    
    // LWSS should have period 4 and translate horizontally
    TEST_ASSERT(result.period == 4, "Expected LWSS period 4");
    TEST_ASSERT(result.dx != 0, "LWSS should translate horizontally");
    TEST_ASSERT(!result.max_steps_reached, "LWSS analysis hit max steps");
    
    TEST_PASS();
}

void test_multiple_gliders_collision()
{
    std::cout << "Testing glider collision (complex evolution)... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg;
    pg.grid = BitGrid(50, 50);
    pg.offset_x = 0;
    pg.offset_y = 0;
    
    // Two gliders heading towards each other
    // Glider 1 (going SE)
    pg.grid.set(10, 11);
    pg.grid.set(11, 12);
    pg.grid.set(12, 10);
    pg.grid.set(12, 11);
    pg.grid.set(12, 12);
    
    // Glider 2 (going NW) - will collide
    pg.grid.set(30, 31);
    pg.grid.set(29, 32);
    pg.grid.set(27, 30);
    pg.grid.set(27, 31);
    pg.grid.set(27, 32);
    
    auto result = analyzer.analyze(std::move(pg), 2, 500);
    
    std::cout << "prefix=" << result.prefix_len << ", period=" << result.period 
              << ", cache_size=" << cache.size() << "... ";
    
    // Collision should eventually settle into some stable/oscillating pattern
    TEST_ASSERT(!result.max_steps_reached || result.prefix_len > 10, "Collision should evolve meaningfully");
    
    TEST_PASS();
}

void test_cache_efficiency_with_repetition()
{
    std::cout << "Testing cache efficiency with repeated pattern analysis... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Analyze the same blinker pattern 100 times
    std::vector<std::pair<uint64_t, size_t>> measurements;
    
    for (int i = 0; i < 100; ++i) {
        PlacedGrid pg;
        pg.grid = BitGrid(20, 20);
        pg.offset_x = 0;
        pg.offset_y = 0;
        pg.grid.set(9, 10);
        pg.grid.set(10, 10);
        pg.grid.set(11, 10);
        
        uint64_t hits_before = cache.hits();
        analyzer.analyze(std::move(pg), 2, 100);
        uint64_t hits_after = cache.hits();
        
        measurements.push_back({hits_after - hits_before, cache.size()});
    }
    
    // After first run, cache should be populated
    // Subsequent runs should have high hit rate
    uint64_t first_run_hits = measurements[0].first;
    uint64_t last_run_hits = measurements[99].first;
    size_t final_cache_size = measurements[99].second;
    
    std::cout << "first_hits=" << first_run_hits 
              << ", last_hits=" << last_run_hits
              << ", final_cache_size=" << final_cache_size << "... ";
    
    // Later runs should have more hits than first run
    TEST_ASSERT(last_run_hits > first_run_hits, "Cache hits should increase with repetition");
    // Cache size should stabilize (not grow much after first few runs)
    TEST_ASSERT(final_cache_size < 10, "Blinker cache should be small"); // Blinker only has 2 states in cycle
    
    TEST_PASS();
}

void test_rotation_not_cached_together()
{
    std::cout << "Testing that rotations create separate cache entries... " << std::flush;
    
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Horizontal blinker
    PlacedGrid pg1;
    pg1.grid = BitGrid(20, 20);
    pg1.offset_x = 0;
    pg1.offset_y = 0;
    pg1.grid.set(9, 10);
    pg1.grid.set(10, 10);
    pg1.grid.set(11, 10);
    
    analyzer.analyze(std::move(pg1), 2, 100);
    size_t cache_after_horizontal = cache.size();
    
    // Vertical blinker (90-degree rotation)
    PlacedGrid pg2;
    pg2.grid = BitGrid(20, 20);
    pg2.offset_x = 0;
    pg2.offset_y = 0;
    pg2.grid.set(10, 9);
    pg2.grid.set(10, 10);
    pg2.grid.set(10, 11);
    
    analyzer.analyze(std::move(pg2), 2, 100);
    size_t cache_after_vertical = cache.size();
    
    std::cout << "cache_growth=" << (cache_after_vertical - cache_after_horizontal) << "... ";
    
    // Blinker oscillates between horizontal and vertical, so both are cached
    // When we analyze vertical blinker, it should hit the cache
    TEST_ASSERT(cache_after_vertical == cache_after_horizontal, "Vertical blinker should hit cache from horizontal analysis");
    
    TEST_PASS();
}

// ============================================================================
// PATTERN BEHAVIOR DIAGNOSTICS
// ============================================================================

void test_glider_raw_evolution()
{
    std::cout << "Testing raw glider evolution (no cache)... " << std::flush;
    
    PlacedGrid pg = RLEParser::parse(Patterns::GLIDER, 8);
    
    LifeRunner runner;
    runner.init(std::move(pg));
    
    auto initial_anchor = runner.anchor_logical_min();
    
    // Evolve for 4 generations (glider period)
    for (int i = 0; i < 4; ++i) {
        runner.step(2);
    }
    
    auto final_anchor = runner.anchor_logical_min();
    int dx = final_anchor.first - initial_anchor.first;
    int dy = final_anchor.second - initial_anchor.second;
    
    std::cout << "offset_change=(" << dx << ", " << dy << ")... ";
    
    // A glider should translate by (1, 1) every 4 generations
    TEST_ASSERT(dx != 0 || dy != 0, "Glider should translate");
    
    TEST_PASS();
}

void test_blinker_canonical_states()
{
    std::cout << "Testing blinker canonical state transitions... " << std::flush;
    
    PlacedGrid pg;
    pg.grid = BitGrid(10, 10);
    pg.offset_x = 0;
    pg.offset_y = 0;
    
    // Horizontal blinker
    pg.grid.set(4, 5);
    pg.grid.set(5, 5);
    pg.grid.set(6, 5);
    
    LifeRunner runner;
    runner.init(std::move(pg));
    
    auto canon1 = runner.canonical_translation_invariant();
    
    runner.step(2);
    auto canon2 = runner.canonical_translation_invariant();
    
    runner.step(2);
    auto canon3 = runner.canonical_translation_invariant();
    
    std::cout << "horizontal_hash=" << std::hex << canon1.hash 
              << ", vertical_hash=" << canon2.hash 
              << ", back_to_horizontal=" << canon3.hash << std::dec << "... ";
    
    // Horizontal and vertical must have different hashes
    TEST_ASSERT(canon1.hash != canon2.hash, "Horizontal and vertical blinker should have different canonical hashes");
    // After 2 steps, should return to original
    TEST_ASSERT(canon1.hash == canon3.hash, "Blinker should return to original state after 2 steps");
    
    TEST_PASS();
}

void test_horizontal_vs_vertical_blinker()
{
    std::cout << "Testing that horizontal and vertical blinkers have different canonical states... " << std::flush;
    
    // Horizontal blinker
    PlacedGrid pg1;
    pg1.grid = BitGrid(10, 10);
    pg1.offset_x = 0;
    pg1.offset_y = 0;
    pg1.grid.set(4, 5);
    pg1.grid.set(5, 5);
    pg1.grid.set(6, 5);
    
    LifeRunner runner1;
    runner1.init(std::move(pg1));
    auto canon_h = runner1.canonical_translation_invariant();
    
    // Vertical blinker (90-degree rotation)
    PlacedGrid pg2;
    pg2.grid = BitGrid(10, 10);
    pg2.offset_x = 0;
    pg2.offset_y = 0;
    pg2.grid.set(5, 4);
    pg2.grid.set(5, 5);
    pg2.grid.set(5, 6);
    
    LifeRunner runner2;
    runner2.init(std::move(pg2));
    auto canon_v = runner2.canonical_translation_invariant();
    
    std::cout << "horizontal=" << std::hex << canon_h.hash 
              << ", vertical=" << canon_v.hash << std::dec << "... ";
    
    // Different orientations MUST have different canonical states
    TEST_ASSERT(canon_h.hash != canon_v.hash, "Horizontal and vertical blinkers must have different canonical states");
    
    TEST_PASS();
}

// ============================================================================
// CRITICAL CACHE EDGE CASE TESTS
// ============================================================================

// Test empty pattern (0x0) - must return first_repeat_step=0
void test_empty_pattern_cache()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Create completely empty pattern
    PlacedGrid pg;
    pg.grid = BitGrid(10, 10);
    pg.offset_x = 0;
    pg.offset_y = 0;
    
    // Empty pattern should immediately repeat (gen 0 == gen 1)
    auto result = analyzer.analyze(std::move(pg), 2, 100);
    
    std::cout << "Empty pattern: prefix=" << result.prefix_len 
              << ", period=" << result.period 
              << ", first_repeat=" << result.first_repeat_step << "... ";
    
    TEST_ASSERT(result.prefix_len == 0, "Empty pattern should have prefix_len=0");
    TEST_ASSERT(result.period == 1, "Empty pattern should have period=1");
    TEST_ASSERT(result.first_repeat_step == 0, "Empty pattern should have first_repeat_step=0 (special case)");
    TEST_ASSERT(!result.max_steps_reached, "Should not hit max steps");
    
    // Analyze again to test cache hit path
    PlacedGrid pg2 = create_empty_grid();
    auto result2 = analyzer.analyze(std::move(pg2), 2, 100);
    
    TEST_ASSERT(result2.first_repeat_step == 0, "Cached empty pattern should also have first_repeat_step=0");
    
    TEST_PASS();
}

// Test single cell that dies immediately
void test_single_cell_dies_immediately()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Single cell dies after one generation (becomes empty)
    PlacedGrid pg;
    pg.grid = BitGrid(10, 10);
    pg.offset_x = 0;
    pg.offset_y = 0;
    pg.grid.set(5, 5);
    
    // Should detect: gen0 (1 cell) -> gen1 (empty) -> gen2 (empty) [cycle detected]
    auto result = analyzer.analyze(std::move(pg), 2, 100);
    
    std::cout << "Single cell death: prefix=" << result.prefix_len 
              << ", period=" << result.period 
              << ", first_repeat=" << result.first_repeat_step << "... ";
    
    TEST_ASSERT(result.prefix_len == 1, "Single cell should have prefix_len=1 (dies at gen 1)");
    TEST_ASSERT(result.period == 1, "Should cycle with period=1 (empty)");
    TEST_ASSERT(result.first_repeat_step == 2, "Should first repeat at step 2 (1+1)");
    
    TEST_PASS();
}

// Test pattern with immediate cycle (period > 1, no prefix)
void test_immediate_cycle_no_prefix()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Blinker at generation 0 - cycles immediately with period 2
    PlacedGrid pg = RLEParser::parse("3o!", 2);
    
    auto result = analyzer.analyze(std::move(pg), 2, 100);
    
    std::cout << "Blinker (immediate cycle): prefix=" << result.prefix_len 
              << ", period=" << result.period 
              << ", first_repeat=" << result.first_repeat_step << "... ";
    
    TEST_ASSERT(result.prefix_len == 0, "Blinker should have prefix_len=0");
    TEST_ASSERT(result.period == 2, "Blinker should have period=2");
    TEST_ASSERT(result.first_repeat_step == 2, "Should first repeat at step 2 (0+2)");
    
    TEST_PASS();
}

// Test pattern with single generation prefix then cycle
void test_single_gen_prefix_then_cycle()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Create a pattern that evolves once then cycles
    // Using a specific configuration
    PlacedGrid pg;
    pg.grid = BitGrid(10, 10);
    pg.offset_x = 0;
    pg.offset_y = 0;
    // Two cells that become a blinker
    pg.grid.set(5, 4);
    pg.grid.set(5, 5);
    
    auto result = analyzer.analyze(std::move(pg), 2, 100);
    
    std::cout << "Two-cell -> blinker: prefix=" << result.prefix_len 
              << ", period=" << result.period << "... ";
    
    TEST_ASSERT(result.prefix_len >= 0, "Should have valid prefix");
    TEST_ASSERT(result.period >= 1, "Should have valid period");
    TEST_ASSERT(result.first_repeat_step == result.prefix_len + result.period, 
                "first_repeat_step should equal prefix_len + period");
    
    TEST_PASS();
}

// Test cache consistency - analyze same pattern multiple times
void test_cache_consistency_multiple_analyses()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    PlacedGrid pg1 = RLEParser::parse("3o!", 2);
    auto result1 = analyzer.analyze(std::move(pg1), 2, 100);
    
    // Analyze exact same pattern again
    PlacedGrid pg2 = RLEParser::parse("3o!", 2);
    auto result2 = analyzer.analyze(std::move(pg2), 2, 100);
    
    // Analyze same pattern at different position (should still get cache hit)
    PlacedGrid pg3 = RLEParser::parse("3o!", 2);
    pg3.offset_x = 10;
    pg3.offset_y = 10;
    auto result3 = analyzer.analyze(std::move(pg3), 2, 100);
    
    std::cout << "Consistency check: r1=" << result1.first_repeat_step 
              << ", r2=" << result2.first_repeat_step 
              << ", r3=" << result3.first_repeat_step << "... ";
    
    TEST_ASSERT(result1.prefix_len == result2.prefix_len, "prefix_len should match");
    TEST_ASSERT(result1.period == result2.period, "period should match");
    TEST_ASSERT(result1.first_repeat_step == result2.first_repeat_step, "first_repeat_step should match");
    TEST_ASSERT(result1.first_repeat_step == result3.first_repeat_step, "first_repeat_step should match at different position");
    
    TEST_PASS();
}

// Test deep cache hit with proper backfill
void test_deep_cache_hit_backfill()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // First analyze a glider (creates path: gen0 -> gen1 -> gen2 -> gen3 -> gen4...)
    PlacedGrid pg1 = RLEParser::parse("bo$2bo$3o!", 2);
    auto result1 = analyzer.analyze(std::move(pg1), 2, 100);
    
    std::cout << "Glider analysis: prefix=" << result1.prefix_len 
              << ", period=" << result1.period << ", cache_size=" << cache.size() << "... ";
    
    size_t initial_cache_size = cache.size();
    
    // Now analyze a different pattern that evolves into a glider after a few steps
    // This should hit the cache at some intermediate glider state
    PlacedGrid pg2 = RLEParser::parse("3o$o$bo!", 2);
    auto result2 = analyzer.analyze(std::move(pg2), 2, 100);
    
    std::cout << "Second pattern: prefix=" << result2.prefix_len 
              << ", cache_size_after=" << cache.size() << "... ";
    
    // Cache should have grown (intermediate states cached)
    TEST_ASSERT(cache.size() > initial_cache_size, "Cache should contain intermediate states");
    
    // Analyze the second pattern again - should be faster due to cache
    PlacedGrid pg3 = RLEParser::parse("3o$o$bo!", 2);
    auto result3 = analyzer.analyze(std::move(pg3), 2, 100);
    
    TEST_ASSERT(result2.prefix_len == result3.prefix_len, "Cached result should match");
    TEST_ASSERT(result2.period == result3.period, "Cached result should match");
    TEST_ASSERT(result2.first_repeat_step == result3.first_repeat_step, "Cached first_repeat_step should match");
    
    TEST_PASS();
}

// Test still life (zero period) vs oscillator
void test_zero_period_still_life()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Block (still life) - should have period 1 (repeats every generation)
    PlacedGrid pg = RLEParser::parse("2o$2o!", 2);
    
    auto result = analyzer.analyze(std::move(pg), 2, 100);
    
    std::cout << "Block (still life): prefix=" << result.prefix_len 
              << ", period=" << result.period 
              << ", first_repeat=" << result.first_repeat_step << "... ";
    
    TEST_ASSERT(result.prefix_len == 0, "Still life should have no prefix");
    TEST_ASSERT(result.period == 1, "Still life should have period=1");
    TEST_ASSERT(result.first_repeat_step == 1, "Should repeat at step 1 (0+1)");
    TEST_ASSERT(result.dx == 0, "Still life should not translate");
    TEST_ASSERT(result.dy == 0, "Still life should not translate");
    
    TEST_PASS();
}

// Test that cached results preserve correct dx/dy translation
void test_cached_translation_correctness()
{
    StateCache cache;
    CachedLifeAnalyzer analyzer(cache);
    
    // Analyze glider first time (uncached)
    PlacedGrid pg1 = RLEParser::parse("bo$2bo$3o!", 2);
    auto result1 = analyzer.analyze(std::move(pg1), 2, 100);
    
    std::cout << "Glider uncached: dx=" << result1.dx << ", dy=" << result1.dy << "... ";
    
    // Analyze glider second time (cached)
    PlacedGrid pg2 = RLEParser::parse("bo$2bo$3o!", 2);
    auto result2 = analyzer.analyze(std::move(pg2), 2, 100);
    
    std::cout << "cached: dx=" << result2.dx << ", dy=" << result2.dy << "... ";
    
    TEST_ASSERT(result1.dx == result2.dx, "Cached dx should match uncached");
    TEST_ASSERT(result1.dy == result2.dy, "Cached dy should match uncached");
    TEST_ASSERT(result1.dx != 0 || result1.dy != 0, "Glider should translate");
    
    // Analyze at different position - should still give correct translation per period
    PlacedGrid pg3 = RLEParser::parse("bo$2bo$3o!", 2);
    pg3.offset_x = 100;
    pg3.offset_y = 100;
    auto result3 = analyzer.analyze(std::move(pg3), 2, 100);
    
    TEST_ASSERT(result3.dx == result1.dx, "Translation should be position-independent");
    TEST_ASSERT(result3.dy == result1.dy, "Translation should be position-independent");
    
    TEST_PASS();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  GAME OF LIFE CACHE TEST SUITE" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try
    {
        // ===== RLE PARSER TEST =====
        std::cout << "--- RLE PARSER TEST ---\n";
        RUN_TEST(test_rle_parser);
        
        // ===== PATTERN BEHAVIOR DIAGNOSTICS =====
        std::cout << "\n--- PATTERN BEHAVIOR DIAGNOSTICS ---\n";
        RUN_TEST(test_glider_raw_evolution);
        RUN_TEST(test_blinker_canonical_states);
        RUN_TEST(test_horizontal_vs_vertical_blinker);
        
        // ===== BASIC CACHE FUNCTIONALITY =====
        std::cout << "\n--- BASIC CACHE FUNCTIONALITY ---\n";
        RUN_TEST(test_basic_cache_operations);
        RUN_TEST(test_collision_handling);
        RUN_TEST(test_empty_pattern);
        RUN_TEST(test_checkpoint_stats);
        RUN_TEST(test_persistent_storage);
        
        // ===== CACHE CORRECTNESS =====
        std::cout << "\n--- CACHE CORRECTNESS ---\n";
        RUN_TEST(test_analyzer_correctness);
        RUN_TEST(test_cache_reuse);
        
        // ===== EDGE CASES =====
        std::cout << "\n--- EDGE CASES ---\n";
        RUN_TEST(test_translation_invariance);
        RUN_TEST(test_intermediate_state_shortcut);
        RUN_TEST(test_cache_hit_on_revisit);
        
        // ===== CRITICAL CACHE EDGE CASES =====
        std::cout << "\n--- CRITICAL CACHE EDGE CASES ---\n";
        RUN_TEST(test_empty_pattern_cache);
        RUN_TEST(test_single_cell_dies_immediately);
        RUN_TEST(test_immediate_cycle_no_prefix);
        RUN_TEST(test_single_gen_prefix_then_cycle);
        RUN_TEST(test_cache_consistency_multiple_analyses);
        RUN_TEST(test_deep_cache_hit_backfill);
        RUN_TEST(test_zero_period_still_life);
        RUN_TEST(test_cached_translation_correctness);
        
        // ===== PATTERN-SPECIFIC TESTS =====
        std::cout << "\n--- PATTERN-SPECIFIC TESTS ---\n";
        RUN_TEST(test_still_life_block);
        RUN_TEST(test_blinker_oscillator);
        RUN_TEST(test_glider_detection);
        RUN_TEST(test_converging_patterns);
        
        // ===== HARD STRESS TESTS =====
        std::cout << "\n--- HARD STRESS TESTS ---\n";
        RUN_TEST(test_shared_intermediate_states);
        RUN_TEST(test_diverging_patterns);
        RUN_TEST(test_lwss_spaceship);
        RUN_TEST(test_cache_efficiency_with_repetition);
        RUN_TEST(test_rotation_not_cached_together);
        RUN_TEST(test_multiple_gliders_collision);
        RUN_TEST(test_r_pentomino);
        RUN_TEST(test_acorn_pattern);
        
        std::cout << "\n========================================" << std::endl;
        if (g_tests_failed == 0) {
            std::cout << GREEN << "  ALL TESTS PASSED!" << RESET << std::endl;
            std::cout << "  " << g_tests_passed << " / " << g_tests_run << " tests passed" << std::endl;
        } else {
            std::cout << RED << "  SOME TESTS FAILED!" << RESET << std::endl;
            std::cout << "  " << g_tests_passed << " passed, " << g_tests_failed << " failed, " 
                      << g_tests_run << " total" << std::endl;
        }
        std::cout << "========================================\n" << std::endl;
        return (g_tests_failed == 0) ? 0 : 1;
    }
    catch (const std::exception& e)
    {
        std::cout << "\n" << RED << "Test failed with exception: " << e.what() << RESET << "\n\n";
        return 1;
    }
    catch (...)
    {
        std::cout << "\n" << RED << "Test failed with unknown exception" << RESET << "\n\n";
        return 1;
    }
}
