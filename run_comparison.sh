#!/bin/bash

# Head-to-Head Comparison Script: TCP NewReno vs TCP NewReno Plus
# This script runs both protocols sequentially at each error rate for fair comparison

echo "=========================================="
echo "TCP NewReno vs TCP NewReno Plus"
echo "Head-to-Head Performance Comparison"
echo "=========================================="

# Clean up previous results (optional - comment out if you want to append)
echo "Cleaning up previous comparison results..."
rm -f tcpreno_and_tcprenoplus_comparison.csv

# Build the project first
echo "Building NS-3 project..."
./ns3 build
if [ $? -ne 0 ]; then
    echo "Build failed! Exiting..."
    exit 1
fi

echo "Starting head-to-head comparison..."

# Define error rates for comprehensive evaluation
ERROR_RATES=(0.15 0.2 0.25 0.3)

echo ""
echo "=========================================="
echo "Running Sequential Comparison"
echo "Error Rates: ${ERROR_RATES[*]}"
echo "=========================================="

# Counter for progress tracking
total_runs=$((${#ERROR_RATES[@]} * 2))  # 2 protocols per error rate
current_run=0

# Run both protocols sequentially at each error rate
for rate in "${ERROR_RATES[@]}"; do
    
    echo ""
    echo "========================================"
    echo "Error Rate: ${rate}"
    echo "========================================"
    
   
    # Run TcpNewRenoPlus (Optimized)  
    ((current_run++))
    echo ""
    echo "Run ${current_run}/${total_runs}: TCP NewReno Plus (Optimized)"
    echo "----------------------------------------"
    echo "Protocol:    TcpNewRenoPlus"
    echo "Error Rate:  ${rate}"
    echo "Thresholds:  TDR=0.20, ITR=0.10"
    echo "Aging:       0.875"
    echo "----------------------------------------"
    
    ./ns3 run "scratch/newreno-plus-test --tcpVariant=TcpNewRenoPlus --errorRate=${rate}"
    
    if [ $? -eq 0 ]; then
        echo "✓ TcpNewRenoPlus completed successfully"
    else
        echo "✗ TcpNewRenoPlus failed"
    fi
    
done

echo ""
echo "=========================================="
echo "Head-to-Head Comparison Complete!"
echo "=========================================="
echo "Results saved to: tcpreno_and_tcprenoplus_comparison.csv"
echo ""



echo ""
echo "Script execution completed."