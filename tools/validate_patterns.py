#!/usr/bin/env python3
"""
Validate pattern_analysis.txt for completeness.
Checks for missing patterns in the sequence.
"""

import re
import sys
import shutil
import os
from datetime import datetime


def validate_patterns(filename='../data/pattern_analysis.txt'):
    """Check for gaps in the pattern sequence."""
    
    print("Reading pattern file...")
    
    indices = []
    pattern = re.compile(r'spiral 0x([0-9a-f]+):')
    
    with open(filename, 'r') as f:
        for line_num, line in enumerate(f, 1):
            match = pattern.match(line.strip())
            if match:
                idx = int(match.group(1), 16)
                indices.append(idx)
    
    if not indices:
        print("ERROR: No patterns found in file!")
        return False
    
    # Sort indices (multithreading may write out of order)
    indices.sort()
    
    # Ignore last 1000 patterns (still being processed async)
    IGNORE_TAIL = 1000
    if len(indices) > IGNORE_TAIL:
        max_validated = indices[-IGNORE_TAIL]
        indices_to_check = [idx for idx in indices if idx < max_validated]
        print(f"Found {len(indices)} total patterns")
        print(f"Validating up to 0x{max_validated:x} ({len(indices_to_check)} patterns)")
        print(f"Ignoring last {IGNORE_TAIL} patterns (async buffer)")
    else:
        indices_to_check = indices
        print(f"Found {len(indices)} patterns (too few to ignore tail)")
    
    if not indices_to_check:
        print("Not enough patterns to validate yet.")
        return True
    
    print(f"Range: 0x{indices_to_check[0]:x} to 0x{indices_to_check[-1]:x}")
    print(f"Expected: {indices_to_check[-1] - indices_to_check[0] + 1} patterns\n")
    
    # Check for gaps and duplicates in validated range
    missing = []
    duplicates = []
    
    for i in range(len(indices_to_check) - 1):
        current = indices_to_check[i]
        next_val = indices_to_check[i + 1]
        
        # Check for duplicates
        if current == next_val:
            duplicates.append(current)
        
        # Check for gaps
        gap = next_val - current
        if gap > 1:
            missing.extend(range(current + 1, next_val))
    
    # Report findings
    print("=" * 70)
    print("VALIDATION RESULTS")
    print("=" * 70)
    
    if not missing and not duplicates:
        print("✓ PASS: No missing or duplicate patterns!")
        print(f"✓ Complete sequence from 0x{indices_to_check[0]:x} to 0x{indices_to_check[-1]:x}")
        return True
    
    if missing:
        print(f"\n✗ FAIL: {len(missing)} missing patterns!\n")
        
        # Show first 20 missing
        print("First missing patterns:")
        for idx in missing[:20]:
            print(f"  0x{idx:x}")
        
        if len(missing) > 20:
            print(f"  ... and {len(missing) - 20} more")
        
        # Find largest gap
        if len(missing) > 1:
            gaps = []
            gap_start = missing[0]
            gap_end = missing[0]
            
            for i in range(1, len(missing)):
                if missing[i] == gap_end + 1:
                    gap_end = missing[i]
                else:
                    gaps.append((gap_start, gap_end, gap_end - gap_start + 1))
                    gap_start = missing[i]
                    gap_end = missing[i]
            gaps.append((gap_start, gap_end, gap_end - gap_start + 1))
            
            gaps.sort(key=lambda x: x[2], reverse=True)
            
            print(f"\nLargest gaps:")
            for start, end, size in gaps[:5]:
                print(f"  0x{start:x} - 0x{end:x} ({size} patterns)")
    
    if duplicates:
        print(f"\n✗ WARNING: {len(duplicates)} duplicate patterns!\n")
        print("First duplicates:")
        for idx in duplicates[:20]:
            print(f"  0x{idx:x}")
        
        if len(duplicates) > 20:
            print(f"  ... and {len(duplicates) - 20} more")
    
    print()
    return False


def find_expected_next(filename='pattern_analysis.txt'):
    """Find the next pattern that should be processed."""
    
    indices = []
    pattern = re.compile(r'spiral 0x([0-9a-f]+):')
    
    with open(filename, 'r') as f:
        for line in f:
            match = pattern.match(line.strip())
            if match:
                indices.append(int(match.group(1), 16))
    
    if not indices:
        return 0
    
    indices.sort()
    
    # Find first gap
    for i in range(len(indices) - 1):
        if indices[i + 1] - indices[i] > 1:
            return indices[i] + 1
    
    # No gaps, return next after last
    return indices[-1] + 1


def main():
    if len(sys.argv) > 1:
        filename = sys.argv[1]
    else:
        filename = '../data/pattern_analysis.txt'
    
    try:
        # Create a snapshot copy to avoid reading a file that's being written
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        snapshot_file = f"pattern_analysis_snapshot_{timestamp}.txt"
        
        print(f"Creating snapshot: {snapshot_file}")
        shutil.copy2(filename, snapshot_file)
        print(f"Snapshot created ({os.path.getsize(snapshot_file)} bytes)\n")
        
        # Validate the snapshot
        is_valid = validate_patterns(snapshot_file)
        
        if not is_valid:
            next_idx = find_expected_next(snapshot_file)
            print(f"\nTo fill gaps, restart from: 0x{next_idx:x}")
        
        # Optionally clean up snapshot
        print(f"\nSnapshot saved as: {snapshot_file}")
        print("(You can delete this file when done)")
        
        # Return instead of sys.exit to avoid debugger exceptions
        return 0 if is_valid else 1
        
    except FileNotFoundError:
        print(f"ERROR: File '{filename}' not found!")
        return 1
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1


if __name__ == '__main__':
    exit_code = main()
    if exit_code != 0:
        print("\nValidation complete. Check output above for details.")
