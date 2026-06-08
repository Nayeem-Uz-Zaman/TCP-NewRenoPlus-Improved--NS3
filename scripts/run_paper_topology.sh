#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

./ns3 run "newreno-plus-test --tcpVariant=TcpNewRenoPlusMod --errorRate=0.01 --scenario=multi --simulationTime=250 --outputFile=newreno_multimode_topology_results_v6.csv"
