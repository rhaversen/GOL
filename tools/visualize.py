#!/usr/bin/env python3
"""
Game of Life Pattern Visualizer
Visualize spiral binary patterns evolving according to Conway's Game of Life rules
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.widgets import Button, TextBox
import sys


def spiral_positions(bits):
    """Generate spiral coordinates for up to 'bits' positions"""
    positions = []
    x, y = 0, 0
    dx, dy = 1, 0
    seg_len = 1
    seg_pass = 0
    steps = 0

    positions.append((x, y))

    for _ in range(1, bits):
        x += dx
        y += dy
        positions.append((x, y))

        steps += 1
        if steps == seg_len:
            steps = 0
            dx, dy = -dy, dx
            seg_pass += 1
            if seg_pass == 2:
                seg_pass = 0
                seg_len += 1

    return positions


def create_pattern(index, bits=64, pad=2):
    """Create a pattern from a spiral binary index"""
    positions = spiral_positions(bits)

    # Get live cells
    live_cells = []
    for i, pos in enumerate(positions):
        if i < bits and (index >> i) & 1:
            live_cells.append(pos)

    if not live_cells:
        # Empty pattern - return minimal grid
        return np.zeros((1 + 2*pad, 1 + 2*pad), dtype=bool)

    # Find bounds
    xs = [x for x, y in live_cells]
    ys = [y for x, y in live_cells]
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)

    # Create grid with padding
    width = maxx - minx + 1 + 2*pad
    height = maxy - miny + 1 + 2*pad

    grid = np.zeros((height, width), dtype=bool)

    # Place cells
    ox = pad - minx
    oy = pad - miny
    for x, y in live_cells:
        grid[y + oy, x + ox] = True

    return grid


def step_life(grid, margin=2):
    """Perform one generation of Conway's Game of Life"""
    # Add margin if needed
    h, w = grid.shape

    # Check if we need to expand
    if np.any(grid[0, :]) or np.any(grid[-1, :]) or np.any(grid[:, 0]) or np.any(grid[:, -1]):
        new_h = h + 2 * margin
        new_w = w + 2 * margin
        new_grid = np.zeros((new_h, new_w), dtype=bool)
        new_grid[margin:margin+h, margin:margin+w] = grid
        grid = new_grid
        h, w = new_h, new_w

    # Count neighbors
    neighbors = np.zeros((h, w), dtype=int)
    for dy in [-1, 0, 1]:
        for dx in [-1, 0, 1]:
            if dx == 0 and dy == 0:
                continue
            neighbors += np.roll(np.roll(grid, dy, axis=0), dx, axis=1)

    # Apply rules
    # Birth: exactly 3 neighbors
    # Survival: 2 or 3 neighbors
    new_grid = (neighbors == 3) | (grid & (neighbors == 2))

    return new_grid


class PatternVisualizer:
    def __init__(self, pattern_id=0x1d, max_gens=1000):
        self.pattern_id = pattern_id
        self.max_gens = max_gens

        self.grid = create_pattern(pattern_id)
        self.generation = 0
        self.running = False

        # Setup plot - simplified
        self.fig, self.ax = plt.subplots(figsize=(10, 10))
        plt.subplots_adjust(bottom=0.15)
        if self.fig.canvas.manager:
            self.fig.canvas.manager.set_window_title('Game of Life Visualizer')

        self.im = self.ax.imshow(
            self.grid, cmap='binary', interpolation='nearest', animated=True)
        self.ax.set_xticks([])
        self.ax.set_yticks([])
        self.update_title()

        # Controls - simpler layout
        ax_input = plt.axes((0.1, 0.08, 0.15, 0.04))
        ax_load = plt.axes((0.26, 0.08, 0.06, 0.04))
        ax_run = plt.axes((0.1, 0.02, 0.08, 0.04))
        ax_stop = plt.axes((0.19, 0.02, 0.08, 0.04))
        ax_step = plt.axes((0.28, 0.02, 0.08, 0.04))
        ax_reset = plt.axes((0.37, 0.02, 0.08, 0.04))
        ax_speed = plt.axes((0.55, 0.02, 0.1, 0.04))

        self.text_input = TextBox(ax_input, 'ID: ', initial=f'{pattern_id:x}')
        self.btn_load = Button(ax_load, 'Load')
        self.btn_load.on_clicked(
            lambda e: self.change_pattern(self.text_input.text))

        self.btn_run = Button(ax_run, 'Run')
        self.btn_run.on_clicked(lambda e: setattr(self, 'running', True))

        self.btn_stop = Button(ax_stop, 'Stop')
        self.btn_stop.on_clicked(lambda e: setattr(self, 'running', False))

        self.btn_step = Button(ax_step, 'Step')
        self.btn_step.on_clicked(self.step_once)

        self.btn_reset = Button(ax_reset, 'Reset')
        self.btn_reset.on_clicked(self.reset)

        self.text_speed = TextBox(ax_speed, 'Speed: ', initial='200')

    def update_title(self):
        pop = np.sum(self.grid)
        self.fig.suptitle(
            f'0x{self.pattern_id:x} | Gen: {self.generation} | Pop: {pop}', fontsize=12)

    def step_once(self, event):
        if self.generation < self.max_gens and np.sum(self.grid) > 0:
            self.grid = step_life(self.grid)
            self.generation += 1
            self.im.set_data(self.grid)
            self.update_title()
            self.fig.canvas.draw_idle()

    def reset(self, event):
        self.grid = create_pattern(self.pattern_id)
        self.generation = 0
        self.running = False
        self.im.set_data(self.grid)
        self.update_title()
        self.fig.canvas.draw_idle()

    def change_pattern(self, text):
        try:
            text = text.strip().lower().lstrip('0x')
            new_id = int(text, 16)
            self.pattern_id = new_id
            self.reset(None)
            self.text_input.set_val(f'{new_id:x}')
        except ValueError:
            pass

    def animate(self, frame):
        if self.running and self.generation < self.max_gens and np.sum(self.grid) > 0:
            self.grid = step_life(self.grid)
            self.generation += 1
            self.im.set_data(self.grid)
            self.update_title()
        return [self.im]

    def show(self):
        try:
            interval = max(50, int(self.text_speed.text))
        except:
            interval = 200

        self.anim = animation.FuncAnimation(
            self.fig, self.animate, interval=interval, blit=True, cache_frame_data=False
        )
        plt.show()


def main():
    if len(sys.argv) > 1:
        try:
            text = sys.argv[1].strip().lower()
            if text.startswith('0x'):
                text = text[2:]
            pattern_id = int(text, 16)
        except ValueError:
            print(f"Invalid pattern ID: {sys.argv[1]}")
            print("Usage: python visualize.py [pattern_id]")
            print("Example: python visualize.py 0x4a")
            sys.exit(1)
    else:
        # Default to an interesting pattern
        pattern_id = 0x1d  # An oscillator from the data

    viz = PatternVisualizer(pattern_id, max_gens=10000)
    viz.show()


if __name__ == '__main__':
    main()
