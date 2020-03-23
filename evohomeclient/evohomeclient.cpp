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
#include <sstream>
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

//using namespace std;

const std::string weekdays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const std::string evo_modes[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};

/*
 * Class construct
 */
EvohomeClient::EvohomeClient()
{
	init();
}
EvohomeClient::EvohomeClient(const std::string &szUser, const std::string szPassword)
{
	init();
	bool login_success;
	login_success = login(szUser, szPassword);

	if (!login_success)
		m_szLastError = "login fail";
}


EvohomeClient::~EvohomeClient()
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
void EvohomeClient::init()
{
	m_tzoffset = -1;
	lastDST = -1;
}


/*
 * Cleanup curl web client
 */
void EvohomeClient::cleanup()
{
	std::cout << "cleanup (v2) not implemented yet\n";
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
 * Login to the evohome portal
 */
bool EvohomeClient::login(const std::string &szUser, const std::string szPassword)
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	vLoginHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	vLoginHeader.push_back("charsets: utf-8");

	std::stringstream pdata;
	pdata << "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=password";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous%20EMEA-V1-Get-Current-User-Account";
	pdata << "&Username=" << EvoHTTPBridge::URLEncode(szUser);
	pdata << "&Password=" << EvoHTTPBridge::URLEncode(szPassword);
	pdata << "&Connection=Keep-Alive";

	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/Auth/OAuth/Token";
	EvoHTTPBridge::SafePOST(szUrl, pdata.str(), vLoginHeader, szResponse, -1);

	if (szResponse[0] == '[') // got unnamed array as reply
	{
		szResponse[0] = ' ';
		int len = static_cast<int>(szResponse.size());
		len--;
		szResponse[len] = ' ';
	}

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(szResponse.c_str(), j_login))
		return false;

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
		return false;

	m_szAccessToken = j_login["access_token"].asString();
	m_szRefreshToken = j_login["refresh_token"].asString();
	m_tTokenExpirationTime = time(NULL) + atoi(j_login["expires_in"].asString().c_str());
	std::stringstream atoken;
	atoken << "Authorization: bearer " << m_szAccessToken;

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(atoken.str());
	m_vEvoHeader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");
	m_vEvoHeader.push_back("charsets: utf-8");

	return user_account();
}


/* 
 * Renew the Authorization token
 */
bool EvohomeClient::renew_login()
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	vLoginHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	vLoginHeader.push_back("charsets: utf-8");

	std::stringstream pdata;
	pdata << "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=refresh_token";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous";
	pdata << "&refresh_token=" << m_szRefreshToken;
	pdata << "&Connection=Keep-Alive";

	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/Auth/OAuth/Token";
	EvoHTTPBridge::SafePOST(szUrl, pdata.str(), vLoginHeader, szResponse, -1);

	if (szResponse[0] == '[') // got unnamed array as reply
	{
		szResponse[0] = ' ';
		int len = static_cast<int>(szResponse.size());
		len--;
		szResponse[len] = ' ';
	}

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(szResponse.c_str(), j_login))
		return false;

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
		return false;

	m_szAccessToken = j_login["access_token"].asString();
	m_szRefreshToken = j_login["refresh_token"].asString();
	m_tTokenExpirationTime = time(NULL) + atoi(j_login["expires_in"].asString().c_str());
	std::stringstream atoken;
	atoken << "Authorization: bearer " << m_szAccessToken;

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(atoken.str());
	m_vEvoHeader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");
	m_vEvoHeader.push_back("charsets: utf-8");

	return user_account();
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeClient::save_auth_to_file(std::string szFilename)
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
bool EvohomeClient::load_auth_from_file(std::string szFilename)
{
	std::stringstream ss;
	std::ifstream myfile (szFilename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			ss << line << '\n';
		}
		myfile.close();
	}
	Json::Value jAuth;
	Json::Reader jReader;
	if (!jReader.parse(ss.str().c_str(), jAuth))
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
bool EvohomeClient::user_account()
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
	Json::Reader jReader;
	if (!jReader.parse(szResponse.c_str(), jUserAccount) || !jUserAccount.isMember("userId"))
		return false;


	m_szUserId = jUserAccount["userId"].asString();
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 ************************************************************************/

