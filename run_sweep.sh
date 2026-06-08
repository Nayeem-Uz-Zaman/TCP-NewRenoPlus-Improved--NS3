#!/bin/bash

# Parameter sweep automation script for TCP NewReno Plus evaluation
# This script runs both baseline TcpNewReno and TcpNewRenoPlus with various parameter combinations

echo "=========================================="
echo "TCP NewReno Plus Parameter Sweep Script"
echo "=========================================="

# Clean up previous results (optional - comment out if you want to append)
echo "Cleaning up previous results..."
rm -f threshold_aging_factor_comparison_results.csv

# Build the project first
echo "Building NS-3 project..."
./ns3 build
if [ $? -ne 0 ]; then
    echo "Build failed! Exiting..."
    exit 1
fi

echo "Starting parameter sweep..."

# Define error rates to test
error_rates=(0.01 0.025 0.05 .1 .15 .2 .25 .3)

# Define parameter combinations for TcpNewRenoPlus
thresholds=(0.1 0.2 0.25)
aging_factors=(0.5 0.75 0.875)

echo ""
echo "=========================================="
echo "Phase 1: Running TcpNewReno Baseline"
echo "=========================================="

# Run baseline with standard TcpNewReno
for err in "${error_rates[@]}"; do
    echo ""
    echo "Running TcpNewReno baseline - Error Rate: ${err}"
    echo "------------------------------------------"
    
    ./ns3 run "scratch/newreno-plus-test --tcpVariant=TcpNewReno --errorRate=${err} --tdrThreshold=0.1 --itrThreshold=0.25 --tdrAgingFactor=0.875 --itrAgingFactor=0.875"
    
    if [ $? -eq 0 ]; then
        echo "✓ TcpNewReno baseline completed for error rate ${err}"
    else
        echo "✗ TcpNewReno baseline failed for error rate ${err}"
    fi
done

echo ""
echo "=========================================="
echo "Phase 2: Running TcpNewRenoPlus Parameter Sweep"
echo "=========================================="

# Counter for progress tracking
total_runs=$((${#error_rates[@]} * ${#thresholds[@]} * ${#aging_factors[@]}))
current_run=0

# Run TcpNewRenoPlus with parameter combinations
for err in "${error_rates[@]}"; do
    for thresh in "${thresholds[@]}"; do
        for aging in "${aging_factors[@]}"; do
            ((current_run++))
            
            echo ""
            echo "Run ${current_run}/${total_runs}: TcpNewRenoPlus Configuration"
            echo "------------------------------------------------"
            echo "Error Rate:      ${err}"
            echo "TDR Threshold:   ${thresh}"
            echo "ITR Threshold:   ${thresh}"
            echo "TDR Aging:       ${aging}"
            echo "ITR Aging:       ${aging}"
            echo "------------------------------------------------"
            
            ./ns3 run "scratch/newreno-plus-test --tcpVariant=TcpNewRenoPlus --errorRate=${err} --tdrThreshold=${thresh} --itrThreshold=${thresh} --tdrAgingFactor=${aging} --itrAgingFactor=${aging}"
            
            if [ $? -eq 0 ]; then
                echo "✓ Configuration completed successfully"
            else
                echo "✗ Configuration failed"
            fi
        done
    done
done

echo ""
echo "=========================================="
echo "Parameter Sweep Complete!"
echo "=========================================="
echo "Results saved to: threshold_aging_factor_comparison_results.csv"
echo ""

# Display summary
if [ -f "threshold_aging_factor_comparison_results.csv" ]; then
    total_lines=$(wc -l < threshold_aging_factor_comparison_results.csv)
    data_lines=$((total_lines - 1))  # Subtract header line
    echo "Total simulations completed: ${data_lines}"
    echo "Expected simulations: $((${#error_rates[@]} + total_runs))"
    echo ""
    
    echo "Summary of results:"
    echo "-------------------"
    echo "Error Rates tested: ${error_rates[*]}"
    echo "Thresholds tested: ${thresholds[*]}" 
    echo "Aging factors tested: ${aging_factors[*]}"
    echo ""
    echo "You can now analyze the results in threshold_aging_factor_comparison_results.csv"
else
    echo "Warning: Results file not found!"
fi

echo "Script execution completed."