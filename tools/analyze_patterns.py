#!/usr/bin/env python3
"""
Pattern Analysis Tool for Game of Life
Analyzes pattern_analysis.txt and categorizes interesting patterns.
"""

import re
from collections import defaultdict
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class Pattern:
    idx: int
    prefix: Optional[int]
    period: Optional[int]
    first_repeat: Optional[int]
    dx: Optional[int]
    dy: Optional[int]
    timeout: bool

    @property
    def is_still_life(self):
        return not self.timeout and self.period == 1 and self.dx == 0 and self.dy == 0

    @property
    def is_oscillator(self):
        return (not self.timeout and self.period is not None and self.period > 1
                and self.dx == 0 and self.dy == 0)

    @property
    def is_spaceship(self):
        return not self.timeout and (self.dx != 0 or self.dy != 0)

    @property
    def is_methuselah(self):
        """Long-lived patterns (prefix > 100)"""
        return not self.timeout and self.prefix and self.prefix > 100

    @property
    def speed(self):
        """Speed for spaceships (cells per generation)"""
        if (not self.is_spaceship or self.period is None or self.period == 0
                or self.dx is None or self.dy is None):
            return 0
        return (abs(self.dx) + abs(self.dy)) / self.period

    def __str__(self):
        if self.timeout:
            return f"0x{self.idx:x}: TIMEOUT"
        return f"0x{self.idx:x}: prefix={self.prefix} period={self.period} dx={self.dx} dy={self.dy}"


def parse_file(filename='pattern_analysis.txt'):
    """Parse the pattern analysis file."""
    patterns = []

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue

            # Extract hex index
            match = re.match(r'spiral 0x([0-9a-f]+):', line)
            if not match:
                continue

            idx = int(match.group(1), 16)

            if 'MAX_STEPS_REACHED' in line:
                patterns.append(
                    Pattern(idx, None, None, None, None, None, True))
            else:
                # Parse the values
                prefix_match = re.search(r'prefix=(-?\d+)', line)
                period_match = re.search(r'period=(-?\d+)', line)
                first_repeat_match = re.search(r'first_repeat=(-?\d+)', line)
                dx_match = re.search(r'dx=(-?\d+)', line)
                dy_match = re.search(r'dy=(-?\d+)', line)

                if not all([prefix_match, period_match, first_repeat_match, dx_match, dy_match]):
                    continue

                assert prefix_match and period_match and first_repeat_match and dx_match and dy_match
                
                prefix = int(prefix_match.group(1))
                period = int(period_match.group(1))
                first_repeat = int(first_repeat_match.group(1))
                dx = int(dx_match.group(1))
                dy = int(dy_match.group(1))

                patterns.append(Pattern(idx, prefix, period,
                                first_repeat, dx, dy, False))

    return patterns


def categorize_patterns(patterns: List[Pattern]):
    """Categorize patterns by type."""
    categories = {
        'still_lifes': [],
        'oscillators': [],
        'spaceships': [],
        'methuselahs': [],
        'timeouts': []
    }

    for p in patterns:
        if p.timeout:
            categories['timeouts'].append(p)
        elif p.is_methuselah:
            categories['methuselahs'].append(p)
        elif p.is_spaceship:
            categories['spaceships'].append(p)
        elif p.is_oscillator:
            categories['oscillators'].append(p)
        elif p.is_still_life:
            categories['still_lifes'].append(p)

    return categories


def print_statistics(patterns: List[Pattern]):
    """Print overall statistics."""
    total = len(patterns)
    timeouts = sum(1 for p in patterns if p.timeout)
    non_timeout = [p for p in patterns if not p.timeout]

    print(f"=" * 70)
    print(f"PATTERN ANALYSIS STATISTICS")
    print(f"=" * 70)
    print(f"Total patterns analyzed: {total}")
    print(f"Timeouts: {timeouts} ({100*timeouts/total:.1f}%)")
    print(f"Completed: {len(non_timeout)} ({100*len(non_timeout)/total:.1f}%)")
    print()


def print_category_stats(categories):
    """Print statistics for each category."""
    print(f"PATTERN CATEGORIES")
    print(f"-" * 70)
    print(f"Still Lifes:  {len(categories['still_lifes']):5d}")
    print(f"Oscillators:  {len(categories['oscillators']):5d}")
    print(f"Spaceships:   {len(categories['spaceships']):5d}")
    print(f"Methuselahs:  {len(categories['methuselahs']):5d}")
    print(f"Timeouts:     {len(categories['timeouts']):5d}")
    print()


