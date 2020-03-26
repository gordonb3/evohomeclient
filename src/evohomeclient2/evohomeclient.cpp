/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <cstring>
#include <string>
#include <ctime>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

#include "evohomeclient.h"
#include "jsoncpp/json.h"
#include "connection/EvoHTTPBridge.hpp"

#define EVOHOME_HOST "https://tccna.honeywell.com"


#ifdef _WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif

#ifndef _WIN32
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif


namespace evohome {
  namespace schedule {
    const std::string daynames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  }; // namespace schedule

  namespace API {
    namespace system {
      const std::string modes[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};
    }; // namespace system

    namespace zone {
      const std::string modes[7] = {"FollowSchedule", "PermanentOverride", "TemporaryOverride", "OpenWindow", "LocalOverride", "RemoteOverride", "Unknown"};
    }; // namespace zone
  }; // namespace API
}; // namespace evohome


/*
 * Class construct
 */
EvohomeClient2::EvohomeClient2()
{
	init();
}
EvohomeClient2::EvohomeClient2(const std::string &szUser, const std::string &szPassword)
{
	init();
	bool login_success;
	login_success = login(szUser, szPassword);

	if (!login_success)
		m_szLastError = "login fail";
}


EvohomeClient2::~EvohomeClient2()
{
	cleanup();
}

/************************************************************************
 *									*
 *	Webclient helpers						*
 *									*
 ************************************************************************/

/*
 * Initialize
 */
void EvohomeClient2::init()
{
	m_tzoffset = -1;
	lastDST = -1;
}


/*
 * Cleanup curl web client
 */
void EvohomeClient2::cleanup()
{
	std::cout << "cleanup (v2) not implemented yet\n";
}



std::string EvohomeClient2::get_last_error()
{
	return m_szLastError;
}


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/

/*
 * Login to the evohome portal
 */
bool EvohomeClient2::login(const std::string &szUser, const std::string &szPassword)
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	vLoginHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	vLoginHeader.push_back("charsets: utf-8");

	std::string szPostdata = "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8&Host=rs.alarmnet.com%2F";
	szPostdata.append("&Cache-Control=no-store%20no-cache&Pragma=no-cache&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous&Connection=Keep-Alive");
	szPostdata.append("&grant_type=password&Username=");
	szPostdata.append(EvoHTTPBridge::URLEncode(szUser));
	szPostdata.append("&Password=");
	szPostdata.append(EvoHTTPBridge::URLEncode(szPassword));

	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/Auth/OAuth/Token";
	EvoHTTPBridge::SafePOST(szUrl, szPostdata, vLoginHeader, szResponse, -1);

	if (szResponse[0] == '[') // got unnamed array as reply
	{
		szResponse[0] = ' ';
		int len = static_cast<int>(szResponse.size());
		len--;
		szResponse[len] = ' ';
	}

	Json::Value jLogin;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &jLogin, nullptr))
		return false;

	std::string szError = "";
	if (jLogin.isMember("error"))
		szError = jLogin["error"].asString();
	if (jLogin.isMember("message"))
		szError = jLogin["message"].asString();
	if (!szError.empty())
		return false;

	m_szAccessToken = jLogin["access_token"].asString();
	m_szRefreshToken = jLogin["refresh_token"].asString();
	m_tTokenExpirationTime = time(NULL) + atoi(jLogin["expires_in"].asString().c_str());

	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");
	m_vEvoHeader.push_back("charsets: utf-8");

	return user_account();
}


/*
 * Renew the Authorization token
 */
bool EvohomeClient2::renew_login()
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	vLoginHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	vLoginHeader.push_back("charsets: utf-8");

	std::string szPostdata = "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8&Host=rs.alarmnet.com%2F";
	szPostdata.append("&Cache-Control=no-store%20no-cache&Pragma=no-cache&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous&Connection=Keep-Alive");

	szPostdata.append("&grant_type=refresh_token&refresh_token=");
	szPostdata.append(m_szRefreshToken);


	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/Auth/OAuth/Token";
	EvoHTTPBridge::SafePOST(szUrl, szPostdata, vLoginHeader, szResponse, -1);

	if (szResponse[0] == '[') // got unnamed array as reply
	{
		szResponse[0] = ' ';
		int len = static_cast<int>(szResponse.size());
		len--;
		szResponse[len] = ' ';
	}

	Json::Value jLogin;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &jLogin, nullptr))
		return false;

	std::string szError = "";
	if (jLogin.isMember("error"))
		szError = jLogin["error"].asString();
	if (jLogin.isMember("message"))
		szError = jLogin["message"].asString();
	if (!szError.empty())
		return false;

	m_szAccessToken = jLogin["access_token"].asString();
	m_szRefreshToken = jLogin["refresh_token"].asString();
	m_tTokenExpirationTime = time(NULL) + atoi(jLogin["expires_in"].asString().c_str());

	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");
	m_vEvoHeader.push_back("charsets: utf-8");

	return user_account();
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeClient2::save_auth_to_file(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value jAuth;

		jAuth["access_token"] = m_szAccessToken;
		jAuth["refresh_token"] = m_szRefreshToken;
		jAuth["expiration_time"] = m_tTokenExpirationTime;

		myfile << jAuth.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load authorization key from a backup file
 */
bool EvohomeClient2::load_auth_from_file(const std::string &szFilename)
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

	m_szAccessToken = jAuth["access_token"].asString();
	m_szRefreshToken = jAuth["refresh_token"].asString();
	m_tTokenExpirationTime = static_cast<time_t>(atoi(jAuth["expiration_time"].asString().c_str()));

	if (time(NULL) > m_tTokenExpirationTime)
		return renew_login();

	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");
	m_vEvoHeader.push_back("charsets: utf-8");

	return user_account();
}


