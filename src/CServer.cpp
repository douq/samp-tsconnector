#include "CServer.h"
#include "CNetwork.h"
#include "CUtils.h"

#include "main.h"


void CServer::Initialize()
{
	//register notify events
	CNetwork::Get()->RegisterEvent(
		boost::regex("notifychannelcreated cid=([0-9]+) cpid=([0-9]+) channel_name=([^ ]+) channel_codec_quality=[0-9]+(?: channel_maxclients=([0-9]+))? channel_order=([0-9]+)(?: channel_flag_([^=]+)=1)?(?: channel_flag_([^=]+)=1)? .*invokerid=([0-9]+) invokername=([^ ]+) invokeruid=([^ \n\r]+)"),
		boost::bind(&CServer::OnChannelCreated, this, _1));

	CNetwork::Get()->RegisterEvent(
		boost::regex("notifychanneldeleted invokerid=([0-9]+) invokername=([^ ]+) invokeruid=([^ ]+) cid=([0-9]+)"),
		boost::bind(&CServer::OnChannelDeleted, this, _1));
	
	//onchannelreorder
	//notifychanneledited cid=([0-9]+) reasonid=10 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ ]+ channel_order=([0-9]+)

	//onchannelmoved
	//notifychannelmoved cid=([0-9]+) cpid=([0-9]+) order=([0-9]+) reasonid=1 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ \n\r]+

	//onchannelrenamed
	//notifychanneledited cid=([0-9]+) reasonid=10 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ ]+ channel_name=([^ \n\r]+)

	//onchannelpasswordtoggled
	//notifychanneledited cid=([0-9]+) reasonid=10 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ ]+ channel_flag_password=([01])

	//onchannelpasswordchanged
	//notifychannelpasswordchanged cid=([0-9]+)
	//NOTE: always called if something with password changed, check if channel_flag_password was changed before fireing "pass-changed" callback

	//onchanneltypechanged
	//notifychanneledited cid=([0-9]+) reasonid=10 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ ]+ (channel_flag_(?:permanent|semi_permanent).*)
	//DATA:
	//	1: cid
	//	2: flags (example: "channel_flag_permanent=0 channel_flag_semi_permanent=1")

	//onchannelsetdefault
	//notifychanneledited cid=([0-9]+) reasonid=10 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ ]+ channel_flag_default=1.*

	//onchannelmaxclientschanged
	//notifychanneledited cid=([0-9]+) reasonid=10 invokerid=[0-9]+ invokername=[^ ]+ invokeruid=[^ ]+ channel_maxclients=([-0-9]+).*


	CNetwork::Get()->Execute("servernotifyregister event=server");
	CNetwork::Get()->Execute("servernotifyregister event=channel id=0");


	//fill up cache
	CNetwork::Get()->Execute(str(fmt::Writer() << "channellist -flags -limit"),
		boost::bind(&CServer::OnChannelList, this, _1));
}

CServer::~CServer()
{
	for (unordered_map<unsigned int, Channel *>::iterator i = m_Channels.begin(), end = m_Channels.end(); i != end; ++i)
		delete i->second;

	for (unordered_map<unsigned int, Client *>::iterator i = m_Clients.begin(), end = m_Clients.end(); i != end; ++i)
		delete i->second;
}



bool CServer::Login(string login, string pass)
{
	if (m_IsLoggedIn == false)
		return false;
	

	CUtils::Get()->EscapeString(login);
	CUtils::Get()->EscapeString(pass);
	
	CNetwork::Get()->Execute(str(fmt::Format("login client_login_name={} client_login_password={}", login, pass)),
		boost::bind(&CServer::OnLogin, this, _1));
	return true;
}

bool CServer::ChangeNickname(string nickname)
{
	if (m_IsLoggedIn == false)
		return false;


	CUtils::Get()->EscapeString(nickname);
	CNetwork::Get()->Execute(str(fmt::Format("clientupdate client_nickname={}", nickname)));
	return true;
}



