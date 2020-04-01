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

#include "evohomeclient2.hpp"
#include "jsoncpp/json.h"
#include "connection/EvoHTTPBridge.hpp"
#include "evohome/jsoncppbridge.hpp"

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
    static const std::string daynames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  }; // namespace schedule

  namespace API {

    namespace system {
      static const std::string modes[7] = {"Auto", "HeatingOff", "AutoWithEco", "Away", "DayOff", "", "Custom"};
    }; // namespace system

    namespace zone {
      static const std::string modes[7] = {"FollowSchedule", "PermanentOverride", "TemporaryOverride", "OpenWindow", "LocalOverride", "RemoteOverride", "Unknown"};
      static const std::string types[2] = {"temperatureZone", "domesticHotWater"};
      static const std::string states[2] = {"Off", "On"};
    }; // namespace zone

    namespace uri {
      static const std::string base = EVOHOME_HOST"/WebAPI/emea/api/v1/";

      static const std::string userAccount = "userAccount";
      static const std::string installationInfo = "location/installationInfo?includeTemperatureControlSystems=True&userId={id}";
      static const std::string status = "location/{id}/status?includeTemperatureControlSystems=True";
      static const std::string systemMode = "temperatureControlSystem/{id}/mode";
      static const std::string zoneSetpoint = "temperatureZone/{id}/heatSetpoint";
      static const std::string dhwState = "domesticHotWater/{id}/state";
      static const std::string zoneSchedule = "{type}/{id}/schedule";
      static const std::string zoneUpcoming = "{type}/{id}/schedule/upcommingSwitchpoints?count=1";


      static std::string get_uri(const std::string &szApiFunction, const std::string &szId = "", const int zoneType = 0)
      {
        std::string result = szApiFunction;

        if (szApiFunction == userAccount)
        {
          // all done
        }
        else if (szApiFunction == installationInfo)
          result.replace(71, 4, szId);
        else if (szApiFunction == status)
          result.replace(9, 4, szId);
        else if ((szApiFunction == zoneSchedule) || (szApiFunction == zoneUpcoming))
        {
          result.replace(7, 4, szId);
          result.replace(0, 6, evohome::API::zone::types[zoneType]);
        }
        else if (szApiFunction == systemMode)
          result.replace(25, 4, szId);
        else if (szApiFunction == zoneSetpoint)
          result.replace(16, 4, szId);
        else if (szApiFunction == dhwState)
          result.replace(17, 4, szId);
        else
          return ""; // invalid input

        return result.insert(0, evohome::API::uri::base);
      }


    }; // namespace uri

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
	login(szUser, szPassword);
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


bool EvohomeClient2::obtain_access_token(const std::string &szCredentials)
{
	std::vector<std::string> vLoginHeader;
	vLoginHeader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	vLoginHeader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	std::string szPostdata = "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8&Host=rs.alarmnet.com%2F";
	szPostdata.append("&Cache-Control=no-store%20no-cache&Pragma=no-cache&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous&Connection=Keep-Alive&");
	szPostdata.append(szCredentials);

	std::string szResponse;
	std::string szUrl = EVOHOME_HOST"/Auth/OAuth/Token";
	EvoHTTPBridge::SafePOST(szUrl, szPostdata, vLoginHeader, szResponse, -1);

	Json::Value jLogin;
	if (evohome::parse_json_string(szResponse, jLogin) < 0)
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
		m_szLastError = "login returned ";
		m_szLastError.append(szError);
		return false;
	}

	m_szAccessToken = jLogin["access_token"].asString();
	m_szRefreshToken = jLogin["refresh_token"].asString();
	m_tTokenExpirationTime = time(NULL) + atoi(jLogin["expires_in"].asString().c_str());
	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");

	return user_account();
}

/*
 * Login to the evohome portal
 */
bool EvohomeClient2::login(const std::string &szUser, const std::string &szPassword)
{
	std::string szCredentials = "grant_type=password&Username=";
	szCredentials.append(EvoHTTPBridge::URLEncode(szUser));
	szCredentials.append("&Password=");
	szCredentials.append(EvoHTTPBridge::URLEncode(szPassword));

	return obtain_access_token(szCredentials);
}