/*
 * Retrieve evohome user info
 */
bool EvohomeClient2::user_account()
{
	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/userAccount";
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	if (szResponse[0] == '[') // got unnamed array as reply
	{
		szResponse[0] = ' ';
		int len = static_cast<int>(szResponse.size());
		len--;
		szResponse[len] = ' ';
	}

	Json::Value jUserAccount;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &jUserAccount, nullptr) || !jUserAccount.isMember("userId"))
		return false;

	m_szUserId = jUserAccount["userId"].asString();
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 ************************************************************************/

void EvohomeClient2::get_dhw(int location, int gateway, int temperatureControlSystem)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

	std::vector<evohome::device::zone>().swap((*myTCS).dhw);
	Json::Value *j_tcs = (*myTCS).jInstallationInfo;

	if (!has_dhw(&(*myTCS)))
		return;

	(*myTCS).dhw.resize(1);
	(*myTCS).dhw[0].jInstallationInfo = &(*j_tcs)["dhw"];
	(*myTCS).dhw[0].szZoneId = (*j_tcs)["dhw"]["dhwId"].asString();;
	(*myTCS).dhw[0].szSystemId = (*myTCS).szSystemId;
	(*myTCS).dhw[0].szGatewayId = (*myTCS).szGatewayId;
	(*myTCS).dhw[0].szLocationId = (*myTCS).szLocationId;
}


void EvohomeClient2::get_zones(int location, int gateway, int temperatureControlSystem)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

	std::vector<evohome::device::zone>().swap((*myTCS).zones);
	Json::Value *j_tcs = (*myTCS).jInstallationInfo;

	if (!(*j_tcs)["zones"].isArray())
		return;

	int l = static_cast<int>((*j_tcs)["zones"].size());
	(*myTCS).zones.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myTCS).zones[i].jInstallationInfo = &(*j_tcs)["zones"][i];
		(*myTCS).zones[i].szZoneId = (*j_tcs)["zones"][i]["zoneId"].asString();
		(*myTCS).zones[i].szSystemId = (*myTCS).szSystemId;
		(*myTCS).zones[i].szGatewayId = (*myTCS).szGatewayId;
		(*myTCS).zones[i].szLocationId = (*myTCS).szLocationId;
	}
}


void EvohomeClient2::get_temperatureControlSystems(int location, int gateway)
{

	evohome::device::gateway *myGateway = &m_vLocations[location].gateways[gateway];

	std::vector<evohome::device::temperatureControlSystem>().swap((*myGateway).temperatureControlSystems);
	Json::Value *j_gw = (*myGateway).jInstallationInfo;

	if (!(*j_gw)["temperatureControlSystems"].isArray())
		return;

	int l = static_cast<int>((*j_gw)["temperatureControlSystems"].size());
	(*myGateway).temperatureControlSystems.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myGateway).temperatureControlSystems[i].jInstallationInfo = &(*j_gw)["temperatureControlSystems"][i];
		(*myGateway).temperatureControlSystems[i].szSystemId = (*j_gw)["temperatureControlSystems"][i]["systemId"].asString();
		(*myGateway).temperatureControlSystems[i].szGatewayId = (*myGateway).szGatewayId;
		(*myGateway).temperatureControlSystems[i].szLocationId =(*myGateway).szLocationId;

		get_zones(location, gateway, i);
		get_dhw(location, gateway, i);
	}
}


void EvohomeClient2::get_gateways(int location)
{
	std::vector<evohome::device::gateway>().swap(m_vLocations[location].gateways);
	Json::Value *j_loc = m_vLocations[location].jInstallationInfo;

	if (!(*j_loc)["gateways"].isArray())
		return;

	int l = static_cast<int>((*j_loc)["gateways"].size());
	m_vLocations[location].gateways.resize(l);
	for (int i = 0; i < l; ++i)
	{
		m_vLocations[location].gateways[i].jInstallationInfo = &(*j_loc)["gateways"][i];
		m_vLocations[location].gateways[i].szGatewayId = (*j_loc)["gateways"][i]["gatewayInfo"]["gatewayId"].asString();
		m_vLocations[location].gateways[i].szLocationId = m_vLocations[location].szLocationId;

		get_temperatureControlSystems(location, i);
	}
}


/*
 * Retrieve evohome installation info
 */
bool EvohomeClient2::full_installation()
{
	std::vector<evohome::device::location>().swap(m_vLocations);

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/location/installationInfo?userId=";
	szUrl.append(m_szUserId);
	szUrl.append("&includeTemperatureControlSystems=True");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	// evohome API returns an unnamed json array which is not accepted by our parser
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

	int l = static_cast<int>(m_jFullInstallation["locations"].size());
	for (int i = 0; i < l; ++i)
	{
		evohome::device::location newloc = evohome::device::location();
		m_vLocations.push_back(newloc);
		m_vLocations[i].jInstallationInfo = &m_jFullInstallation["locations"][i];
		m_vLocations[i].szLocationId = m_jFullInstallation["locations"][i]["locationInfo"]["locationId"].asString();

		get_gateways(i);
	}
	return true;
}


