/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeOldClient
#define _EvohomeOldClient

#include <map>
#include <vector>
#include <string>
#include "jsoncpp/json.h"


class EvohomeOldClient
{
public:
	typedef struct location
	{
		std::string szLocationId;
		Json::Value *installationInfo;
		Json::Value *status;
	} _sLocation;

	EvohomeOldClient();
	EvohomeOldClient(std::string user, std::string password);
	~EvohomeOldClient();
	void cleanup();

	bool login(std::string user, std::string password);
	bool save_auth_to_file(std::string szFilename);
	bool load_auth_from_file(std::string szFilename);

	bool full_installation();
	std::string get_zone_temperature(std::string locationId, std::string zoneId, int decimals);

	Json::Value m_jFullInstallation;
	std::vector<_sLocation> m_vLocations;

	std::string get_last_error();

private:
	void init();

	std::string m_szSessionId;
	time_t m_tLastWebCall;
	std::string m_szUserId;
	std::vector<std::string> m_vEvoHeader;

	std::string m_szLastError;
};

#endif
