#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkComparison");

int main(int argc, char *argv[])
{
    bool enableTcp = true;
    double simTime = 20.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableTcp", "Use TCP if true, UDP if false", enableTcp);
    cmd.Parse(argc, argv);

    NodeContainer csmaNodes;
    csmaNodes.Create(3); // Two LAN nodes + Router

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(2); // One Wi-Fi STA + one extra
    NodeContainer wifiApNode = csmaNodes.Get(2); // Router as AP

    // ---------- CSMA Setup ----------
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    NetDeviceContainer csmaDevices = csma.Install(csmaNodes);

    // ---------- Wi-Fi Setup ----------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-hybrid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);


// ---------- Mobility Setup (required for Wi-Fi) ----------
MobilityHelper mobility;

// Place STA nodes in a line or small area
mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                              "MinX", DoubleValue(0.0),
                              "MinY", DoubleValue(0.0),
                              "DeltaX", DoubleValue(5.0),
                              "DeltaY", DoubleValue(5.0),
                              "GridWidth", UintegerValue(3),
                              "LayoutType", StringValue("RowFirst"));
mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
mobility.Install(wifiStaNodes);

// AP node (router)
MobilityHelper mobilityAp;
mobilityAp.SetMobilityModel("ns3::ConstantPositionMobilityModel");
mobilityAp.Install(wifiApNode);



    // ---------- Internet Stack ----------
    InternetStackHelper stack;
    stack.Install(csmaNodes);
    stack.Install(wifiStaNodes);

    // ---------- IP Addressing ----------
    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces = address.Assign(staDevices);
    address.Assign(apDevice);

    // ---------- Application Setup ----------
    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(wifiInterfaces.GetAddress(0), port));

    if (enableTcp)
    {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(0));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(simTime));

        OnOffHelper sourceHelper("ns3::TcpSocketFactory", sinkAddress);
        sourceHelper.SetAttribute("DataRate", StringValue("10Mbps"));
        sourceHelper.SetAttribute("PacketSize", UintegerValue(1024));
        sourceHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        sourceHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer sourceApp = sourceHelper.Install(csmaNodes.Get(0));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(simTime));
    }
    else
    {
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes.Get(0));
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(simTime));

        OnOffHelper sourceHelper("ns3::UdpSocketFactory", sinkAddress);
        sourceHelper.SetAttribute("DataRate", StringValue("10Mbps"));
        sourceHelper.SetAttribute("PacketSize", UintegerValue(1024));
        sourceHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        sourceHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer sourceApp = sourceHelper.Install(csmaNodes.Get(0));
        sourceApp.Start(Seconds(2.0));
        sourceApp.Stop(Seconds(simTime));
    }

    // ---------- Flow Monitor ----------
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ---------- Data Analysis ----------
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " → " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << flow.second.txPackets << "\n";
        std::cout << "  Rx Packets: " << flow.second.rxPackets << "\n";
        std::cout << "  Throughput: " << (flow.second.rxBytes * 8.0 / (simTime - 2) / 1e6) << " Mbps\n";
        std::cout << "  Delay: " << (flow.second.delaySum.GetSeconds() / flow.second.rxPackets) << " s\n";
        std::cout << "  Packet Delivery Ratio: "
                  << (100.0 * flow.second.rxPackets / flow.second.txPackets) << "%\n\n";
    }

    monitor->SerializeToXmlFile("hybrid_network_flowmon.xml", true, true);

    Simulator::Destroy();
    return 0;
}