/*
 * Renew the Authorization token
 */
bool EvohomeClient2::renew_login()
{
	if (m_szRefreshToken.empty())
		return false;

	std::string szCredentials = "grant_type=refresh_token&refresh_token=";
	szCredentials.append(m_szRefreshToken);

	return obtain_access_token(szCredentials);
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
		jAuth["expiration_time"] = static_cast<int>(m_tTokenExpirationTime);

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
	if (evohome::parse_json_string(szFileContent, jAuth) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	m_szAccessToken = jAuth["access_token"].asString();
	m_szRefreshToken = jAuth["refresh_token"].asString();
	m_tTokenExpirationTime = static_cast<time_t>(atoi(jAuth["expiration_time"].asString().c_str()));

	if (time(NULL) > m_tTokenExpirationTime)
	{
		bool bRenew = renew_login();
		if (bRenew)
			save_auth_to_file(szFilename);
		return bRenew;
	}

	std::string szAuthBearer = "Authorization: bearer ";
	szAuthBearer.append(m_szAccessToken);

	m_vEvoHeader.clear();
	m_vEvoHeader.push_back(szAuthBearer);
	m_vEvoHeader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	m_vEvoHeader.push_back("content-type: application/json");

	return user_account();
}


/*
 * Retrieve evohome user info
 */
bool EvohomeClient2::user_account()
{
	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::userAccount);
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	Json::Value jUserAccount;
	if (evohome::parse_json_string(szResponse, jUserAccount) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	m_szUserId = jUserAccount["userId"].asString();
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 ************************************************************************/

void EvohomeClient2::get_dhw(const int location, const int gateway, const int temperatureControlSystem)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

	std::vector<evohome::device::zone>().swap((*myTCS).dhw);

	if (!has_dhw(myTCS))
		return;

	Json::Value *jTCS = (*myTCS).jInstallationInfo;
	(*myTCS).dhw.resize(1);
	(*myTCS).dhw[0].jInstallationInfo = &(*jTCS)["dhw"];
	(*myTCS).dhw[0].szZoneId = (*jTCS)["dhw"]["dhwId"].asString();;
	(*myTCS).dhw[0].szSystemId = (*myTCS).szSystemId;
	(*myTCS).dhw[0].szGatewayId = (*myTCS).szGatewayId;
	(*myTCS).dhw[0].szLocationId = (*myTCS).szLocationId;
}


void EvohomeClient2::get_zones(const int location, const int gateway, const int temperatureControlSystem)
{
	evohome::device::temperatureControlSystem *myTCS = &m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];

	std::vector<evohome::device::zone>().swap((*myTCS).zones);
	Json::Value *jTCS = (*myTCS).jInstallationInfo;

	if (!(*jTCS)["zones"].isArray())
		return;

	int l = static_cast<int>((*jTCS)["zones"].size());
	(*myTCS).zones.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myTCS).zones[i].jInstallationInfo = &(*jTCS)["zones"][i];
		(*myTCS).zones[i].szZoneId = (*jTCS)["zones"][i]["zoneId"].asString();
		(*myTCS).zones[i].szSystemId = (*myTCS).szSystemId;
		(*myTCS).zones[i].szGatewayId = (*myTCS).szGatewayId;
		(*myTCS).zones[i].szLocationId = (*myTCS).szLocationId;
	}
}


