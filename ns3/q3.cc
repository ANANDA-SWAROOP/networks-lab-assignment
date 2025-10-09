#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HiddenExposedExample");

int main(int argc, char *argv[])
{
    bool enableRtsCts = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS mechanism", enableRtsCts);
    cmd.Parse(argc, argv);

    // Simulation parameters
    double simTime = 10.0;
    uint32_t packetSize = 1024;
    DataRate dataRate("6Mbps");

    // Create nodes
    NodeContainer nodes;
    nodes.Create(4); // A, B, C, D

    // Configure Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("HiddenExposedNetwork");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Set RTS/CTS threshold
   if (enableRtsCts)
{
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold",
                UintegerValue(0)); // enable RTS/CTS for all packets
    NS_LOG_UNCOND("RTS/CTS enabled");
}
else
{
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/RtsCtsThreshold",
                UintegerValue(999999)); // effectively disable RTS/CTS
    NS_LOG_UNCOND("RTS/CTS disabled");
}

    // Set mobility (linear positions)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // A
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));  // B
    positionAlloc->Add(Vector(100.0, 0.0, 0.0)); // C
    positionAlloc->Add(Vector(150.0, 0.0, 0.0)); // D
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Applications
    uint16_t port = 9;
    OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff1.SetConstantRate(dataRate, packetSize);
    onoff1.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff1.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    onoff2.SetConstantRate(dataRate, packetSize);
    onoff2.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff2.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    onoff1.Install(nodes.Get(0)); // A -> B
    onoff2.Install(nodes.Get(3)); // D -> C

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinks = sink.Install(nodes.Get(1));
    sinks.Add(sink.Install(nodes.Get(2)));
    sinks.Start(Seconds(0.0));
    sinks.Stop(Seconds(simTime));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Animation
    AnimationInterface anim("hidden_exposed.xml");
    anim.UpdateNodeDescription(0, "A");
    anim.UpdateNodeDescription(1, "B");
    anim.UpdateNodeDescription(2, "C");
    anim.UpdateNodeDescription(3, "D");
    anim.UpdateNodeColor(0, 255, 0, 0);
    anim.UpdateNodeColor(3, 0, 0, 255);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0;
    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double throughput = (flow.second.rxBytes * 8.0) / (simTime * 1000000.0);
        totalThroughput += throughput;
        NS_LOG_UNCOND("Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << "): "
                              << throughput << " Mbps, Lost Packets: " << flow.second.lostPackets);
    }

    NS_LOG_UNCOND("Total Throughput: " << totalThroughput << " Mbps");

    Simulator::Destroy();
    return 0;
}
