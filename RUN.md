# Reproducing the TCP NewReno+ Experiments

This document describes how to build the project, run the main ns-3 simulation, and regenerate plots.

## Prerequisites
- Linux with a C++ toolchain (`g++` or `clang++`)
- Python 3
- CMake
- Required Python packages: `pandas`, `matplotlib`, `numpy`

Install Python packages with:
```bash
python3 -m pip install pandas matplotlib numpy
```

## Build ns-3
```bash
cd /home/nayeem/Ns3/ns-allinone-3.45/ns-3.45
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

## Run the main paper topology
The main simulation program is `scratch/newreno-plus-test.cc`.

Example command:
```bash
./ns3 run "newreno-plus-test --tcpVariant=TcpNewRenoPlusMod --errorRate=0.01 --scenario=multi --simulationTime=250 --outputFile=newreno_multimode_topology_results_v6.csv"
```

### Available options
- `--tcpVariant` : `TcpNewReno`, `TcpNewRenoPlus`, or `TcpNewRenoPlusMod`
- `--errorRate` : packet error rate on the wireless link (0.0 to 1.0)
- `--scenario` : `single` or `multi`
- `--simulationTime` : duration in seconds
- `--outputFile` : CSV filename to save results

## Run a sweep
A number of existing sweep scripts are included in the repository:
- `run_sweep.sh`
- `run_modified_sweep.sh`
- `run_wifi_sweep.sh`
- `run_wpan_sweep.sh`

These scripts perform batch runs over multiple parameter values. Review them before use.

## Helper scripts
This repo includes simple helper scripts for the main workflow:
- `scripts/run_paper_topology.sh` — run the main paper topology simulation
- `scripts/plot_results.sh` — regenerate plots with `plot_results.py`

Use these when you want a quick standard run.

## Generate plots
The most important plotting script is `plot_results.py`.

Example command:
```bash
python3 plot_results.py
```

If you want to visualize other CSV results, inspect the plotting scripts and update the input filename accordingly.

## Output locations
- Simulation CSVs: root repository files such as `newreno_multimode_topology_results.csv`
- Plots: `plots/`
- Reports: `report.pdf`, `paper.pdf`

## Notes
- Output CSV files are written to the current working directory by default.
- If a CSV file already exists, the simulation appends new rows rather than overwriting data.
- The `scratch/newreno-plus-test.cc` program uses `PacketSink` bytes received to compute throughput and segment counts.