void EvohomeClient2::get_temperatureControlSystems(const int location, const int gateway)
{

	evohome::device::gateway *myGateway = &m_vLocations[location].gateways[gateway];

	std::vector<evohome::device::temperatureControlSystem>().swap((*myGateway).temperatureControlSystems);
	Json::Value *jGateway = (*myGateway).jInstallationInfo;

	if (!(*jGateway)["temperatureControlSystems"].isArray())
		return;

	int l = static_cast<int>((*jGateway)["temperatureControlSystems"].size());
	(*myGateway).temperatureControlSystems.resize(l);
	for (int i = 0; i < l; ++i)
	{
		(*myGateway).temperatureControlSystems[i].jInstallationInfo = &(*jGateway)["temperatureControlSystems"][i];
		(*myGateway).temperatureControlSystems[i].szSystemId = (*jGateway)["temperatureControlSystems"][i]["systemId"].asString();
		(*myGateway).temperatureControlSystems[i].szGatewayId = (*myGateway).szGatewayId;
		(*myGateway).temperatureControlSystems[i].szLocationId = (*myGateway).szLocationId;

		get_zones(location, gateway, i);
		get_dhw(location, gateway, i);
	}
}


void EvohomeClient2::get_gateways(const int location)
{
	std::vector<evohome::device::gateway>().swap(m_vLocations[location].gateways);
	Json::Value *jLocation = m_vLocations[location].jInstallationInfo;

	if (!(*jLocation)["gateways"].isArray())
		return;

	int l = static_cast<int>((*jLocation)["gateways"].size());
	m_vLocations[location].gateways.resize(l);
	for (int i = 0; i < l; ++i)
	{
		m_vLocations[location].gateways[i].jInstallationInfo = &(*jLocation)["gateways"][i];
		m_vLocations[location].gateways[i].szGatewayId = (*jLocation)["gateways"][i]["gatewayInfo"]["gatewayId"].asString();
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::installationInfo, m_szUserId);
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	// evohome API returns an unnamed json array which is not accepted by our parser
	szResponse.insert(0, "{\"locations\": ");
	szResponse.append("}");

	m_jFullInstallation.clear();
	if (evohome::parse_json_string(szResponse, m_jFullInstallation) < 0)
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
bool EvohomeClient2::get_status(int location)
{
	Json::Value *jLocation, *jGateway, *jTCS;
	if ((m_vLocations.size() == 0) || m_vLocations[location].szLocationId.empty())
		return false;

	bool valid_json = true;

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::status, m_vLocations[location].szLocationId);
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	m_jFullStatus.clear();
	if (evohome::parse_json_string(szResponse, m_jFullStatus) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}
	m_vLocations[location].jStatus = &m_jFullStatus;
	jLocation = m_vLocations[location].jStatus;

	// get gateway status
	if ((*jLocation)["gateways"].isArray())
	{
		int lgw = static_cast<int>((*jLocation)["gateways"].size());
		for (int igw = 0; igw < lgw; ++igw)
		{
			m_vLocations[location].gateways[igw].jStatus = &(*jLocation)["gateways"][igw];
			jGateway = m_vLocations[location].gateways[igw].jStatus;

			// get temperatureControlSystem status
			if ((*jGateway)["temperatureControlSystems"].isArray())
			{
				int ltcs = static_cast<int>((*jGateway)["temperatureControlSystems"].size());
				for (int itcs = 0; itcs < ltcs; itcs++)
				{
					m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].jStatus = &(*jGateway)["temperatureControlSystems"][itcs];
					jTCS = m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].jStatus;

/* ToDo: possible pitfall - does status return objects in the same order as installationInfo?
 *       according to API description a location can (currently) only contain one gateway and
 *       a gateway can only contain one TCS, so we should be safe up to this point. Is it also
 *       safe for zones though, or should we match the zone ID to be sure?
 */

					// get zone status
					if ((*jTCS)["zones"].isArray())
					{
						int lz = static_cast<int>((*jTCS)["zones"].size());
						for (int iz = 0; iz < lz; iz++)
						{
							m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].zones[iz].jStatus = &(*jTCS)["zones"][iz];
						}
					}
					else
						valid_json = false;

					if (has_dhw(&m_vLocations[location].gateways[igw].temperatureControlSystems[itcs]))
					{
						m_vLocations[location].gateways[igw].temperatureControlSystems[itcs].dhw[0].jStatus = &(*jTCS)["dhw"];
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


bool EvohomeClient2::get_status_by_ID(std::string szLocationId)
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
	for (int il = 0; il < numLocations; il++)
	{
		if (m_vLocations[il].szLocationId == szLocationId)
			return &m_vLocations[il];
	}
	return NULL;
}


evohome::device::gateway *EvohomeClient2::get_gateway_by_ID(std::string szGatewayId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			if (m_vLocations[il].gateways[igw].szGatewayId == szGatewayId)
				return &m_vLocations[il].gateways[igw];
		}
	}
	return NULL;
}


