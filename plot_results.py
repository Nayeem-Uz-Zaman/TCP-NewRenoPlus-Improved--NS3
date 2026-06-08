import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

# ── Load & clean ──────────────────────────────────────────────────────────────
df = pd.read_csv("real one_r.csv")
df = df.dropna(subset=["Scenario"])          # drop blank separator rows
df["ErrorRate"] = df["ErrorRate"].astype(float)
df["AvgThroughputMbps"] = df["AvgThroughputMbps"].astype(float)
df["AvgUniqueSegments"] = df["AvgUniqueSegments"].astype(float)
df = df[df["ErrorRate"] != 0.1]              # exclude 10% error rate

# ── Visuals ───────────────────────────────────────────────────────────────────
VARIANTS   = ["TcpNewReno", "TcpNewRenoPlus", "TcpNewRenoPlusMod"]
LABELS     = ["TCP NewReno", "TCP NewReno+", "TCP NewReno+ Mod"]
COLORS     = ["#e74c3c", "#2ecc71", "#3498db"]
MARKERS    = ["o", "s", "^"]
SCENARIOS  = [("single", "Single Flow  (Bottleneck = 7 Mbps)"),
              ("multi",  "Multi Flow   (Bottleneck = 12 Mbps)")]

error_rates = sorted(df["ErrorRate"].unique())
x = np.arange(len(error_rates))
x_labels = [str(e) for e in error_rates]

# ── Figure 1 – Average Throughput ─────────────────────────────────────────────
fig1, axes1 = plt.subplots(1, 2, figsize=(14, 5), sharey=False)
fig1.suptitle("Average Throughput vs. Error Rate", fontsize=15, fontweight="bold", y=1.01)

for ax, (scenario, title) in zip(axes1, SCENARIOS):
    sub = df[df["Scenario"] == scenario]
    for var, label, color, marker in zip(VARIANTS, LABELS, COLORS, MARKERS):
        row = sub[sub["TCPVariant"] == var].sort_values("ErrorRate")
        ax.plot(x, row["AvgThroughputMbps"].values,
                color=color, marker=marker, linewidth=2, markersize=7, label=label)

    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Avg Throughput (Mbps)", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.yaxis.set_minor_locator(mticker.AutoMinorLocator())
    ax.grid(True, which="major", linestyle="--", alpha=0.5)
    ax.grid(True, which="minor", linestyle=":",  alpha=0.3)
    ax.legend(fontsize=9)

fig1.tight_layout()
fig1.savefig("plot_throughput.png", dpi=150, bbox_inches="tight")
print("Saved: plot_throughput.png")

# ── Figure 2 – Average Unique Segments ───────────────────────────────────────
fig2, axes2 = plt.subplots(1, 2, figsize=(14, 5), sharey=False)
fig2.suptitle("Average Unique Segments Delivered vs. Error Rate",
              fontsize=15, fontweight="bold", y=1.01)

for ax, (scenario, title) in zip(axes2, SCENARIOS):
    sub = df[df["Scenario"] == scenario]
    for var, label, color, marker in zip(VARIANTS, LABELS, COLORS, MARKERS):
        row = sub[sub["TCPVariant"] == var].sort_values("ErrorRate")
        ax.plot(x, row["AvgUniqueSegments"].values,
                color=color, marker=marker, linewidth=2, markersize=7, label=label)

    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Avg Unique Segments", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.yaxis.set_minor_locator(mticker.AutoMinorLocator())
    ax.grid(True, which="major", linestyle="--", alpha=0.5)
    ax.grid(True, which="minor", linestyle=":",  alpha=0.3)
    ax.legend(fontsize=9)

fig2.tight_layout()
fig2.savefig("plot_segments.png", dpi=150, bbox_inches="tight")
print("Saved: plot_segments.png")

# ── Figure 3 – Grouped bar chart (throughput, both scenarios side by side) ────
fig3, axes3 = plt.subplots(1, 2, figsize=(14, 5))
fig3.suptitle("Throughput Comparison – Grouped Bar Chart",
              fontsize=15, fontweight="bold", y=1.01)

bar_width = 0.22
offsets = np.array([-1, 0, 1]) * bar_width

for ax, (scenario, title) in zip(axes3, SCENARIOS):
    sub = df[df["Scenario"] == scenario]
    for i, (var, label, color) in enumerate(zip(VARIANTS, LABELS, COLORS)):
        row = sub[sub["TCPVariant"] == var].sort_values("ErrorRate")
        ax.bar(x + offsets[i], row["AvgThroughputMbps"].values,
               width=bar_width, color=color, alpha=0.85, label=label,
               edgecolor="white", linewidth=0.6)

    ax.set_title(title, fontsize=11, pad=8)
    ax.set_xlabel("Error Rate", fontsize=10)
    ax.set_ylabel("Avg Throughput (Mbps)", fontsize=10)
    ax.set_xticks(x)
    ax.set_xticklabels(x_labels)
    ax.grid(True, axis="y", linestyle="--", alpha=0.5)
    ax.legend(fontsize=9)

fig3.tight_layout()
fig3.savefig("plot_bar_throughput.png", dpi=150, bbox_inches="tight")
print("Saved: plot_bar_throughput.png")

plt.show()
