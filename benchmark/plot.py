"""
High-Resolution Publication-Quality Benchmark Plotting Suite
Generates 300 DPI benchmark plots for:
1. Part 1: Multi-Client Throughput Scalability
2. Part 2: Case 1 (Abort & Wipe) vs Case 2 (Session Resumption) under Probabilistic Faults
3. Part 2 Enhanced: Concurrency Speedup with Multi-Connection TCP Streaming
"""

import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker

# Configure matplotlib for publication-quality output
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'Helvetica', 'sans-serif']
plt.rcParams['axes.edgecolor'] = '#333333'
plt.rcParams['axes.linewidth'] = 1.0

# Determine results directory
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..'))
RESULTS_DIR = os.path.join(REPO_ROOT, 'Results')

if not os.path.exists(RESULTS_DIR):
    os.makedirs(RESULTS_DIR, exist_ok=True)


def plot_part1():
    csv_path = os.path.join(RESULTS_DIR, 'Part1_throughput.csv')
    if not os.path.exists(csv_path):
        print(f"[WARN] {csv_path} not found, skipping Part 1 plot.")
        return

    df = pd.read_csv(csv_path)
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=300)

    color_primary = '#1a5276'
    color_accent = '#2980b9'

    ax.plot(df['Clients'], df['Throughput_MBps'],
            marker='o', markersize=9, linewidth=2.5,
            color=color_primary, markerfacecolor=color_accent,
            markeredgecolor='white', markeredgewidth=2,
            label='Multi-threaded Server Throughput')

    # Data value callouts
    for _, row in df.iterrows():
        c, tp = int(row['Clients']), float(row['Throughput_MBps'])
        ax.annotate(f"{tp:.1f} MB/s",
                    xy=(c, tp), xytext=(0, 10),
                    textcoords="offset points", ha='center', va='bottom',
                    fontsize=9.5, fontweight='bold', color='#1a5276',
                    bbox=dict(boxstyle='round,pad=0.25', facecolor='#eaf2f8', edgecolor='#aed6f1', alpha=0.9))

    ax.set_title('Part 1: Multi-Client Throughput Scalability', fontsize=14, fontweight='bold', pad=15, color='#1c2833')
    ax.set_xlabel('Number of Concurrent Clients', fontsize=11, fontweight='bold', labelpad=10, color='#2c3e50')
    ax.set_ylabel('Aggregated Throughput (MB/s)', fontsize=11, fontweight='bold', labelpad=10, color='#2c3e50')

    ax.set_xticks(df['Clients'])
    ax.xaxis.set_major_formatter(ticker.FormatStrFormatter('%d'))
    ax.grid(True, linestyle='--', alpha=0.5, color='#bdc3c7')
    ax.set_axisbelow(True)

    y_max = df['Throughput_MBps'].max() * 1.18
    ax.set_ylim(0, y_max)

    ax.legend(loc='lower right', frameon=True, facecolor='#ffffff', edgecolor='#d5dbdb', fontsize=10)
    plt.tight_layout()

    out_path = os.path.join(RESULTS_DIR, 'Part1_throughput.png')
    plt.savefig(out_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"[OK] Generated: {out_path} ({os.path.getsize(out_path):,} bytes)")


def plot_part2_cases():
    csv_path = os.path.join(RESULTS_DIR, 'Part2_throughput.csv')
    if not os.path.exists(csv_path):
        print(f"[WARN] {csv_path} not found, skipping Part 2 plot.")
        return

    df = pd.read_csv(csv_path)
    fig, ax = plt.subplots(figsize=(9.5, 5.5), dpi=300)

    c1_color = '#c0392b'  # Crimson Red for failure/wipe
    c2_color = '#27ae60'  # Emerald Green for session resumption

    ax.plot(df['FailProb'], df['Case1_Throughput_MBps'],
            marker='o', markersize=8.5, linewidth=2.4, linestyle='--',
            color=c1_color, markerfacecolor='#e74c3c', markeredgecolor='white', markeredgewidth=1.8,
            label='Case 1: Unmanaged Session (Abort & Wipe)')

    ax.plot(df['FailProb'], df['Case2_Throughput_MBps'],
            marker='s', markersize=8.5, linewidth=2.4, linestyle='-',
            color=c2_color, markerfacecolor='#2ecc71', markeredgecolor='white', markeredgewidth=1.8,
            label='Case 2: Stateful Session Checkpointing')

    # Data value callouts for Case 1
    for _, row in df.iterrows():
        p, c1 = float(row['FailProb']), float(row['Case1_Throughput_MBps'])
        ax.annotate(f"{c1:.2f} MB/s",
                    xy=(p, c1), xytext=(0, -16 if c1 < 50 else -18),
                    textcoords="offset points", ha='center', va='top',
                    fontsize=8.5, fontweight='bold', color=c1_color,
                    bbox=dict(boxstyle='round,pad=0.2', facecolor='#fdedec', edgecolor='#f5b7b1', alpha=0.9))

    # Data value callouts for Case 2
    for _, row in df.iterrows():
        p, c2 = float(row['FailProb']), float(row['Case2_Throughput_MBps'])
        ax.annotate(f"{c2:.2f} MB/s",
                    xy=(p, c2), xytext=(0, 10),
                    textcoords="offset points", ha='center', va='bottom',
                    fontsize=8.5, fontweight='bold', color=c2_color,
                    bbox=dict(boxstyle='round,pad=0.2', facecolor='#eafaf1', edgecolor='#a9dfbf', alpha=0.9))

    ax.set_title('Part 2: Fault-Tolerance Resilience (Case 1 vs Case 2)', fontsize=14, fontweight='bold', pad=15, color='#1c2833')
    ax.set_xlabel('Synthetic Fault Probability ($P_{fail}$)', fontsize=11, fontweight='bold', labelpad=10, color='#2c3e50')
    ax.set_ylabel('Effective Throughput (MB/s)', fontsize=11, fontweight='bold', labelpad=10, color='#2c3e50')

    ax.set_xticks(df['FailProb'])
    ax.grid(True, linestyle='--', alpha=0.5, color='#bdc3c7')
    ax.set_axisbelow(True)

    y_max = max(df['Case1_Throughput_MBps'].max(), df['Case2_Throughput_MBps'].max()) * 1.18
    ax.set_ylim(0, y_max)

    ax.legend(loc='upper right', frameon=True, facecolor='#ffffff', edgecolor='#d5dbdb', fontsize=10)
    plt.tight_layout()

    out_path = os.path.join(RESULTS_DIR, 'Part2_Cases_throughput.png')
    plt.savefig(out_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"[OK] Generated: {out_path} ({os.path.getsize(out_path):,} bytes)")