evohome::device::temperatureControlSystem *EvohomeClient2::get_temperatureControlSystem_by_ID(std::string szSystemId)
{
	if (m_vLocations.size() == 0)
		full_installation();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCSs; itcs++)
			{
				if (m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].szSystemId == szSystemId)
					return &m_vLocations[il].gateways[igw].temperatureControlSystems[itcs];
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
	for (int il = 0; il < numLocations; il++)
	{
		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCSs; itcs++)
			{
				int numZones = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].zones.size());
				for (int iz = 0; iz < numZones; iz++)
				{
					if (m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].zones[iz].szZoneId == szZoneId)
						return &m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].zones[iz];
				}
				if (m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].dhw.size() > 0)
				{
					if (m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].dhw[0].szZoneId == szZoneId)
						return &m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].dhw[0];
				}
			}
		}
	}
	return NULL;
}


evohome::device::temperatureControlSystem *EvohomeClient2::get_zone_temperatureControlSystem(evohome::device::zone *zone)
{
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		if (m_vLocations[il].szLocationId == zone->szLocationId)
		{
			int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
			for (int igw = 0; igw < numGateways; igw++)
			{
				if (m_vLocations[il].gateways[igw].szGatewayId == zone->szGatewayId)
				{
					int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
					for (int itcs = 0; itcs < numTCSs; itcs++)
					{
						if (m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].szSystemId == zone->szSystemId)
							return &m_vLocations[il].gateways[igw].temperatureControlSystems[itcs];
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
	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneUpcoming, szZoneId, 0);
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	Json::Value jSwitchPoint;
	if (evohome::parse_json_string(szResponse, jSwitchPoint) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return "";
	}

	std::string szSwitchpoint = jSwitchPoint["time"].asString();
	szSwitchpoint.append("Z");
	return szSwitchpoint;
}


/*
 * Retrieve a zone's schedule
 */
bool EvohomeClient2::get_zone_schedule(const std::string szZoneId)
{
	return get_zone_schedule_ex(szZoneId, 0);
}
bool EvohomeClient2::get_dhw_schedule(const std::string szDHWId)
{
	return get_zone_schedule_ex(szDHWId, 1);
}
bool EvohomeClient2::get_zone_schedule_ex(const std::string szZoneId, const int zoneType)
{

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneSchedule, szZoneId, zoneType);
	std::string szResponse;
	EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

	if (!szResponse.find("\"id\""))
		return false;
	evohome::device::zone *zone = get_zone_by_ID(szZoneId);
	if (zone == NULL)
		return false;

	if (evohome::parse_json_string(szResponse, zone->schedule) < 0)
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
 * Extended function also fills szCurrentSetpoint with the current target temperature
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
		int zoneType = ((*hz->jInstallationInfo).isMember("dhwId")) ? 1 : 0;
		if (!get_zone_schedule_ex(hz->szZoneId, zoneType))
			return "";
	}
	return get_next_switchpoint(hz->schedule);
}
std::string EvohomeClient2::get_next_switchpoint(Json::Value &jSchedule)
{
	std::string szCurrentSetpoint;
	return get_next_switchpoint(jSchedule, szCurrentSetpoint, -1, false);
}
std::string EvohomeClient2::get_next_switchpoint(Json::Value &jSchedule, std::string &szCurrentSetpoint, int force_weekday, bool convert_to_utc)
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
		szCurrentSetpoint = jSchedule["currentSetpoint"].asString();
		if (convert_to_utc)
			return local_to_utc(jSchedule["nextSwitchpoint"].asString());
		else
			return jSchedule["nextSwitchpoint"].asString();
	}

	std::string szTime;
	bool found = false;
	szCurrentSetpoint = "";
	for (uint8_t d = 0; ((d < 7) && !found); d++)
	{
		int tryDay = (wday + d) % 7;
		std::string szTryDay = (std::string)evohome::schedule::daynames[tryDay];
		Json::Value *jDaySchedule;
		// find day
		int numSchedules = static_cast<int>(jSchedule["dailySchedules"].size());
		for (int i = 0; ((i < numSchedules) && !found); i++)
		{
			jDaySchedule = &jSchedule["dailySchedules"][i];
			if (((*jDaySchedule).isMember("dayOfWeek")) && ((*jDaySchedule)["dayOfWeek"] == szTryDay))
				found = true;
		}
		if (!found)
			continue;

		found = false;
		int numSwitchpoints = static_cast<int>((*jDaySchedule)["switchpoints"].size());
		for (int i = 0; ((i < numSwitchpoints) && !found); ++i)
		{
			szTime = (*jDaySchedule)["switchpoints"][i]["timeOfDay"].asString();
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
			else if ((*jDaySchedule)["switchpoints"][i].isMember("temperature"))
				szCurrentSetpoint = (*jDaySchedule)["switchpoints"][i]["temperature"].asString();
			else
				szCurrentSetpoint = (*jDaySchedule)["switchpoints"][i]["dhwState"].asString();
		}
	}

	if (szCurrentSetpoint.empty()) // got a direct match for the next switchpoint, need to go back in time to find the current setpoint
	{
		found = false;
		for (uint8_t d = 1; ((d < 7) && !found); d++)
		{
			int tryDay = (wday - d + 7) % 7;
			std::string szTryDay = (std::string)evohome::schedule::daynames[tryDay];
			Json::Value *jDaySchedule;
			// find day
			int numSchedules = static_cast<int>(jSchedule["dailySchedules"].size());
			for (int i = 0; ((i < numSchedules) && !found); i++)
			{
				jDaySchedule = &jSchedule["dailySchedules"][i];
				if (((*jDaySchedule).isMember("dayOfWeek")) && ((*jDaySchedule)["dayOfWeek"] == szTryDay))
					found = true;
			}
			if (!found)
				continue;

			found = false;
			int j = static_cast<int>((*jDaySchedule)["switchpoints"].size());
			if (j > 0)
			{
				j--;
				if ((*jDaySchedule)["switchpoints"][j].isMember("temperature"))
					szCurrentSetpoint = (*jDaySchedule)["switchpoints"][j]["temperature"].asString();
				else
					szCurrentSetpoint = (*jDaySchedule)["switchpoints"][j]["dhwState"].asString();
				found = true;
			}
		}
	}

	if (!found)
		return "";

	sprintf_s(cDate, 30, "%04d-%02d-%02dT%sA", ltime.tm_year + 1900, ltime.tm_mon + 1, ltime.tm_mday, szTime.c_str()); // localtime => use CET to indicate that it is not UTC
	szDatetime = std::string(cDate);
	jSchedule["currentSetpoint"] = szCurrentSetpoint;
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
 * Extended function also fills szCurrentSetpoint with the current target temperature
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
		int zoneType = ((*hz->jInstallationInfo).isMember("dhwId")) ? 1 : 0;
		if (!get_zone_schedule_ex(hz->szZoneId, zoneType))
			return "";
	}
	return get_next_utcswitchpoint(hz->schedule);
}
std::string EvohomeClient2::get_next_utcswitchpoint(Json::Value &jSchedule)
{
	std::string szCurrentSetpoint;
	return get_next_switchpoint(jSchedule, szCurrentSetpoint, -1, true);
}
std::string EvohomeClient2::get_next_utcswitchpoint(Json::Value &jSchedule, std::string &szCurrentSetpoint, int force_weekday)
{
	return get_next_switchpoint(jSchedule, szCurrentSetpoint, force_weekday, true);
}



