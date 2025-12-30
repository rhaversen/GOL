#!/usr/bin/env python3
"""
Convert RLE pattern to spiral hex encoding.
Usage: python rle_to_spiral.py <pattern.rle>
"""

import sys

def parse_rle(rle_string):
    """Parse RLE format and return list of (x, y) live cells."""
    lines = rle_string.strip().split('\n')
    
    # Skip header comments
    pattern_lines = []
    for line in lines:
        line = line.strip()
        if line.startswith('#'):
            continue
        if line.startswith('x'):
            continue
        pattern_lines.append(line)
    
    rle = ''.join(pattern_lines).rstrip('!')
    
    cells = []
    x, y = 0, 0
    count = ''
    
    for char in rle:
        if char.isdigit():
            count += char
        elif char == 'b':  # Dead cells
            n = int(count) if count else 1
            x += n
            count = ''
        elif char == 'o':  # Live cells
            n = int(count) if count else 1
            for _ in range(n):
                cells.append((x, y))
                x += 1
            count = ''
        elif char == '$':  # End of line
            n = int(count) if count else 1
            y += n
            x = 0
            count = ''
    
    return cells

def spiral_position(index):
    """Get (x, y) position for spiral bit index."""
    if index == 0:
        return (0, 0)
    
    x, y = 0, 0
    dx, dy = 1, 0
    seg_len = 1
    seg_pass = 0
    steps = 0
    
    for k in range(1, index + 1):
        x += dx
        y += dy
        
        if k == index:
            return (x, y)
        
        steps += 1
        if steps == seg_len:
            steps = 0
            ndx, ndy = -dy, dx
            dx, dy = ndx, ndy
            seg_pass += 1
            if seg_pass == 2:
                seg_pass = 0
                seg_len += 1
    
    return (x, y)

def cells_to_spiral_hex(cells):
    """Convert list of (x, y) cells to spiral hex encoding."""
    # Normalize to origin
    if not cells:
        return 0x0, 0, 0
    
    min_x = min(x for x, y in cells)
    min_y = min(y for x, y in cells)
    
    normalized = [(x - min_x, y - min_y) for x, y in cells]
    
    # Build spiral position lookup
    spiral_map = {}
    for bit in range(64):
        pos = spiral_position(bit)
        spiral_map[pos] = bit
    
    # Try different offsets to find best fit
    best_hex = None
    best_count = 0
    
    for offset_x in range(-10, 11):
        for offset_y in range(-10, 11):
            hex_val = 0
            count = 0
            
            for x, y in normalized:
                test_pos = (x + offset_x, y + offset_y)
                if test_pos in spiral_map:
                    bit = spiral_map[test_pos]
                    hex_val |= (1 << bit)
                    count += 1
            
            if count > best_count:
                best_count = count
                best_hex = hex_val
    
    return best_hex, best_count, len(normalized)

def main():
    if len(sys.argv) < 2:
        print("Usage: python rle_to_spiral.py <rle_string>")
        print("\nExample RLE for Mazing:")
        print('x = 12, y = 8, rule = B3/S23')
        print('2bo6bo$2bo6bo$2bo6bo$obo6bob2o$2obo6bobo$2bo6bo$2bo6bo$2bo6bo!')
        return
    
    rle = ' '.join(sys.argv[1:])
    
    cells = parse_rle(rle)
    print(f"Parsed {len(cells)} live cells")
    
    if len(cells) > 64:
        print(f"WARNING: Pattern has {len(cells)} cells, but only 64 bits available!")
    
    hex_val, matched, total = cells_to_spiral_hex(cells)
    
    print(f"\nBest match: 0x{hex_val:x}")
    print(f"Matched {matched}/{total} cells")
    
    if matched < total:
        print(f"\nWARNING: Only {matched} out of {total} cells fit in 64-bit spiral!")
        print("Pattern may be too large or wrong orientation for this encoding.")

if __name__ == '__main__':
    main()
