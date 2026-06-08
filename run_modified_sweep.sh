#!/bin/bash

# Modified NewReno+ Sweep — TcpNewRenoPlusMod (Three-Factor Decision)
#
# Runs both scenarios × all 4 error rates for TcpNewRenoPlusMod only.
# Results are saved exclusively to newrenoplus_modified_v1.csv.
#
#   Scenario "single" : 1 TCP flow,  7 Mbps bottleneck  (Table 2 in paper)
#   Scenario "multi"  : 2 TCP flows, 12 Mbps bottleneck (Table 3 in paper)
#   Error rates       : 1%, 2.5%, 5%, 10%

RESULTS_FILE="newrenoplus_modified_v10.csv"
ERROR_RATES=(0.01 0.025 0.05 0.10)
VARIANT="TcpNewRenoPlusMod"
SCENARIOS=("single" "multi")
SIM_TIME=250

echo "============================================================"
echo " TcpNewRenoPlusMod — Three-Factor Modified NewReno+ Sweep"
echo "============================================================"
echo " Variant     : ${VARIANT}"
echo " Scenarios   : single (7 Mbps) | multi (12 Mbps)"
echo " Error Rates : 1% | 2.5% | 5% | 10%"
echo " Output      : ${RESULTS_FILE}"
echo "============================================================"

# ---- Build ---------------------------------------------------------------
echo ""
echo "[*] Building NS-3..."
./ns3 build
if [ $? -ne 0 ]; then
    echo "[!] Build failed. Exiting."
    exit 1
fi
echo "[+] Build successful."
echo ""

# ---- Run sweep -----------------------------------------------------------
total_runs=$(( ${#SCENARIOS[@]} * ${#ERROR_RATES[@]} ))
current_run=0
failed_runs=()

for scenario in "${SCENARIOS[@]}"; do
    echo "============================================================"
    if [ "$scenario" = "single" ]; then
        echo " SCENARIO: single  (1 TCP flow, 7 Mbps bottleneck — Table 2)"
    else
        echo " SCENARIO: multi   (2 TCP flows, 12 Mbps bottleneck — Table 3)"
    fi
    echo "============================================================"

    for rate in "${ERROR_RATES[@]}"; do
        pct=$(echo "$rate * 100" | bc)
        (( current_run++ ))

        echo ""
        echo "[Run ${current_run}/${total_runs}]  ${VARIANT}  |  ${scenario}  |  ${pct}% error"
        echo "------------------------------------------------------------"

        ./ns3 run "scratch/newreno-plus-test \
            --tcpVariant=${VARIANT} \
            --errorRate=${rate} \
            --scenario=${scenario} \
            --simulationTime=${SIM_TIME} \
            --udpDataRate=5.0 \
            --outputFile=${RESULTS_FILE}"

        if [ $? -eq 0 ]; then
            echo "[+] OK"
        else
            echo "[!] FAILED"
            failed_runs+=("${scenario}@${pct}%")
        fi
    done
    echo ""
done

# ---- Summary -------------------------------------------------------------
echo "============================================================"
echo " Sweep Complete"
echo "============================================================"
echo " Total runs : ${total_runs}"
echo " Failed     : ${#failed_runs[@]}"
if [ ${#failed_runs[@]} -gt 0 ]; then
    echo " Failed runs:"
    for f in "${failed_runs[@]}"; do
        echo "   - ${f}"
    done
fi
echo ""
echo " Results saved to: ${RESULTS_FILE}"
echo ""

# ---- Pretty-print CSV ----------------------------------------------------
if [ -f "${RESULTS_FILE}" ]; then
    echo "--- Results Preview ---"
    column -t -s',' "${RESULTS_FILE}"
    echo ""
fi
