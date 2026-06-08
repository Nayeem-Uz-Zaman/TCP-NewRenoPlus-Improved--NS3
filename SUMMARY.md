# Project Summary — TCP NewReno+ Modified (ns-3)

This repository demonstrates a modified TCP NewReno algorithm designed for wireless network performance improvement.

## What this project contains
- A custom ns-3 simulation program: `scratch/newreno-plus-test.cc`
- Evaluation of three TCP variants on mixed wired/wireless topologies
- Plotting and data analysis scripts
- A written project report and paper draft

## Main results
- Evaluates throughput and unique segment delivery under wireless packet error rates
- Compares `TcpNewReno`, `TcpNewRenoPlus`, and `TcpNewRenoPlusMod`
- Shows how the modified algorithm reacts less aggressively to wireless loss, helping preserve throughput

## Key files
- `scratch/newreno-plus-test.cc`
- `plot_results.py`
- `report.pdf`
- `paper.pdf`
- `RUN.md`
- `plots/`

## Running the project
1. Build ns-3: `./ns3 configure --enable-examples --enable-tests && ./ns3 build`
2. Run the simulation: `./ns3 run "newreno-plus-test --tcpVariant=TcpNewRenoPlusMod --errorRate=0.01 --scenario=multi --simulationTime=250 --outputFile=newreno_multimode_topology_results_v6.csv"`
3. Generate plots: `python3 plot_results.py`

## Why it matters
This project is a portfolio-ready implementation of protocol research that blends simulation, data analysis, and report writing.
