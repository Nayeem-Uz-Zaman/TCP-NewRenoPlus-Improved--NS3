#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WpanMobileSim");

int
main(int argc, char* argv[])
{
    // ─── Command-line parameters ───────────────────────────────────────────
    std::string tcpVariant = "TcpNewRenoPlusMod";
    uint32_t    nNodes     = 40;
    uint32_t    nFlows     = 20;
    uint32_t    pktPerSec  = 200;
    double      speed      = 10.0;  // m/s
    double      simTime    = 30.0;
    std::string outputFile = "wpan_mobile_results.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP variant: TcpNewReno or TcpNewRenoPlusMod", tcpVariant);
    cmd.AddValue("nNodes",     "Total number of mobile nodes",                 nNodes);
    cmd.AddValue("nFlows",     "Number of TCP flows",                          nFlows);
    cmd.AddValue("pktPerSec",  "Packets per second per flow",                  pktPerSec);
    cmd.AddValue("speed",      "Node speed in m/s",                            speed);
    cmd.AddValue("simTime",    "Simulation time in seconds",                   simTime);
    cmd.AddValue("outputFile", "CSV output file path",                         outputFile);
    cmd.Parse(argc, argv);

    // Validate TCP variant
    if (tcpVariant != "TcpNewReno" && tcpVariant != "TcpNewRenoPlusMod")
    {
        NS_FATAL_ERROR("Unknown tcpVariant: " << tcpVariant
                       << ". Use TcpNewReno or TcpNewRenoPlusMod.");
    }

    // Safety guard: can't have more flows than unique directed node pairs
    if (nFlows > nNodes * (nNodes - 1))
    {
        NS_LOG_WARN("nFlows (" << nFlows << ") clamped to " << nNodes * (nNodes - 1));
        nFlows = nNodes * (nNodes - 1);
    }

    // ─── Global TCP configuration ──────────────────────────────────────────
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::" + tcpVariant));
    // Small segment size to avoid 6LoWPAN fragmentation masking TCP behaviour.
    // 802.15.4 max payload after MAC/6LoWPAN headers ≈ 80–90 bytes.
    Config::SetDefault("ns3::TcpSocket::SegmentSize",  UintegerValue(80));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd",  UintegerValue(1));

    // ─── Nodes ─────────────────────────────────────────────────────────────
    NodeContainer nodes;
    nodes.Create(nNodes);

    
    double areaSize = 50.0;

    std::ostringstream xRange, yRange;
    xRange << "ns3::UniformRandomVariable[Min=0.0|Max=" << areaSize << "]";
    yRange << "ns3::UniformRandomVariable[Min=0.0|Max=" << areaSize << "]";

    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomRectanglePositionAllocator");
    pos.Set("X", StringValue(xRange.str()));
    pos.Set("Y", StringValue(yRange.str()));
    Ptr<PositionAllocator> posAlloc = pos.Create()->GetObject<PositionAllocator>();

    std::ostringstream speedStr;
    speedStr << "ns3::UniformRandomVariable[Min=" << speed * 0.8
             << "|Max=" << speed * 1.2 << "]";

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed",             StringValue(speedStr.str()),
                              "Pause",             StringValue("ns3::UniformRandomVariable[Min=0.0|Max=2.0]"),
                              "PositionAllocator", PointerValue(posAlloc));
    mobility.SetPositionAllocator(posAlloc);
    mobility.Install(nodes);

    // ─── LR-WPAN (802.15.4 PHY + MAC) ─────────────────────────────────────
    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    lrWpanHelper.AddPropagationLossModel("ns3::LogDistancePropagationLossModel");

    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // Associate all devices into a single PAN (PAN ID = 5)
    lrWpanHelper.CreateAssociatedPan(lrwpanDevices, 5);

    
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);  // IPv6 only
    internetv6.Install(nodes);

    // ─── 6LoWPAN adaptation layer ──────────────────────────────────────────
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixDevices = sixlowpan.Install(lrwpanDevices);

    // ─── IPv6 addressing ───────────────────────────────────────────────────
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(sixDevices);

    // ─── Energy constants (analytical model) ──────────────────────────────────
    // No LrWpanRadioEnergyModelHelper exists in NS-3.45. Energy is computed as
    // E = V × I × t × nNodes (constant-power model, CC2420-class 802.15.4 radio)
    const double supplyVoltage = 3.3;    // V
    const double currentA     = 0.0174; // A (17.4 mA CC2420)

    // ─── Applications (TCP flows) ───────────────────────────────────────────
    uint32_t    payloadSize = 80;   // bytes — fits within 6LoWPAN single frame
    std::string dataRate    =
        std::to_string(pktPerSec * payloadSize * 8) + "bps";

    uint16_t basePort = 9000;

    // Single-hop: no routing convergence needed; start traffic at 2 s
    double trafficStart = 2.0;

    // Generate nFlows random unique (src, dst) pairs
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();

    ApplicationContainer sinkApps, sourceApps;
    std::set<std::pair<uint32_t, uint32_t>> usedPairs;
    uint32_t flowsInstalled = 0;
    uint32_t attempts       = 0;
    uint32_t maxAttempts    = nFlows * 200;

    while (flowsInstalled < nFlows && attempts < maxAttempts)
    {
        attempts++;
        uint32_t srcIdx = (uint32_t)rng->GetInteger(0, nNodes - 1);
        uint32_t dstIdx = (uint32_t)rng->GetInteger(0, nNodes - 1);
        if (srcIdx == dstIdx)
            continue;
        auto pair = std::make_pair(srcIdx, dstIdx);
        if (usedPairs.count(pair))
            continue;
        usedPairs.insert(pair);

        uint16_t port = basePort + flowsInstalled;

        // Destination address: global IPv6 address (index 1; index 0 = link-local)
        Ipv6Address dstAddr = interfaces.GetAddress(dstIdx, 1);

        // Sink on destination node
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              Inet6SocketAddress(Ipv6Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(dstIdx));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(simTime));
        sinkApps.Add(sinkApp);

        // OnOff source — rate-limited to honour pktPerSec
        OnOffHelper source("ns3::TcpSocketFactory",
                           Inet6SocketAddress(dstAddr, port));
        source.SetAttribute("PacketSize", UintegerValue(payloadSize));
        source.SetAttribute("DataRate",   DataRateValue(DataRate(dataRate)));
        source.SetAttribute("OnTime",     StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        source.SetAttribute("OffTime",    StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer sourceApp = source.Install(nodes.Get(srcIdx));
        sourceApp.Start(Seconds(trafficStart));
        sourceApp.Stop(Seconds(simTime - 1.0));
        sourceApps.Add(sourceApp);

        flowsInstalled++;
    }

    NS_LOG_INFO("Installed " << flowsInstalled << " TCP flows.");

    // ─── FlowMonitor ───────────────────────────────────────────────────────
    FlowMonitorHelper flowMonHelper;
    Ptr<FlowMonitor> flowMon = flowMonHelper.InstallAll();

    // ─── Run ───────────────────────────────────────────────────────────────
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ─── Collect FlowMonitor metrics ───────────────────────────────────────
    flowMon->CheckForLostPackets();
    Ptr<Ipv6FlowClassifier> classifier =
        DynamicCast<Ipv6FlowClassifier>(flowMonHelper.GetClassifier6());
    FlowMonitor::FlowStatsContainer stats = flowMon->GetFlowStats();

    double totalTxPkts   = 0;
    double totalRxPkts   = 0;
    double totalLostPkts = 0;
    double totalRxBytes  = 0;
    double totalDelayS   = 0;

    for (auto& fs : stats)
    {
        Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow(fs.first);
        if (t.protocol != 6)  // TCP only
            continue;

        totalTxPkts   += fs.second.txPackets;
        totalRxPkts   += fs.second.rxPackets;
        totalLostPkts += fs.second.lostPackets;
        totalRxBytes  += fs.second.rxBytes;
        totalDelayS   += fs.second.delaySum.GetSeconds();
    }

    double effectiveTime = simTime - trafficStart;
    double throughput  = (totalRxBytes * 8.0) / effectiveTime / 1e6;  // Mbps
    double avgDelayMs  = (totalRxPkts > 0)
                           ? (totalDelayS * 1000.0 / totalRxPkts)
                           : 0.0;
    double pdr         = (totalTxPkts > 0)
                           ? (totalRxPkts / totalTxPkts * 100.0)
                           : 0.0;
    double dropRatio   = (totalTxPkts > 0)
                           ? (totalLostPkts / totalTxPkts * 100.0)
                           : 0.0;

    // ─── Compute energy analytically ───────────────────────────────────────
    // E = V × I × t × nNodes  (constant power model, CC2420 radio)
    double totalEnergyJ = supplyVoltage * currentA * simTime * nNodes;

    Simulator::Destroy();

    // ─── Console summary ───────────────────────────────────────────────────
    std::cout << "========================================\n"
              << "  WPAN Mobile Simulation Results\n"
              << "========================================\n"
              << "  TCP Variant  : " << tcpVariant      << "\n"
              << "  Nodes        : " << nNodes          << "\n"
              << "  Flows        : " << flowsInstalled   << "\n"
              << "  Pkt/s        : " << pktPerSec       << "\n"
              << "  Speed (m/s)  : " << speed           << "\n"
              << "  Sim Time (s) : " << simTime         << "\n"
              << "  Routing      : Single-hop (RPL not in NS-3.45; 50x50m area)\n"
              << "----------------------------------------\n"
              << "  Throughput   : " << throughput  << " Mbps\n"
              << "  Avg Delay    : " << avgDelayMs  << " ms\n"
              << "  PDR          : " << pdr         << " %\n"
              << "  Drop Ratio   : " << dropRatio   << " %\n"
              << "  Energy       : " << totalEnergyJ << " J\n"
              << "========================================\n";

    // ─── CSV output ────────────────────────────────────────────────────────
    bool writeHeader = false;
    {
        std::ifstream checkFile(outputFile);
        if (!checkFile.good())
            writeHeader = true;
    }

    std::ofstream csv(outputFile, std::ios::app);
    if (writeHeader)
    {
        csv << "tcpVariant,nNodes,nFlows,pktPerSec,speed,"
            << "throughput_mbps,avgDelay_ms,PDR_pct,dropRatio_pct,energy_J\n";
    }
    csv << tcpVariant     << ","
        << nNodes         << ","
        << flowsInstalled << ","
        << pktPerSec      << ","
        << speed          << ","
        << throughput     << ","
        << avgDelayMs     << ","
        << pdr            << ","
        << dropRatio      << ","
        << totalEnergyJ   << "\n";
    csv.close();

    std::cout << "Results appended to: " << outputFile << "\n";

    return 0;
}