def find_interesting(patterns: List[Pattern], categories):
    """Find interesting patterns."""
    non_timeout = [p for p in patterns if not p.timeout]

    print(f"INTERESTING PATTERNS")
    print(f"=" * 70)

    # Longest-lived (methuselahs)
    if non_timeout:
        longest = max(
            non_timeout, key=lambda p: p.prefix if p.prefix is not None else 0)
        print(f"\nLongest-lived pattern (methuselah):")
        print(f"  {longest}")

    # Highest period oscillators
    oscillators = categories['oscillators']
    if oscillators:
        by_period = sorted(
            oscillators, key=lambda p: p.period if p.period is not None else 0, reverse=True)
        print(f"\nTop 5 highest-period oscillators:")
        for p in by_period[:5]:
            print(f"  {p}")

    # Spaceships
    spaceships = categories['spaceships']
    if spaceships:
        print(f"\nSpaceships:")

        # Group by speed and direction
        by_speed = sorted(spaceships, key=lambda p: p.speed, reverse=True)
        print(f"\n  Fastest spaceships:")
        for p in by_speed[:5]:
            print(f"    {p} (speed={p.speed:.3f}c)")

        # Orthogonal vs diagonal
        orthogonal = [p for p in spaceships if (p.dx == 0) != (p.dy == 0)]
        diagonal = [p for p in spaceships if p.dx != 0 and p.dy != 0]

        print(f"\n  Orthogonal: {len(orthogonal)}")
        if orthogonal:
            for p in orthogonal[:3]:
                print(f"    {p}")

        print(f"\n  Diagonal: {len(diagonal)}")
        if diagonal:
            for p in diagonal[:3]:
                print(f"    {p}")

        # Unusual directions
        unusual = [p for p in spaceships if p.dx is not None and p.dy is not None
                   and abs(p.dx) != abs(p.dy) and p.dx != 0 and p.dy != 0]
        if unusual:
            print(f"\n  Unusual directions (non-45° diagonal):")
            for p in unusual[:5]:
                print(f"    {p}")

    # Period breakdown
    if non_timeout:
        period_counts = defaultdict(int)
        for p in non_timeout:
            if p.period is not None:
                period_counts[p.period] += 1

        print(f"\nPeriod distribution:")
        for period in sorted(period_counts.keys())[:10]:
            count = period_counts[period]
            print(f"  Period {period:3d}: {count:5d} patterns")

    print()


def find_specific_types(patterns: List[Pattern]):
    """Look for specific well-known pattern types."""
    print(f"INTERESTING OSCILLATORS & SPACESHIPS")
    print(f"=" * 70)

    non_timeout = [p for p in patterns if not p.timeout]

    # Build period counts for oscillators (stationary only)
    oscillators_by_period = defaultdict(list)
    for p in non_timeout:
        if p.period is not None and p.period >= 2 and p.dx == 0 and p.dy == 0:
            oscillators_by_period[p.period].append(p)
    
    # Show oscillators by period (ascending order)
    if oscillators_by_period:
        print(f"\nOscillators by period (stationary patterns):")
        for period in sorted(oscillators_by_period.keys()):
            count = len(oscillators_by_period[period])
            print(f"  Period {period:2d}: {count:5d} patterns", end="")
            
            # Show examples for rare periods (3+)
            if period >= 3 and count <= 20:
                print(f"  (rare!)")
                for p in oscillators_by_period[period][:5]:
                    print(f"    {p}")
            else:
                print()
    
    # Spaceships
    spaceships = [p for p in non_timeout if p.dx != 0 or p.dy != 0]
    if spaceships:
        print(f"\nSpaceships (moving patterns):")
        
        # Group all spaceships by period
        ships_by_period = defaultdict(list)
        for p in spaceships:
            if p.period is not None:
                ships_by_period[p.period].append(p)
        
        # Show by period
        for period in sorted(ships_by_period.keys()):
            ships = ships_by_period[period]
            
            # Separate gliders from other ships
            gliders = [p for p in ships if p.dx is not None and p.dy is not None
                       and abs(p.dx) == 1 and abs(p.dy) == 1]
            other_ships = [p for p in ships if p not in gliders]
            
            if gliders and other_ships:
                print(f"  Period {period:2d}: {len(ships):5d} total ({len(gliders)} gliders, {len(other_ships)} other)")
            elif gliders:
                print(f"  Period {period:2d}: {len(gliders):5d} gliders")
            else:
                print(f"  Period {period:2d}: {len(other_ships):5d} spaceships")

    print()
def main():
    """Main entry point."""
    print("\nLoading patterns...")
    patterns = parse_file()

    if not patterns:
        print("No patterns found in ../data/pattern_analysis.txt")
        return

    print(f"Loaded {len(patterns)} patterns\n")

    # Overall statistics
    print_statistics(patterns)

    # Categorize
    categories = categorize_patterns(patterns)
    print_category_stats(categories)

    # Find interesting patterns
    find_interesting(patterns, categories)

    # Look for specific types
    find_specific_types(patterns)

if __name__ == '__main__':
    main()
