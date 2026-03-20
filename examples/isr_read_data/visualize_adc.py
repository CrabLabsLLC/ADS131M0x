"""
Plot ADS131M0x ADC data pasted from idf.py monitor output.

Usage:
    python visualize_adc.py
    Then paste the monitor output (DATA START through DATA END + stats).
    Press Enter on an empty line when done.
"""

import re
import sys

import matplotlib.pyplot as plt


def read_input():
    """Read pasted monitor output, return (samples, stats_line)."""
    print("Paste monitor output below (empty line to finish):\n")

    samples = []
    collecting = False
    stats_line = None

    while True:
        try:
            line = input()
        except EOFError:
            break

        if line.strip() == "":
            break

        if "DATA START" in line:
            collecting = True
            samples.clear()
            continue

        if "DATA END" in line:
            collecting = False
            continue

        if "Elapsed" in line and "Sampling rate" in line:
            stats_line = line.strip()
            continue

        if collecting:
            try:
                samples.append(int(line.strip()))
            except ValueError:
                pass

    return samples, stats_line


def parse_stats(stats_line):
    """Extract elapsed_us and sampling_rate from the stats log line."""
    if not stats_line:
        return None, None

    m = re.search(r"Elapsed:\s*(\d+)\s*us.*Sampling rate:\s*([\d.]+)\s*Hz", stats_line)
    if m:
        return int(m.group(1)), float(m.group(2))
    return None, None


def plot_waveform(samples, elapsed_us, sampling_rate):
    """Plot the captured ADC waveform."""
    n = len(samples)
    if n == 0:
        print("No samples to plot.")
        return

    if sampling_rate and sampling_rate > 0:
        dt = 1.0 / sampling_rate
        t = [i * dt * 1000 for i in range(n)]  # milliseconds
        x_label = "Time (ms)"
    else:
        t = list(range(n))
        x_label = "Sample Index"

    title = f"ADS131M0x CH0 — {n} samples"
    if sampling_rate:
        title += f" @ {sampling_rate:.1f} Hz"
    if elapsed_us:
        title += f"  ({elapsed_us / 1000:.1f} ms)"

    fig, ax = plt.subplots(figsize=(12, 5))
    ax.plot(t, samples, linewidth=0.6)
    ax.set_xlabel(x_label)
    ax.set_ylabel("Raw ADC Code (24-bit signed)")
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    plt.show()


def main():
    samples, stats_line = read_input()
    elapsed_us, sampling_rate = parse_stats(stats_line)

    print(f"\nParsed {len(samples)} samples")
    if stats_line:
        print(f"Stats: {stats_line}")

    plot_waveform(samples, elapsed_us, sampling_rate)


if __name__ == "__main__":
    main()