void EvohomeClient::get_dhw(int location, int gateway, int temperatureControlSystem)
{
	_sTemperatureControlSystem *myTCS = &locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

	std::vector<zone>().swap((*myTCS).dhw);
	Json::Value *j_tcs = (*myTCS).installationInfo;

	if (!has_dhw(&(*myTCS)))
		return;

	(*myTCS).dhw.resize(1);
	(*myTCS).dhw[0].installationInfo = &(*j_tcs)["dhw"];
	(*myTCS).dhw[0].zoneId = (*j_tcs)["dhw"]["dhwId"].asString();;
	(*myTCS).dhw[0].systemId = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].systemId;
	(*myTCS).dhw[0].gatewayId = locations[location].gateways[gateway].gatewayId;
	(*myTCS).dhw[0].locationId = locations[location].locationId;
}


void EvohomeClient::get_zones(int location, int gateway, int temperatureControlSystem)
{
	_sTemperatureControlSystem *myTCS = &locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

	std::vector<zone>().swap((*myTCS).zones);
	Json::Value *j_tcs = (*myTCS).installationInfo;

	if (!(*j_tcs)["zones"].isArray())
		return;

	int l = static_cast<int>((*j_tcs)["zones"].size());
	(*myTCS).zones.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myTCS).zones[i].installationInfo = &(*j_tcs)["zones"][i];
		(*myTCS).zones[i].zoneId = (*j_tcs)["zones"][i]["zoneId"].asString();
		(*myTCS).zones[i].systemId = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].systemId;
		(*myTCS).zones[i].gatewayId = locations[location].gateways[gateway].gatewayId;
		(*myTCS).zones[i].locationId = locations[location].locationId;
	}
}


void EvohomeClient::get_temperatureControlSystems(int location, int gateway)
{

	_sGateway *myGateway = &locations[location].gateways[gateway];

	std::vector<temperatureControlSystem>().swap((*myGateway).temperatureControlSystems);
	Json::Value *j_gw = (*myGateway).installationInfo;

	if (!(*j_gw)["temperatureControlSystems"].isArray())
		return;

	int l = static_cast<int>((*j_gw)["temperatureControlSystems"].size());
	(*myGateway).temperatureControlSystems.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myGateway).temperatureControlSystems[i].installationInfo = &(*j_gw)["temperatureControlSystems"][i];
		(*myGateway).temperatureControlSystems[i].systemId = (*j_gw)["temperatureControlSystems"][i]["systemId"].asString();
		(*myGateway).temperatureControlSystems[i].gatewayId = locations[location].gateways[gateway].gatewayId;
		(*myGateway).temperatureControlSystems[i].locationId = locations[location].locationId;

		get_zones(location, gateway, i);
		get_dhw(location, gateway, i);
	}
}


void EvohomeClient::get_gateways(int location)
{
	std::vector<gateway>().swap(locations[location].gateways);
	Json::Value *j_loc = locations[location].installationInfo;

	if (!(*j_loc)["gateways"].isArray())
		return;

	int l = static_cast<int>((*j_loc)["gateways"].size());
	locations[location].gateways.resize(l);
	for (int i = 0; i < l; ++i)
	{
		locations[location].gateways[i].installationInfo = &(*j_loc)["gateways"][i];
		locations[location].gateways[i].gatewayId = (*j_loc)["gateways"][i]["gatewayInfo"]["gatewayId"].asString();
		locations[location].gateways[i].locationId = locations[location].locationId;

		get_temperatureControlSystems(location, i);
	}
}


/* 
 * Retrieve evohome installation info
 */
