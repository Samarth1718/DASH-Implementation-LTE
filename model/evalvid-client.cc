#include
#include
#include
#include
#include
#include
#include
#include
#include
#include
#include
#include
#include
#include
"../../evalvid/model/evalvid-client.h"
"ns3/log.h"
"ns3/ipv4-address.h"
"ns3/nstime.h"
"ns3/inet-socket-address.h"
"ns3/socket.h"
"ns3/simulator.h"
"ns3/socket-factory.h"
"ns3/packet.h"
"ns3/uinteger.h"
<stdlib.h>
<stdio.h>
"ns3/string.h"
"http-header.h"
namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("EvalvidClient");
NS_OBJECT_ENSURE_REGISTERED (EvalvidClient);
TypeId
EvalvidClient::GetTypeId (void)
{
static TypeId tid = TypeId ("ns3::EvalvidClient")
.SetParent<Application> ()
.AddConstructor<EvalvidClient> ()
.AddAttribute("VideoId", "The Id of the video that is played.",
UintegerValue(0),
MakeUintegerAccessor(&EvalvidClient::m_videoId),
MakeUintegerChecker<uint32_t>(1))
.AddAttribute ("RemoteAddress",
"The destination Ipv4Address of the outbound packets",
Ipv4AddressValue (),
MakeIpv4AddressAccessor (&EvalvidClient::m_peerAddress),
MakeIpv4AddressChecker ())
.AddAttribute ("RemotePort", "The destination port of the outbound
packets",
UintegerValue (100),
MakeUintegerAccessor (&EvalvidClient::m_peerPort),
MakeUintegerChecker<uint16_t> ())
.AddAttribute ("ReceiverDumpFilename",
"Receiver Dump Filename",
StringValue(""),
MakeStringAccessor(&EvalvidClient::receiverDumpFileName),
MakeStringChecker())
.AddAttribute ("PacketPayload",
"Packet Payload, i.e. MTU - (SEQ_HEADER + UDP_HEADER +
IP_HEADER). "
"This is the same value used to hint video with MP4Box.
Default: 1460.",
/*In our case: MTU
- (SEQ_HEADER + TCP_HEADER +
IP_HEADER + HTTP HEADER + MPEG HEADER)
*
so:
1500 - (12
+
20
+
20
+
28
+
32)
= 1388
* But for now we use 460 to avoid TCP truncation.
* */
UintegerValue (460),
MakeUintegerAccessor (&EvalvidClient::m_packetPayload),
MakeUintegerChecker<uint16_t> ())
;
return tid;
}
EvalvidClient::EvalvidClient () : m_bitRate(80000), // default bitrate in bps
m_segmentId(0) // seems to start with 0
{
NS_LOG_FUNCTION_NOARGS ();
m_sendEvent = EventId ();
m_parser.SetApp(this); // So the parser knows where to send the received
messages
}
EvalvidClient::~EvalvidClient ()
{
NS_LOG_FUNCTION_NOARGS ();
}
void
EvalvidClient::SetRemote (Ipv4Address ip, uint16_t port)
{
m_peerAddress = ip;
m_peerPort = port;
}
MpegPlayer&
EvalvidClient::GetPlayer(){
return m_player;
}
void
EvalvidClient::DoDispose (void)
{
NS_LOG_FUNCTION_NOARGS ();
Application::DoDispose ();
}
double
EvalvidClient::GetBitRateEstimate()
{
return m_bitrateEstimate;
}
void
EvalvidClient::StartApplication (void)
{
NS_LOG_FUNCTION_NOARGS();
if (m_socket == 0)
{
TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
m_socket = Socket::CreateSocket (GetNode (), tid);
if (m_socket->GetSocketType() != Socket::NS3_SOCK_STREAM
&& m_socket->GetSocketType() != Socket::NS3_SOCK_SEQPACKET)
NS_FATAL_ERROR ("Using HTTP with an incompatible socket type. "
"HTTP requires SOCK_STREAM or SOCK_SEQPACKET. "
"In other words, use TCP instead of UDP.");
m_socket->Bind ();
m_socket->Connect (InetSocketAddress (m_peerAddress, m_peerPort));
}
receiverDumpFile.open(receiverDumpFileName.c_str(), ios::out);
if (receiverDumpFile.fail())
{
NS_FATAL_ERROR(">> EvalvidClient: Error while opening output file: " <<
receiverDumpFileName.c_str());
return;
}
m_socket->SetRecvCallback (MakeCallback (&EvalvidClient::HandleRead, this));
m_socket->SetConnectCallback(
MakeCallback(&EvalvidClient::ConnectionSucceeded, this),
MakeCallback(&EvalvidClient::ConnectionFailed, this));
}
void
EvalvidClient::ConnectionSucceeded(Ptr<Socket> socket)
{
NS_LOG_FUNCTION(this << socket);
NS_LOG_LOGIC("Connection succeeded!");
//m_connected = true;
Send(); //Request Segment
}
void
EvalvidClient::ConnectionFailed(Ptr<Socket> socket)
{
NS_LOG_FUNCTION(this << socket);
NS_LOG_LOGIC("Connection Failed");
}
void
EvalvidClient::Send (void) /* Request segment. */
{
NS_LOG_FUNCTION_NOARGS ();
Ptr<Packet> p = Create<Packet> (100);
SeqTsHeader seqTs;
seqTs.SetSeq (0);
p->AddHeader (seqTs);
HTTPHeader httpHeader;//Achilleas
httpHeader.SetSeq(1);
httpHeader.SetMessageType(HTTP_REQUEST);
httpHeader.SetVideoId(m_videoId);
httpHeader.SetResolution(m_bitRate);
httpHeader.SetSegmentId(m_segmentId++);
p->AddHeader(httpHeader);
m_socket->Send (p);
m_requestTime = Simulator::Now();
m_segment_bytes = 0;
NS_LOG_INFO (">> EvalvidClient: Sending request for video streaming to
EvalvidServer at "
<< m_peerAddress << ":" << m_peerPort);
}
void
EvalvidClient::StopApplication ()
{
NS_LOG_FUNCTION_NOARGS ();
receiverDumpFile.close();
Simulator::Cancel (m_sendEvent);
}
void
EvalvidClient::HandleRead (Ptr<Socket> socket)
{
NS_LOG_FUNCTION (this << socket);
m_parser.ReadSocket(socket);
}
void
EvalvidClient::MessageReceived(Packet message)
{
NS_LOG_FUNCTION(this << message);
MPEGHeader mpegHeader;
HTTPHeader httpHeader;
SeqTsHeader seqTs;
// Send the frame to the player
m_player.ReceiveFrame(&message); //TODO: In case frame consists of more than 1
packets, play them all together.
m_segment_bytes += message.GetSize();
m_totBytes += message.GetSize();
message.RemoveHeader(mpegHeader);
message.RemoveHeader(httpHeader);
message.RemoveHeader(seqTs);
receiverDumpFile << std::fixed <<
Simulator::Now().ToDouble(ns3::Time::S)
<<
<< seqTs.GetSeq()
<<
" << message.GetSize()
<<
std::setprecision(4) <<
std::setfill(' ') << std::setw(16) <<
std::setfill(' ') <<
std::setw(16) <<
"id "
"tcp
std::endl;
// Calculate the buffering time
switch (m_player.m_state)
{
case MPEG_PLAYER_PLAYING:
m_sumDt += m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
break;
case MPEG_PLAYER_PAUSED:
break;
case MPEG_PLAYER_DONE:
return;
default:
NS_FATAL_ERROR("WRONG STATE");
}
// If we received the last frame of the segment
if (mpegHeader.GetFrameId()!= 0 && mpegHeader.GetFrameId() %
MPEG_FRAMES_PER_SEGMENT == 0)
{
m_segmentFetchTime = Simulator::Now() - m_requestTime;
NS_LOG_INFO(Simulator::Now().GetSeconds() << " bytes: " << m_segment_bytes
<<" segmentTime: " << m_segmentFetchTime.GetSeconds()
<<" segmentRate: " << 8 * m_segment_bytes /
m_segmentFetchTime.GetSeconds());
// Feed the bitrate info to the player
AddBitRate(Simulator::Now(),
8 * m_segment_bytes / m_segmentFetchTime.GetSeconds());
Time currDt = m_player.GetRealPlayTime(mpegHeader.GetPlaybackTime());
// And tell the player to monitor the buffer level
LogBufferLevel(currDt);
Time bufferDelay;
uint32_t prevBitrate = m_bitRate;
uint32_t nextRate = 8 * m_segment_bytes / m_segmentFetchTime.GetSeconds();
CalcNextSegment(nextRate, m_bitRate, bufferDelay);
if (prevBitrate != m_bitRate)
{
m_rateChanges++;
}
if (bufferDelay == Seconds(0))
{
std::cout <<"Buffer delay!" <<std::endl;
Send();
}
else
{
m_player.SchduleBufferWakeup(bufferDelay, this); //if we want to
schedule a segment request after delaying the buffer.
}
std::cout
<< Simulator::Now().GetSeconds() << " Node: " << m_id
<< " newBitRate: " << m_bitRate << " oldBitRate: " <<
prevBitrate
<<
<<
<<
<<
<<
" estBitRate: " << GetBitRateEstimate() << " interTime: "
m_player.m_interruption_time.GetSeconds() << " T: "
currDt.GetSeconds() << " dT: "
(m_lastDt >= 0 ? (currDt - m_lastDt).GetSeconds() : 0)
" del: " << bufferDelay << std::endl;
NS_LOG_INFO("==== Last frame received. Requesting segment " <<
m_segmentId);
m_lastDt = currDt;
}
}
void
EvalvidClient::CalcNextSegment(uint32_t currRate, uint32_t & nextRate, Time &
delay)
{
nextRate = currRate;
delay = Seconds(0);
}
void
EvalvidClient::AddBitRate(Time time, double bitrate)
{
m_bitrates[time] = bitrate;
double sum = 0;
int count = 0;
for (std::map<Time, double>::iterator it = m_bitrates.begin();
it != m_bitrates.end(); ++it)
{
if (it->first < (Simulator::Now() - m_window))
{
m_bitrates.erase(it->first);
}
else
{
sum += it->second;
count++;
}
}
m_bitrateEstimate = sum / count;
}
void
EvalvidClient::LogBufferLevel(Time t)
{
m_bufferState[Simulator::Now()] = t;
for (std::map<Time, Time>::iterator it = m_bufferState.begin(); it !=
m_bufferState.end(); ++it)
{
if (it->first < (Simulator::Now() - m_window))
{
m_bufferState.erase(it->first);
}
}
}
}
