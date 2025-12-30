# Game of Life Pattern Analyzer

A high-performance multithreaded analyzer for Conway's Game of Life patterns using spiral binary encoding.

## Project Structure

```
gol/
├── src/                    # C++ source files
│   ├── main.cpp           # Main application with multithreading
│   ├── BitGrid.cpp        # Bit-packed grid implementation
│   ├── CanonicalState.cpp # Translation-invariant state comparison
│   ├── SpiralGenerator.cpp # Spiral binary pattern generation
│   └── LifeRunner.cpp     # Game of Life simulation with Floyd's cycle detection
├── include/               # C++ header files
│   ├── BitGrid.h
│   ├── BitUtils.h
│   ├── Bounds.h
│   ├── CanonicalState.h
│   ├── LifeRunner.h
│   ├── LifeTypes.h
│   └── SpiralGenerator.h
├── tools/                 # Python analysis tools
│   ├── visualize.py       # Interactive pattern visualizer
│   ├── analyze_patterns.py # Statistical analysis tool
│   ├── validate_patterns.py # Gap detection and validation
│   └── rle_to_spiral.py   # RLE format converter
├── build/                 # Build artifacts (executables, .obj, .pdb)
├── data/                  # Analysis output
│   └── pattern_analysis.txt # Pattern analysis results
├── docs/                  # Documentation
│   └── DAG_IMPLEMENTATION.md
└── .vscode/              # VS Code configuration
    ├── tasks.json        # Build tasks
    └── launch.json       # Debug configurations
```

## Features

- **Multithreaded Analysis**: Uses all available CPU cores for maximum performance (150+ patterns/sec)
- **Floyd's Cycle Detection**: Efficiently detects still lifes, oscillators, and spaceships
- **Translation-Invariant**: Correctly identifies gliders and moving patterns
- **Gap Filling**: Automatically detects and fills missing patterns on startup
- **Resume Support**: Continues from the last analyzed pattern
- **Progress Tracking**: Real-time progress reporting with ETA calculation

## Building

### Requirements
- MSVC compiler (Visual Studio)
- Python 3.x with NumPy and Matplotlib (for tools)

### Compile
Press `Ctrl+Shift+B` in VS Code or run:
```bash
cl.exe /O2 /Oi /GL /EHsc /nologo /DNDEBUG /Iinclude /Febuild\GameOfLife.exe src\main.cpp src\BitGrid.cpp src\CanonicalState.cpp src\SpiralGenerator.cpp src\LifeRunner.cpp /link /LTCG
```

## Running

### Main Analyzer
```bash
build\GameOfLife.exe
```
Output is written to `data/pattern_analysis.txt`

### Tools

**Visualize a Pattern:**
```bash
python tools\visualize.py
```

**Analyze Statistics:**
```bash
python tools\analyze_patterns.py
```

**Validate Completeness:**
```bash
python tools\validate_patterns.py
```

**Convert RLE to Spiral:**
```bash
python tools\rle_to_spiral.py
```

## Pattern Format

Each line in `data/pattern_analysis.txt` follows this format:
```
spiral 0xHEX: prefix=N period=N first_repeat=N dx=N dy=N
```

Where:
- `HEX`: Spiral binary index
- `prefix`: Steps before entering cycle
- `period`: Cycle length
- `first_repeat`: Generation of first cycle repeat
- `dx, dy`: Translation per cycle (0,0 = stationary)

Patterns that don't stabilize within 1000 steps timeout:
```
spiral 0xHEX: timeout
```

## Debug Configurations

Available in VS Code (F5):
- **Game of Life**: Run main analyzer
- **Visualizer**: Launch pattern visualizer
- **Analyze Patterns**: Run statistical analysis
- **Validate Patterns**: Check for gaps

## Performance

- **Optimization Flags**: `/O2 /Oi /GL /LTCG` (5-10x speedup)
- **Multithreading**: Hardware concurrency threads
- **Batched I/O**: 100 patterns per file write
- **No Console I/O**: Silent execution for maximum speed

## Interesting Patterns

The analyzer categorizes patterns into:
- **Still Lifes**: period=1, stationary
- **Oscillators**: period>1, stationary
- **Spaceships**: Moving patterns (gliders, etc.)
- **Methuselahs**: Long-lived patterns (prefix>100)
