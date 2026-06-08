#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/energy-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/yans-wifi-helper.h"

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

using namespace ns3;
using namespace ns3::energy;

NS_LOG_COMPONENT_DEFINE("WifiMobileSim");

int
main(int argc, char* argv[])
{
    // ─── Command-line parameters ───────────────────────────────────────────
    std::string tcpVariant = "TcpNewRenoPlusMod"; // or "TcpNewReno"
    uint32_t    nNodes     = 40;    // total mobile nodes
    uint32_t    nFlows     = 20;    // number of TCP flows
    uint32_t    pktPerSec  = 200;   // packets per second per flow
    double      speed      = 10.0;  // node speed (m/s)
    double      simTime    = 40.0;  // simulation time (s)
    std::string outputFile = "wifi_mobile_results.csv";

    std::string routing    = "OLSR";  // OLSR or AODV

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant", "TCP variant: TcpNewReno or TcpNewRenoPlusMod", tcpVariant);
    cmd.AddValue("nNodes",     "Total number of mobile nodes",                 nNodes);
    cmd.AddValue("nFlows",     "Number of TCP flows",                          nFlows);
    cmd.AddValue("pktPerSec",  "Packets per second per flow",                  pktPerSec);
    cmd.AddValue("speed",      "Node speed in m/s",                            speed);
    cmd.AddValue("simTime",    "Simulation time in seconds",                   simTime);
    cmd.AddValue("outputFile", "CSV output file path",                         outputFile);
    cmd.AddValue("routing",    "Routing protocol: OLSR or AODV",               routing);
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
    Config::SetDefault("ns3::TcpSocket::SegmentSize",       UintegerValue(1024));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd",       UintegerValue(1));
    // Disable WiFi fragmentation and RTS/CTS for frames < 2200 bytes
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold",
                       StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                       StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue("DsssRate11Mbps"));

    // ─── Nodes ─────────────────────────────────────────────────────────────
    NodeContainer nodes;
    nodes.Create(nNodes);

    // ─── WiFi (802.11b ad-hoc) ─────────────────────────────────────────────
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",    StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate11Mbps"));

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", DoubleValue(7.5));
    wifiPhy.Set("TxPowerEnd",   DoubleValue(7.5));
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // ─── Mobility (Random Waypoint, 300x300 m) ─────────────────────────────
    MobilityHelper mobility;

    // Scale area with node count to keep density constant (~1 node per 1000 m²)
    double areaSize = std::sqrt(nNodes * 1000.0);
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

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed",             StringValue(speedStr.str()),
                              "Pause",             StringValue("ns3::UniformRandomVariable[Min=0.0|Max=2.0]"),
                              "PositionAllocator", PointerValue(posAlloc));
    mobility.SetPositionAllocator(posAlloc);
    mobility.Install(nodes);

    // ─── Energy model ──────────────────────────────────────────────────────
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    EnergySourceContainer energySources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    DeviceEnergyModelContainer deviceEnergy =
        radioEnergyHelper.Install(devices, energySources);

    // ─── Internet stack (OLSR or AODV routing) ────────────────────────────
    InternetStackHelper internet;
    if (routing == "AODV")
    {
        AodvHelper aodv;
        internet.SetRoutingHelper(aodv);
    }
    else
    {
        OlsrHelper olsr;
        internet.SetRoutingHelper(olsr);
    }
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // ─── Applications (TCP flows) ───────────────────────────────────────────
    // Payload size in bytes, data rate derived from pktPerSec
    uint32_t    payloadSize = 1024; // bytes
    std::string dataRate    =
        std::to_string(pktPerSec * payloadSize * 8) + "bps";

    uint16_t    basePort    = 8000;

    // Generate nFlows random unique (src, dst) pairs
    Ptr<UniformRandomVariable> rng = CreateObject<UniformRandomVariable>();
    rng->SetAttribute("Min", DoubleValue(0));
    rng->SetAttribute("Max", DoubleValue(nNodes - 1));

    ApplicationContainer sinkApps, sourceApps;

    std::set<std::pair<uint32_t, uint32_t>> usedPairs;
    uint32_t flowsInstalled = 0;
    uint32_t attempts       = 0;
    uint32_t maxAttempts    = nFlows * 100;

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

        // Sink on destination node
        PacketSinkHelper sink("ns3::TcpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(nodes.Get(dstIdx));
        sinkApp.Start(Seconds(0.5));
        sinkApp.Stop(Seconds(simTime));
        sinkApps.Add(sinkApp);

        // OnOff source — honours the pktPerSec-derived data rate
        OnOffHelper source("ns3::TcpSocketFactory",
                           InetSocketAddress(interfaces.GetAddress(dstIdx), port));
        source.SetAttribute("PacketSize", UintegerValue(payloadSize));
        source.SetAttribute("DataRate",   DataRateValue(DataRate(dataRate)));
        source.SetAttribute("OnTime",     StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        source.SetAttribute("OffTime",    StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer sourceApp = source.Install(nodes.Get(srcIdx));
        // Start at t=5 s to allow routing protocol to converge before traffic
        sourceApp.Start(Seconds(5.0));
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
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowMonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = flowMon->GetFlowStats();

    double totalTxPkts  = 0;
    double totalRxPkts  = 0;
    double totalLostPkts= 0;
    double totalRxBytes = 0;
    double totalDelayS  = 0;

    for (auto& fs : stats)
    {
        // Only count TCP flows (ignore AODV control traffic)
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(fs.first);
        if (t.protocol != 6) // 6 = TCP
            continue;

        totalTxPkts   += fs.second.txPackets;
        totalRxPkts   += fs.second.rxPackets;
        totalLostPkts += fs.second.lostPackets;
        totalRxBytes  += fs.second.rxBytes;
        totalDelayS   += fs.second.delaySum.GetSeconds();
    }

    double throughput  = (totalRxBytes * 8.0) / simTime / 1e6; // Mbps
    double avgDelayMs  = (totalRxPkts > 0)
                           ? (totalDelayS * 1000.0 / totalRxPkts)
                           : 0.0;
    double pdr         = (totalTxPkts > 0)
                           ? (totalRxPkts / totalTxPkts * 100.0)
                           : 0.0;
    double dropRatio   = (totalTxPkts > 0)
                           ? (totalLostPkts / totalTxPkts * 100.0)
                           : 0.0;

    // ─── Collect energy ────────────────────────────────────────────────────
    double totalEnergyJ = 0.0;
    for (auto it = deviceEnergy.Begin(); it != deviceEnergy.End(); ++it)
    {
        totalEnergyJ += (*it)->GetTotalEnergyConsumption();
    }

    Simulator::Destroy();

    // ─── Console summary ───────────────────────────────────────────────────
    std::cout << "========================================\n"
              << "  WiFi Mobile Simulation Results\n"
              << "========================================\n"
              << "  TCP Variant  : " << tcpVariant     << "\n"
              << "  Nodes        : " << nNodes         << "\n"
              << "  Flows        : " << flowsInstalled  << "\n"
              << "  Pkt/s        : " << pktPerSec      << "\n"
              << "  Speed (m/s)  : " << speed          << "\n"
              << "  Sim Time (s) : " << simTime        << "\n"
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
