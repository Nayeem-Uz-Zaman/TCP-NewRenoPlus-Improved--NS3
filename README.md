# TCP-NewRenoPlus-Improved-NS3

An ns-3 project implementing and evaluating a modified TCP NewReno algorithm for wireless networks.

This repository includes:
- `scratch/newreno-plus-test.cc`: the main ns-3 simulation program for the paper topology.
- `plot_results.py`, `plot_reno_comparison.py`, `plot_wifi.py`, `plot_wpan.py`: plotting utilities.
- `report.pdf` / `paper.pdf`: final report and paper draft.
- `plots/`: selected generated figures.
- CSV results files from simulation runs.
- `RUN.md`: step-by-step reproduction guide.
- `SUMMARY.md`: concise project summary.

## Project focus
This work modifies TCP NewReno to better distinguish wireless random loss from congestion loss, then evaluates performance in mixed wired/wireless scenarios. The implementation compares three TCP variants:
- `TcpNewReno`
- `TcpNewRenoPlus`
- `TcpNewRenoPlusMod`

## Key contributions
- Implemented a modified TCP NewReno congestion control variant in ns-3.
- Reproduced the paper topology using a mixed wired and Wi-Fi network.
- Collected throughput and unique-segment metrics under varying error rates.
- Generated plots that compare the new algorithm with standard TCP variants.
- Packaged the experiment workflow for reproducibility.

## Repository layout
- `scratch/newreno-plus-test.cc` — main simulation program
- `src/` — ns-3 source tree and modified protocol code
- `plots/` — published figure images
- `report.tex`, `report.pdf` — full project report
- `paper.md`, `paper.pdf` — paper draft and write-up
- `RUN.md` — reproduction instructions
- `SUMMARY.md` — short project summary
- `scripts/` — helper shell scripts for simulation and plotting

## Quick start
```bash
cd /home/nayeem/Ns3/ns-allinone-3.45/ns-3.45
./ns3 configure --enable-examples --enable-tests
./ns3 build
./scripts/run_paper_topology.sh
./scripts/plot_results.sh
```

If you prefer direct command usage, you can also run:
```bash
./ns3 run "newreno-plus-test --tcpVariant=TcpNewRenoPlusMod --errorRate=0.01 --scenario=multi --simulationTime=250 --outputFile=newreno_multimode_topology_results_v6.csv"
python3 plot_results.py
```

## Reproducibility
Refer to `RUN.md` for detailed environment setup, build commands, and experiment workflows.

## License
The ns-3 code in this repository is distributed under the GNU GPL v2.0 only (`LICENSE`).