bool CServer::CreateChannel(string name)
{
	if (m_IsLoggedIn == false)
		return false;

	if (name.empty())
		return false;


	CUtils::Get()->EscapeString(name);
	CNetwork::Get()->Execute(str(fmt::Writer() << "channelcreate channel_name=" << name));
	return true;
}

bool CServer::DeleteChannel(Channel::Id_t cid)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;


	delete m_Channels.at(cid);
	m_Channels.erase(cid);

	CNetwork::Get()->Execute(str(fmt::Format("channeldelete cid={} force=1", cid)));
	return true;
}

bool CServer::SetChannelName(Channel::Id_t cid, string name)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;

	if (name.empty())
		return false;


	m_Channels.at(cid)->Name = name;

	CUtils::Get()->EscapeString(name);
	CNetwork::Get()->Execute(str(fmt::Format("channeledit cid={} channel_name={}", cid, name)));
	return true;
}

bool CServer::SetChannelDescription(Channel::Id_t cid, string desc)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;

	//TODO: check if empty desc is OK
	//if (desc.empty())
	//	return false;


	CUtils::Get()->EscapeString(desc);
	CNetwork::Get()->Execute(str(fmt::Format("channeledit cid={} channel_description={}", cid, desc)));
	return true;
}

bool CServer::SetChannelType(Channel::Id_t cid, unsigned int type)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;


	string type_flag_str;
	switch(type) 
	{
		case CHANNEL_TYPE_PERMANENT:
			type_flag_str = "channel_flag_permanent";
		break;

		case CHANNEL_TYPE_SEMI_PERMANENT:
			type_flag_str = "channel_flag_semi_permanent";
		break;

		case CHANNEL_TYPE_TEMPORARY:
			type_flag_str = "channel_flag_temporary";
		break;

		default:
			return false;
	}

	CNetwork::Get()->Execute(str(fmt::Format("channeledit cid={} {}=1", cid, type_flag_str)));
	return true;
}

bool CServer::SetChannelPassword(Channel::Id_t cid, string password)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;


	m_Channels.at(cid)->HasPassword = (password.empty() == false);

	CUtils::Get()->EscapeString(password);
	CNetwork::Get()->Execute(str(fmt::Format("channeledit cid={} channel_password={}", cid, password)));
	return true;
}

bool CServer::SetChannelUserLimit(Channel::Id_t cid, int maxusers)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;

	if (maxusers < -1)
		return false;


	m_Channels.at(cid)->MaxClients = maxusers;

	CNetwork::Get()->Execute(str(fmt::Format("channeledit cid={} channel_maxclients={}", cid, maxusers)));
	return true;
}

bool CServer::SetChannelParentId(Channel::Id_t cid, Channel::Id_t pcid)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;

	if (IsValidChannel(pcid) == false)
		return false;


	m_Channels.at(cid)->ParentId = pcid;

	CNetwork::Get()->Execute(str(fmt::Format("channelmove cid={} cpid={}", cid, pcid)));
	return true;
}

bool CServer::SetChannelOrderId(Channel::Id_t cid, Channel::Id_t ocid)
{
	if (m_IsLoggedIn == false)
		return false;

	if (IsValidChannel(cid) == false)
		return false;

	if (IsValidChannel(ocid) == false)
		return false;


	m_Channels.at(cid)->OrderId = ocid;

	CNetwork::Get()->Execute(str(fmt::Format("channeledit cid={} channel_order={}", cid, ocid)));
	return true;
}



void CServer::OnLogin(vector<string> &res)
{
	Initialize();
	m_IsLoggedIn = true;
}