/************************************************************************
 *									*
 *	Evohome system status retrieval					*
 *									*
 ************************************************************************/

/*
 * Retrieve evohome status info
 */
bool EvohomeClient2::get_status(std::string szLocationId)
{
	if (m_vLocations.size() == 0)
		return false;
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int i = 0; i < numLocations; i++)
	{
		if (m_vLocations[i].szLocationId == szLocationId)
			return get_status(i);
	}
	return false;
}
bool EvohomeClient2::get_status(int location)
{
	Json::Value *j_loc, *j_gw, *j_tcs;
	if ((m_vLocations.size() == 0) || m_vLocations[location].szLocationId.empty())
		return false;

	bool valid_json = true;

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/location/";
	szUrl.append(m_vLocations[location].szLocationId);
	szUrl.append("/status?includeTemperatureControlSystems=True");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	m_jFullStatus.clear();
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &m_jFullStatus, nullptr))
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}
	m_vLocations[location].jStatus = &m_jFullStatus;
	j_loc = m_vLocations[location].jStatus;

	// get gateway status
	if ((*j_loc)["gateways"].isArray())
	{
		int lgw = static_cast<int>((*j_loc)["gateways"].size());
		for (int igw = 0; igw < lgw; ++igw)
		{
			m_vLocations[location].gateways[igw].jStatus = &(*j_loc)["gateways"][igw];
			j_gw = m_vLocations[location].gateways[igw].jStatus;

			// get temperatureControlSystem status
			if ((*j_gw)["temperatureControlSystems"].isArray())
			{
				int ltcs = static_cast<int>((*j_gw)["temperatureControlSystems"].size());
				for (int itcs = 0; itcs < ltcs; itcs++)
				{
					m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].jStatus = &(*j_gw)["temperatureControlSystems"][itcs];
					j_tcs = m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].jStatus;

/* ToDo: possible pitfall - does status return objects in the same order as installationInfo?
 *       according to API description a location can (currently) only contain one gateway and
 *       a gateway can only contain one TCS, so we should be safe up to this point. Is it also
 *       safe for zones though, or should we match the zone ID to be sure?
 */

					// get zone status
					if ((*j_tcs)["zones"].isArray())
					{
						int lz = static_cast<int>((*j_tcs)["zones"].size());
						for (int iz = 0; iz < lz; iz++)
						{
							m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].jStatus = &(*j_tcs)["zones"][iz];
						}
					}
					else
						valid_json = false;

					if (has_dhw(&m_vLocations[location].gateways[igw].temperatureControlSystems[itcs]))
					{
						m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].dhw[0].jStatus = &(*j_tcs)["dhw"];
					}

				}
			}
			else
				valid_json = false;
		}
	}
	else
		valid_json = false;

	return valid_json;
}


/************************************************************************
 *									*
 *	Locate Evohome elements						*
 *									*
 ************************************************************************/


evohome::device::location *EvohomeClient2::get_location_by_ID(std::string szLocationId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int l = 0; l < numLocations; l++)
	{
		if (m_vLocations[l].szLocationId == szLocationId)
			return &m_vLocations[l];
	}
	return NULL;
}


evohome::device::gateway *EvohomeClient2::get_gateway_by_ID(std::string szGatewayId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int l = 0; l < numLocations; l++)
	{
		int numGateways = static_cast<int>(m_vLocations[l].gateways.size());
		for (int g = 0; g < numGateways; g++)
		{
			if (m_vLocations[l].gateways[g].szGatewayId == szGatewayId)
				return &m_vLocations[l].gateways[g];
		}
	}
	return NULL;
}


evohome::device::temperatureControlSystem *EvohomeClient2::get_temperatureControlSystem_by_ID(std::string szSystemId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int l = 0; l < numLocations; l++)
	{
		int numGateways = static_cast<int>(m_vLocations[l].gateways.size());
		for (int g = 0; g < numGateways; g++)
		{
			int numTCSs = static_cast<int>(m_vLocations[l].gateways[g].temperatureControlSystems.size());
			for (int t = 0; t < numTCSs; t++)
			{
				if (m_vLocations[l].gateways[g].temperatureControlSystems[t].szSystemId == szSystemId)
					return &m_vLocations[l].gateways[g].temperatureControlSystems[t];
			}
		}
	}
	return NULL;
}


evohome::device::zone *EvohomeClient2::get_zone_by_ID(std::string szZoneId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int l = 0; l < numLocations; l++)
	{
		int numGateways = static_cast<int>(m_vLocations[l].gateways.size());
		for (int g = 0; g < numGateways; g++)
		{
			int numTCSs = static_cast<int>(m_vLocations[l].gateways[g].temperatureControlSystems.size());
			for (int t = 0; t < numTCSs; t++)
			{
				int numZones = static_cast<int>(m_vLocations[l].gateways[g].temperatureControlSystems[t].zones.size());
				for (int z = 0; z < numZones; z++)
				{
					if (m_vLocations[l].gateways[g].temperatureControlSystems[t].zones[z].szZoneId == szZoneId)
						return &m_vLocations[l].gateways[g].temperatureControlSystems[t].zones[z];
				}
				if (m_vLocations[l].gateways[g].temperatureControlSystems[t].dhw.size() > 0)
				{
					if (m_vLocations[l].gateways[g].temperatureControlSystems[t].dhw[0].szZoneId == szZoneId)
						return &m_vLocations[l].gateways[g].temperatureControlSystems[t].dhw[0];
				}
			}
		}
	}
	return NULL;
}


