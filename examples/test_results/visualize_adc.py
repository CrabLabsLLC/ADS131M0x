"""
Plot ADS131M0x ADC data from a text file.

Supports:
  - GAIN COMPARE blocks  → side-by-side top/bottom sine wave plots
  - SWEEP CONFIG blocks   → one subplot per OSR/GAIN combo

Usage:
    python visualize_adc.py [filepath]
    If no filepath is provided, it defaults to adc_test_results.txt
"""

import re
import sys
import os

import matplotlib.pyplot as plt


def read_file(filepath):
    """Read monitor output from a file, return list of (header, samples, stats) tuples."""
    print(f"Reading from {filepath}...\n")

    blocks = []
    current_header = None
    current_samples = []
    current_stats = None
    collecting = False

    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line in f:
                stripped = line.strip()
                if stripped == "":
                    continue

                # Detect block headers
                gain_match = re.search(r"GAIN COMPARE:.*", stripped)
                sweep_match = re.search(r"SWEEP CONFIG:.*", stripped)
                if gain_match or sweep_match:
                    current_header = gain_match.group(0) if gain_match else sweep_match.group(0)
                    current_samples = []
                    current_stats = None
                    continue

                if "DATA START" in stripped:
                    collecting = True
                    current_samples = []
                    continue

                if "DATA END" in stripped:
                    collecting = False
                    blocks.append((current_header or f"Block {len(blocks)+1}", list(current_samples), current_stats))
                    continue

                if "Elapsed" in stripped and "Sampling rate" in stripped:
                    current_stats = stripped
                    # Attach stats to the most recent block
                    if blocks:
                        header, samples, _ = blocks[-1]
                        blocks[-1] = (header, samples, stripped)
                    continue

                if collecting:
                    # Support both one-per-line and comma-separated formats
                    for token in stripped.split(","):
                        token = token.strip()
                        if token:
                            try:
                                current_samples.append(int(token))
                            except ValueError:
                                pass
    except FileNotFoundError:
        print(f"Error: Could not find {filepath}")
        sys.exit(1)

    return blocks


def parse_stats(stats_line):
    """Extract elapsed_us and sampling_rate from a stats log line."""
    if not stats_line:
        return None, None
    m = re.search(r"Elapsed:\s*(\d+)\s*us.*Sampling rate:\s*([\d.]+)\s*Hz", stats_line)
    if m:
        return int(m.group(1)), float(m.group(2))
    return None, None


def plot_gain_comparison(blocks, out_dir):
    """Plot gain comparison blocks as top/bottom subplots."""
    fig, axes = plt.subplots(len(blocks), 1, figsize=(14, 4 * len(blocks)), sharex=True)
    if len(blocks) == 1:
        axes = [axes]

    for ax, (header, samples, stats_line) in zip(axes, blocks):
        n = len(samples)
        if n == 0:
            continue

        elapsed_us, sampling_rate = parse_stats(stats_line)
        if sampling_rate and sampling_rate > 0:
            dt = 1.0 / sampling_rate
            t = [i * dt * 1000 for i in range(n)]
            x_label = "Time (ms)"
        else:
            t = list(range(n))
            x_label = "Sample Index"

        ax.plot(t, samples, linewidth=0.8, color='#2077B4')
        ax.set_ylabel("Raw ADC Code")
        ax.set_title(header, fontsize=11, fontweight='bold')
        ax.grid(True, alpha=0.3)

        if stats_line:
            ax.text(0.99, 0.97, f"{n} samples", transform=ax.transAxes,
                    ha='right', va='top', fontsize=9, color='gray')

    axes[-1].set_xlabel(x_label)
    fig.suptitle("Gain Comparison — 500 Hz Sine on AIN0", fontsize=13, fontweight='bold')
    fig.tight_layout()
    
    out_path = os.path.join(out_dir, "gain_comparison.png")
    plt.savefig(out_path, dpi=150)
    print(f"Saved plot to {out_path}")
    # plt.show() # Uncomment if you want interactive window


def plot_sweep(blocks, out_dir):
    """Plot sweep blocks in a grid."""
    n_blocks = len(blocks)
    cols = min(3, n_blocks)
    rows = (n_blocks + cols - 1) // cols

    fig, axes = plt.subplots(rows, cols, figsize=(6 * cols, 4 * rows), squeeze=False)

    for idx, (header, samples, stats_line) in enumerate(blocks):
        ax = axes[idx // cols][idx % cols]
        n = len(samples)
        if n == 0:
            continue

        elapsed_us, sampling_rate = parse_stats(stats_line)
        if sampling_rate and sampling_rate > 0:
            dt = 1.0 / sampling_rate
            t = [i * dt * 1000 for i in range(n)]
            x_label = "Time (ms)"
        else:
            t = list(range(n))
            x_label = "Sample Index"

        ax.plot(t, samples, linewidth=0.6, color='#2077B4')
        ax.set_title(header, fontsize=9, fontweight='bold')
        ax.set_ylabel("Raw Code")
        ax.set_xlabel(x_label)
        ax.grid(True, alpha=0.3)

        info_parts = [f"{n} pts"]
        if sampling_rate:
            info_parts.append(f"{sampling_rate:.0f} Hz")
        if elapsed_us:
            info_parts.append(f"{elapsed_us/1000:.1f} ms")
        ax.text(0.99, 0.97, "  ".join(info_parts), transform=ax.transAxes,
                ha='right', va='top', fontsize=8, color='gray')

    # Hide unused subplots
    for idx in range(n_blocks, rows * cols):
        axes[idx // cols][idx % cols].set_visible(False)

    fig.suptitle("Parameter Sweep — Signal Characterization", fontsize=13, fontweight='bold')
    fig.tight_layout()
    
    out_path = os.path.join(out_dir, "parameter_sweep.png")
    plt.savefig(out_path, dpi=150)
    print(f"Saved plot to {out_path}")
    # plt.show() # Uncomment if you want interactive window


def main():
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
    else:
        # Default to the workspace relative path
        filepath = os.path.join(os.path.dirname(os.path.dirname(__file__)), r".claude\docs\adc_test_results.txt")

    blocks = read_file(filepath)

    if not blocks:
        print("No data blocks found.")
        return

    print(f"Parsed {len(blocks)} data block(s):")
    for header, samples, stats in blocks:
        print(f"  {header}: {len(samples)} samples")

    out_dir = os.path.dirname(os.path.abspath(filepath))

    # Separate gain-compare blocks from sweep blocks
    gain_blocks = [(h, s, st) for h, s, st in blocks if "GAIN" in h and "COMPARE" in h]
    sweep_blocks = [(h, s, st) for h, s, st in blocks if "SWEEP" in h]

    if gain_blocks:
        plot_gain_comparison(gain_blocks, out_dir)

    if sweep_blocks:
        plot_sweep(sweep_blocks, out_dir)

    # Fallback: plot anything that didn't match either category
    other = [b for b in blocks if b not in gain_blocks and b not in sweep_blocks]
    if other:
        plot_gain_comparison(other, out_dir)


if __name__ == "__main__":
    main()