void CServer::OnChannelList(vector<string> &res)
{
	/*
	data (newline = space):
		cid=1
		pid=0
		channel_order=0
		channel_name=Default\sChannel
		channel_flag_default=1
		channel_flag_password=0
		channel_flag_permanent=1
		channel_flag_semi_permanent=0
		channel_needed_subscribe_power=0
		....
		channel_maxclients=-1
	*/
	for (vector<string>::iterator i = res.begin(), end = res.end(); i != end; ++i)
	{
		unsigned int
			cid = 0,
			pid = 0,
			order = 0,
			is_default = 0,
			has_password = 0,
			is_permanent = 0,
			is_semi_perm = 0;

		int max_clients = -1;

		string name;

		CUtils::Get()->ParseField(*i, "cid", cid);
		CUtils::Get()->ParseField(*i, "pid", pid);
		CUtils::Get()->ParseField(*i, "channel_order", order);
		CUtils::Get()->ParseField(*i, "channel_name", name);
		CUtils::Get()->ParseField(*i, "channel_flag_default", is_default);
		CUtils::Get()->ParseField(*i, "channel_flag_password", has_password);
		CUtils::Get()->ParseField(*i, "channel_flag_permanent", is_permanent);
		CUtils::Get()->ParseField(*i, "channel_flag_semi_permanent", is_semi_perm);
		CUtils::Get()->ParseField(*i, "channel_maxclients", max_clients);

		CUtils::Get()->UnEscapeString(name);


		Channel *chan = new Channel;
		chan->ParentId = pid;
		chan->OrderId = order;
		chan->Name = name;
		chan->HasPassword = has_password != 0;
		if (is_permanent != 0)
			chan->Type = CHANNEL_TYPE_PERMANENT;
		else if (is_semi_perm != 0)
			chan->Type = CHANNEL_TYPE_SEMI_PERMANENT;
		else
			chan->Type = CHANNEL_TYPE_TEMPORARY;
		chan->MaxClients = max_clients;

		if (is_default != 0)
			m_DefaultChannel = cid;
		m_Channels.insert(unordered_map<unsigned int, Channel *>::value_type(cid, chan));
	}
}



void CServer::OnChannelCreated(boost::smatch &result)
{
	unsigned int
		id = 0,
		parent_id = 0,
		order_id = 0;
	int maxclients = -1;
	unsigned int type = CHANNEL_TYPE_INVALID;
	string name;

	//unsigned int creator_id = 0;
	//string
	//	creator_name,
	//	creator_uid;

	CUtils::Get()->ConvertStringToInt(result[1].str(), id);
	CUtils::Get()->ConvertStringToInt(result[2].str(), parent_id);
	name = result[3].str();
	CUtils::Get()->ConvertStringToInt(result[4].str(), maxclients);
	CUtils::Get()->ConvertStringToInt(result[5].str(), order_id);
	string type_flag_str(result[6].str());
	if (type_flag_str.find("channel_flag_permanent") != string::npos)
		type = CHANNEL_TYPE_PERMANENT;
	else if (type_flag_str.find("channel_flag_semi_permanent") != string::npos)
		type = CHANNEL_TYPE_SEMI_PERMANENT;
	else
		type = CHANNEL_TYPE_TEMPORARY;
	string extra_flag_str(result[7].str());
	//CUtils::Get()->ConvertStringToInt(result[8].str(), creator_id);
	//creator_name = result[9].str();
	//creator_uid = result[10].str();


	Channel *chan = new Channel;
	chan->ParentId = parent_id;
	chan->OrderId = order_id;
	chan->Name = name;
	chan->Type = type;
	chan->HasPassword = (extra_flag_str.find("password") != string::npos);

	if (extra_flag_str.find("default") != string::npos)
		m_DefaultChannel = id;
	m_Channels.insert(unordered_map<unsigned int, Channel *>::value_type(id, chan));
}

void CServer::OnChannelDeleted(boost::smatch &result)
{
	/*unsigned int invoker_id = 0;
	string
		invoker_name,
		invoker_uid;*/
	unsigned int cid = 0;

	//CUtils::Get()->ConvertStringToInt(result[1].str(), invoker_id);
	//invoker_name = result[2].str();
	//invoker_uid = result[3].str();
	CUtils::Get()->ConvertStringToInt(result[4].str(), cid);

	m_Channels.erase(cid);
}
