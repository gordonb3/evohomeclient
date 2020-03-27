/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeV1Client
#define _EvohomeV1Client

#include <vector>
#include <string>
#include "jsoncpp/json.h"

#include "evohome/devices.hpp"


class EvohomeClient
{
public:
	EvohomeClient();
	EvohomeClient(const std::string &user, const std::string &password);
	~EvohomeClient();
	void cleanup();

	bool login(const std::string &user, const std::string &password);
	bool save_auth_to_file(const std::string &szFilename);
	bool load_auth_from_file(const std::string &szFilename);

	bool full_installation();
	std::string get_zone_temperature(const std::string szLocationId, const std::string szZoneId, const int numDecimals);

	std::string get_last_error();

	std::vector<evohome::device::location> m_vLocations;

private:
	void init();

	Json::Value m_jFullInstallation;
	std::string m_szSessionId;
	time_t m_tLastWebCall;
	std::string m_szUserId;
	std::vector<std::string> m_vEvoHeader;
	std::string m_szLastError;
};

#endif