def plot_part2_enhanced():
    csv_path = os.path.join(RESULTS_DIR, 'Part2_Enhanced_throughput.csv')
    if not os.path.exists(csv_path):
        print(f"[WARN] {csv_path} not found, skipping Part 2 Enhanced plot.")
        return

    df = pd.read_csv(csv_path)
    fig, ax = plt.subplots(figsize=(9, 5.5), dpi=300)

    color_primary = '#6c3483'  # Royal Purple
    color_accent = '#8e44ad'

    ax.plot(df['Connections'], df['Throughput_MBps'],
            marker='^', markersize=9.5, linewidth=2.5,
            color=color_primary, markerfacecolor=color_accent,
            markeredgecolor='white', markeredgewidth=2,
            label='Parallel Multi-TCP Workers ($P_{fail} = 0.05$)')

    # Data value callouts
    for _, row in df.iterrows():
        c, tp = int(row['Connections']), float(row['Throughput_MBps'])
        ax.annotate(f"{tp:.2f} MB/s",
                    xy=(c, tp), xytext=(0, 10),
                    textcoords="offset points", ha='center', va='bottom',
                    fontsize=9.5, fontweight='bold', color=color_primary,
                    bbox=dict(boxstyle='round,pad=0.25', facecolor='#f4ecf7', edgecolor='#d2b4de', alpha=0.9))

    # Add speedup annotation between 1 and 8 connections
    if len(df) >= 4:
        tp_1 = df.loc[df['Connections'] == 1, 'Throughput_MBps'].values[0]
        tp_8 = df.loc[df['Connections'] == 8, 'Throughput_MBps'].values[0]
        speedup = tp_8 / tp_1
        ax.text(0.05, 0.85, f"Concurrency Speedup: {speedup:.1f}x\n(1 to 8 parallel streams)",
                transform=ax.transAxes, fontsize=10.5, fontweight='bold',
                color='#4a235a', bbox=dict(boxstyle='round,pad=0.5', facecolor='#ebdef0', edgecolor='#bb8fce', alpha=0.9))

    ax.set_title('Part 2 Enhanced: Concurrency Scaling under 5% Faults', fontsize=14, fontweight='bold', pad=15, color='#1c2833')
    ax.set_xlabel('Number of Parallel TCP Worker Connections', fontsize=11, fontweight='bold', labelpad=10, color='#2c3e50')
    ax.set_ylabel('Useful Data Throughput (MB/s)', fontsize=11, fontweight='bold', labelpad=10, color='#2c3e50')

    ax.set_xticks(df['Connections'])
    ax.xaxis.set_major_formatter(ticker.FormatStrFormatter('%d'))
    ax.grid(True, linestyle='--', alpha=0.5, color='#bdc3c7')
    ax.set_axisbelow(True)

    y_max = df['Throughput_MBps'].max() * 1.25
    ax.set_ylim(0, y_max)

    ax.legend(loc='lower right', frameon=True, facecolor='#ffffff', edgecolor='#d5dbdb', fontsize=10)
    plt.tight_layout()

    out_path = os.path.join(RESULTS_DIR, 'Part2_Enhanced_throughput.png')
    plt.savefig(out_path, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"[OK] Generated: {out_path} ({os.path.getsize(out_path):,} bytes)")


if __name__ == '__main__':
    print(f"Generating publication-quality benchmark plots in: {RESULTS_DIR}")
    plot_part1()
    plot_part2_cases()
    plot_part2_enhanced()
    print("All benchmark plots generated successfully.")
