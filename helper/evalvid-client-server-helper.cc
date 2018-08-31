#include "../../evalvid/helper/evalvid-client-server-helper.h"
#include "ns3/evalvid-client.h"
#include "ns3/evalvid-server.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
namespace ns3 {
EvalvidServerHelper::EvalvidServerHelper (){}
EvalvidServerHelper::EvalvidServerHelper (uint16_t port)
{
m_factory.SetTypeId (EvalvidServer::GetTypeId ());
SetAttribute ("Port", UintegerValue (port));
}
void
EvalvidServerHelper::SetAttribute(std::string name,const AttributeValue&value)
{
m_factory.Set (name, value);
}
ApplicationContainer EvalvidServerHelper::Install (NodeContainer c)
{
ApplicationContainer apps;
for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
{
Ptr<Node> node = *i;
m_server = m_factory.Create<EvalvidServer> ();
node->AddApplication (m_server);
apps.Add (m_server);
}
return apps;
}
Ptr<EvalvidServer> EvalvidServerHelper::GetServer (void)
{
return m_server;
}
EvalvidClientHelper::EvalvidClientHelper () {}
EvalvidClientHelper::EvalvidClientHelper (Ipv4Address ip,uint16_t port)
{
m_factory.SetTypeId (EvalvidClient::GetTypeId ());
SetAttribute ("RemoteAddress", Ipv4AddressValue (ip));
SetAttribute ("RemotePort", UintegerValue (port));
}
void
EvalvidClientHelper::SetAttribute(std::string name,const AttributeValue &value)
{
m_factory.Set (name, value);
}
ApplicationContainer EvalvidClientHelper::Install (NodeContainer c)
{
ApplicationContainer apps;
for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
{
Ptr<Node> node = *i;
Ptr<EvalvidClient> client = m_factory.Create<EvalvidClient>();
node->AddApplication (client);
apps.Add (client);
}
return apps;
}
}
