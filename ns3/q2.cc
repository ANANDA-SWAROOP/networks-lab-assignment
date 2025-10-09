#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiredMeshUDP");

int main (int argc, char *argv[])
{
    uint32_t nNodes = 5;
    bool broadcastMode = true; // default
    double simTime = 10.0;

    CommandLine cmd;
    cmd.AddValue("broadcastMode", "true for one-to-all, false for one-to-one", broadcastMode);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nNodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Fully connected mesh: connect every pair of nodes
    std::vector<NetDeviceContainer> linkDevices;
    std::vector<Ipv4InterfaceContainer> linkInterfaces;
    uint32_t subnet = 1;

    for (uint32_t i = 0; i < nNodes; i++)
    {
        for (uint32_t j = i + 1; j < nNodes; j++)
        {
            NodeContainer pair(nodes.Get(i), nodes.Get(j));
            NetDeviceContainer nd = p2p.Install(pair);
            linkDevices.push_back(nd);

            std::ostringstream subnetAddr;
            subnetAddr << "10.1." << subnet++ << ".0";
            Ipv4AddressHelper ipv4;
            ipv4.SetBase(subnetAddr.str().c_str(), "255.255.255.0");
            Ipv4InterfaceContainer iface = ipv4.Assign(nd);
            linkInterfaces.push_back(iface);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9000;

    // Application setup
    if (broadcastMode)
    {
        // Node 0 sends to all other nodes (one-to-many)
        for (uint32_t i = 1; i < nNodes; i++)
        {
            OnOffHelper onoff("ns3::UdpSocketFactory",
                              InetSocketAddress(linkInterfaces[i - 1].GetAddress(1), port));
            onoff.SetConstantRate(DataRate("10Mbps"));
            onoff.SetAttribute("PacketSize", UintegerValue(1024));
            ApplicationContainer sender = onoff.Install(nodes.Get(0));
            sender.Start(Seconds(1.0));
            sender.Stop(Seconds(simTime - 1));

            // Packet sink on each receiver
            PacketSinkHelper sink("ns3::UdpSocketFactory",
                                  InetSocketAddress(Ipv4Address::GetAny(), port));
            ApplicationContainer receiver = sink.Install(nodes.Get(i));
            receiver.Start(Seconds(0.5));
            receiver.Stop(Seconds(simTime));
        }
    }
    else
    {
        // One-to-one scenario: Node 0 -> Node 1
        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(linkInterfaces[0].GetAddress(1), port));
        onoff.SetConstantRate(DataRate("10Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer sender = onoff.Install(nodes.Get(0));
        sender.Start(Seconds(1.0));
        sender.Stop(Seconds(simTime - 1));

        PacketSinkHelper sink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer receiver = sink.Install(nodes.Get(1));
        receiver.Start(Seconds(0.5));
        receiver.Stop(Seconds(simTime));
    }

    // Flow Monitor to gather metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // NetAnim visualization
    AnimationInterface anim("mesh-udp-performance.xml");
    for (uint32_t i = 0; i < nNodes; i++)
    {
        anim.SetConstantPosition(nodes.Get(i), i * 25, 25 * (i % 2));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalThroughput = 0.0;
    double totalDelay = 0.0;
    uint32_t receivedPackets = 0;
    uint32_t sentPackets = 0;

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds()) / 1e6;
        double delay = (flow.second.delaySum.GetSeconds()) / flow.second.rxPackets;

        totalThroughput += throughput;
        totalDelay += delay;
        receivedPackets += flow.second.rxPackets;
        sentPackets += flow.second.txPackets;
    }

    double avgThroughput = totalThroughput / stats.size();
    double avgDelay = totalDelay / stats.size();
    double pdr = (double)receivedPackets / sentPackets * 100.0;

    std::cout << "\n================= Simulation Results =================" << std::endl;
    std::cout << "Mode: " << (broadcastMode ? "Broadcast (1-to-All)" : "Unicast (1-to-1)") << std::endl;
    std::cout << "Average Throughput: " << avgThroughput << " Mbps" << std::endl;
    std::cout << "Average Delay: " << avgDelay * 1000 << " ms" << std::endl;
    std::cout << "Packet Delivery Ratio: " << pdr << " %" << std::endl;
    std::cout << "=====================================================\n" << std::endl;

    monitor->SerializeToXmlFile("mesh-udp-performance.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}
