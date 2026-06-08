# RUN.md — Build, Run & Reproduce Guide

Complete step-by-step instructions to build NS-3, run all three simulations, and regenerate all result plots from scratch.

---

## 📋 Prerequisites

### System Dependencies (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install -y \
    build-essential g++ cmake python3 python3-pip git \
    libsqlite3-dev libgsl-dev libgtk-3-dev \
    libxml2-dev libboost-all-dev
```

### Python Dependencies

```bash
pip3 install matplotlib pandas numpy
```

### NS-3 Version

This project was built and tested with **NS-3 v3.45** (the version in this repository). No other version is guaranteed to work.

---

## 🔨 Step 1: Build NS-3

From inside the `ns-3.45/` directory:

```bash
# Configure with examples and tests enabled
./ns3 configure --enable-examples --enable-tests

# Build all modules (takes several minutes on first run)
./ns3 build
```

> **Note:** If the build fails, check that all system dependencies above are installed. NS-3 will print which optional dependency is missing.

---

## 🧪 Step 2: Simulation 1 — Wired/Wireless Hybrid (Paper Topology)

### Single run (one variant, one error rate)

```bash
./ns3 run "newreno-plus-test \
    --tcpVariant=TcpNewRenoPlusMod \
    --errorRate=0.01 \
    --scenario=multi \
    --simulationTime=250 \
    --outputFile=my_results.csv"
```

#### All command-line options:

| Option | Default | Description |
|--------|---------|-------------|
| `--tcpVariant` | `TcpNewReno` | One of: `TcpNewReno`, `TcpNewRenoPlus`, `TcpNewRenoPlusMod` |
| `--errorRate` | `0.01` | Wireless packet error rate (0.0 – 1.0) |
| `--scenario` | `multi` | `single` (1 TCP flow, 7 Mbps) or `multi` (2 flows, 12 Mbps) |
| `--simulationTime` | `250.0` | Simulation duration in seconds |
| `--udpDataRate` | `5.0` | UDP background traffic rate in Mbps |
| `--outputFile` | `results.csv` | Output CSV filename |
| `--verbose` | `false` | Enable detailed NS-3 logging |

### Reproduce all paper results (automated sweep)

```bash
# All 3 variants × 2 scenarios × 4 error rates (24 runs × ~4 min each)
bash run_error_sweep.sh

# Extended sweep for TcpNewRenoPlusMod with varied parameters
bash run_modified_sweep.sh

# 2-variant comparison (NewReno vs NewReno+)
bash run_comparison.sh
```

---

## 📡 Step 3: Simulation 2 — WiFi Mobile MANET (802.11b)

### Single run

```bash
./ns3 run "wifi-mobile-sim \
    --tcpVariant=TcpNewRenoPlusMod \
    --nNodes=40 \
    --nFlows=20 \
    --pktPerSec=200 \
    --speed=10 \
    --simTime=40 \
    --routing=OLSR \
    --outputFile=wifi_results.csv"
```

#### All command-line options:

| Option | Default | Description |
|--------|---------|-------------|
| `--tcpVariant` | `TcpNewRenoPlusMod` | `TcpNewReno` or `TcpNewRenoPlusMod` |
| `--nNodes` | `40` | Total number of mobile nodes |
| `--nFlows` | `20` | Number of concurrent TCP flows |
| `--pktPerSec` | `200` | Packets per second per flow |
| `--speed` | `10.0` | Node speed in m/s (Random Waypoint) |
| `--simTime` | `40.0` | Simulation time in seconds |
| `--routing` | `OLSR` | Routing protocol: `OLSR` or `AODV` |
| `--outputFile` | `wifi_mobile_results.csv` | Output CSV |

### Reproduce all WiFi sweep results

```bash
# Sweeps 4 parameters × 4–5 values × 2 variants = ~40 runs
bash run_wifi_sweep.sh
```

---

## 🌐 Step 4: Simulation 3 — WPAN / IoT (802.15.4 · 6LoWPAN)

### Single run

```bash
./ns3 run "wpan-mobile-sim \
    --tcpVariant=TcpNewRenoPlusMod \
    --nNodes=40 \
    --nFlows=20 \
    --pktPerSec=200 \
    --speed=10 \
    --simTime=30 \
    --outputFile=wpan_results.csv"
```

#### All command-line options:

| Option | Default | Description |
|--------|---------|-------------|
| `--tcpVariant` | `TcpNewRenoPlusMod` | `TcpNewReno` or `TcpNewRenoPlusMod` |
| `--nNodes` | `40` | Total number of nodes in the PAN |
| `--nFlows` | `20` | Number of TCP over 6LoWPAN flows |
| `--pktPerSec` | `200` | Packets per second per flow |
| `--speed` | `10.0` | Node speed in m/s |
| `--simTime` | `30.0` | Simulation time in seconds |
| `--outputFile` | `wpan_mobile_results.csv` | Output CSV |

### Reproduce all WPAN sweep results

```bash
bash run_wpan_sweep.sh
```

---

## 📊 Step 5: Generate All Plots

```bash
# Wired/wireless 3-variant comparison (reads Final_simulation_results.csv or real one_r.csv)
python3 plot_results.py

# WiFi MANET sweeps — generates 20 charts in plots/wifi/
python3 plot_wifi.py

# WPAN sweeps — generates 20 charts in plots/wpan/
python3 plot_wpan.py

# 2-variant comparison (NewReno vs NewReno+)
python3 plot_reno_comparison.py
```

All plots are saved as high-resolution PNGs (150 DPI) in their respective `plots/` subdirectories.

---

## 📁 Output Files

| File | Contents |
|------|---------|
| `Final_simulation_results.csv` | 3-variant × 4 error rates × 2 scenarios |
| `wifi_mobile_results.csv` | WiFi MANET sweep across 4 parameters |
| `wpan_mobile_results.csv` | WPAN sweep across 4 parameters |
| `plots/newReno_vs_newRenoPlus_newrenoplusmod/` | 3 comparison charts |
| `plots/wifi/` | 20 WiFi sweep plots |
| `plots/wpan/` | 20 WPAN sweep plots |

---

## ⏱️ Estimated Run Times

| Task | Approximate Time |
|------|-----------------|
| NS-3 build (first time) | 15–30 minutes |
| Single simulation run (250s sim) | 4–8 minutes |
| Full paper sweep (`run_error_sweep.sh`) | ~2–3 hours |
| Full WiFi sweep (`run_wifi_sweep.sh`) | ~1–2 hours |
| Full WPAN sweep (`run_wpan_sweep.sh`) | ~30–60 minutes |

> **Tip:** The sweep scripts suppress NS-3 output by default. Remove `> /dev/null 2>&1` in the script's `run_sim()` function to see per-run output.

---

## 🐛 Troubleshooting

**Build error: TypeId not found**
> Make sure you are using NS-3 v3.45. The `TcpNewRenoPlus` and `TcpNewRenoPlusMod` TypeIds are registered in this version.

**Python plot errors: file not found**
> Run the corresponding sweep script first to generate the CSV data before plotting.

**WiFi simulation hangs**
> Large node counts (80+) with many flows can take a long time. Reduce `--simTime` or `--nNodes` for quick tests.

**WPAN: very low throughput**
> This is expected. 802.15.4 has a ~250 kbps PHY rate; with 6LoWPAN overhead and 80-byte segments, effective TCP throughput is in the Kbps range.