/*
 * Backup all schedules to a file
 */
bool EvohomeClient2::schedules_backup(const std::string &szFilename)
{
	std::ofstream myfile (szFilename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value jBackupSchedule;

		int numLocations = static_cast<int>(m_vLocations.size());
		for (int il = 0; il < numLocations; il++)
		{
			Json::Value *jLocation = m_vLocations[il].jInstallationInfo;
			std::string szLocationId = (*jLocation)["locationInfo"]["locationId"].asString();
			if (szLocationId.empty())
				continue;

			Json::Value jBackupScheduleLocation;
			jBackupScheduleLocation["locationId"] = szLocationId;
			jBackupScheduleLocation["name"] = (*jLocation)["locationInfo"]["name"].asString();

			int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
			for (int igw = 0; igw < numGateways; igw++)
			{
				Json::Value *jGateway = m_vLocations[il].gateways[igw].jInstallationInfo;
				std::string szGatewayId = (*jGateway)["gatewayInfo"]["gatewayId"].asString();
				if (szGatewayId.empty())
					continue;

				Json::Value jBackupScheduleGateway;
				jBackupScheduleGateway["gatewayId"] = szGatewayId;

				int numTCSs = static_cast<int>(m_vLocations[il].gateways[igw].temperatureControlSystems.size());
				for (int itcs = 0; itcs < numTCSs; itcs++)
				{
					Json::Value *jTCS = m_vLocations[il].gateways[igw].temperatureControlSystems[itcs].jInstallationInfo;
					std::string szTCSId = (*jTCS)["systemId"].asString();

					if (szTCSId.empty())
						continue;

					Json::Value jBackupScheduleTCS;
					jBackupScheduleTCS["systemId"] = szTCSId;
					if (!(*jTCS)["zones"].isArray())
						continue;

					int numZones = static_cast<int>((*jTCS)["zones"].size());
					for (int iz = 0; iz < numZones; iz++)
					{
						std::string szZoneId = (*jTCS)["zones"][iz]["zoneId"].asString();
						if (szZoneId.empty())
							continue;

						std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneSchedule, szZoneId, 0);
						std::string szResponse;
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

						if (!szResponse.find("\"id\""))
							continue;

						Json::Value jDailySchedule;
						if (evohome::parse_json_string(szResponse, jDailySchedule) < 0)
						{
							m_szLastError = "Failed to parse server response as JSON";
							continue;
						}

						Json::Value jBackupScheduleZone;
						jBackupScheduleZone["zoneId"] = szZoneId;
						jBackupScheduleZone["name"] = (*jTCS)["zones"][iz]["name"].asString();
						if (jDailySchedule["dailySchedules"].isArray())
							jBackupScheduleZone["dailySchedules"] = jDailySchedule["dailySchedules"];
						else
							jBackupScheduleZone["dailySchedules"] = Json::arrayValue;
						jBackupScheduleTCS[szZoneId] = jBackupScheduleZone;
					}
// Hot Water
					if (has_dhw(il, igw, itcs))
					{
						std::string szHotWaterId = (*jTCS)["dhw"]["dhwId"].asString();
						if (szHotWaterId.empty())
							continue;

						std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneSchedule, szHotWaterId, 1);
						std::string szResponse;
						EvoHTTPBridge::SafeGET(szUrl, m_vEvoHeader, szResponse, -1);

						if ( ! szResponse.find("\"id\""))
							return false;

						Json::Value jDailySchedule;
						if (evohome::parse_json_string(szResponse, jDailySchedule) < 0)
						{
							m_szLastError = "Failed to parse server response as JSON";
							continue;
						}

						Json::Value jBackupScheduleDHW;
						jBackupScheduleDHW["dhwId"] = szHotWaterId;
						if (jDailySchedule["dailySchedules"].isArray())
							jBackupScheduleDHW["dailySchedules"] = jDailySchedule["dailySchedules"];
						else
							jBackupScheduleDHW["dailySchedules"] = Json::arrayValue;
						jBackupScheduleTCS[szHotWaterId] = jBackupScheduleDHW;
					}
					jBackupScheduleGateway[szTCSId] = jBackupScheduleTCS;
				}
				jBackupScheduleLocation[szGatewayId] = jBackupScheduleGateway;
			}
			jBackupSchedule[szLocationId] = jBackupScheduleLocation;
		}

		myfile << jBackupSchedule.toStyledString() << "\n";
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

	Json::Value jSchedule;

	if (evohome::parse_json_string(szFileContent, jSchedule) < 0)
	{
		m_szLastError = "Failed to parse server response as JSON";
		return false;
	}

	Json::Value::Members locations = jSchedule.getMemberNames();
	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		if (jSchedule[locations[il]].isString())
			continue;
		Json::Value::Members gateways = jSchedule[locations[il]].getMemberNames();

		int numGateways = static_cast<int>(gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			if (jSchedule[locations[il]][gateways[igw]].isString())
				continue;

			Json::Value *jGateway = &jSchedule[locations[il]][gateways[igw]];
			Json::Value::Members temperatureControlSystems = (*jGateway).getMemberNames();

			int numTCS = static_cast<int>(temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCS; itcs++)
			{
				if ((*jGateway)[temperatureControlSystems[itcs]].isString())
					continue;

				Json::Value *jTCS = &(*jGateway)[temperatureControlSystems[itcs]];
				Json::Value::Members zones = (*jTCS).getMemberNames();

				int numZones = static_cast<int>(zones.size());
				for (int iz = 0; iz < numZones; iz++)
				{
					if ((*jTCS)[zones[iz]].isString())
						continue;
					evohome::device::zone *zone = get_zone_by_ID(zones[iz]);
					if (zone != NULL)
						zone->schedule = (*jTCS)[zones[iz]];
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
	return set_zone_schedule_ex(szZoneId, 0, jSchedule);
}
bool EvohomeClient2::set_dhw_schedule(const std::string szDHWId, Json::Value *jSchedule)
{
	return set_zone_schedule_ex(szDHWId, 1, jSchedule);
}
bool EvohomeClient2::set_zone_schedule_ex(const std::string szZoneId, int zoneType, Json::Value *jSchedule)
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneSchedule, szZoneId, zoneType);
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

	int numLocations = static_cast<int>(m_vLocations.size());
	for (int il = 0; il < numLocations; il++)
	{
		std::cout << "  Location: " << m_vLocations[il].szLocationId << "\n";

		int numGateways = static_cast<int>(m_vLocations[il].gateways.size());
		for (int igw = 0; igw < numGateways; igw++)
		{
			evohome::device::gateway *gw = &m_vLocations[il].gateways[igw];
			std::cout << "    Gateway: " << (*gw).szGatewayId << "\n";

			int numTCS = static_cast<int>((*gw).temperatureControlSystems.size());
			for (int itcs = 0; itcs < numTCS; itcs++)
			{
				evohome::device::temperatureControlSystem *tcs = &(*gw).temperatureControlSystems[itcs];
				std::cout << "      System: " << (*tcs).szSystemId << "\n";

				int numZones = static_cast<int>((*tcs).zones.size());
				for (int iz = 0; iz < numZones; iz++)
				{
					evohome::device::zone *zone = &(*tcs).zones[iz];
					std::cout << "        Zone: " << (*(*zone).jInstallationInfo)["name"].asString() << "\n";
					set_zone_schedule((*zone).szZoneId, &(*zone).schedule);
				}
				if (has_dhw(tcs))
				{
					std::string dhwId = (*(*tcs).jStatus)["dhw"]["dhwId"].asString();
					std::cout << "        Hot water\n";
					set_dhw_schedule(dhwId, &(*tcs).dhw[0].schedule);
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::systemMode, szSystemId);
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneSetpoint, szZoneId);
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::zoneSetpoint, szZoneId);
	std::string szResponse;
	EvoHTTPBridge::SafePUT(szUrl, szPutData, m_vEvoHeader, szResponse, -1);

	if (szResponse.find("\"id\""))
		return true;
	return false;
}


bool EvohomeClient2::has_dhw(int location, int gateway, int temperatureControlSystem)
{
	return has_dhw(&m_vLocations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem]);
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

	std::string szUrl = evohome::API::uri::get_uri(evohome::API::uri::dhwState, dhwId);
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

