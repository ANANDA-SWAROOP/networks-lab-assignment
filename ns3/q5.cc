#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BusQueueManagement");

int main(int argc, char *argv[])
{
    bool useRed = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useRed", "Use RED queue instead of DropTail", useRed);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(10);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
    csma.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = csma.Install(nodes);

    // Queue management setup
    TrafficControlHelper tch;
    if (useRed) {
        tch.SetRootQueueDisc("ns3::RedQueueDisc");
    } else {
        tch.SetRootQueueDisc("ns3::DropTailQueueDisc");
    }
    QueueDiscContainer qdiscs = tch.Install(devices);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup OnOff traffic (each node sends to the last node)
    uint16_t port = 9;
    for (uint32_t i = 0; i < nodes.GetN() - 1; ++i) {
        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(interfaces.GetAddress(9), port));
        onoff.SetAttribute("DataRate", StringValue("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
        ApplicationContainer app = onoff.Install(nodes.Get(i));
        app.Start(Seconds(1.0 + i * 0.1));
        app.Stop(Seconds(10.0));
    }

    // Sink on node 9
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(9));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(11.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim visualization
    AnimationInterface anim("bus_queue_management.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.SetConstantPosition(nodes.Get(i), 10 * i, 20);
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    // Flow monitor results
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint32_t totalTx = 0, totalRx = 0;
    for (auto &flow : stats) {
        totalTx += flow.second.txPackets;
        totalRx += flow.second.rxPackets;
    }

    std::cout << "\n--- Queue Type: "
              << (useRed ? "RED" : "DropTail") << " ---\n";
    std::cout << "Total Packets Sent: " << totalTx << "\n";
    std::cout << "Total Packets Received: " << totalRx << "\n";
    std::cout << "Packets Dropped: " << (totalTx - totalRx) << "\n";
    std::cout << "Packet Delivery Ratio: "
              << (100.0 * totalRx / totalTx) << "%\n";

    Simulator::Destroy();
    return 0;
}