evohome::device::temperatureControlSystem *EvohomeClient2::get_zone_temperatureControlSystem(evohome::device::zone *zone)
{
	size_t l,g,t;
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int l = 0; l < numLocations; l++)
	{
		if (m_vLocations[l].szLocationId == zone->szLocationId)
		{
			int numGateways = static_cast<int>(m_vLocations[l].gateways.size());
			for (int g = 0; g < numGateways; g++)
			{
				if (m_vLocations[l].gateways[g].szGatewayId == zone->szGatewayId)
				{
					int numTCSs = static_cast<int>(m_vLocations[l].gateways[g].temperatureControlSystems.size());
					for (int t = 0; t < numTCSs; t++)
					{
						if (m_vLocations[l].gateways[g].temperatureControlSystems[t].szSystemId == zone->szSystemId)
							return &m_vLocations[l].gateways[g].temperatureControlSystems[t];
					}
				}
			}
		}
	}
	return NULL;
}


/************************************************************************
 *									*
 *	Schedule handlers						*
 *									*
 ************************************************************************/

/*
 * Retrieve a zone's next switchpoint
 *
 * Returns ISO datatime string relative to UTC (timezone 'Z')
 */
std::string EvohomeClient2::request_next_switchpoint(std::string szZoneId)
{
	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
	szUrl.append(szZoneId);
	szUrl.append("/schedule/upcommingSwitchpoints?count=1");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	if (szResponse[0] == '[') // received unnamed array as reply
	{
		szResponse[0] = ' ';
		int len = static_cast<int>(szResponse.size());
		len--;
		szResponse[len] = ' ';
	}

	Json::Value jSwitchPoint;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &jSwitchPoint, nullptr) || !jSwitchPoint.isMember("time"))
		return "";

	std::string szSwitchpoint = jSwitchPoint["time"].asString();
	szSwitchpoint.append("Z");
	return szSwitchpoint;
}


/*
 * Retrieve a zone's schedule
 */
bool EvohomeClient2::get_zone_schedule(const std::string szZoneId)
{
	return get_zone_schedule_ex(szZoneId, "temperatureZone");
}
bool EvohomeClient2::get_dhw_schedule(const std::string szDHWId)
{
	return get_zone_schedule_ex(szDHWId, "domesticHotWater");
}
bool EvohomeClient2::get_zone_schedule_ex(const std::string szZoneId, const std::string szZoneType)
{
	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/";
	szUrl.append(szZoneType);
	szUrl.append("/");
	szUrl.append(szZoneId);
	szUrl.append("/schedule");
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	if (!szResponse.find("\"id\""))
		return false;
	evohome::device::zone *zone = get_zone_by_ID(szZoneId);
	if (zone == NULL)
		return false;

	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &zone->schedule, nullptr))
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}
	return true;
}


/*
 * Find a zone's next switchpoint (localtime)
 *
 * Returns ISO datatime string relative to localtime (hardcoded as timezone 'A')
 * Extended function also fills current_setpoint with the current target temperature
 */
