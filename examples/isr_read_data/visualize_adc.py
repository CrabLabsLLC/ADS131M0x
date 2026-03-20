"""
Capture ADC data from ESP32 via idf.py monitor and plot the waveform.

Usage:
    python visualize_adc.py                  # auto-detect COM port
    python visualize_adc.py --port COM7      # explicit port
    python visualize_adc.py --baud 115200    # explicit baud rate
"""

import argparse
import re
import subprocess
import sys

import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt


def find_esp_port():
    """Auto-detect the first USB-serial port likely connected to an ESP32."""
    for port in serial.tools.list_ports.comports():
        desc = (port.description or "").lower()
        if any(k in desc for k in ("cp210", "ch340", "ftdi", "usb", "uart")):
            return port.device
    return None


def capture_data(port, baud):
    """
    Open the serial port, wait for DATA START / DATA END markers,
    and return (samples, stats_line).
    """
    ser = serial.Serial(port, baud, timeout=1)
    print(f"Listening on {port} @ {baud} baud  (waiting for DATA START ...)")

    samples = []
    collecting = False
    stats_line = None

    try:
        while True:
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()

            if "DATA START" in line:
                collecting = True
                samples.clear()
                print(">>> DATA START received, collecting samples...")
                continue

            if "DATA END" in line:
                collecting = False
                print(f">>> DATA END received, got {len(samples)} samples")
                # Read the next few lines to grab timing stats
                for _ in range(5):
                    raw = ser.readline()
                    if not raw:
                        continue
                    sline = raw.decode("utf-8", errors="replace").strip()
                    if "Elapsed" in sline and "Sampling rate" in sline:
                        stats_line = sline
                        print(f">>> {stats_line}")
                        break
                break

            if collecting:
                try:
                    samples.append(int(line))
                except ValueError:
                    pass  # skip non-integer log lines
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        ser.close()

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
    parser = argparse.ArgumentParser(description="Capture & plot ADS131M0x ADC data from ESP32")
    parser.add_argument("--port", type=str, default=None, help="Serial port (e.g. COM7)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default 115200)")
    args = parser.parse_args()

    port = args.port or find_esp_port()
    if not port:
        print("ERROR: No serial port found. Specify with --port COM7", file=sys.stderr)
        sys.exit(1)

    samples, stats_line = capture_data(port, args.baud)
    elapsed_us, sampling_rate = parse_stats(stats_line)
    plot_waveform(samples, elapsed_us, sampling_rate)


if __name__ == "__main__":
    main()
