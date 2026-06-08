#!/bin/bash
# run_wifi_sweep.sh
#
# Sweeps all 4 parameter dimensions for the 802.11 mobile simulation.
# Each parameter is varied while the others are held at default values.
# Both TcpNewReno (baseline) and TcpNewRenoPlusMod (modified) are run
# for every combination so the results can be compared on the same graph.
#
# Defaults: nNodes=40, nFlows=20, pktPerSec=200, speed=10, simTime=100
#
# Output: wifi_mobile_results.csv  (all results appended, tcpVariant column
#         distinguishes which algorithm was used)
#
# Usage:  bash run_wifi_sweep.sh

set -e

SIM="wifi-mobile-sim"
OUTPUT="wifi_mobile_results.csv"
SIM_TIME=40

# Default values (held constant when not the swept parameter)
DEF_NODES=40
DEF_FLOWS=20
DEF_PPS=200
DEF_SPEED=10

VARIANTS=("TcpNewReno" "TcpNewRenoPlusMod")

# Remove old results so output is fresh
rm -f "$OUTPUT"

run_sim() {
    local variant="$1"
    local nodes="$2"
    local flows="$3"
    local pps="$4"
    local speed="$5"
    echo "  --> $variant | nodes=$nodes flows=$flows pps=$pps speed=$speed"
    ./ns3 run "$SIM \
        --tcpVariant=$variant \
        --nNodes=$nodes \
        --nFlows=$flows \
        --pktPerSec=$pps \
        --speed=$speed \
        --simTime=$SIM_TIME \
        --outputFile=$OUTPUT" > /dev/null 2>&1
}

TOTAL_RUNS=$(( 4 * 4 * 2 ))   # 4 sweeps × 4 node values + 5 others × 2 variants
RUN=0

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
echo " All $TOTAL_RUNS runs complete."
echo " Results saved to: $OUTPUT"
echo " Rows in CSV: $(wc -l < $OUTPUT)"
echo "========================================"

echo ""
echo "Generating comparison plots..."
python3 plot_wifi.py
echo "Plots saved to: plots/wifi/"