std::string EvohomeClient2::get_next_switchpoint(std::string szZoneId)
{
	evohome::device::zone *zone = get_zone_by_ID(szZoneId);
	if (zone == NULL)
		return "";
	return get_next_switchpoint(zone);
}
std::string EvohomeClient2::get_next_switchpoint(evohome::device::temperatureControlSystem *tcs, int zone)
{
	if (tcs->zones[zone].schedule.isNull())
	{
		if (!get_zone_schedule(tcs->zones[zone].szZoneId))
			return "";
	}
	return get_next_switchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient2::get_next_switchpoint(evohome::device::zone *hz)
{
	if (hz->schedule.isNull())
	{
		std::string zoneType = ((*hz->jInstallationInfo).isMember("dhwId")) ? "domesticHotWater" : "temperatureZone";
		if (!get_zone_schedule_ex(hz->szZoneId, zoneType))
			return "";
	}
	return get_next_switchpoint(hz->schedule);
}
std::string EvohomeClient2::get_next_switchpoint(Json::Value &jSchedule)
{
	std::string current_setpoint;
	return get_next_switchpoint(jSchedule, current_setpoint, -1, false);
}
std::string EvohomeClient2::get_next_switchpoint(Json::Value &jSchedule, std::string &current_setpoint, int force_weekday, bool convert_to_utc)
{
	if (jSchedule.isNull())
		return "";

	struct tm ltime;
	time_t now = time(0);
	localtime_r(&now, &ltime);
	int year = ltime.tm_year;
	int month = ltime.tm_mon;
	int day = ltime.tm_mday;
	int wday = (force_weekday >= 0) ? (force_weekday % 7) : ltime.tm_wday;
	char cDate[30];
	sprintf_s(cDate, 30, "%04d-%02d-%02dT%02d:%02d:%02dA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	std::string szDatetime = std::string(cDate);
	if (szDatetime <= jSchedule["nextSwitchpoint"].asString()) // our current cached values are still valid
	{
		current_setpoint = jSchedule["currentSetpoint"].asString();
		if (convert_to_utc)
			return local_to_utc(jSchedule["nextSwitchpoint"].asString());
		else
			return jSchedule["nextSwitchpoint"].asString();
	}

	std::string szTime;
	bool found = false;
	current_setpoint = "";
	for (uint8_t d = 0; ((d < 7) && !found); d++)
	{
		int tryday = (wday + d) % 7;
		std::string s_tryday = (std::string)evohome::schedule::daynames[tryday];
		Json::Value *j_day;
		// find day
		for (size_t i = 0; ((i < jSchedule["dailySchedules"].size()) && !found); i++)
		{
			j_day = &jSchedule["dailySchedules"][(int)(i)];
			if (((*j_day).isMember("dayOfWeek")) && ((*j_day)["dayOfWeek"] == s_tryday))
				found = true;
		}
		if (!found)
			continue;

		found = false;
		for (size_t i = 0; ((i < (*j_day)["switchpoints"].size()) && !found); ++i)
		{
			szTime = (*j_day)["switchpoints"][(int)(i)]["timeOfDay"].asString();
			ltime.tm_isdst = -1;
			ltime.tm_year = year;
			ltime.tm_mon = month;
			ltime.tm_mday = day + d;
			ltime.tm_hour = std::atoi(szTime.substr(0, 2).c_str());
			ltime.tm_min = std::atoi(szTime.substr(3, 2).c_str());
			ltime.tm_sec = std::atoi(szTime.substr(6, 2).c_str());
			time_t ntime = mktime(&ltime);
			if (ntime > now)
				found = true;
			else if ((*j_day)["switchpoints"][(int)(i)].isMember("temperature"))
				current_setpoint = (*j_day)["switchpoints"][(int)(i)]["temperature"].asString();
			else
				current_setpoint = (*j_day)["switchpoints"][(int)(i)]["dhwState"].asString();
		}
	}

	if (current_setpoint.empty()) // got a direct match for the next switchpoint, need to go back in time to find the current setpoint
	{
		found = false;
		for (uint8_t d = 1; ((d < 7) && !found); d++)
		{
			int tryday = (wday - d + 7) % 7;
			std::string s_tryday = (std::string)evohome::schedule::daynames[tryday];
			Json::Value *j_day;
			// find day
			for (size_t i = 0; ((i < jSchedule["dailySchedules"].size()) && !found); i++)
			{
				j_day = &jSchedule["dailySchedules"][(int)(i)];
				if (((*j_day).isMember("dayOfWeek")) && ((*j_day)["dayOfWeek"] == s_tryday))
					found = true;
			}
			if (!found)
				continue;

			found = false;
			size_t l = (*j_day)["switchpoints"].size();
			if (l > 0)
			{
				l--;
				if ((*j_day)["switchpoints"][(int)(l)].isMember("temperature"))
					current_setpoint = (*j_day)["switchpoints"][(int)(l)]["temperature"].asString();
				else
					current_setpoint = (*j_day)["switchpoints"][(int)(l)]["dhwState"].asString();
				found = true;
			}
		}
	}

	if (!found)
		return "";

	sprintf_s(cDate, 30, "%04d-%02d-%02dT%sA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, szTime.c_str()); // localtime => use CET to indicate that it is not UTC
	szDatetime = std::string(cDate);
	jSchedule["currentSetpoint"] = current_setpoint;
	jSchedule["nextSwitchpoint"] = szDatetime;
	if (convert_to_utc)
		return local_to_utc(szDatetime);
	else
		return szDatetime;
}


/*
 * Find a zone's next switchpoint (UTC)
 *
 * Returns ISO datatime string relative to UTZ (timezone 'Z')
 * Extended function also fills current_setpoint with the current target temperature
 */
std::string EvohomeClient2::get_next_utcswitchpoint(std::string szZoneId)
{
	evohome::device::zone *zone = get_zone_by_ID(szZoneId);
	if (zone == NULL)
		return "";
	return get_next_utcswitchpoint(zone);
}
std::string EvohomeClient2::get_next_utcswitchpoint(evohome::device::temperatureControlSystem *tcs, int zone)
{
	if (tcs->zones[zone].schedule.isNull())
	{
		if (!get_zone_schedule(tcs->zones[zone].szZoneId))
			return "";
	}
	return get_next_utcswitchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient2::get_next_utcswitchpoint(evohome::device::zone *hz)
{
	if (hz->schedule.isNull())
	{
		std::string zoneType = ((*hz->jInstallationInfo).isMember("dhwId")) ? "domesticHotWater" : "temperatureZone";
		if (!get_zone_schedule_ex(hz->szZoneId, zoneType))
			return "";
	}
	return get_next_utcswitchpoint(hz->schedule);
}
std::string EvohomeClient2::get_next_utcswitchpoint(Json::Value &jSchedule)
{
	std::string current_setpoint;
	return get_next_switchpoint(jSchedule, current_setpoint, -1, true);
}
std::string EvohomeClient2::get_next_utcswitchpoint(Json::Value &jSchedule, std::string &current_setpoint, int force_weekday)
{
	return get_next_switchpoint(jSchedule, current_setpoint, force_weekday, true);
}



/*
 * Backup all schedules to a file
 */
bool EvohomeClient2::schedules_backup(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value j_sched;

		int numLocations = static_cast<int>(m_vLocations.size());
		for (int il = 0; il < numLocations; il++)
		{
			Json::Value *j_loc = m_vLocations[il].jInstallationInfo;
			std::string s_locId = (*j_loc)["locationInfo"]["locationId"].asString();
			if (s_locId.empty())
				continue;

			Json::Value j_locsched;
			j_locsched["locationId"] = s_locId;
			j_locsched["name"] = (*j_loc)["locationInfo"]["name"].asString();

			int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
			for (int igw = 0; igw < numGateways; igw++)
			{
				Json::Value *j_gw = m_vLocations[il].gateways[igw].jInstallationInfo;
				std::string s_gwId = (*j_gw)["gatewayInfo"]["gatewayId"].asString();
				if (s_gwId.empty())
					continue;

				Json::Value j_gwsched;
				j_gwsched["gatewayId"] = s_gwId;

				int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
				for (int itcs = 0; itcs < numTCSs; itcs++)
				{
					Json::Value *j_tcs = m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].jInstallationInfo;
					std::string s_tcsId = (*j_tcs)["systemId"].asString();

					if (s_tcsId.empty())
						continue;

					Json::Value j_tcssched;
					j_tcssched["systemId"] = s_tcsId;
					if (!(*j_tcs)["zones"].isArray())
						continue;

					int numZones = static_cast<int>((*j_tcs)["zones"].size());
					for (int iz = 0; iz < numZones; iz++)
					{
						std::string szZoneId = (*j_tcs)["zones"][(int)(iz)]["zoneId"].asString();
						if (szZoneId.empty())
							continue;

						std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
						szUrl.append(szZoneId);
						szUrl.append("/schedule");
						std::string szResponse;
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

						if (!szResponse.find("\"id\""))
							continue;

						Json::Value j_week;
						Json::CharReaderBuilder jBuilder;
						std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
						if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &j_week, nullptr))
							continue;

						Json::Value j_zonesched;
						j_zonesched["zoneId"] = szZoneId;
						j_zonesched["name"] = (*j_tcs)["zones"][(int)(iz)]["name"].asString();
						if (j_week["dailySchedules"].isArray())
							j_zonesched["dailySchedules"] = j_week["dailySchedules"];
						else
							j_zonesched["dailySchedules"] = Json::arrayValue;
						j_tcssched[szZoneId] = j_zonesched;
					}
// Hot Water
					if (has_dhw(il, igw, itcs))
					{
						std::string s_dhwId = (*j_tcs)["dhw"]["dhwId"].asString();
						if (s_dhwId.empty())
							continue;

						std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/domesticHotWater/";
						szUrl.append(s_dhwId);
						szUrl.append("/schedule");
						std::string szResponse;
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

						if ( ! szResponse.find("\"id\""))
							return false;

						Json::Value j_week;
						Json::CharReaderBuilder jBuilder;
						std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
						if (!jReader->parse(szResponse.c_str(), szResponse.c_str() + szResponse.size(), &j_week, nullptr))
							continue;

						Json::Value j_dhwsched;
						j_dhwsched["dhwId"] = s_dhwId;
						if (j_week["dailySchedules"].isArray())
							j_dhwsched["dailySchedules"] = j_week["dailySchedules"];
						else
							j_dhwsched["dailySchedules"] = Json::arrayValue;
						j_tcssched[s_dhwId] = j_dhwsched;
					}
					j_gwsched[s_tcsId] = j_tcssched;
				}
				j_locsched[s_gwId] = j_gwsched;
			}
			j_sched[s_locId] = j_locsched;
		}

		myfile << j_sched.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load all schedules from a schedule backup file
 */
bool EvohomeClient2::read_schedules_from_file(const std::string &szFilename)
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
	if (szFileContent == "")
		return false;

	Json::Value j_sched;
	Json::CharReaderBuilder jBuilder;
	std::unique_ptr<Json::CharReader> jReader(jBuilder.newCharReader());
	if (!jReader->parse(szFileContent.c_str(), szFileContent.c_str() + szFileContent.size(), &j_sched, nullptr))
		return false;


	Json::Value::Members locations = j_sched.getMemberNames();
	for (size_t l = 0; l < m_vLocations.size(); l++)
	{
		if (j_sched[locations[l]].isString())
			continue;
		Json::Value::Members gateways = j_sched[locations[l]].getMemberNames();
		for (size_t g = 0; g < gateways.size(); g++)
		{
			if (j_sched[locations[l]][gateways[g]].isString())
				continue;
			Json::Value::Members temperatureControlSystems = j_sched[locations[l]][gateways[g]].getMemberNames();
			for (size_t t = 0; t < temperatureControlSystems.size(); t++)
			{
				if (j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]].isString())
					continue;
				Json::Value::Members zones = j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]].getMemberNames();
				for (size_t z = 0; z < zones.size(); z++)
				{
					if (j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]][zones[z]].isString())
						continue;
					evohome::device::zone *zone = get_zone_by_ID(zones[z]);
					if (zone != NULL)
						zone->schedule = j_sched[locations[l]][gateways[g]][temperatureControlSystems[t]][zones[z]];
				}
			}
		}
	}
	return true;
}