bool EvohomeClient::full_installation()
{
	std::vector<location>().swap(locations);

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/location/installationInfo?userId=";
	szUrl.append(m_szUserId);
	szUrl.append("&includeTemperatureControlSystems=True");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	// evohome API returns an unnamed json array which is not accepted by our parser
	szResponse.insert(0, "{\"locations\": ");
	szResponse.append("}");

	Json::Reader jReader;
	m_jFullInstallation.clear();
	if (!jReader.parse(szResponse, m_jFullInstallation) || !m_jFullInstallation["locations"].isArray())
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	int l = static_cast<int>(m_jFullInstallation["locations"].size());
	for (int i = 0; i < l; ++i)
	{
		location newloc = location();
		locations.push_back(newloc);
		locations[i].installationInfo = &m_jFullInstallation["locations"][i];
		locations[i].locationId = m_jFullInstallation["locations"][i]["locationInfo"]["locationId"].asString();

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
bool EvohomeClient::get_status(std::string locationId)
{
	if (locations.size() == 0)
		return false;
	for (size_t i = 0; i < locations.size(); i++)
	{
		if (locations[i].locationId == locationId)
			return get_status(i);
	}
	return false;
}
bool EvohomeClient::get_status(int location)
{
	Json::Value *j_loc, *j_gw, *j_tcs;
	if ((locations.size() == 0) || locations[location].locationId.empty())
		return false;

	bool valid_json = true;

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/location/";
	szUrl.append(locations[location].locationId);
	szUrl.append("/status?includeTemperatureControlSystems=True");
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	Json::Reader jReader;
	m_jFullStatus.clear();
	if (!jReader.parse(szResponse, m_jFullStatus))
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}
	locations[location].status = &m_jFullStatus;
	j_loc = locations[location].status;

	// get gateway status
	if ((*j_loc)["gateways"].isArray())
	{
		size_t lgw = (*j_loc)["gateways"].size();
		for (size_t igw = 0; igw < lgw; ++igw)
		{
			locations[location].gateways[igw].status = &(*j_loc)["gateways"][(int)(igw)];
			j_gw = locations[location].gateways[igw].status;

			// get temperatureControlSystem status
			if ((*j_gw)["temperatureControlSystems"].isArray())
			{
				size_t ltcs = (*j_gw)["temperatureControlSystems"].size();
				for (size_t itcs = 0; itcs < ltcs; itcs++)
				{
					locations[location].gateways[igw].temperatureControlSystems[itcs].status = &(*j_gw)["temperatureControlSystems"][(int)(itcs)];
					j_tcs = locations[location].gateways[igw].temperatureControlSystems[itcs].status;

/* ToDo: possible pitfall - does status return objects in the same order as installationInfo?
 *       according to API description a location can (currently) only contain one gateway and
 *       a gateway can only contain one TCS, so we should be safe up to this point. Is it also
 *       safe for zones though, or should we match the zone ID to be sure?
 */

					// get zone status
					if ((*j_tcs)["zones"].isArray())
					{
						size_t lz = (*j_tcs)["zones"].size();
						for (size_t iz = 0; iz < lz; iz++)
						{
							locations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].status = &(*j_tcs)["zones"][(int)(iz)];
						}
					}
					else
						valid_json = false;

					if (has_dhw(&locations[location].gateways[igw].temperatureControlSystems[itcs]))
					{
						locations[location].gateways[igw].temperatureControlSystems[itcs].dhw[0].status = &(*j_tcs)["dhw"];
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


EvohomeClient::location* EvohomeClient::get_location_by_ID(std::string locationId)
{
	if (locations.size() == 0)
		full_installation();
	size_t l;
	for (l = 0; l < locations.size(); l++)
	{
		if (locations[l].locationId == locationId)
			return &locations[l];
	}
	return NULL;
}


EvohomeClient::gateway* EvohomeClient::get_gateway_by_ID(std::string gatewayId)
{
	if (locations.size() == 0)
		full_installation();
	size_t l,g;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			if (locations[l].gateways[g].gatewayId == gatewayId)
				return &locations[l].gateways[g];
		}
	}
	return NULL;
}


EvohomeClient::temperatureControlSystem* EvohomeClient::get_temperatureControlSystem_by_ID(std::string systemId)
{
	if (locations.size() == 0)
		full_installation();
	size_t l,g,t;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				if (locations[l].gateways[g].temperatureControlSystems[t].systemId == systemId)
					return &locations[l].gateways[g].temperatureControlSystems[t];
			}
		}
	}
	return NULL;
}


EvohomeClient::zone* EvohomeClient::get_zone_by_ID(std::string zoneId)
{
	if (locations.size() == 0)
		full_installation();
	size_t l,g,t,z;
	for (l = 0; l < locations.size(); l++)
	{
		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				for (z = 0; z < locations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					if (locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId == zoneId)
						return &locations[l].gateways[g].temperatureControlSystems[t].zones[z];
				}
				if (locations[l].gateways[g].temperatureControlSystems[t].dhw.size() > 0)
				{
					if (locations[l].gateways[g].temperatureControlSystems[t].dhw[0].zoneId == zoneId)
						return &locations[l].gateways[g].temperatureControlSystems[t].dhw[0];
				}
			}
		}
	}
	return NULL;
}


