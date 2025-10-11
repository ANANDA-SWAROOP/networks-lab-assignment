#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include <vector>
#include <sstream>

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("BusTopologyWorking");

static void AddIfNotPresent(NetDeviceContainer &c, Ptr<NetDevice> d)
{
  for (uint32_t i = 0; i < c.GetN(); ++i)
    {
      if (c.Get(i) == d) return;
    }
  c.Add(d);
}

int main(int argc, char *argv[])
{
  std::string queueType = "DropTail";
  double simTime = 10.0;
  uint32_t numNodes = 10;
  uint32_t queueSizePackets = 50;
  uint32_t centralNode = numNodes / 2;

  CommandLine cmd;
  cmd.AddValue("queueType", "Queue type: DropTail or RED", queueType);
  cmd.AddValue("simTime", "Simulation time (s)", simTime);
  cmd.AddValue("numNodes", "Number of nodes", numNodes);
  cmd.AddValue("queueSize", "Queue size in packets", queueSizePackets);
  cmd.AddValue("central", "Central node index (0..numNodes-1)", centralNode);
  cmd.Parse(argc, argv);

  if (numNodes < 3) NS_FATAL_ERROR("Need at least 3 nodes.");

  NodeContainer nodes;
  nodes.Create(numNodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));

  std::vector<NetDeviceContainer> links;
  links.reserve(numNodes - 1);
  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      links.push_back(p2p.Install(nodes.Get(i), nodes.Get(i + 1)));
    }

  InternetStackHelper internet;
  internet.Install(nodes);

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> ifs;
  ifs.reserve(links.size());
  for (uint32_t i = 0; i < links.size(); ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << (i + 1) << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      ifs.push_back(address.Assign(links[i]));
    }

  TrafficControlHelper tch;
  if (queueType == "RED")
    {
      NS_LOG_UNCOND("Using RED queue discipline");
      tch.SetRootQueueDisc("ns3::RedQueueDisc",
                           "MinTh", DoubleValue (5.0),
                           "MaxTh", DoubleValue (15.0),
                           "QueueLimit", UintegerValue(queueSizePackets));
    }
  else
    {
      NS_LOG_UNCOND("Using DropTail (FifoQueueDisc)");
      std::string qsize = std::to_string(queueSizePackets) + "p";
      tch.SetRootQueueDisc("ns3::FifoQueueDisc",
                           "MaxSize", QueueSizeValue(QueueSize(qsize)));
    }

  if (centralNode >= numNodes) centralNode = numNodes / 2;
  NetDeviceContainer centralDevices;
  if (centralNode > 0)
    {
      AddIfNotPresent(centralDevices, links[centralNode - 1].Get(1));
    }
  if (centralNode < links.size())
    {
      AddIfNotPresent(centralDevices, links[centralNode].Get(0));
    }

  if (centralDevices.GetN() == 0)
    {
      NS_FATAL_ERROR("No central devices found to install queue-disc.");
    }

  tch.Install(centralDevices);

  uint16_t port = 9000;
  Address sinkAddress = InetSocketAddress(ifs.back().GetAddress(1), port);

  for (uint32_t i = 0; i < numNodes - 1; ++i)
    {
      OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
      onoff.SetAttribute("DataRate", StringValue("2Mbps"));
      onoff.SetAttribute("PacketSize", UintegerValue(1024));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      double start = 1.0 + i * 0.05;
      onoff.SetAttribute("StartTime", TimeValue(Seconds(start)));
      onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));
      onoff.Install(nodes.Get(i));
    }

  PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sink.Install(nodes.Get(numNodes - 1));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(simTime + 0.1));

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop(Seconds(simTime + 0.1));
  Simulator::Run();

  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  uint64_t totalLost = 0;
  std::vector<double> throughputs;
  for (auto &kv : stats)
    {
      const FlowMonitor::FlowStats &f = kv.second;
      totalLost += f.lostPackets;
      double rxBps = double(f.rxBytes) * 8.0 / simTime;
      throughputs.push_back(rxBps);
    }

  double sum = 0.0, sumSq = 0.0;
  for (double x : throughputs) { sum += x; sumSq += x * x; }
  double fairness = 0.0;
  if (!throughputs.empty() && sumSq > 0.0) fairness = (sum * sum) / (throughputs.size() * sumSq);

  NS_LOG_UNCOND("========================================");
  NS_LOG_UNCOND("Queue Type: " << queueType);
  NS_LOG_UNCOND("Central node index: " << centralNode);
  NS_LOG_UNCOND("Flows measured: " << throughputs.size());
  NS_LOG_UNCOND("Total packets dropped (FlowMonitor lostPackets): " << totalLost);
  NS_LOG_UNCOND("Jain's Fairness Index: " << fairness);
  NS_LOG_UNCOND("Simulation time: " << simTime << " s");
  NS_LOG_UNCOND("========================================");

  Simulator::Destroy();
  return 0;
}