/*
 * Set a zone's schedule
 */
bool EvohomeClient2::set_zone_schedule(const std::string szZoneId, Json::Value *jSchedule)
{
	return set_zone_schedule_ex(szZoneId, "temperatureZone", jSchedule);
}
bool EvohomeClient2::set_dhw_schedule(const std::string szDHWId, Json::Value *jSchedule)
{
	return set_zone_schedule_ex(szDHWId, "domesticHotWater", jSchedule);
}
bool EvohomeClient2::set_zone_schedule_ex(const std::string szZoneId, const std::string szZoneType, Json::Value *jSchedule)
{
	std::string szPutdata = (*jSchedule).toStyledString();
	int numChars = static_cast<int>(szPutdata.length());
	for (int i = 0; i < numChars; i++)
	{
		char c = szPutdata[i];
		if (c == '"')
		{
			i++;
			c = szPutdata[i];
			if ((c > 0x60) && (c < 0x7b))
				szPutdata[i] = c ^ 0x20;
		}
	}

	size_t insertAt = 0;
	while ((insertAt = szPutdata.find("\"Temperature\"")) != std::string::npos)
	{
		insertAt++;
		szPutdata.insert(insertAt, "Target");
		insertAt +=17;
	}

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/";
	szUrl.append(szZoneType);
	szUrl.append("/");
	szUrl.append(szZoneId);
	szUrl.append("/schedule");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szPutdata, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Restore all schedules from a backup file
 */
bool EvohomeClient2::schedules_restore(const std::string &szFilename)
{
	if ( ! read_schedules_from_file(szFilename) )
		return false;

	std::cout << "Restoring schedules from file " << szFilename << "\n";
	unsigned int l,g,t,z;
	for (l = 0; l < m_vLocations.size(); l++)
	{
		std::cout << "  Location: " << m_vLocations[l].szLocationId << "\n";

		for (g = 0; g < m_vLocations[l].gateways.size(); g++)
		{
			std::cout << "    Gateway: " << m_vLocations[l].gateways[g].szGatewayId << "\n";

			for (t = 0; t < m_vLocations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				std::cout << "      System: " << m_vLocations[l].gateways[g].temperatureControlSystems[t].szSystemId << "\n";

				for (z = 0; z < m_vLocations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					std::cout << "        Zone: " << (*m_vLocations[l].gateways[g].temperatureControlSystems[t].zones[z].jInstallationInfo)["name"].asString() << "\n";
					set_zone_schedule(m_vLocations[l].gateways[g].temperatureControlSystems[t].zones[z].szZoneId, &m_vLocations[l].gateways[g].temperatureControlSystems[t].zones[z].schedule);
				}
				if (has_dhw(&m_vLocations[l].gateways[g].temperatureControlSystems[t]))
				{
					std::string dhwId = (*m_vLocations[l].gateways[g].temperatureControlSystems[t].jStatus)["dhw"]["dhwId"].asString();
					std::cout << "        Hot water\n";
					set_dhw_schedule(dhwId, &m_vLocations[l].gateways[g].temperatureControlSystems[t].dhw[0].schedule);
				}
			}
		}
	}
	return true;
}


/************************************************************************
 *									*
 *	Time functions							*
 *									*
 ************************************************************************/

bool EvohomeClient2::verify_date(std::string szDateTime)
{
	if (szDateTime.length() < 10)
		return false;
	std::string szDate = szDateTime.substr(0,10);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(szDateTime.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(szDateTime.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(szDateTime.substr(8, 2).c_str());
	mtime.tm_hour = 12; // midday time - prevent date shift because of DST
	mtime.tm_min = 0;
	mtime.tm_sec = 0;
	time_t ntime = mktime(&mtime);
	if (ntime == -1)
		return false;
	char cDate[12];
	sprintf_s(cDate, 12, "%04d-%02d-%02d", mtime.tm_year+1900, mtime.tm_mon+1, mtime.tm_mday);
	return (szDate == std::string(cDate));
}


bool EvohomeClient2::verify_datetime(std::string szDateTime)
{
	if (szDateTime.length() < 19)
		return false;
	std::string szDate = szDateTime.substr(0,10);
	std::string szTime = szDateTime.substr(11,8);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(szDateTime.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(szDateTime.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(szDateTime.substr(8, 2).c_str());
	mtime.tm_hour = atoi(szDateTime.substr(11, 2).c_str());
	mtime.tm_min = atoi(szDateTime.substr(14, 2).c_str());
	mtime.tm_sec = atoi(szDateTime.substr(17, 2).c_str());
	time_t ntime = mktime(&mtime);
	if (ntime == -1)
		return false;
	char cDate[12];
	sprintf_s(cDate, 12, "%04d-%02d-%02d", mtime.tm_year+1900, mtime.tm_mon+1, mtime.tm_mday);
	char cTime[12];
	sprintf_s(cTime, 12, "%02d:%02d:%02d", mtime.tm_hour, mtime.tm_min, mtime.tm_sec);
	return ( (szDate == std::string(cDate)) && (szTime == std::string(cTime)) );
}


/*
 * Convert a localtime ISO datetime string to UTC
 */
std::string EvohomeClient2::local_to_utc(std::string local_time)
{
	if (local_time.size() <  19)
		return "";
	if (m_tzoffset == -1)
	{
		// calculate timezone offset once
		time_t now = time(0);
		struct tm utime;
		gmtime_r(&now, &utime);
		utime.tm_isdst = -1;
		m_tzoffset = (int)difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(local_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(local_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(local_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(local_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(local_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(local_time.substr(17, 2).c_str()) + m_tzoffset;
	mktime(&ltime);
	if (lastDST == -1)
		lastDST = ltime.tm_isdst;
	else if ((lastDST != ltime.tm_isdst) && (lastDST != -1)) // DST changed - must recalculate timezone offset
	{
		ltime.tm_hour -= (ltime.tm_isdst - lastDST);
		lastDST = ltime.tm_isdst;
		m_tzoffset = -1;
	}
	char cUntil[22];
	sprintf_s(cUntil, 22, "%04d-%02d-%02dT%02d:%02d:%02dZ", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return std::string(cUntil);
}


/*
 * Convert a UTC ISO datetime string to localtime
 */
std::string EvohomeClient2::utc_to_local(std::string utc_time)
{
	if (utc_time.size() <  19)
		return "";
	if (m_tzoffset == -1)
	{
		// calculate timezone offset once
		time_t now = time(0);
		struct tm utime;
		gmtime_r(&now, &utime);
		utime.tm_isdst = -1;
		m_tzoffset = (int)difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str()) - m_tzoffset;
	mktime(&ltime);
	if (lastDST == -1)
		lastDST = ltime.tm_isdst;
	else if ((lastDST != ltime.tm_isdst) && (lastDST != -1)) // DST changed - must recalculate timezone offset
	{
		ltime.tm_hour += (ltime.tm_isdst - lastDST);
		lastDST = ltime.tm_isdst;
		m_tzoffset = -1;
	}
	char cUntil[40];
	sprintf_s(cUntil, 40, "%04d-%02d-%02dT%02d:%02d:%02dA", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return std::string(cUntil);
}


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/

/*
 * Set the system mode
 *
 * Note: setting an end date does not appear to work on most installations
 */
bool EvohomeClient2::set_system_mode(const std::string szSystemId, const std::string mode, const std::string date_until)
{
	int i = 0;
	int s = static_cast<int>(sizeof(evohome::API::system::modes));
	while (s > 0)
	{
		if (evohome::API::system::modes[i] == mode)
			return set_system_mode(szSystemId, i, date_until);
		s -= static_cast<int>(sizeof(evohome::API::system::modes[i]));
		i++;
	}
	return false;
}
bool EvohomeClient2::set_system_mode(const std::string szSystemId, const int mode, const std::string date_until)
{
	std::string szPutData = "{\"SystemMode\":";
	szPutData.append(std::to_string(mode));
	szPutData.append(",\"TimeUntil\":");
	if (date_until.empty())
		szPutData.append("null");
	else if (!verify_date(date_until))
		return false;
	else
	{
		szPutData.append("\"");
		szPutData.append(date_until.substr(0,10));
		szPutData.append("T00:00:00Z");
	}
	szPutData.append(",\"Permanent\":");
	if (date_until.empty())
		szPutData.append("true}");
	else
		szPutData.append("false}");

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureControlSystem/";
	szUrl.append(szSystemId);
	szUrl.append("/mode");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}



/*
 * Override a zone's target temperature
 */
bool EvohomeClient2::set_temperature(std::string szZoneId, std::string temperature, std::string time_until)
{
	std::string szPutData = "{\"HeatSetpointValue\":";
	szPutData.append(temperature);
	szPutData.append(",\"SetpointMode\":");
	if (time_until.empty())
		szPutData.append("1");
	else if (!verify_datetime(time_until))
		return false;
	else
		szPutData.append("2");
	szPutData.append(",\"TimeUntil\":");
	if (time_until.empty())
		szPutData.append("null}");
	else
	{
		szPutData.append("\"");
		szPutData.append(time_until.substr(0,10));
		szPutData.append("T");
		szPutData.append(time_until.substr(11,8));
		szPutData.append("Z\"}");
	}

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
	szUrl.append(szZoneId);
	szUrl.append("/heatSetpoint");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient2::set_temperature(std::string szZoneId, std::string temperature)
{
	return set_temperature(szZoneId, temperature, "");
}


/*
 * Cancel a zone's target temperature override
 */
bool EvohomeClient2::cancel_temperature_override(std::string szZoneId)
{
	std::string szPutData = "{\"HeatSetpointValue\":0.0,\"SetpointMode\":0,\"TimeUntil\":null}";

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
	szUrl.append(szZoneId);
	szUrl.append("/heatSetpoint");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient2::has_dhw(int location, int gateway, int temperatureControlSystem)
{
	evohome::device::temperatureControlSystem *tcs = &m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
	return has_dhw(tcs);
}
bool EvohomeClient2::has_dhw(evohome::device::temperatureControlSystem *tcs)
{
	return (*tcs->jInstallationInfo).isMember("dhw");
}


bool EvohomeClient2::is_single_heating_system()
{
	if (m_vLocations.size() == 0)
		full_installation();
	return ( (m_vLocations.size() == 1) &&
		 (m_vLocations[0].gateways.size() == 1) &&
		 (m_vLocations[0].gateways[0].temperatureControlSystems.size() == 1) );
}


/*
 * Set mode for Hot Water device
 */
bool EvohomeClient2::set_dhw_mode(std::string dhwId, std::string mode, std::string time_until)
{
	std::string szPutData = "{\"State\":";
	if (mode == "on")
		szPutData.append("1");
	else
		szPutData.append("0");
	szPutData.append(",\"Mode\":");
	if (mode == "auto")
		szPutData.append("0");
	else if (time_until.empty())
		szPutData.append("1");
	else if (!verify_datetime(time_until))
		return false;
	else
		szPutData.append("2");
	szPutData.append(",\"UntilTime\":");
	if (time_until.empty())
		szPutData.append("null}");
	else
	{
		szPutData.append("\"");
		szPutData.append(time_until.substr(0,10));
		szPutData.append("T");
		szPutData.append(time_until.substr(11,8));
		szPutData.append("Z\"}");
	}

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/domesticHotWater/";
	szUrl.append(dhwId);
	szUrl.append("/state");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient2::set_dhw_mode(std::string dhwId, std::string mode)
{
	return set_dhw_mode(dhwId, mode, "");
}

