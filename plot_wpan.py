#!/usr/bin/env python3
"""
plot_wpan.py

Reads wpan_mobile_results.csv and generates 20 comparison graphs:
  4 varied parameters × 5 metrics
  Each graph shows TcpNewReno vs TcpNewRenoPlusMod on the same axes.

Output: plots/wpan/{metric}_vs_{param}.png  (20 files)

Usage:
    python3 plot_wpan.py [--csv wpan_mobile_results.csv] [--outdir plots/wpan]
"""

import argparse
import os
import sys

import matplotlib.pyplot as plt
import pandas as pd

# ── Configuration ──────────────────────────────────────────────────────────

METRICS = {
    "throughput_mbps": "Network Throughput (Mbps)",
    "avgDelay_ms":     "Average End-to-End Delay (ms)",
    "PDR_pct":         "Packet Delivery Ratio (%)",
    "dropRatio_pct":   "Packet Drop Ratio (%)",
    "energy_J":        "Total Energy Consumption (J)",
}

PARAMS = {
    "nNodes":    "Number of Nodes",
    "nFlows":    "Number of Flows",
    "pktPerSec": "Packets Per Second",
    "speed":     "Node Speed (m/s)",
}

# Defaults held constant in each sweep (used to filter rows for the sweep)
DEFAULTS = {
    "nNodes":    40,
    "nFlows":    20,
    "pktPerSec": 200,
    "speed":     10,
}

VARIANT_STYLES = {
    "TcpNewReno":       {"color": "#e74c3c", "marker": "o", "linestyle": "-",  "label": "TcpNewReno (baseline)"},
    "TcpNewRenoPlusMod":{"color": "#2980b9", "marker": "s", "linestyle": "--", "label": "TcpNewRenoPlusMod (modified)"},
}

# ── Main ───────────────────────────────────────────────────────────────────

def parse_args():
    p = argparse.ArgumentParser(description="Plot wpan-mobile-sim comparison graphs")
    p.add_argument("--csv",    default="wpan_mobile_results.csv", help="Input CSV file")
    p.add_argument("--outdir", default="plots/wpan",              help="Output directory for PNGs")
    return p.parse_args()


def load_data(csv_path: str) -> pd.DataFrame:
    if not os.path.exists(csv_path):
        print(f"ERROR: CSV file not found: {csv_path}", file=sys.stderr)
        print("Run 'bash run_wpan_sweep.sh' first to generate results.", file=sys.stderr)
        sys.exit(1)
    df = pd.read_csv(csv_path)
    print(f"Loaded {len(df)} rows from {csv_path}")
    print(f"Columns: {list(df.columns)}")
    return df


def plot_sweep(df: pd.DataFrame, param: str, metric: str, outdir: str):
    """
    For a given (param, metric) pair:
    - Filter rows where all OTHER params are at their default values
    - Group by tcpVariant and the swept param
    - Plot both variants on the same graph
    """
    mask = pd.Series([True] * len(df), index=df.index)
    for other_param, default_val in DEFAULTS.items():
        if other_param != param:
            mask &= (df[other_param] == default_val)

    subset = df[mask].copy()

    if subset.empty:
        print(f"  WARNING: No data for {param} vs {metric} after filtering. Skipping.")
        return

    fig, ax = plt.subplots(figsize=(8, 5))

    for variant, style in VARIANT_STYLES.items():
        vdata = subset[subset["tcpVariant"] == variant]
        if vdata.empty:
            continue
        grouped = vdata.groupby(param)[metric].mean().reset_index()
        grouped = grouped.sort_values(param)

        ax.plot(
            grouped[param],
            grouped[metric],
            color=style["color"],
            marker=style["marker"],
            linestyle=style["linestyle"],
            linewidth=2,
            markersize=7,
            label=style["label"],
        )

    ax.set_xlabel(PARAMS[param], fontsize=12)
    ax.set_ylabel(METRICS[metric], fontsize=12)
    ax.set_title(
        f"{METRICS[metric]}\nvs {PARAMS[param]}\n"
        f"(IEEE 802.15.4 Mobile, Single-hop, 50\u00d750 m)",
        fontsize=11, pad=10
    )
    ax.legend(fontsize=10)
    ax.grid(True, linestyle="--", alpha=0.5)
    ax.tick_params(labelsize=10)

    plt.tight_layout()

    filename = f"{metric}_vs_{param}.png"
    filepath = os.path.join(outdir, filename)
    plt.savefig(filepath, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"  Saved: {filepath}")


def main():
    args = parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    df = load_data(args.csv)

    total = len(PARAMS) * len(METRICS)
    done  = 0

    print(f"\nGenerating {total} comparison graphs → {args.outdir}/\n")

    for param in PARAMS:
        for metric in METRICS:
            done += 1
            print(f"[{done}/{total}] {metric} vs {param}")
            plot_sweep(df, param, metric, args.outdir)

    print(f"\nDone. {done} graphs saved to: {args.outdir}/")


if __name__ == "__main__":
    main()
