/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <fstream>
#include "evohomeclient.h"
#include <stdexcept>

#include "connection/EvoHTTPBridge.hpp"
#include "evohome/jsoncppbridge.hpp"


#define EVOHOME_HOST "https://tccna.honeywell.com"

#define SESSION_EXPIRATION_TIME 899


/*
 * Class construct
 */
EvohomeClient::EvohomeClient()
{
	init();
}
EvohomeClient::EvohomeClient(const std::string &user, const std::string &password)
{
	init();
	login(user, password);
}

EvohomeClient::~EvohomeClient()
{
	cleanup();
}

/************************************************************************
 *									*
 *	Curl helpers							*
 *									*
 ************************************************************************/

/*
 * Initialize curl web client
 */
void EvohomeClient::init()
{
}


/*
 * Cleanup curl web client
 */
void EvohomeClient::cleanup()
{
	std::cout << "cleanup (v1) not implemented yet\n";
}


std::string EvohomeClient::get_last_error()
{
	return m_szLastError;
}


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/


/* 
 * login to evohome web server
 */
bool EvohomeClient::login(const std::string &user, const std::string &password)
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	vLoginHeader.push_back("content-type: application/json");

	std::string szPostdata = "{'Username': '";
	szPostdata.append(user);
	szPostdata.append("', 'Password': '");
	szPostdata.append(EvoHTTPBridge::URLEncode(password));
	szPostdata.append("', 'ApplicationId': '91db1612-73fd-4500-91b2-e63b069b185c'}");

	std::string szUrl = EVOHOME_HOST"/WebAPI/api/Session";
	std::string szResponse;
	EvoHTTPBridge::SafePOST(szUrl, szPostdata, vLoginHeader, szResponse, -1);
	m_tLastWebCall = time(NULL);

	Json::Value jLogin;
	if (evohome::parse_json_string(szResponse, jLogin) < 0)
		return false;

	std::string szError = "";
	if (jLogin.isMember("error"))
		szError = jLogin["error"].asString();
	if (jLogin.isMember("message"))
		szError = jLogin["message"].asString();
	if (!szError.empty())
	{
		m_szLastError = "Login to Evohome server failed with message: ";
		m_szLastError.append(szError);
		return false;
	}

	if (!jLogin.isMember("sessionId") || !jLogin.isMember("userInfo") || !jLogin["userInfo"].isObject() || !jLogin["userInfo"].isMember("userID"))
	{
		m_szLastError = "Login to Evohome server did not return required data";
		return false;
	}

	m_szSessionId = jLogin["sessionId"].asString();
	m_szUserId = jLogin["userInfo"]["userID"].asString();

	std::string szAuthBearer = "sessionId: ";
	szAuthBearer.append(m_szSessionId);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	return true;
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeClient::save_auth_to_file(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value jAuth;

		jAuth["session_id"] = m_szSessionId;
		jAuth["last_use"] = static_cast<int>(m_tLastWebCall);
		jAuth["user_id"] = m_szUserId;

		myfile << jAuth.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load authorization key from a backup file
 */
bool EvohomeClient::load_auth_from_file(const std::string &szFilename)
{
	std::string szFileContent;
	std::ifstream myfile (szFilename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			szFileContent.append(line);
			szFileContent.append("\n");
		}
		myfile.close();
	}
	Json::Value jAuth;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szFileContent.c_str(), szFileContent.c_str() + szFileContent.size(), &jAuth, nullptr))
		return false;

	m_szSessionId = jAuth["session_id"].asString();
	m_tLastWebCall = static_cast<time_t>(atoi(jAuth["last_use"].asString().c_str()));
	m_szUserId = jAuth["user_id"].asString();

	if ((time(NULL) - m_tLastWebCall) > SESSION_EXPIRATION_TIME)
		return false;

	std::string szAuthBearer = "sessionId: ";
	szAuthBearer.append(m_szSessionId);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations					*
 *									*
 ************************************************************************/

/* 
 * Retrieve evohome installation info
 */
bool EvohomeClient::full_installation()
{
	std::vector<evohome::device::location>().swap(m_vLocations);

	std::string szUrl = EVOHOME_HOST"/WebAPI/api/locations/?userId=";
	szUrl.append(m_szUserId);
	szUrl.append("&allData=True");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);
	m_tLastWebCall = time(NULL);

	// evohome old API returns an unnamed json array which is not accepted by our parser
	szResponse.insert(0, "{\"locations\": ");
	szResponse.append("}");

	m_jFullInstallation.clear();
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &m_jFullInstallation, nullptr) || !m_jFullInstallation["locations"].isArray())
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	if (!m_jFullInstallation["locations"].isArray())
		return false;

	int l = static_cast<int>(m_jFullInstallation["locations"].size());
	for (int i = 0; i < l; ++i)
	{
		evohome::device::location newloc = evohome::device::location();
		m_vLocations.push_back(newloc);
		m_vLocations[i].jInstallationInfo = &m_jFullInstallation["locations"][i];
		m_vLocations[i].szLocationId = (*m_vLocations[i].jInstallationInfo)["locationID"].asString();
	}

	return true;
}


/* 
 * Extract a zone's temperature from system status
 */
std::string EvohomeClient::get_zone_temperature(const std::string szLocationId, const std::string szZoneId, const int numDecimals)
{
	if ((m_vLocations.size() == 0) && (!full_installation()))
		return "";

	int numLocations = static_cast<int>(m_vLocations.size());
	for (int iloc = 0; iloc < numLocations; iloc++)
	{
		Json::Value *jLocation = m_vLocations[iloc].jInstallationInfo;
		if (!(*jLocation).isMember("devices") || !(*jLocation)["devices"].isArray())
			continue;

		int numDevices = static_cast<int>((*jLocation)["devices"].size());
		for (int idev = 0; idev < numDevices; idev++)
		{
			Json::Value *jDevice = &(*jLocation)["devices"][idev];
			if (!(*jDevice).isMember("deviceID") || !(*jDevice).isMember("thermostat") || !(*jDevice)["thermostat"].isMember("indoorTemperature"))
				continue;
			if ((*jDevice).isMember("locationID") && ((*jDevice)["locationID"].asString() != szLocationId))
			{
				idev = 128; // move to next location
				continue;
			}
			if ((*jDevice)["deviceID"].asString() != szZoneId)
				continue;

			double temperature = (*jDevice)["thermostat"]["indoorTemperature"].asDouble();
			if (temperature > 127) // allow rounding error
				return "128"; // unit is offline

			static char cTemperature[10];
			if (numDecimals < 2)
				sprintf(cTemperature, "%.01f", ((floor((temperature * 10) + 0.5) / 10) + 0.0001));
			else			
				sprintf(cTemperature, "%.02f", ((floor((temperature * 100) + 0.5) / 100) + 0.0001));
			return std::string(cTemperature);
		}
	}
	return "";
}
