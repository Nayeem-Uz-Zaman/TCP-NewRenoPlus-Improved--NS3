# Reproducing the main experiments

This document shows the minimum steps to reproduce the experiments and plots in this repo.

Prerequisites
- A Linux machine with a C++ toolchain (g++), Python 3, and CMake.
- Build ns-3 using the included `ns3` helper script.

Basic build

```bash
cd /home/nayeem/Ns3/ns-allinone-3.45/ns-3.45
./ns3 configure --enable-examples --enable-tests
./ns3 build
```

Run the paper topology simulation (example)

```bash
./ns3 run "newreno-plus-test --tcpVariant=TcpNewRenoPlusMod --errorRate=0.01 --scenario=multi --simulationTime=250 --outputFile=newreno_multimode_topology_results_v6.csv"
```

Notes
- Output CSV files are written to the current directory (see `outputFile` argument).
- Multiple helper scripts exist: `run_sweep.sh`, `run_wifi_sweep.sh`, `run_wpan_sweep.sh` for batch runs.
- Use the `plot_results.py` and other `plot_*.py` scripts to regenerate figures from CSV files.

Contact
- If you have issues reproducing, open an issue on the repo or contact the author.
