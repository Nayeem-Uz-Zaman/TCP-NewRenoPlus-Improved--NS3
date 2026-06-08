# Summary — TCP NewReno+ Modified (Project)

This repository contains an ns-3 implementation and evaluation of a modified
TCP NewReno algorithm intended to improve performance in wireless environments.

What to find here
- `scratch/newreno-plus-test.cc` — simulation program that reproduces the paper's topology.
- `plot_results.py`, `plot_reno_comparison.py`, `plot_wifi.py` — plotting utilities.
- `paper.pdf` / `report.pdf` — the written report and paper draft.
- `plots/` — generated figures used in the report.
- CSV files with experiment outputs (root) — e.g., `newreno_multimode_topology_results.csv`.

Highlights
- Experiments compare `TcpNewReno`, `TcpNewRenoPlus`, and `TcpNewRenoPlusMod` under
  different wireless error rates and topologies.
- The repo includes scripts to run parameter sweeps and regenerate publication figures.

See `RUN.md` for reproduction instructions and the main `README.md` for a project overview.
