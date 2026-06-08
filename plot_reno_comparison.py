"""
plot_reno_comparison.py
Compares TcpNewReno vs TcpNewRenoPlus (and TcpNewRenoPlusMod) from real one_r.csv.
Produces:
  - reno_throughput.png   : line charts – Avg Throughput vs Error Rate
  - reno_segments.png     : line charts – Avg Unique Segments vs Error Rate
  - reno_bar_compare.png  : grouped bar charts – side-by-side per error rate
  - reno_gain.png         : % gain of NewReno+ over NewReno per scenario
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

# ── Load & clean ──────────────────────────────────────────────────────────────
df = pd.read_csv("real one_r.csv")
df = df.dropna(subset=["Scenario"])
df["ErrorRate"]        = df["ErrorRate"].astype(float)
df["AvgThroughputMbps"]= df["AvgThroughputMbps"].astype(float)
df["AvgUniqueSegments"]= df["AvgUniqueSegments"].astype(float)

# ── Config ────────────────────────────────────────────────────────────────────
VARIANTS  = ["TcpNewReno", "TcpNewRenoPlus"]
LABELS    = ["TCP NewReno", "TCP NewReno+"]
COLORS    = ["#e74c3c", "#2ecc71"]
MARKERS   = ["o", "s"]
LINE_STYLES = ["-", "--"]

SCENARIOS = [("single", "Single Flow  (Bottleneck = 7 Mbps)"),
             ("multi",  "Multi Flow   (Bottleneck = 12 Mbps)")]

df = df[df["ErrorRate"] != 0.1]   # exclude 10% error rate
error_rates = sorted(df["ErrorRate"].unique())
x           = np.arange(len(error_rates))
x_labels    = [str(e) for e in error_rates]

# ─────────────────────────────────────────────────────────────────────────────
# Figure 1 – Average Throughput (line)
# ─────────────────────────────────────────────────────────────────────────────
fig1, axes1 = plt.subplots(1, 2, figsize=(14, 5))
fig1.suptitle("TCP NewReno vs NewReno+ — Average Throughput",
              fontsize=14, fontweight="bold")

for ax, (scenario, title) in zip(axes1, SCENARIOS):
    sub = df[df["Scenario"] == scenario]
    for var, label, color, marker, ls in zip(VARIANTS, LABELS, COLORS, MARKERS, LINE_STYLES):
        row = sub[sub["TCPVariant"] == var].sort_values("ErrorRate")
        if row.empty:
            continue
        ax.plot(x[:len(row)], row["AvgThroughputMbps"].values,
                color=color, marker=marker, linestyle=ls,
                linewidth=2.2, markersize=8, label=label)

    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Avg Throughput (Mbps)", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.yaxis.set_minor_locator(mticker.AutoMinorLocator())
    ax.grid(True, which="major", linestyle="--", alpha=0.5)
    ax.grid(True, which="minor", linestyle=":",  alpha=0.25)
    ax.legend(fontsize=9)

fig1.tight_layout()
fig1.savefig("reno_throughput.png", dpi=150, bbox_inches="tight")
print("Saved: reno_throughput.png")

# ─────────────────────────────────────────────────────────────────────────────
# Figure 2 – Average Unique Segments (line)
# ─────────────────────────────────────────────────────────────────────────────
fig2, axes2 = plt.subplots(1, 2, figsize=(14, 5))
fig2.suptitle("TCP NewReno vs NewReno+ — Avg Unique Segments Delivered",
              fontsize=14, fontweight="bold")

for ax, (scenario, title) in zip(axes2, SCENARIOS):
    sub = df[df["Scenario"] == scenario]
    for var, label, color, marker, ls in zip(VARIANTS, LABELS, COLORS, MARKERS, LINE_STYLES):
        row = sub[sub["TCPVariant"] == var].sort_values("ErrorRate")
        if row.empty:
            continue
        ax.plot(x[:len(row)], row["AvgUniqueSegments"].values,
                color=color, marker=marker, linestyle=ls,
                linewidth=2.2, markersize=8, label=label)

    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Avg Unique Segments", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.yaxis.set_minor_locator(mticker.AutoMinorLocator())
    ax.grid(True, which="major", linestyle="--", alpha=0.5)
    ax.grid(True, which="minor", linestyle=":",  alpha=0.25)
    ax.legend(fontsize=9)

fig2.tight_layout()
fig2.savefig("reno_segments.png", dpi=150, bbox_inches="tight")
print("Saved: reno_segments.png")

# ─────────────────────────────────────────────────────────────────────────────
# Figure 3 – Grouped Bar Chart (throughput per error rate)
# ─────────────────────────────────────────────────────────────────────────────
n_vars   = len(VARIANTS)
bar_w    = 0.22
offsets  = np.linspace(-(n_vars - 1) / 2, (n_vars - 1) / 2, n_vars) * bar_w

fig3, axes3 = plt.subplots(1, 2, figsize=(14, 5))
fig3.suptitle("TCP NewReno vs NewReno+ — Throughput (Grouped Bars)",
              fontsize=14, fontweight="bold")

for ax, (scenario, title) in zip(axes3, SCENARIOS):
    sub = df[df["Scenario"] == scenario]
    for idx, (var, label, color, offset) in enumerate(zip(VARIANTS, LABELS, COLORS, offsets)):
        row = sub[sub["TCPVariant"] == var].sort_values("ErrorRate")
        if row.empty:
            continue
        ax.bar(x[:len(row)] + offset, row["AvgThroughputMbps"].values,
               width=bar_w, color=color, label=label,
               edgecolor="white", linewidth=0.7, alpha=0.9)

    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Avg Throughput (Mbps)", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.yaxis.set_minor_locator(mticker.AutoMinorLocator())
    ax.grid(True, axis="y", linestyle="--", alpha=0.45)
    ax.legend(fontsize=9)

fig3.tight_layout()
fig3.savefig("reno_bar_compare.png", dpi=150, bbox_inches="tight")
print("Saved: reno_bar_compare.png")

# ─────────────────────────────────────────────────────────────────────────────
# Figure 4 – % Throughput Gain of NewReno+ over NewReno
# ─────────────────────────────────────────────────────────────────────────────
fig4, axes4 = plt.subplots(1, 2, figsize=(14, 5))
fig4.suptitle("Throughput Improvement Over TCP NewReno (%)",
              fontsize=14, fontweight="bold")

gain_variants = [("TcpNewRenoPlus", "TCP NewReno+", "#2ecc71", "s", "--")]

for ax, (scenario, title) in zip(axes4, SCENARIOS):
    sub     = df[df["Scenario"] == scenario].sort_values("ErrorRate")
    base    = sub[sub["TCPVariant"] == "TcpNewReno"].set_index("ErrorRate")["AvgThroughputMbps"]

    for var, label, color, marker, ls in gain_variants:
        row = sub[sub["TCPVariant"] == var].set_index("ErrorRate")["AvgThroughputMbps"]
        if row.empty:
            continue
        gain = ((row - base) / base * 100).dropna()
        ax.plot(x[:len(gain)], gain.values,
                color=color, marker=marker, linestyle=ls,
                linewidth=2.2, markersize=8, label=label)

    ax.axhline(0, color="gray", linewidth=1, linestyle=":")
    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Throughput Gain over NewReno (%)", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.yaxis.set_minor_locator(mticker.AutoMinorLocator())
    ax.grid(True, which="major", linestyle="--", alpha=0.5)
    ax.grid(True, which="minor", linestyle=":",  alpha=0.25)
    ax.legend(fontsize=9)

fig4.tight_layout()
fig4.savefig("reno_gain.png", dpi=150, bbox_inches="tight")
print("Saved: reno_gain.png")

plt.show()