EvohomeClient::temperatureControlSystem* EvohomeClient::get_zone_temperatureControlSystem(EvohomeClient::zone* zone)
{
	size_t l,g,t;
	for (l = 0; l < locations.size(); l++)
	{
		if (locations[l].locationId != zone->locationId)
		{
			for (g = 0; g < locations[l].gateways.size(); g++)
			{
				if (locations[l].gateways[g].gatewayId != zone->gatewayId)
				{
					for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
					{
						if (locations[l].gateways[g].temperatureControlSystems[t].systemId == zone->systemId)
							return &locations[l].gateways[g].temperatureControlSystems[t];
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
std::string EvohomeClient::request_next_switchpoint(std::string zoneId)
{
	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
	szUrl.append(zoneId);
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

	Json::Value j_sp;
	Json::Reader jReader;
	if (!jReader.parse(szResponse.c_str(), j_sp) || !j_sp.isMember("time"))
		return "";

	std::stringstream ss;
	ss << j_sp["time"].asString() << 'Z';
	return ss.str();
}


/*
 * Retrieve a zone's schedule
 */
bool EvohomeClient::get_zone_schedule(std::string zoneId)
{
	return get_zone_schedule(zoneId, "temperatureZone");
}
bool EvohomeClient::get_dhw_schedule(std::string dhwId)
{
	return get_zone_schedule(dhwId, "domesticHotWater");
}
bool EvohomeClient::get_zone_schedule(std::string zoneId, std::string zoneType)
{
	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/";
	szUrl.append(zoneType);
	szUrl.append("/");
	szUrl.append(zoneId);
	szUrl.append("/schedule");
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	if (!szResponse.find("\"id\""))
		return false;
	EvohomeClient::zone* zone = get_zone_by_ID(zoneId);
	if (zone == NULL)
		return false;
	Json::Reader jReader;
	if (!jReader.parse(szResponse.c_str(), zone->schedule))
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
std::string EvohomeClient::get_next_switchpoint(std::string zoneId)
{
	EvohomeClient::zone* zone = get_zone_by_ID(zoneId);
	if (zone == NULL)
		return "";
	return get_next_switchpoint(zone);
}
std::string EvohomeClient::get_next_switchpoint(EvohomeClient::temperatureControlSystem* tcs, int zone)
{
	if (tcs->zones[zone].schedule.isNull())
	{
		if (!get_zone_schedule(tcs->zones[zone].zoneId))
			return "";
	}
	return get_next_switchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient::get_next_switchpoint(zone* hz)
{
	if (hz->schedule.isNull())
	{
		std::string zoneType = ((*hz->installationInfo).isMember("dhwId")) ? "domesticHotWater" : "temperatureZone";
		if (!get_zone_schedule(hz->zoneId, zoneType))
			return "";
	}
	return get_next_switchpoint(hz->schedule);
}
std::string EvohomeClient::get_next_switchpoint(Json::Value &schedule)
{
	std::string current_setpoint;
	return get_next_switchpoint_ex(schedule, current_setpoint, -1, false);
}
std::string EvohomeClient::get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint)
{
	return get_next_switchpoint_ex(schedule, current_setpoint, -1, false);
}
std::string EvohomeClient::get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday)
{
	return get_next_switchpoint_ex(schedule, current_setpoint, -1, false);
}
std::string EvohomeClient::get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday, bool convert_to_utc)
{
	if (schedule.isNull())
		return "";

	struct tm ltime;
	time_t now = time(0);
	localtime_r(&now, &ltime);
	int year = ltime.tm_year;
	int month = ltime.tm_mon;
	int day = ltime.tm_mday;
	int wday = (force_weekday >= 0) ? (force_weekday % 7) : ltime.tm_wday;
	char rdata[30];
	sprintf_s(rdata, 30, "%04d-%02d-%02dT%02d:%02d:%02dA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	std::string szdatetime = std::string(rdata);
	if (szdatetime <= schedule["nextSwitchpoint"].asString()) // our current cached values are still valid
	{
		current_setpoint = schedule["currentSetpoint"].asString();
		if (convert_to_utc)
			return local_to_utc(schedule["nextSwitchpoint"].asString());
		else
			return schedule["nextSwitchpoint"].asString();
	}

	std::string sztime;
	bool found = false;
	current_setpoint = "";
	for (uint8_t d = 0; ((d < 7) && !found); d++)
	{
		int tryday = (wday + d) % 7;
		std::string s_tryday = (std::string)weekdays[tryday];
		Json::Value* j_day;
		// find day
		for (size_t i = 0; ((i < schedule["dailySchedules"].size()) && !found); i++)
		{
			j_day = &schedule["dailySchedules"][(int)(i)];
			if (((*j_day).isMember("dayOfWeek")) && ((*j_day)["dayOfWeek"] == s_tryday))
				found = true;
		}
		if (!found)
			continue;

		found = false;
		for (size_t i = 0; ((i < (*j_day)["switchpoints"].size()) && !found); ++i)
		{
			sztime = (*j_day)["switchpoints"][(int)(i)]["timeOfDay"].asString();
			ltime.tm_isdst = -1;
			ltime.tm_year = year;
			ltime.tm_mon = month;
			ltime.tm_mday = day + d;
			ltime.tm_hour = std::atoi(sztime.substr(0, 2).c_str());
			ltime.tm_min = std::atoi(sztime.substr(3, 2).c_str());
			ltime.tm_sec = std::atoi(sztime.substr(6, 2).c_str());
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
			std::string s_tryday = (std::string)weekdays[tryday];
			Json::Value* j_day;
			// find day
			for (size_t i = 0; ((i < schedule["dailySchedules"].size()) && !found); i++)
			{
				j_day = &schedule["dailySchedules"][(int)(i)];
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

	sprintf_s(rdata, 30, "%04d-%02d-%02dT%sA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, sztime.c_str()); // localtime => use CET to indicate that it is not UTC
	szdatetime = std::string(rdata);
	schedule["currentSetpoint"] = current_setpoint;
	schedule["nextSwitchpoint"] = szdatetime;
	if (convert_to_utc)
		return local_to_utc(szdatetime);
	else
		return szdatetime;
}


/*
 * Find a zone's next switchpoint (UTC)
 *
 * Returns ISO datatime string relative to UTZ (timezone 'Z')
 * Extended function also fills current_setpoint with the current target temperature
 */
std::string EvohomeClient::get_next_utcswitchpoint(std::string zoneId)
{
	EvohomeClient::zone* zone = get_zone_by_ID(zoneId);
	if (zone == NULL)
		return "";
	return get_next_utcswitchpoint(zone);
}
std::string EvohomeClient::get_next_utcswitchpoint(EvohomeClient::temperatureControlSystem* tcs, int zone)
{
	if (tcs->zones[zone].schedule.isNull())
	{
		if (!get_zone_schedule(tcs->zones[zone].zoneId))
			return "";
	}
	return get_next_utcswitchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient::get_next_utcswitchpoint(zone* hz)
{
	if (hz->schedule.isNull())
	{
		std::string zoneType = ((*hz->installationInfo).isMember("dhwId")) ? "domesticHotWater" : "temperatureZone";
		if (!get_zone_schedule(hz->zoneId, zoneType))
			return "";
	}
	return get_next_utcswitchpoint(hz->schedule);
}
std::string EvohomeClient::get_next_utcswitchpoint(Json::Value &schedule)
{
	std::string current_setpoint;
	return get_next_switchpoint_ex(schedule, current_setpoint, -1, true);
}
std::string EvohomeClient::get_next_utcswitchpoint_ex(Json::Value &schedule, std::string &current_setpoint)
{
	return get_next_switchpoint_ex(schedule, current_setpoint, -1, true);
}
std::string EvohomeClient::get_next_utcswitchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday)
{
	return get_next_switchpoint_ex(schedule, current_setpoint, force_weekday, true);
}



/*
 * Backup all schedules to a file
 */
bool EvohomeClient::schedules_backup(std::string szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value j_sched;

		size_t il;
		for (il = 0; il < locations.size(); il++)
		{
			Json::Value *j_loc = locations[il].installationInfo;
			std::string s_locId = (*j_loc)["locationInfo"]["locationId"].asString();
			if (s_locId.empty())
				continue;

			Json::Value j_locsched;
			j_locsched["locationId"] = s_locId;
			j_locsched["name"] = (*j_loc)["locationInfo"]["name"].asString();

			size_t igw;
			for (igw = 0; igw < locations[il].gateways.size(); igw++)
			{
				Json::Value *j_gw = locations[il].gateways[igw].installationInfo;
				std::string s_gwId = (*j_gw)["gatewayInfo"]["gatewayId"].asString();
				if (s_gwId.empty())
					continue;

				Json::Value j_gwsched;
				j_gwsched["gatewayId"] = s_gwId;

				size_t itcs;
				for (itcs = 0; itcs < locations[il].gateways[igw].temperatureControlSystems.size(); itcs++)
				{
					Json::Value *j_tcs = locations[il].gateways[igw].temperatureControlSystems[itcs].installationInfo;
					std::string s_tcsId = (*j_tcs)["systemId"].asString();

					if (s_tcsId.empty())
						continue;

					Json::Value j_tcssched;
					j_tcssched["systemId"] = s_tcsId;
					if (!(*j_tcs)["zones"].isArray())
						continue;

					size_t iz;
					for (iz = 0; iz < (*j_tcs)["zones"].size(); iz++)
					{
						std::string s_zoneId = (*j_tcs)["zones"][(int)(iz)]["zoneId"].asString();
						if (s_zoneId.empty())
							continue;

						std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
						szUrl.append(s_zoneId);
						szUrl.append("/schedule");
						std::string szResponse;
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

						if (!szResponse.find("\"id\""))
							continue;

						Json::Reader jReader;
						Json::Value j_week;
						if (!jReader.parse(szResponse, j_week))
							continue;

						Json::Value j_zonesched;
						j_zonesched["zoneId"] = s_zoneId;
						j_zonesched["name"] = (*j_tcs)["zones"][(int)(iz)]["name"].asString();
						if (j_week["dailySchedules"].isArray())
							j_zonesched["dailySchedules"] = j_week["dailySchedules"];
						else
							j_zonesched["dailySchedules"] = Json::arrayValue;
						j_tcssched[s_zoneId] = j_zonesched;
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

						Json::Reader jReader;
						Json::Value j_week;
						if (!jReader.parse(szResponse, j_week))
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
bool EvohomeClient::read_schedules_from_file(std::string szFilename)
{
	std::stringstream ss;
	std::ifstream myfile (szFilename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			ss << line << '\n';
		}
		myfile.close();
	}
	std::string s_fcontent = ss.str();
	if (s_fcontent == "")
		return false;

	Json::Reader jReader;
	Json::Value j_sched;
	if (!jReader.parse(s_fcontent, j_sched))
		return false;


	Json::Value::Members locations = j_sched.getMemberNames();
	for (size_t l = 0; l < locations.size(); l++)
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
					EvohomeClient::zone* zone = get_zone_by_ID(zones[z]);
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
bool EvohomeClient::set_zone_schedule(std::string zoneId, Json::Value *schedule)
{
	return set_zone_schedule(zoneId, "temperatureZone", schedule);
}
bool EvohomeClient::set_dhw_schedule(std::string dhwId, Json::Value *schedule)
{
	return set_zone_schedule(dhwId, "domesticHotWater", schedule);
}
bool EvohomeClient::set_zone_schedule(std::string zoneId, std::string zoneType, Json::Value *schedule)
{
	std::string postdata = (*schedule).toStyledString();
	std::stringstream ss;
	unsigned int i;
	unsigned int j = 0;
	char c;
	char word[30];
	for (i = 0; i < postdata.length(); i++)
	{
		c = postdata[i];
		if ( ((c|0x20) <= 0x60) || ((c|0x20) >= 0x7b) )
		{
			if (j > 0)
			{
				word[j] = '\0';
				if (strcmp(word,"Temperature") == 0)
					ss << "TargetTemperature";
				else
					ss << word;
				j = 0;
			}
			ss << c;
		}
		else
		{
			if (j == 0)
				word[j] = toupper(c);
			else
				word[j] = c;
			j++;
		}
	}

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/";
	szUrl.append(zoneType);
	szUrl.append("/");
	szUrl.append(zoneId);
	szUrl.append("/schedule");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, ss.str(), m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}


/*
 * Restore all schedules from a backup file
 */
bool EvohomeClient::schedules_restore(std::string szFilename)
{
	if ( ! read_schedules_from_file(szFilename) )
		return false;

	std::cout << "Restoring schedules from file " << szFilename << "\n";
	unsigned int l,g,t,z;
	for (l = 0; l < locations.size(); l++)
	{
		std::cout << "  Location: " << locations[l].locationId << "\n";

		for (g = 0; g < locations[l].gateways.size(); g++)
		{
			std::cout << "    Gateway: " << locations[l].gateways[g].gatewayId << "\n";

			for (t = 0; t < locations[l].gateways[g].temperatureControlSystems.size(); t++)
			{
				std::cout << "      System: " << locations[l].gateways[g].temperatureControlSystems[t].systemId << "\n";

				for (z = 0; z < locations[l].gateways[g].temperatureControlSystems[t].zones.size(); z++)
				{
					std::cout << "        Zone: " << (*locations[l].gateways[g].temperatureControlSystems[t].zones[z].installationInfo)["name"].asString() << "\n";
					set_zone_schedule(locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId, &locations[l].gateways[g].temperatureControlSystems[t].zones[z].schedule);
				}
				if (has_dhw(&locations[l].gateways[g].temperatureControlSystems[t]))
				{
					std::string dhwId = (*locations[l].gateways[g].temperatureControlSystems[t].status)["dhw"]["dhwId"].asString();
					std::cout << "        Hot water\n";
					set_dhw_schedule(dhwId, &locations[l].gateways[g].temperatureControlSystems[t].dhw[0].schedule);
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

bool EvohomeClient::verify_date(std::string szDateTime)
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


bool EvohomeClient::verify_datetime(std::string szDateTime)
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
std::string EvohomeClient::local_to_utc(std::string local_time)
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
std::string EvohomeClient::utc_to_local(std::string utc_time)
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
bool EvohomeClient::set_system_mode(std::string systemId, int mode)
{
	return set_system_mode(systemId, mode, "");
}
bool EvohomeClient::set_system_mode(std::string systemId, std::string mode)
{
	return set_system_mode(systemId, mode, "");
}
bool EvohomeClient::set_system_mode(std::string systemId, std::string mode, std::string date_until)
{
	int i = 0;
	int s = sizeof(evo_modes);
	while (s > 0)
	{
		if (evo_modes[i] == mode)
			return set_system_mode(systemId, i, date_until);
		s -= sizeof(evo_modes[i]);
		i++;
	}
	return false;
}
bool EvohomeClient::set_system_mode(std::string systemId, int mode, std::string date_until)
{
	std::stringstream data;
	data << "{\"SystemMode\":" << mode;
	if (date_until == "")
		data << ",\"TimeUntil\":null,\"Permanent\":true}";
	else
	{
		if ( ! verify_date(date_until) )
			return false;
		data << ",\"TimeUntil\":\"" << date_until.substr(0,10) << "T00:00:00Z\",\"Permanent\":false}";
	}
	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureControlSystem/";
	szUrl.append(systemId);
	szUrl.append("/mode");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, data.str(), m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}



/* 
 * Override a zone's target temperature
 */
bool EvohomeClient::set_temperature(std::string zoneId, std::string temperature, std::string time_until)
{
	std::stringstream data;
	data << "{\"HeatSetpointValue\":" << temperature;
	if (time_until == "")
		data << ",\"SetpointMode\":1,\"TimeUntil\":null}";
	else
	{
		if ( ! verify_datetime(time_until) )
			return false;
		data << ",\"SetpointMode\":2,\"TimeUntil\":\"" << time_until.substr(0,10) << "T" << time_until.substr(11,8) << "Z\"}";
	}

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
	szUrl.append(zoneId);
	szUrl.append("/heatSetpoint");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, data.str(), m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_temperature(std::string zoneId, std::string temperature)
{
	return set_temperature(zoneId, temperature, "");
}


/* 
 * Cancel a zone's target temperature override
 */
bool EvohomeClient::cancel_temperature_override(std::string zoneId)
{
	std::string szData = "{\"HeatSetpointValue\":0.0,\"SetpointMode\":0,\"TimeUntil\":null}";

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/temperatureZone/";
	szUrl.append(zoneId);
	szUrl.append("/heatSetpoint");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szData, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient::has_dhw(int location, int gateway, int temperatureControlSystem)
{
	EvohomeClient::temperatureControlSystem *tcs = &locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
	return has_dhw(tcs);
}
bool EvohomeClient::has_dhw(EvohomeClient::temperatureControlSystem *tcs)
{
	return (*tcs->installationInfo).isMember("dhw");
}


bool EvohomeClient::is_single_heating_system()
{
	if (locations.size() == 0)
		full_installation();
	return ( (locations.size() == 1) &&
		 (locations[0].gateways.size() == 1) &&
		 (locations[0].gateways[0].temperatureControlSystems.size() == 1) );
}


/* 
 * Set mode for Hot Water device
 */
bool EvohomeClient::set_dhw_mode(std::string dhwId, std::string mode, std::string time_until)
{
	std::stringstream data;
	if (mode == "auto")
		data << "{\"State\":0,\"Mode\":0,\"UntilTime\":null}";
	else
	{
		data << "{\"State\":";
		if (mode == "on")
			data << 1;
		else
			data << 0;
		if (time_until == "")
			data << ",\"Mode\":1,\"UntilTime\":null}";
		else
		{
			if ( ! verify_datetime(time_until) )
				return false;
			data << ",\"Mode\":2,\"UntilTime\":\"" << time_until.substr(0,10) << "T" << time_until.substr(11,8) << "Z\"}";
		}
	}

	std::string szUrl = EVOHOME_HOST"/WebAPI/emea/api/v1/domesticHotWater/";
	szUrl.append(dhwId);
	szUrl.append("/state");
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, data.str(), m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_dhw_mode(std::string dhwId, std::string mode)
{
	return set_dhw_mode(dhwId, mode, "");
}

