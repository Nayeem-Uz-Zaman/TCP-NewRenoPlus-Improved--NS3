#!/bin/bash
# run_wifi_sweep_resume.sh
#
# Resumes the wifi sweep from wherever it left off.
# Before each run, checks if that exact combination already exists in the CSV.
# If found, it skips the run. New results are appended to the existing file.
#
# Safe to re-run multiple times — will never duplicate a row.

SIM="wifi-mobile-sim"
OUTPUT="wifi_mobile_results.csv"
SIM_TIME=40

DEF_NODES=40
DEF_FLOWS=20
DEF_PPS=200
DEF_SPEED=10

VARIANTS=("TcpNewReno" "TcpNewRenoPlusMod")

TOTAL_RUNS=$(( 4 * 5 * 2 ))
RUN=0
SKIPPED=0
DONE=0

# Write header only if file doesn't exist yet
if [ ! -f "$OUTPUT" ]; then
    echo "tcpVariant,nNodes,nFlows,pktPerSec,speed,throughput_mbps,avgDelay_ms,PDR_pct,dropRatio_pct,energy_J" > "$OUTPUT"
fi

# Check if a given combination already exists in the CSV.
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

# ── Sweep 1: Vary number of nodes ─────────────────────────────────────────
echo ""
echo "========================================"
echo " Sweep 1/4: Varying Number of Nodes"
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
echo "========================================"
for speed in 5 10 15 20 25; do
    for variant in "${VARIANTS[@]}"; do
        RUN=$((RUN + 1))
        echo "[$RUN/$TOTAL_RUNS] speed=$speed m/s"
        run_sim "$variant" "$DEF_NODES" "$DEF_FLOWS" "$DEF_PPS" "$speed"
    done
done

echo ""
echo "========================================"
echo " Sweep complete."
echo " Skipped (already done): $SKIPPED"
echo " Newly run:              $DONE"
echo " Results saved to:       $OUTPUT"
echo " Total rows in CSV:      $(wc -l < $OUTPUT)"
echo "========================================"

echo ""
echo "Generating comparison plots..."
python3 plot_wifi.py
echo "Plots saved to: plots/wifi/"
