# DAG-Based Pattern Analysis Implementation

## Overview

Build a Directed Acyclic Graph (DAG) of canonical states to cache computation and skip redundant work when analyzing Game of Life patterns.

## Core Concepts

### Canonical State

A translation-invariant representation of a pattern:

- Width, height, bit data
- Hash for fast comparison
- Already implemented in `CanonicalState` struct

### State Cache Entry

```cpp
struct CachedCycleInfo {
    int64_t steps_to_cycle;    // How many steps from this state to enter cycle
    int64_t cycle_start_gen;   // Absolute generation where cycle starts
    int64_t period;            // Cycle period
    int64_t dx;                // Translation per cycle
    int64_t dy;
    uint64_t cycle_state_hash; // Hash of the state where cycle starts
};
```

## Data Structures

### Main Cache

```cpp
std::unordered_map<uint64_t, CachedCycleInfo> state_cache;
```

- Key: `CanonicalState.hash`
- Value: Cycle information from that state forward

### Collision Handling

Since hash collisions are possible:

```cpp
std::unordered_map<uint64_t, std::vector<CanonicalState>> hash_buckets;
```

Store full canonical states for hash collision resolution.

## Algorithm

### Phase 1: Pattern Evolution with Caching

```text
For each pattern index:
  1. Initialize pattern
  2. current_gen = 0
  3. path = []  // Track states we visit

  4. While not done:
     a. Get canonical_state = current pattern (translation-invariant)
     b. hash = canonical_state.hash

     c. Check cache:
        if hash in state_cache AND states match:
           // Cache hit!
           info = state_cache[hash]
           prefix = current_gen + info.steps_to_cycle
           period = info.period
           first_repeat = prefix + period
           dx = info.dx
           dy = info.dy

           // Backfill all states we visited on this path
           for (i, state) in enumerate(path):
              state_cache[state.hash] = {
                 steps_to_cycle: info.steps_to_cycle + (len(path) - i),
                 cycle_start_gen: info.cycle_start_gen,
                 period: info.period,
                 dx: info.dx,
                 dy: info.dy,
                 cycle_state_hash: info.cycle_state_hash
              }

           DONE - output results

     d. No cache hit - continue:
        path.append(canonical_state)
        step()
        current_gen++

     e. Check for cycle in current path (Floyd's algorithm)
        If cycle detected:
           - Compute prefix, period, dx, dy
           - Cache all states in path
           - DONE

     f. If current_gen > MAX_STEPS:
        - Output MAX_STEPS_REACHED
        - DONE (don't cache - might not be a real cycle)
```

### Phase 2: Cache Population

When a cycle is found at generation G with period P:

```text
For each state S at generation i in path (i < G):
  state_cache[S.hash] = {
    steps_to_cycle: G - i,
    cycle_start_gen: G,
    period: P,
    dx: dx,
    dy: dy,
    cycle_state_hash: cycle_state.hash
  }
```

## Performance Benefits

### Before Caching

- Pattern A: 100 steps to find cycle
- Pattern B: Reaches same state as A at step 50, then 50 more steps
- Total: 150 steps

### After Caching

- Pattern A: 100 steps, cache all states
- Pattern B: 50 steps, cache hit, done!
- Total: 100 steps (33% savings)

With many patterns converging to few attractors:

- First 100 patterns: ~10,000 steps total
- Next 100 patterns: ~2,000 steps (80% cache hit rate)

## Memory Considerations

### Storage Requirements

- Each canonical state: ~(width × height / 64) × 8 bytes + overhead
- Typical small pattern (10×10): ~16 bytes data + 40 bytes overhead = ~56 bytes
- Cache info: ~40 bytes
- Total per state: ~96 bytes

### Estimates

- 1 million unique states: ~100 MB
- 10 million unique states: ~1 GB

For 64-bit spiral patterns (2^64 possibilities), we won't store all, just visited states.

## Implementation Files

### New Files

- `StateCache.h` / `StateCache.cpp`: Cache data structure and lookup
- `CachedLifeAnalyzer.h` / `CachedLifeAnalyzer.cpp`: Main analysis loop with caching

### Modified Files

- `CanonicalState.h`: Add comparison operator for collision resolution
- `main.cpp`: Use `CachedLifeAnalyzer` instead of direct `LifeRunner`

## Edge Cases

### Hash Collisions

```cpp
bool states_equal(const CanonicalState& a, const CanonicalState& b) {
    return a.w == b.w &&
           a.h == b.h &&
           a.words == b.words;
}
```

Always verify full state equality when hash matches.

### Cycles vs Still Lifes

- Still life: prefix=0, period=1, state repeats itself
- Oscillator: period>1, cycles through states
- Cache both the same way

### Translation (Gliders)

- Track dx, dy per cycle
- Canonical state is translation-invariant
- Multiple translated versions → same canonical state
- Cache stores the abstract cycle, not position

## Future Optimizations

### 1. Persistent Cache

Save cache to disk, reload between runs:

```cpp
void save_cache(const std::string& filename);
void load_cache(const std::string& filename);
```

### 2. Parallel Processing

Multiple threads analyze different patterns, shared cache with mutex:

```cpp
std::mutex cache_mutex;
// Lock only for cache reads/writes, not during simulation
```

### 3. Incremental Saving

Periodically checkpoint cache to handle crashes:

```cpp
if (patterns_analyzed % 1000 == 0) {
    save_cache("state_cache_checkpoint.bin");
}
```

### 4. LRU Eviction

If memory constrained, keep most recently/frequently used states:

```cpp
std::unordered_map<uint64_t, CachedEntry> cache;
std::list<uint64_t> lru_list;
const size_t MAX_CACHE_SIZE = 10'000'000;
```

## Testing Strategy

### Unit Tests

1. Cache hit/miss detection
2. Correct cycle info retrieval
3. Hash collision handling
4. Path backfill correctness

### Integration Tests

1. Run with cache disabled, record results
2. Run with cache enabled, verify identical results
3. Measure speedup factor
4. Verify cache hit rate increases over time

### Benchmark Patterns

- Known stable patterns (should be instant after first)
- Known oscillators (period-2, period-3)
- Gliders (test translation tracking)
- Methuselahs (test long prefix caching)

## Expected Results

For 0x0 to 0x10000 (65536 patterns):

- Without cache: ~10-30 minutes
- With cache: ~2-5 minutes (assuming 70%+ hit rate after warmup)
- Cache size: ~50k-200k unique states (~5-20 MB)

Patterns with dx≠0 or dy≠0 should be very rare, making caching highly effective.
