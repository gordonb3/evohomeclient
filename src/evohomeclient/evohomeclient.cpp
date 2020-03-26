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


#define EVOHOME_HOST "https://tccna.honeywell.com"

#define SESSION_EXPIRATION_TIME 899


/*
 * Class construct
 */
EvohomeOldClient::EvohomeOldClient()
{
	init();
}
EvohomeOldClient::EvohomeOldClient(const std::string &user, const std::string &password)
{
	init();
	login(user, password);
}

EvohomeOldClient::~EvohomeOldClient()
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
void EvohomeOldClient::init()
{
}


/*
 * Cleanup curl web client
 */
void EvohomeOldClient::cleanup()
{
	std::cout << "cleanup (v1) not implemented yet\n";
}


std::string EvohomeOldClient::get_last_error()
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
bool EvohomeOldClient::login(const std::string &user, const std::string &password)
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

	if (szResponse[0] == '[') // received unnamed array as reply
	{
		szResponse[0] = ' ';
		size_t len = szResponse.size();
		len--;
		szResponse[len] = ' ';
	}

	Json::Value jLogin;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &jLogin, nullptr))
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

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
	m_vEvoHeader.push_back("applicationId: 91db1612-73fd-4500-91b2-e63b069b185c");
	m_vEvoHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	return true;
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeOldClient::save_auth_to_file(const std::string &szFilename)
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
bool EvohomeOldClient::load_auth_from_file(const std::string &szFilename)
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
	m_vEvoHeader.push_back("applicationId: 91db1612-73fd-4500-91b2-e63b069b185c");
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
bool EvohomeOldClient::full_installation()
{
	std::vector<_sLocation>().swap(m_vLocations);

	std::string szUrl = EVOHOME_HOST"/WebAPI/api/locations/?userId=";
	szUrl.append(m_szUserId);
	szUrl.append("&allData=True");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

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
		_sLocation newloc = _sLocation();
		m_vLocations.push_back(newloc);
		m_vLocations[i].jInstallationInfo = &m_jFullInstallation["locations"][i];
		m_vLocations[i].szLocationId = (*m_vLocations[i].jInstallationInfo)["locationID"].asString();
	}

	return true;
}


/* 
 * Extract a zone's temperature from system status
 */
std::string EvohomeOldClient::get_zone_temperature(std::string locationId, std::string zoneId, int decimals)
{
	if ((m_vLocations.size() == 0) && (!full_installation()))
		return "";

	int multiplier = (decimals >= 2) ? 100:10;

	for (size_t iloc = 0; iloc < m_vLocations.size(); iloc++)
	{
		Json::Value *j_loc = m_vLocations[iloc].jInstallationInfo;
		if (!(*j_loc).isMember("devices") || !(*j_loc)["devices"].isArray())
			continue;

		for (size_t idev = 0; idev < (*j_loc)["devices"].size(); idev++)
		{
			Json::Value *j_dev = &(*j_loc)["devices"][(int)(idev)];
			if (!(*j_dev).isMember("deviceID") || !(*j_dev).isMember("thermostat") || !(*j_dev)["thermostat"].isMember("indoorTemperature"))
				continue;
			if ((*j_dev).isMember("locationID") && ((*j_dev)["locationID"].asString() != locationId))
			{
				idev = 128; // move to next location
				continue;
			}
			if ((*j_dev)["deviceID"].asString() != zoneId)
				continue;

			double temperature = (*j_dev)["thermostat"]["indoorTemperature"].asDouble();
			if (temperature > 127) // allow rounding error
				return "128"; // unit is offline

			// limit output to two decimals
			std::stringstream sstemp;
			sstemp << ((floor((temperature * multiplier) + 0.5) / multiplier) + 0.0001);
			std::string sztemp = sstemp.str();

			sstemp.str("");
			bool found = false;
			int i;
			for (i = 0; (i < 6) && !found; i++)
			{
				sstemp << sztemp[i];
				if (sztemp[i] == '.')
					found = true;
			}
			sstemp << sztemp[i];
			if (decimals > 1)
				sstemp << sztemp[i+1];
			return sstemp.str();
		}
	}
	return "";
}
