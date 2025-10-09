// dumbbell-tcp.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DumbbellTcpExample");

// Global log file pointers for cwnd tracing
static Ptr<OutputStreamWrapper> cwndStream1;
static Ptr<OutputStreamWrapper> cwndStream2;

void
CwndTracer1 (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << "\n";
}
void
CwndTracer2 (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << newCwnd << "\n";
}

int
main (int argc, char *argv[])
{
    uint32_t nLeft = 2;
    uint32_t nRight = 2;
    std::string tcpVariant = "ns3::TcpNewReno";  // default
    double simTime = 20.0;  // seconds
    std::string animFile = "dumbbell-tcp.xml";

    CommandLine cmd;
    cmd.AddValue ("tcpVariant", "TCP variant to use: ns3::TcpTahoe, ns3::TcpReno, ns3::TcpNewReno, ns3::TcpVegas, ns3::TcpVeno, ns3::TcpCubic", tcpVariant);
    cmd.AddValue ("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue ("animFile", "NetAnim XML output file", animFile);
    cmd.Parse (argc, argv);

    // Select TCP variant
    if (tcpVariant.compare ("ns3::TcpTahoe") == 0)
    {
        TypeId tcpTid = TypeId::LookupByName ("ns3::TcpTahoe");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));
    }
    else if (tcpVariant.compare ("ns3::TcpReno") == 0)
    {
        TypeId tcpTid = TypeId::LookupByName ("ns3::TcpReno");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));
    }
    else if (tcpVariant.compare ("ns3::TcpNewReno") == 0)
    {
        TypeId tcpTid = TypeId::LookupByName ("ns3::TcpNewReno");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));
    }
    else if (tcpVariant.compare ("ns3::TcpVegas") == 0)
    {
        TypeId tcpTid = TypeId::LookupByName ("ns3::TcpVegas");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));
    }
    else if (tcpVariant.compare ("ns3::TcpVeno") == 0)
    {
        TypeId tcpTid = TypeId::LookupByName ("ns3::TcpVeno");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));
    }
    else if (tcpVariant.compare ("ns3::TcpCubic") == 0)
    {
        TypeId tcpTid = TypeId::LookupByName ("ns3::TcpCubic");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (tcpTid));
    }
    else
    {
        NS_FATAL_ERROR ("Unsupported TCP variant. Supported variants: ns3::TcpTahoe, ns3::TcpReno, ns3::TcpNewReno, ns3::TcpVegas, ns3::TcpVeno, ns3::TcpCubic");
    }

    // Create topology: dumbbell helper
    // Configure the helpers
    PointToPointHelper leftLeaf; 
    leftLeaf.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    leftLeaf.SetChannelAttribute ("Delay", StringValue ("2ms"));
    
    PointToPointHelper rightLeaf; 
    rightLeaf.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    rightLeaf.SetChannelAttribute ("Delay", StringValue ("2ms"));
    
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    bottleneck.SetChannelAttribute ("Delay", StringValue ("20ms"));
    
    // Create dumbbell helper with correct constructor signature
    PointToPointDumbbellHelper db (nLeft, leftLeaf, nRight, rightLeaf, bottleneck);

    // Install Internet stack
    InternetStackHelper stack;
    db.InstallStack (stack);

    // Assign IP addresses
    db.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

    // Install applications: BulkSend (client) & PacketSink (server)
    uint16_t port = 50000;

    for (uint32_t i = 0; i < nLeft; ++i)
    {
        Ptr<Node> leftNode = db.GetLeft (i);
        Ptr<Node> rightNode = db.GetRight (i);
        // BulkSend from left to right
        BulkSendHelper bulk ("ns3::TcpSocketFactory",
                             InetSocketAddress (db.GetRightIpv4Address (i), port));
        bulk.SetAttribute ("MaxBytes", UintegerValue (0)); // unlimited
        ApplicationContainer cApps = bulk.Install (leftNode);
        cApps.Start (Seconds (1.0));
        cApps.Stop (Seconds (simTime - 0.5));

        // Sink on server (right side)
        PacketSinkHelper sink ("ns3::TcpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), port));
        ApplicationContainer sApps = sink.Install (rightNode);
        sApps.Start (Seconds (0.0));
        sApps.Stop (Seconds (simTime));
    }

    // Enable NetAnim
    AnimationInterface anim (animFile);
    // Optionally set node positions:
    for (uint32_t i = 0; i < nLeft; i++)
    {
        anim.SetConstantPosition (db.GetLeft (i), 0.0, i * 20.0);
    }
    for (uint32_t i = 0; i < nRight; i++)
    {
        anim.SetConstantPosition (db.GetRight (i), 100.0, i * 20.0);
    }
    // Position central routers (GetLeft() and GetRight() without parameters return the bridge nodes)
    anim.SetConstantPosition (db.GetLeft (), 40.0, 20.0);
    anim.SetConstantPosition (db.GetRight (), 60.0, 20.0);

    // Tracing: cwnd (commented out due to timing issues)
    // TODO: Implement proper cwnd tracing with correct timing
    /*
    std::ostringstream fname1, fname2;
    fname1 << "cwnd-flow1-" << tcpVariant << ".txt";
    fname2 << "cwnd-flow2-" << tcpVariant << ".txt";
    cwndStream1 = Create<OutputStreamWrapper> (fname1.str ().c_str (), std::ios::out);
    cwndStream2 = Create<OutputStreamWrapper> (fname2.str ().c_str (), std::ios::out);
    */

    // Run simulation
    Simulator::Stop (Seconds (simTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}
