#pragma once
#ifndef INC_SERVER_H
#define INC_SERVER_H


#include <string>
#include <list>
#include <vector>
#include <boost/unordered_map.hpp>
#include <boost/regex.hpp>

#include "CSingleton.h"

using std::string;
using std::list;
using std::vector;
using boost::unordered_map;


enum E_CHANNEL_TYPES
{
	CHANNEL_TYPE_INVALID,
	CHANNEL_TYPE_TEMPORARY,
	CHANNEL_TYPE_SEMI_PERMANENT,
	CHANNEL_TYPE_PERMANENT
};

enum E_KICK_TYPES
{
	KICK_TYPE_INVALID,
	KICK_TYPE_CHANNEL,
	KICK_TYPE_SERVER
};


struct Client
{
	Client() :
		Id(0)
	{}
	
	unsigned int Id;

	string
		Uid,
		Nickname;

	string Desc;

	struct Channel *CurrentChannel;
};

struct Channel
{
	Channel() :
		Id(0),
		ParentId(0),
		OrderId(0),

		Type(CHANNEL_TYPE_INVALID),
		IsDefault(false),
		HasPassword(false),

		MaxClients(-1)
	{}
	
	unsigned int
		Id,
		ParentId,
		OrderId;

	string Name;
	unsigned int Type; //temporary, permanent, semi-permanent
	bool
		IsDefault,
		HasPassword;

	list<unsigned int> Clients;
	int MaxClients;
};


class CServer : public CSingleton <CServer>
{
	friend class CSingleton <CServer>;
private: //variables
	unordered_map<unsigned int, Channel *> m_Channels;
	unordered_map<unsigned int, Client *> m_Clients;

private: //constructor / deconstructor
	CServer() {}
	~CServer() {}


public: //functions
	void Initialize();

	void Login(string login, string pass, string nickname);

	void CreateChannel(string name);


public: //network callbacks
	void OnLogin(vector<string> &res);
	void OnChannelList(vector<string> &res);


public: //event callbacks
	void OnChannelCreated(boost::smatch &result);
};


#endif // INC_SERVER_H