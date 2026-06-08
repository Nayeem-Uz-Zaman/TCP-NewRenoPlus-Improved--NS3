#!/bin/bash
# run_wpan_sweep.sh
#
# Parameter sweep for the IEEE 802.15.4 (LR-WPAN) mobile simulation.
# Stack: LrWpan → SixLowPan → IPv6 + RIPng → TCP
#
# Both TcpNewReno (baseline) and TcpNewRenoPlusMod (modified) are run
# for every combination so results can be compared on the same graphs.
#
# Defaults: nNodes=40, nFlows=20, pktPerSec=200, speed=10, simTime=30
# Nodes swept: 20, 40, 60, 80  (100 excluded — hardware constraint)
# pktPerSec:   100, 200, 300, 400, 500  (deliberate overload of 250 kbps link)
#
# Safe to re-run — skips combinations already present in the CSV.
#
# Output: wpan_mobile_results.csv
# Usage:  bash run_wpan_sweep.sh

set -e

SIM="wpan-mobile-sim"
OUTPUT="wpan_mobile_results.csv"
SIM_TIME=30

DEF_NODES=40
DEF_FLOWS=20
DEF_PPS=200
DEF_SPEED=10

VARIANTS=("TcpNewReno" "TcpNewRenoPlusMod")

TOTAL_RUNS=$(( 4 * 5 * 2 ))   # 4 sweeps × 5 values × 2 variants = 40
RUN=0
SKIPPED=0
DONE=0

# Write CSV header only if file doesn't exist yet
if [ ! -f "$OUTPUT" ]; then
    echo "tcpVariant,nNodes,nFlows,pktPerSec,speed,throughput_mbps,avgDelay_ms,PDR_pct,dropRatio_pct,energy_J" > "$OUTPUT"
fi

# Check if a combination already exists in the CSV (exact match, no clamping)
already_done() {
    local variant="$1" nodes="$2" flows="$3" pps="$4" speed="$5"
    grep -q "^${variant},${nodes},${flows},${pps},${speed}," "$OUTPUT" 2>/dev/null
}

run_sim() {
    local variant="$1" nodes="$2" flows="$3" pps="$4" speed="$5"
    if already_done "$variant" "$nodes" "$flows" "$pps" "$speed"; then
        echo "  [SKIP] $variant | nodes=$nodes flows=$flows pps=$pps speed=$speed (already done)"
        SKIPPED=$((SKIPPED + 1))
        return
    fi
    echo "  [RUN ] $variant | nodes=$nodes flows=$flows pps=$pps speed=$speed"
    ./ns3 run "$SIM \
        --tcpVariant=$variant \
        --nNodes=$nodes \
        --nFlows=$flows \
        --pktPerSec=$pps \
        --speed=$speed \
        --simTime=$SIM_TIME \
        --outputFile=$OUTPUT" > /dev/null 2>&1
    DONE=$((DONE + 1))
}

# ── Build first ────────────────────────────────────────────────────────────
echo "========================================"
echo " Building NS-3..."
echo "========================================"
./ns3 build
echo " Build complete."
echo ""

# ── Sweep 1: Vary number of nodes ─────────────────────────────────────────
echo "========================================"
echo " Sweep 1/4: Varying Number of Nodes"
echo " (20, 40, 60, 80 — 100 excluded)"
echo "========================================"
for nodes in 20 40 60 80; do
    for variant in "${VARIANTS[@]}"; do
        RUN=$((RUN + 1))
        echo "[$RUN/$TOTAL_RUNS] nNodes=$nodes"
        run_sim "$variant" "$nodes" "$DEF_FLOWS" "$DEF_PPS" "$DEF_SPEED"
    done
done

# ── Sweep 2: Vary number of flows ─────────────────────────────────────────
echo ""
echo "========================================"
echo " Sweep 2/4: Varying Number of Flows"
echo " (10, 20, 30, 40, 50)"
echo "========================================"
for flows in 10 20 30 40 50; do
    for variant in "${VARIANTS[@]}"; do
        RUN=$((RUN + 1))
        echo "[$RUN/$TOTAL_RUNS] nFlows=$flows"
        run_sim "$variant" "$DEF_NODES" "$flows" "$DEF_PPS" "$DEF_SPEED"
    done
done

# ── Sweep 3: Vary packets per second ──────────────────────────────────────
echo ""
echo "========================================"
echo " Sweep 3/4: Varying Packets Per Second"
echo " (100, 200, 300, 400, 500)"
echo "========================================"
for pps in 100 200 300 400 500; do
    for variant in "${VARIANTS[@]}"; do
        RUN=$((RUN + 1))
        echo "[$RUN/$TOTAL_RUNS] pktPerSec=$pps"
        run_sim "$variant" "$DEF_NODES" "$DEF_FLOWS" "$pps" "$DEF_SPEED"
    done
done

# ── Sweep 4: Vary node speed ──────────────────────────────────────────────
echo ""
echo "========================================"
echo " Sweep 4/4: Varying Node Speed"
echo " (5, 10, 15, 20, 25 m/s)"
echo "========================================"
for speed in 5 10 15 20 25; do
    for variant in "${VARIANTS[@]}"; do
        RUN=$((RUN + 1))
        echo "[$RUN/$TOTAL_RUNS] speed=$speed m/s"
        run_sim "$variant" "$DEF_NODES" "$DEF_FLOWS" "$DEF_PPS" "$speed"
    done
done

# ── Summary ────────────────────────────────────────────────────────────────
echo ""
echo "========================================"
echo " Sweep Complete"
echo "========================================"
echo " Skipped (already done) : $SKIPPED"
echo " Newly run              : $DONE"
echo " Results saved to       : $OUTPUT"
echo " Total rows in CSV      : $(wc -l < $OUTPUT)"
echo "========================================"

echo ""
echo "Generating comparison plots..."
python3 plot_wpan.py
echo "Plots saved to: plots/wpan/"
