#include "../../evalvid/model/evalvid-server.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include <ns3/tcp-socket.h>
#include "mpeg-header.h"
#include "http-header.h"
using namespace std;
namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("EvalvidServer");
NS_OBJECT_ENSURE_REGISTERED (EvalvidServer);
TypeId
EvalvidServer::GetTypeId (void)
{
static TypeId tid = TypeId ("ns3::EvalvidServer")
.SetParent<Application> ()
.AddConstructor<EvalvidServer> ()
.AddAttribute ("Port",
"Port on which we listen for incoming packets.",
UintegerValue (100),
MakeUintegerAccessor (&EvalvidServer::m_port),
MakeUintegerChecker<uint16_t> ())
.AddAttribute ("SenderDumpFilename",
"Sender Dump Filename",
StringValue(""),
MakeStringAccessor(&EvalvidServer::m_senderTraceFileName),
MakeStringChecker())
.AddAttribute ("SenderTraceFilename",
"Sender trace Filename",
StringValue(""),
MakeStringAccessor(&EvalvidServer::m_videoTraceFileName),
MakeStringChecker())
.AddAttribute ("PacketPayload",
"MTU",
/*In our case:
MTU
- (SEQ_HEADER + TCP_HEADER + IP_HEADER + HTTP HEADER + MPEG
HEADER) so: 1500 - (12 + 20 + 20 + 28 + 32) = 1388
But for now we use 460 to avoid TCP truncation.*
*/
UintegerValue (460),
MakeUintegerAccessor (&EvalvidServer::m_packetPayload),
MakeUintegerChecker<uint16_t> ())
;
return tid;
}
EvalvidServer::EvalvidServer ()
{
m_socket = 0;
m_port = 0;
m_numOfFrames = 0;
m_packetPayload = 0;
m_packetId = 0;
m_sendEvent = EventId ();
packetcount = 0;
m_totalRx = 0;
videoId = -1;
}
EvalvidServer::~EvalvidServer ()
{
NS_LOG_FUNCTION (this);
}
void
EvalvidServer::DoDispose (void)
{
NS_LOG_FUNCTION (this);
Application::DoDispose ();
}
void
EvalvidServer::StartApplication (void)
{
NS_LOG_FUNCTION_NOARGS();
Ptr<Socket> socket = 0;
if (socket == 0)
{
TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
socket = Socket::CreateSocket (GetNode (), tid);
InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (),
m_port);
socket->Bind (local);
socket->Listen();
socket->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead,
this));
socket->SetAcceptCallback(
MakeNullCallback<bool, Ptr<Socket>, const Address
&>(),
MakeCallback(&EvalvidServer::HandleAccept, this));
}
/*
//For IPv6 only:
Ptr<Socket> socket6 = 0;
if (socket6 == 0)
{
TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");//A
socket6 = Socket::CreateSocket (GetNode (), tid);
Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny
(), m_port);
socket6->Bind (local);
socket6->Listen();
socket6->SetRecvCallback (MakeCallback (&EvalvidServer::HandleRead,
this));
}*/
Setup(); //Setup the video(s) for distribution.
}
void
EvalvidServer::StopApplication ()
{
NS_LOG_FUNCTION_NOARGS();
//Simulator::Cancel (m_sendEvent);
}
void
EvalvidServer::Setup()
{
NS_LOG_FUNCTION_NOARGS();
m_videoInfoStruct_t *videoInfoStruct;
uint32_t frameId;
string frameType;
uint32_t frameSize;
uint16_t numOfTcpPackets;
double sendTime;
double lastSendTime = 0.0;
ifstream videoTraceFile(m_videoTraceFileName.c_str(), ios::in);
if (videoTraceFile.fail())
{
NS_FATAL_ERROR(">> EvalvidServer: Error while opening video trace
file: " << m_videoTraceFileName.c_str());
return;
}
while (videoTraceFile >> frameId >> frameType >> frameSize >> numOfTcpPackets
>> sendTime)
{
videoInfoStruct = new m_videoInfoStruct_t;
videoInfoStruct->frameType = frameType;
videoInfoStruct->frameSize = frameSize;
videoInfoStruct->numOfTcpPackets = frameSize/m_packetPayload;
videoInfoStruct->packetInterval = Seconds(sendTime - lastSendTime);
m_videoInfoMap.insert (pair<uint32_t, m_videoInfoStruct_t*>(frameId,
videoInfoStruct));
//NS_LOG_LOGIC(">> EvalvidServer: " << frameId << "\t" << frameType
<< "\t" <<frameSize << "\t" << numOfTcpPackets << "\t" << sendTime);
lastSendTime = sendTime;
}
m_numOfFrames = frameId;
m_videoInfoMapIt = m_videoInfoMap.begin();
m_senderTraceFile.open(m_senderTraceFileName.c_str(), ios::out);
if (m_senderTraceFile.fail())
{
NS_FATAL_ERROR(">> EvalvidServer: Error while opening sender trace
file: " << m_senderTraceFileName.c_str());
return;
}
}
void
EvalvidServer::Send () /*Sends one frame at a time! Calls itself until the last frame
of the segment is sent.*/
{
NS_LOG_FUNCTION( this << Simulator::Now().GetSeconds());
if (m_videoInfoMapIt != m_videoInfoMap.end())
{
for(int i=0; i<m_videoInfoMapIt->second->numOfTcpPackets; i++)
{
Ptr<Packet> p = Create<Packet> (m_packetPayload);//originally 1460 bytes
m_packetId++;
m_senderTraceFile << std::fixed << std::setprecision(4) <<
Simulator::Now().ToDouble(Time::S)
<< std::setfill(' ') << std::setw(16) << "id " <<
m_packetId
<< std::setfill(' ') << std::setw(16) << "tcp " << p-
>GetSize()
<< std::endl;
/* Add headers to the packet. */
SeqTsHeader seqTs;
seqTs.SetSeq (m_packetId);
HTTPHeader http_header;
http_header.SetMessageType(HTTP_RESPONSE);
http_header.SetVideoId(videoId);
http_header.SetResolution(clientResolution);
http_header.SetSegmentId(segmentId);
MPEGHeader mpeg_header;
mpeg_header.SetFrameId(m_videoInfoMapIt->first);
mpeg_header.SetPlaybackTime(
MilliSeconds(
(m_videoInfoMapIt->first - 1)
* MPEG_TIME_BETWEEN_FRAMES)); //50 FPS
mpeg_header.SetType(m_videoInfoMapIt->second->frameType);
mpeg_header.SetSize(m_videoInfoMapIt->second->frameSize);
p->AddHeader (seqTs);
p->AddHeader(http_header);
p->AddHeader(mpeg_header);
packetcount++;
std::cout <<"Server: bytes sent->"<<p->GetSize() <<" frame size->"
<<mpeg_header.GetSize() <<" packetcount:"<<packetcount <<std::endl;
m_socket->SendTo(p, 0, m_peerAddress);
}
Ptr<Packet> p = Create<Packet> (m_videoInfoMapIt->second->frameSize %
m_packetPayload);
m_packetId++;
m_senderTraceFile << std::fixed << std::setprecision(4)
<< Simulator::Now().ToDouble(Time::S)
<< std::setfill(' ') << std::setw(16) << "id " << m_packetId
<< std::setfill(' ') << std::setw(16) << "tcp " << p>GetSize()
<< std::setfill(' ') << std::setw(16) << "frame id "
<< m_videoInfoMapIt->first << std::endl;
/* Add headers to the packet. */
SeqTsHeader seqTs;
seqTs.SetSeq (m_packetId);
HTTPHeader http_header;
http_header.SetMessageType(HTTP_RESPONSE);
http_header.SetVideoId(videoId);
http_header.SetResolution(clientResolution);
http_header.SetSegmentId(segmentId);
MPEGHeader mpeg_header;
mpeg_header.SetFrameId(m_videoInfoMapIt->first);
mpeg_header.SetPlaybackTime(
MilliSeconds((m_videoInfoMapIt->first + (segmentId *
MPEG_FRAMES_PER_SEGMENT))
* MPEG_TIME_BETWEEN_FRAMES)); //50 fps
mpeg_header.SetType(m_videoInfoMapIt->second->frameType);
mpeg_header.SetSize(m_videoInfoMapIt->second->frameSize);
p->AddHeader (seqTs);
p->AddHeader(http_header);
p->AddHeader(mpeg_header);
packetcount++;
std::cout <<"Server: bytes sent->"<<p->GetSize() <<" frame size->"
<<mpeg_header.GetSize() <<" packetcount:"<<packetcount <<std::endl;
m_socket->SendTo(p, 0, m_peerAddress);
m_videoInfoMapIt++;
if (m_videoInfoMapIt == m_videoInfoMap.end())
{
NS_LOG_INFO(">> EvalvidServer: Video streaming successfully completed!");
}
else if ((m_videoInfoMapIt->first - 1) % MPEG_FRAMES_PER_SEGMENT == 0)
{
NS_LOG_INFO(">> EvalvidServer: Sending segment " <<segmentId <<"
complete!");
}
else
{
if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0)
{
m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
}
else
{
m_sendEvent = Simulator::Schedule (m_videoInfoMapIt->second->
packetInterval, &EvalvidServer::Send, this);
}
}
else
{
NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
}
}
void
EvalvidServer::HandleRead (Ptr<Socket> socket)
{
//NS_LOG_FUNCTION_NOARGS();
Ptr<Packet> packet;
Address from;
m_socket = socket;
while ((packet = socket->RecvFrom (from)))
{
m_peerAddress = from;
m_totalRx += packet->GetSize();
/*Extract info from received client request packet. */
HTTPHeader header;
packet->RemoveHeader(header);
videoId = header.GetVideoId();
segmentId = header.GetSegmentId();
clientResolution = header.GetResolution();
if (InetSocketAddress::IsMatchingType (from))
{
NS_LOG_INFO (">> EvalvidServer: Client at " <<
InetSocketAddress::ConvertFrom (from).GetIpv4 ()
<< " is requesting a video streaming.");
}
/*//For IPv6 only:
else if (Inet6SocketAddress::IsMatchingType (from))
{
NS_LOG_INFO (">> EvalvidServer: Client at " <<
Inet6SocketAddress::ConvertFrom (from).GetIpv6 ()
<< " is requesting a video streaming.");
}*/
if (m_videoInfoMapIt != m_videoInfoMap.end())
{
NS_LOG_INFO(">> EvalvidServer: Starting video streaming...");
if (m_videoInfoMapIt->second->packetInterval.GetSeconds() == 0)
{
m_sendEvent = Simulator::ScheduleNow (&EvalvidServer::Send, this);
}
else
{
m_sendEvent = Simulator::Schedule(m_videoInfoMapIt->second->
packetInterval, &EvalvidServer::Send, this);
}
}
else
{
NS_FATAL_ERROR(">> EvalvidServer: Frame does not exist!");
}
//m_rxTrace(packet, from);
}
}
void
EvalvidServer::HandleAccept(Ptr<Socket> s, const Address& from)
{
NS_LOG_FUNCTION(this << s << from);
s->SetRecvCallback(MakeCallback(&EvalvidServer::HandleRead, this));
}
}
