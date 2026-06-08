#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"

#include <fstream>
#include <iomanip>
#include <ctime>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NewRenoPlusTest");

int
main(int argc, char* argv[])
{
    // -----------------------------------------------------------------------
    // Command-line parameters
    // -----------------------------------------------------------------------
    std::string tcpVariant    = "TcpNewReno";
    double      errorRate     = 0.01;
    double      simulationTime = 250.0;
    uint32_t    payloadSize   = 1000;   // bytes — 1 segment = 1000 bytes
    double      udpDataRate   = 5.0;    // Mbps — fills bottleneck to match paper congestion
    bool        verbose       = false;
    // "single" : 1 TCP flow, 7 Mbps bottleneck  (Table 2 in paper)
    // "multi"  : 2 TCP flows, 12 Mbps bottleneck (Table 3 in paper)
    std::string scenario      = "multi";
    std::string outputFile    = "newreno_multimode_topology_results_v6.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcpVariant",     "TCP variant (TcpNewReno, TcpNewRenoPlus, or TcpNewRenoPlusMod)", tcpVariant);
    cmd.AddValue("errorRate",      "Packet error rate (0.0 to 1.0)",             errorRate);
    cmd.AddValue("simulationTime", "Simulation duration in seconds",             simulationTime);
    cmd.AddValue("udpDataRate",    "UDP CBR background traffic rate in Mbps",    udpDataRate);
    cmd.AddValue("scenario",       "Scenario: single (1 TCP, 7Mbps) or multi (2 TCP, 12Mbps)", scenario);
    cmd.AddValue("outputFile",     "Output CSV filename",                        outputFile);
    cmd.AddValue("verbose",        "Enable verbose logging",                     verbose);
    cmd.Parse(argc, argv);

    // Bottleneck bandwidth follows the scenario as specified in the paper
    bool   isSingle        = (scenario == "single");
    double bottleneckMbps  = isSingle ? 7.0 : 12.0;

    // -----------------------------------------------------------------------
    // Validate TCP variant
    // -----------------------------------------------------------------------
    std::string tcpTypeId = "ns3::" + tcpVariant;
    TypeId tcpTid;
    if (!TypeId::LookupByNameFailSafe(tcpTypeId, &tcpTid))
    {
        std::cerr << "Error: TypeId " << tcpTypeId << " not found." << std::endl;
        return 1;
    }

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(tcpTid));
    Config::SetDefault("ns3::TcpSocket::SegmentSize",    UintegerValue(payloadSize));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize",     UintegerValue(1 << 21));
    Config::SetDefault("ns3::TcpSocket::SndBufSize",     UintegerValue(1 << 21));


    if (verbose)
    {
        LogComponentEnable("NewRenoPlusTest",    LOG_LEVEL_INFO);
        LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
        LogComponentEnable("PacketSink",          LOG_LEVEL_INFO);
    }

    std::cout << "==========================================" << std::endl;
    std::cout << " TCP NewReno+ Evaluation — Paper Topology " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << " TCP Variant    : " << tcpVariant                        << std::endl;
    std::cout << " Scenario       : " << scenario                          << std::endl;
    std::cout << " Bottleneck     : " << bottleneckMbps << " Mbps"         << std::endl;
    std::cout << " Error Rate     : " << (errorRate * 100.0) << "%"        << std::endl;
    std::cout << " Simulation Time: " << simulationTime << " s"            << std::endl;
    std::cout << "==========================================" << std::endl;

    // -----------------------------------------------------------------------
    // Step 1: Create nodes
    //   Paper: 3 wired + 1 head node + 1 gateway AP + 5 wireless STAs
    // -----------------------------------------------------------------------
    NodeContainer wiredNodes;
    wiredNodes.Create(3);                  // wired[0], wired[1], wired[2]

    NodeContainer routers;
    routers.Create(2);
    Ptr<Node> headNode  = routers.Get(0);  // aggregation point
    Ptr<Node> gatewayAp = routers.Get(1);  // AP / gateway

    NodeContainer wirelessNodes;
    wirelessNodes.Create(5);               // sta[0]..sta[4]

    NS_LOG_INFO("Step 1: Created 3 wired, 2 router, 5 wireless nodes");

    // -----------------------------------------------------------------------
    // Step 2: Wired links — 5 Mbps / 2 ms per wired node to head node
    // -----------------------------------------------------------------------
    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate",  StringValue("5Mbps"));
    p2pAccess.SetChannelAttribute("Delay",    StringValue("2ms"));

    NetDeviceContainer wired0Devices = p2pAccess.Install(wiredNodes.Get(0), headNode);
    NetDeviceContainer wired1Devices = p2pAccess.Install(wiredNodes.Get(1), headNode);
    NetDeviceContainer wired2Devices = p2pAccess.Install(wiredNodes.Get(2), headNode);

    // Bottleneck link: head node → gateway AP
    // Paper: 7 Mbps for single-TCP scenario, 12 Mbps for two-TCP scenario
    std::ostringstream bottleneckRate;
    bottleneckRate << bottleneckMbps << "Mbps";

    PointToPointHelper p2pBottleneck;
    p2pBottleneck.SetDeviceAttribute("DataRate",  StringValue(bottleneckRate.str()));
    p2pBottleneck.SetChannelAttribute("Delay",    StringValue("10ms"));

    NetDeviceContainer bottleneckDevices = p2pBottleneck.Install(headNode, gatewayAp);

    NS_LOG_INFO("Step 2: P2P wired links installed, bottleneck=" << bottleneckRate.str());

    // -----------------------------------------------------------------------
    // Step 3: 802.11g Wi-Fi — AP on gatewayAp, STAs on wirelessNodes
    // -----------------------------------------------------------------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper     wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    Ssid ssid = Ssid("newreno-plus-net");

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid",             SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true),
                    "BeaconInterval",   TimeValue(MicroSeconds(102400)));
    NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, gatewayAp);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid",           SsidValue(ssid),
                    "ActiveProbing",  BooleanValue(true));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, wirelessNodes);

    if (errorRate > 0.0)
    {
        for (uint32_t i = 0; i < staDevices.GetN(); ++i)
        {
            Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
            em->SetAttribute("ErrorRate", DoubleValue(errorRate));
            em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
            Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice>(staDevices.Get(i));
            if (dev)
            {
                dev->GetPhy()->SetPostReceptionErrorModel(em);
            }
        }

        NS_LOG_INFO("RateErrorModel on WiFi PHY AP->STA only: "
                    << errorRate * 100.0 << "%");
    }

    NS_LOG_INFO("Step 3: 802.11g Wi-Fi configured; RateErrorModel on AP->STA PHY only");

    // -----------------------------------------------------------------------
    // Step 4: Mobility — static positions
    // -----------------------------------------------------------------------
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    Ptr<ListPositionAllocator> apPos = CreateObject<ListPositionAllocator>();
    apPos->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPos);
    mobility.Install(gatewayAp);

    Ptr<ListPositionAllocator> staPos = CreateObject<ListPositionAllocator>();
    staPos->Add(Vector( 3.0,  0.0, 0.0));  // sta[0]
    staPos->Add(Vector( 0.0,  3.0, 0.0));  // sta[1]
    staPos->Add(Vector(-3.0,  0.0, 0.0));  // sta[2]
    staPos->Add(Vector( 0.0, -3.0, 0.0));  // sta[3]
    staPos->Add(Vector( 2.0,  2.0, 0.0));  // sta[4]
    mobility.SetPositionAllocator(staPos);
    mobility.Install(wirelessNodes);

    NS_LOG_INFO("Step 4: Node positions assigned");

    // -----------------------------------------------------------------------
    // Step 5: Internet stack + IP addressing
    // -----------------------------------------------------------------------
    InternetStackHelper internet;
    internet.Install(wiredNodes);
    internet.Install(routers);
    internet.Install(wirelessNodes);

    Ipv4AddressHelper ipv4;

    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(wired0Devices);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    ipv4.Assign(wired1Devices);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    ipv4.Assign(wired2Devices);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    ipv4.Assign(bottleneckDevices);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staIfaces = ipv4.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Step 5: IP addresses assigned, routing tables populated");

    // -----------------------------------------------------------------------
    // Step 6: Applications — paper defines 4 connections:
    //
    //   TCP flow 1 : wired[0] (sender) → sta[0] (receiver)   — always present
    //   TCP flow 2 : wired[1] (sender) → sta[1] (receiver)   — multi only
    //   UDP flow 1 : wired[2] (sender) → sta[2] (receiver)   — always present
    //   UDP flow 2 : sta[3]   (sender) → sta[4] (receiver)   — always present
    //                  (wireless-to-wireless via the AP)
    // -----------------------------------------------------------------------
    const uint16_t tcpPort1  = 8080;
    const uint16_t tcpPort2  = 8081;
    const uint16_t udpPort1  = 9090;
    const uint16_t udpPort2  = 9091;

    // ---- TCP flow 1: wired[0] → sta[0] ------------------------------------
    PacketSinkHelper tcpSinkHelper1("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), tcpPort1));
    ApplicationContainer tcpSink1App = tcpSinkHelper1.Install(wirelessNodes.Get(0));

    BulkSendHelper tcpBulk1("ns3::TcpSocketFactory",
                             InetSocketAddress(staIfaces.GetAddress(0), tcpPort1));
    tcpBulk1.SetAttribute("MaxBytes",  UintegerValue(0));
    tcpBulk1.SetAttribute("SendSize",  UintegerValue(payloadSize));
    ApplicationContainer tcpSrc1App = tcpBulk1.Install(wiredNodes.Get(0));

    // ---- TCP flow 2: wired[1] → sta[1]  (multi scenario only) ------------
    ApplicationContainer tcpSink2App;
    ApplicationContainer tcpSrc2App;
    if (!isSingle)
    {
        PacketSinkHelper tcpSinkHelper2("ns3::TcpSocketFactory",
                                        InetSocketAddress(Ipv4Address::GetAny(), tcpPort2));
        tcpSink2App = tcpSinkHelper2.Install(wirelessNodes.Get(1));

        BulkSendHelper tcpBulk2("ns3::TcpSocketFactory",
                                 InetSocketAddress(staIfaces.GetAddress(1), tcpPort2));
        tcpBulk2.SetAttribute("MaxBytes", UintegerValue(0));
        tcpBulk2.SetAttribute("SendSize", UintegerValue(payloadSize));
        tcpSrc2App = tcpBulk2.Install(wiredNodes.Get(1));
    }

    // ---- UDP flow 1: wired[2] → sta[2]  (wired to wireless CBR) ----------
    std::ostringstream udpRateStr;
    udpRateStr << udpDataRate << "Mbps";

    PacketSinkHelper udpSinkHelper1("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), udpPort1));
    ApplicationContainer udpSink1App = udpSinkHelper1.Install(wirelessNodes.Get(2));

    OnOffHelper udpOnOff1("ns3::UdpSocketFactory",
                          InetSocketAddress(staIfaces.GetAddress(2), udpPort1));
    udpOnOff1.SetAttribute("OnTime",    StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpOnOff1.SetAttribute("OffTime",   StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    udpOnOff1.SetAttribute("DataRate",  DataRateValue(DataRate(udpRateStr.str())));
    udpOnOff1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpSrc1App = udpOnOff1.Install(wiredNodes.Get(2));

    // ---- UDP flow 2: sta[3] → sta[4]  (wireless-to-wireless CBR via AP) --
    PacketSinkHelper udpSinkHelper2("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), udpPort2));
    ApplicationContainer udpSink2App = udpSinkHelper2.Install(wirelessNodes.Get(4));

    OnOffHelper udpOnOff2("ns3::UdpSocketFactory",
                          InetSocketAddress(staIfaces.GetAddress(4), udpPort2));
    udpOnOff2.SetAttribute("OnTime",    StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpOnOff2.SetAttribute("OffTime",   StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    udpOnOff2.SetAttribute("DataRate",  DataRateValue(DataRate(udpRateStr.str())));
    udpOnOff2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer udpSrc2App = udpOnOff2.Install(wirelessNodes.Get(3));

    NS_LOG_INFO("Step 6: All applications installed");

    // -----------------------------------------------------------------------
    // Step 7: Application start/stop times
    //   Sinks start early; sources start at 5 s to allow Wi-Fi association
    // -----------------------------------------------------------------------
    tcpSink1App.Start(Seconds(1.0));   tcpSink1App.Stop(Seconds(simulationTime));
    tcpSrc1App.Start(Seconds(5.0));    tcpSrc1App.Stop(Seconds(simulationTime - 1.0));

    if (!isSingle)
    {
        tcpSink2App.Start(Seconds(1.0)); tcpSink2App.Stop(Seconds(simulationTime));
        tcpSrc2App.Start(Seconds(5.0));  tcpSrc2App.Stop(Seconds(simulationTime - 1.0));
    }

    udpSink1App.Start(Seconds(1.0));   udpSink1App.Stop(Seconds(simulationTime));
    udpSrc1App.Start(Seconds(5.0));    udpSrc1App.Stop(Seconds(simulationTime - 1.0));

    udpSink2App.Start(Seconds(1.0));   udpSink2App.Stop(Seconds(simulationTime));
    udpSrc2App.Start(Seconds(5.0));    udpSrc2App.Stop(Seconds(simulationTime - 1.0));

    Simulator::Stop(Seconds(simulationTime));

    NS_LOG_INFO("Step 7: Start/stop times configured");

    // -----------------------------------------------------------------------
    // Step 8: Prepare CSV
    // -----------------------------------------------------------------------
    std::string csvFile = outputFile;

    std::ifstream checkFile(csvFile);
    bool writeHeader = !checkFile.good();
    checkFile.close();

    // -----------------------------------------------------------------------
    // Step 9: Run
    // -----------------------------------------------------------------------
    std::cout << "\nRunning simulation for " << simulationTime << " s ..." << std::endl;
    Simulator::Run();

    // -----------------------------------------------------------------------
    // Step 10: Collect metrics
    //
    //   Paper metric: "number of unique segments" received at the TCP sink.
    //   For multi scenario the paper reports the AVERAGE of the two flows.
    // -----------------------------------------------------------------------
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(tcpSink1App.Get(0));
    uint64_t bytes1       = sink1->GetTotalRx();
    uint32_t segments1    = bytes1 / payloadSize;

    uint64_t bytes2    = 0;
    uint32_t segments2 = 0;
    if (!isSingle)
    {
        Ptr<PacketSink> sink2 = DynamicCast<PacketSink>(tcpSink2App.Get(0));
        bytes2    = sink2->GetTotalRx();
        segments2 = bytes2 / payloadSize;
    }

    // Average unique segments (matches paper's reported metric)
    uint32_t avgSegments = isSingle ? segments1
                                    : (segments1 + segments2) / 2;
    double   avgThroughputMbps = isSingle
        ? (bytes1 * 8.0) / (simulationTime * 1e6)
        : ((bytes1 + bytes2) * 8.0) / (2.0 * simulationTime * 1e6);

    // -----------------------------------------------------------------------
    // Step 11: Write CSV
    // -----------------------------------------------------------------------
    std::ofstream csvOut(csvFile, std::ios::app);
    if (csvOut.is_open())
    {
        if (writeHeader)
        {
            csvOut << "Timestamp,Scenario,BottleneckMbps,TCPVariant,ErrorRate,"
                   << "Segments_Flow1,Segments_Flow2,AvgUniqueSegments,AvgThroughputMbps"
                   << std::endl;
        }

        std::time_t now = std::time(nullptr);
        char timestamp[64];
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now));

        csvOut << timestamp                                          << ","
               << scenario                                          << ","
               << std::fixed << std::setprecision(0) << bottleneckMbps << ","
               << tcpVariant                                        << ","
               << std::fixed << std::setprecision(4) << errorRate   << ","
               << segments1                                          << ","
               << (isSingle ? 0 : segments2)                        << ","
               << avgSegments                                        << ","
               << std::fixed << std::setprecision(3) << avgThroughputMbps
               << std::endl;

        csvOut.close();
        std::cout << "Results appended to " << csvFile << std::endl;
    }

    NS_LOG_INFO("Step 11: CSV written");

    // -----------------------------------------------------------------------
    // Console summary
    // -----------------------------------------------------------------------
    std::cout << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "           SIMULATION RESULTS             " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << " TCP Variant      : " << tcpVariant                       << std::endl;
    std::cout << " Scenario         : " << scenario                         << std::endl;
    std::cout << " Bottleneck       : " << bottleneckMbps << " Mbps"        << std::endl;
    std::cout << " Error Rate       : " << std::fixed << std::setprecision(1)
              << (errorRate * 100.0) << "%"                                 << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << " TCP Flow 1 Segments : " << segments1                     << std::endl;
    if (!isSingle)
    std::cout << " TCP Flow 2 Segments : " << segments2                     << std::endl;
    std::cout << " Avg Unique Segments : " << avgSegments                   << std::endl;
    std::cout << " Avg Throughput      : " << std::fixed << std::setprecision(3)
              << avgThroughputMbps << " Mbps"                               << std::endl;
    std::cout << "==========================================" << std::endl;

    Simulator::Destroy();
    return 0;
}
