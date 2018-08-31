#include "ns3/csma-helper.h"
#include "ns3/evalvid-client-server-helper.h"
#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("Lena_Example");
int main (int argc, char *argv[])
{
/*
Enable log components.*/
LogComponentEnable ("EvalvidClient", LOG_LEVEL_INFO);
LogComponentEnable ("EvalvidServer", LOG_LEVEL_ALL);
/*
Declare variables.*/
uint16_t numberOfNodes = 1;
double distance = 60.0;
uint16_t serverPort = 1124;
/*
Create Helpers*/
Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>
();
lteHelper->SetEpcHelper (epcHelper);
Ptr<Node> pgw = epcHelper->GetPgwNode ();
/*
Create our remote host. Install the Internet stack.*/
NodeContainer remoteHostContainer;
remoteHostContainer.Create (1);
Ptr<Node> remoteHost = remoteHostContainer.Get (0);
InternetStackHelper internet;
internet.Install (remoteHostContainer);
/*
Create a P2P connection between the P-GW and our remote host. Assign IP
addresses.*/
PointToPointHelper p2ph;
p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
Ipv4AddressHelper ipv4h;
ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
/*
Add a static network route for our remote host.*/
Ipv4StaticRoutingHelper ipv4RoutingHelper;
Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address ("7.0.0.0"), Ipv4Mask
("255.0.0.0"), 1);
/*
Create our UE and eNodeB nodes. Install LTE functionality, Internet stack.*/
NodeContainer ueNodes, enbNodes;
enbNodes.Create(1);
ueNodes.Create(numberOfNodes);
NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);
internet.Install (ueNodes);
/*
Create a mobility model and install our nodes in it. (Initial position is
defined by ListPositionAllocator.)*/
Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>
();
for (uint16_t i = 0; i < numberOfNodes; i++)
{
positionAlloc->Add (Vector(distance * i, 0, 0));
}
MobilityHelper mobility;
mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
mobility.SetPositionAllocator(positionAlloc);
mobility.Install(enbNodes);
mobility.Install(ueNodes);
/*
*
*
*
1.Assign IP addresses to our UE nodes.
2.Add a static network route for each one.
3.Set P-GW as the Default Gateway.
4.Attach to eNodeB.*/
Ipv4InterfaceContainer ueIpIface;
ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
{
Ptr<Node> ueNode = ueNodes.Get (u);
Ptr<Ipv4StaticRouting> ueStaticRouting =
ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress
(), 1);
}
for (uint16_t i = 0; i < numberOfNodes; i++)
{
lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(0));
// side effect: the default EPS bearer will be activated
}
NS_LOG_INFO ("Create Applications.");
/*
Create Server application. Install on our remote host.*/
EvalvidServerHelper server(serverPort);
server.SetAttribute ("SenderTraceFilename", StringValue("st_highway_cif.st"));
server.SetAttribute ("SenderDumpFilename" , StringValue("sd_a01_lte"));
ApplicationContainer apps = server.Install(remoteHostContainer.Get(0));
apps.Start (Seconds (1.0));
apps.Stop (Seconds (10.0));
/*
Create Client applications. Install on our UE nodes.*/
for (uint16_t i = 0; i < numberOfNodes; i++)
{
EvalvidClientHelper client (internetIpIfaces.GetAddress
(1),serverPort);//serverIP,serverPort
std::ostringstream ReceiverDumpFilename;
ReceiverDumpFilename <<"rd_a0" <<i <<"_lte";
client.SetAttribute ("ReceiverDumpFilename",
StringValue(ReceiverDumpFilename.str()));
client.SetAttribute ("VideoId", UintegerValue(1));
apps = client.Install (ueNodes.Get(i));
apps.Start (Seconds (1.1));
apps.Stop (Seconds (9.9)); //Note: Clients start after and finish
before the Server.
}
lteHelper->EnableTraces (); //Enables trace sinks for PHY, MAC, RLC and PDCP.
NS_LOG_INFO ("Run Simulation.");
Simulator::Stop(Seconds(10));//Set Simulation stop time.
Simulator::Run();
//Start Simulation.
Simulator::Destroy();
return 0;
}
