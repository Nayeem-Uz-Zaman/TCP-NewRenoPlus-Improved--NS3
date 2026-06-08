#!/bin/bash

# Error Rate Sweep — Paper-faithful topology
# Runs both paper scenarios × both variants × all 4 error rates
#
#   Scenario "single" : 1 TCP flow,  7 Mbps bottleneck  (Table 2 in paper)
#   Scenario "multi"  : 2 TCP flows, 12 Mbps bottleneck (Table 3 in paper)
#   Error rates       : 1%, 2.5%, 5%, 10%
#   Variants          : TcpNewReno, TcpNewRenoPlus

RESULTS_FILE="newreno_multimode_topology_results_v5.csv"
ERROR_RATES=(0.01 0.025 0.05 0.10 0.15)
VARIANTS=("TcpNewReno" "TcpNewRenoPlus")
SCENARIOS=("single" "multi")
SIM_TIME=250

echo "============================================================"
echo " TCP NewReno vs TCP NewReno Plus — Full Paper Sweep"
echo " Scenarios : single (7 Mbps) | multi (12 Mbps)"
echo " Error Rates: 1% | 2.5% | 5% | 10%"
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

# ---- Run sweep -----------------------------------------------------------
total_runs=$(( ${#SCENARIOS[@]} * ${#ERROR_RATES[@]} * ${#VARIANTS[@]} ))
current_run=0
failed_runs=()

for scenario in "${SCENARIOS[@]}"; do
    echo ""
    echo "============================================================"
    if [ "$scenario" = "single" ]; then
        echo " SCENARIO: single  (1 TCP flow, 7 Mbps bottleneck — Table 2)"
    else
        echo " SCENARIO: multi   (2 TCP flows, 12 Mbps bottleneck — Table 3)"
    fi
    echo "============================================================"

    for rate in "${ERROR_RATES[@]}"; do
        pct=$(echo "$rate * 100" | bc)
        echo ""
        echo " -- Error Rate: ${pct}% --"

        for variant in "${VARIANTS[@]}"; do
            (( current_run++ ))
            echo ""
            echo "[Run ${current_run}/${total_runs}]  ${variant}  |  ${scenario}  |  ${pct}% error"
            echo "------------------------------------------------------------"

            ./ns3 run "scratch/newreno-plus-test \
                --tcpVariant=${variant} \
                --errorRate=${rate} \
                --scenario=${scenario} \
                --simulationTime=${SIM_TIME} \
                --udpDataRate=5.0"

            if [ $? -eq 0 ]; then
                echo "[+] OK"
            else
                echo "[!] FAILED"
                failed_runs+=("${variant}/${scenario}@${pct}%")
            fi
        done
    done
done

# ---- Summary -------------------------------------------------------------
echo ""
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
