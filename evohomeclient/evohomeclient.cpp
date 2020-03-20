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
//#include "webclient.h"
#include "evohomeclient.h"
#include "jsoncpp/json.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <stdexcept>

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
EvohomeClient::EvohomeClient(std::string user, std::string password)
{
	init();
	bool login_success;
	try
	{
		login_success = login(user, password);
	}
	catch (...)
	{
		throw;
	}

	if (!login_success)
		throw std::invalid_argument( "login fail" );
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
 * Throws std::runtime_error from (#webclient) web_connection_init
 */
void EvohomeClient::init()
{
	tzoffset = -1;
	lastDST = -1;
}


/*
 * Cleanup curl web client
 */
void EvohomeClient::cleanup()
{
	std::cout << "cleanup (v2) not implemented yet\n";
}


/*
 * Execute web query
 * Throws std::invalid_argument
 */
std::string EvohomeClient::send_receive_data(std::string szUrl, std::vector<std::string> &vExtraHeaders)
{
	std::string szResponse;
	std::string szHost = EVOHOME_HOST;
	EvoHTTPBridge::GET(szHost.append(szUrl), vExtraHeaders, szResponse, -1);
	return szResponse;
}
std::string EvohomeClient::send_receive_data(std::string szUrl, std::string szPostdata, std::vector<std::string> &vExtraHeaders)
{
	std::string szResponse;
	std::string szHost = EVOHOME_HOST;
	EvoHTTPBridge::POST(szHost.append(szUrl), szPostdata, vExtraHeaders, szResponse, -1);
	return szResponse;
}
std::string EvohomeClient::put_receive_data(std::string szUrl, std::string szPutdata, std::vector<std::string> &vExtraHeaders)
{
	std::string szResponse;
	std::string szHost = EVOHOME_HOST;
	EvoHTTPBridge::POST(szHost.append(szUrl), szPutdata, vExtraHeaders, szResponse, -1);
	return szResponse;
}


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/

/* 
 * Login to the evohome portal
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::login(std::string user, std::string password)
{
	std::vector<std::string> lheader;
	lheader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	lheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader.push_back("charsets: utf-8");

	std::stringstream pdata;
	pdata << "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=password";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous%20EMEA-V1-Get-Current-User-Account";
	pdata << "&Username=" << EvoHTTPBridge::URLEncode(user);
	pdata << "&Password=" << EvoHTTPBridge::URLEncode(password);
	pdata << "&Connection=Keep-Alive";

	std::string s_res;
	try
	{
		s_res = send_receive_data("/Auth/OAuth/Token", pdata.str(), lheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_login))
		return false;

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
		return false;

	v2access_token = j_login["access_token"].asString();
	v2refresh_token = j_login["refresh_token"].asString();
	v2token_expiration_time = time(NULL) + atoi(j_login["expires_in"].asString().c_str());
	std::stringstream atoken;
	atoken << "Authorization: bearer " << v2access_token;

	evoheader.clear();
	evoheader.push_back(atoken.str());
	evoheader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	evoheader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	evoheader.push_back("content-type: application/json");
	evoheader.push_back("charsets: utf-8");

	bool got_uid;
	try
	{
		got_uid = user_account();
	}
	catch (...)
	{
		throw;
	}
	return got_uid;
}


/* 
 * Renew the Authorization token
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::renew_login()
{
	std::vector<std::string> lheader;
	lheader.push_back("Authorization: Basic YjAxM2FhMjYtOTcyNC00ZGJkLTg4OTctMDQ4YjlhYWRhMjQ5OnRlc3Q=");
	lheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader.push_back("charsets: utf-8");

	std::stringstream pdata;
	pdata << "installationInfo-Type=application%2Fx-www-form-urlencoded;charset%3Dutf-8";
	pdata << "&Host=rs.alarmnet.com%2F";
	pdata << "&Cache-Control=no-store%20no-cache";
	pdata << "&Pragma=no-cache";
	pdata << "&grant_type=refresh_token";
	pdata << "&scope=EMEA-V1-Basic%20EMEA-V1-Anonymous";
	pdata << "&refresh_token=" << v2refresh_token;
	pdata << "&Connection=Keep-Alive";

	std::string s_res;
	try
	{
		s_res = send_receive_data("/Auth/OAuth/Token", pdata.str(), lheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_login))
		return false;

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
		return false;

	v2access_token = j_login["access_token"].asString();
	v2refresh_token = j_login["refresh_token"].asString();
	v2token_expiration_time = time(NULL) + atoi(j_login["expires_in"].asString().c_str());
	std::stringstream atoken;
	atoken << "Authorization: bearer " << v2access_token;

	evoheader.clear();
	evoheader.push_back(atoken.str());
	evoheader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	evoheader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	evoheader.push_back("content-type: application/json");
	evoheader.push_back("charsets: utf-8");

	bool got_uid;
	try
	{
		got_uid = user_account();
	}
	catch (...)
	{
		throw;
	}
	return got_uid;
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeClient::save_auth_to_file(std::string filename)
{
	std::ofstream myfile (filename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value j_auth;

		j_auth["access_token"] = v2access_token;
		j_auth["refresh_token"] = v2refresh_token;
		j_auth["expiration_time"] = v2token_expiration_time;

		myfile << j_auth.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load authorization key from a backup file
 */
bool EvohomeClient::load_auth_from_file(std::string filename)
{
	std::stringstream ss;
	std::ifstream myfile (filename.c_str());
	if ( myfile.is_open() )
	{
		std::string line;
		while ( getline (myfile,line) )
		{
			ss << line << '\n';
		}
		myfile.close();
	}
	Json::Value j_auth;
	Json::Reader jReader;
	if (!jReader.parse(ss.str().c_str(), j_auth))
		return false;

	v2access_token = j_auth["access_token"].asString();
	v2refresh_token = j_auth["refresh_token"].asString();
	v2token_expiration_time = static_cast<time_t>(atoi(j_auth["expiration_time"].asString().c_str()));

	if (time(NULL) > v2token_expiration_time)
		return renew_login();

	std::stringstream atoken;
	atoken << "Authorization: bearer " << v2access_token;

	evoheader.clear();
	evoheader.push_back(atoken.str());
	evoheader.push_back("applicationId: b013aa26-9724-4dbd-8897-048b9aada249");
	evoheader.push_back("accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	evoheader.push_back("content-type: application/json");
	evoheader.push_back("charsets: utf-8");

	bool got_uid;
	try
	{
		got_uid = user_account();
	}
	catch (...)
	{
		throw;
	}
	return got_uid;
}


/* 
 * Retrieve evohome user info
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::user_account()
{
	std::string s_res;
	try
	{
		s_res = send_receive_data("/WebAPI/emea/api/v1/userAccount", evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res[0] == '[') // got unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}
	Json::Value j_account;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_account) || !j_account.isMember("userId"))
		return false;

	v2uid = j_account["userId"].asString();
	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 ************************************************************************/

void EvohomeClient::get_dhw(int location, int gateway, int temperatureControlSystem)
{
	std::vector<zone>().swap(locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw);

	if (!has_dhw(&locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem]))
		return;

	Json::Value *j_tcs = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].installationInfo;

	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw.resize(1);
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw[0].installationInfo = &(*j_tcs)["dhw"];
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw[0].zoneId = (*j_tcs)["dhw"]["dhwId"].asString();;
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw[0].systemId = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].systemId;
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw[0].gatewayId = locations[location].gateways[gateway].gatewayId;
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].dhw[0].locationId = locations[location].locationId;
}


void EvohomeClient::get_zones(int location, int gateway, int temperatureControlSystem)
{
	std::vector<zone>().swap(locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones);
	Json::Value *j_tcs = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].installationInfo;

	if (!(*j_tcs)["zones"].isArray())
		return;

	size_t l = (*j_tcs)["zones"].size();
	locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones.resize(l);
	for (size_t i = 0; i < l; ++i)
	{
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].installationInfo = &(*j_tcs)["zones"][(int)(i)];
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].zoneId = (*j_tcs)["zones"][(int)(i)]["zoneId"].asString();
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].systemId = locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].systemId;
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].gatewayId = locations[location].gateways[gateway].gatewayId;
		locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem].zones[i].locationId = locations[location].locationId;
	}
}


void EvohomeClient::get_temperatureControlSystems(int location, int gateway)
{
	std::vector<temperatureControlSystem>().swap(locations[location].gateways[gateway].temperatureControlSystems);
	Json::Value *j_gw = locations[location].gateways[gateway].installationInfo;

	if (!(*j_gw)["temperatureControlSystems"].isArray())
		return;

	size_t l = (*j_gw)["temperatureControlSystems"].size();
	locations[location].gateways[gateway].temperatureControlSystems.resize(l);
	for (size_t i = 0; i < l; ++i)
	{
		locations[location].gateways[gateway].temperatureControlSystems[i].installationInfo = &(*j_gw)["temperatureControlSystems"][(int)(i)];
		locations[location].gateways[gateway].temperatureControlSystems[i].systemId = (*j_gw)["temperatureControlSystems"][(int)(i)]["systemId"].asString();
		locations[location].gateways[gateway].temperatureControlSystems[i].gatewayId = locations[location].gateways[gateway].gatewayId;
		locations[location].gateways[gateway].temperatureControlSystems[i].locationId = locations[location].locationId;

		get_zones(location, gateway, (int)(i));
		get_dhw(location, gateway, (int)(i));
	}
}


void EvohomeClient::get_gateways(int location)
{
	std::vector<gateway>().swap(locations[location].gateways);
	Json::Value *j_loc = locations[location].installationInfo;

	if (!(*j_loc)["gateways"].isArray())
		return;

	size_t l = (*j_loc)["gateways"].size();
	locations[location].gateways.resize(l);
	for (size_t i = 0; i < l; ++i)
	{
		locations[location].gateways[i].installationInfo = &(*j_loc)["gateways"][(int)(i)];
		locations[location].gateways[i].gatewayId = (*j_loc)["gateways"][(int)(i)]["gatewayInfo"]["gatewayId"].asString();
		locations[location].gateways[i].locationId = locations[location].locationId;

		get_temperatureControlSystems(location, (int)(i));
	}
}


/* 
 * Retrieve evohome installation info
 * Throws std::invalid_argument
 */
bool EvohomeClient::full_installation()
{
	std::vector<location>().swap(locations);
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/location/installationInfo?userId=" << v2uid << "&includeTemperatureControlSystems=True";

	// evohome v1 interface does not correctly format the json output
	std::stringstream ss_jdata;
	try
	{
		ss_jdata << "{\"locations\": " << send_receive_data(url.str(), evoheader) << "}";
	}
	catch (...)
	{
		throw;
	}
	Json::Reader jReader;
	j_fi.clear();
	if (!jReader.parse(ss_jdata.str(), j_fi) || !j_fi["locations"].isArray())
	{
		throw std::invalid_argument( "Failed to parse server response as JSON" );
		return false;
	}

	size_t l = j_fi["locations"].size();
	for (size_t i = 0; i < l; ++i)
	{
		location newloc = location();
		locations.push_back(newloc);
		locations[i].installationInfo = &j_fi["locations"][(int)(i)];
		locations[i].locationId = j_fi["locations"][(int)(i)]["locationInfo"]["locationId"].asString();

		get_gateways((int)(i));
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
 * Throws std::invalid_argument
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
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/location/" << locations[location].locationId << "/status?includeTemperatureControlSystems=True";
	std::string s_res;
	try
	{
		s_res = send_receive_data(url.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}

	Json::Reader jReader;
	j_stat.clear();
	if (!jReader.parse(s_res, j_stat))
	{
		throw std::invalid_argument( "Failed to parse server response as JSON" );
		return false;
	}
	locations[location].status = &j_stat;
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
 * Throws std::invalid_argument from (web) send_receive_data
 *
 * Returns ISO datatime string relative to UTC (timezone 'Z')
 */
std::string EvohomeClient::request_next_switchpoint(std::string zoneId)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/schedule/upcommingSwitchpoints?count=1";
	std::string s_res;
	try
	{
		s_res = send_receive_data(url.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res[0] == '[') // received unnamed array as reply
	{
		s_res[0] = ' ';
		size_t len = s_res.size();
		len--;
		s_res[len] = ' ';
	}

	Json::Value j_sp;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_sp) || !j_sp.isMember("time"))
		return "";

	std::stringstream ss;
	ss << j_sp["time"].asString() << 'Z';
	return ss.str();
}


/*
 * Retrieve a zone's schedule
 * Throws std::invalid_argument from (web) send_receive_data
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
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/" << zoneType << "/" << zoneId << "/schedule";
	std::string s_res;
	try
	{
		s_res = send_receive_data(url.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (!s_res.find("\"id\""))
		return false;
	EvohomeClient::zone* zone = get_zone_by_ID(zoneId);
	if (zone == NULL)
		return false;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), zone->schedule))
	{
		throw std::invalid_argument( "Failed to parse server response as JSON" );
		return false;
	}
	return true;
}


/*
 * Find a zone's next switchpoint (localtime)
 * May throw std::invalid_argument if the zone's schedule was not fetched before calling this function
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
		bool got_schedule;
		try
		{
			got_schedule = get_zone_schedule(tcs->zones[zone].zoneId);
		}
		catch (...)
		{
			throw;
		}
		if (!got_schedule)
			return "";
	}
	return get_next_switchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient::get_next_switchpoint(zone* hz)
{
	if (hz->schedule.isNull())
	{
		std::string zoneType = ((*hz->installationInfo).isMember("dhwId")) ? "domesticHotWater" : "temperatureZone";
		bool got_schedule;
		try
		{
			got_schedule = get_zone_schedule(hz->zoneId, zoneType);
		}
		catch (...)
		{
			throw;
		}
		if (!got_schedule)
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
 * May throw std::invalid_argument if the zone's schedule was not fetched before calling this function
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
		bool got_schedule;
		try
		{
			got_schedule = get_zone_schedule(tcs->zones[zone].zoneId);
		}
		catch (...)
		{
			throw;
		}
		if (!got_schedule)
			return "";
	}
	return get_next_utcswitchpoint(tcs->zones[zone].schedule);
}
std::string EvohomeClient::get_next_utcswitchpoint(zone* hz)
{
	if (hz->schedule.isNull())
	{
		std::string zoneType = ((*hz->installationInfo).isMember("dhwId")) ? "domesticHotWater" : "temperatureZone";
		bool got_schedule;
		try
		{
			got_schedule = get_zone_schedule(hz->zoneId, zoneType);
		}
		catch (...)
		{
			throw;
		}
		if (!got_schedule)
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
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::schedules_backup(std::string filename)
{
	std::ofstream myfile (filename.c_str(), std::ofstream::trunc);
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
						std::stringstream url;
						std::string s_zoneId = (*j_tcs)["zones"][(int)(iz)]["zoneId"].asString();
						if (s_zoneId.empty())
							continue;

						url << "/WebAPI/emea/api/v1/temperatureZone/" << s_zoneId << "/schedule";
						std::string s_res;
						try
						{
							s_res = send_receive_data(url.str(), evoheader);
						}
						catch (...)
						{
							throw;
						}
						if (!s_res.find("\"id\""))
							continue;

						Json::Reader jReader;
						Json::Value j_week;
						if (!jReader.parse(s_res, j_week))
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

						std::stringstream url;
						url << "/WebAPI/emea/api/v1/domesticHotWater/" << s_dhwId << "/schedule";
						std::string s_res;
						try
						{
							s_res = send_receive_data(url.str(), evoheader);
						}
						catch (...)
						{
							throw;
						}
						if ( ! s_res.find("\"id\""))
							return false;

						Json::Reader jReader;
						Json::Value j_week;
						if (!jReader.parse(s_res, j_week))
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
bool EvohomeClient::read_schedules_from_file(std::string filename)
{
	std::stringstream ss;
	std::ifstream myfile (filename.c_str());
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
 * Throws std::invalid_argument from (web) send_receive_data
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
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/" << zoneType << "/" << zoneId << "/schedule";
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

	std::string s_res;
	try
	{
		s_res = put_receive_data(url.str(), ss.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res.find("\"id\""))
		return true;
	return false;
}


/*
 * Restore all schedules from a backup file
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::schedules_restore(std::string filename)
{
	if ( ! read_schedules_from_file(filename) )
		return false;

	std::cout << "Restoring schedules from file " << filename << "\n";
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
					try
					{
						set_zone_schedule(locations[l].gateways[g].temperatureControlSystems[t].zones[z].zoneId, &locations[l].gateways[g].temperatureControlSystems[t].zones[z].schedule);
					}
					catch (...)
					{
						throw;
					}
				}
				if (has_dhw(&locations[l].gateways[g].temperatureControlSystems[t]))
				{
					std::string dhwId = (*locations[l].gateways[g].temperatureControlSystems[t].status)["dhw"]["dhwId"].asString();
					std::cout << "        Hot water\n";
					try
					{
						set_dhw_schedule(dhwId, &locations[l].gateways[g].temperatureControlSystems[t].dhw[0].schedule);
					}
					catch (...)
					{
						throw;
					}
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

bool EvohomeClient::verify_date(std::string date)
{
	if (date.length() < 10)
		return false;
	std::string s_date = date.substr(0,10);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(date.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(date.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(date.substr(8, 2).c_str());
	mtime.tm_hour = 12; // midday time - prevent date shift because of DST
	mtime.tm_min = 0;
	mtime.tm_sec = 0;
	time_t ntime = mktime(&mtime);
	if ( ntime == -1)
		return false;
	char rdata[12];
	sprintf_s(rdata,12,"%04d-%02d-%02d",mtime.tm_year+1900,mtime.tm_mon+1,mtime.tm_mday);
	return (s_date == std::string(rdata));
}

	
bool EvohomeClient::verify_datetime(std::string datetime)
{
	if (datetime.length() < 19)
		return false;
	std::string s_date = datetime.substr(0,10);
	std::string s_time = datetime.substr(11,8);
	struct tm mtime;
	mtime.tm_isdst = -1;
	mtime.tm_year = atoi(datetime.substr(0, 4).c_str()) - 1900;
	mtime.tm_mon = atoi(datetime.substr(5, 2).c_str()) - 1;
	mtime.tm_mday = atoi(datetime.substr(8, 2).c_str());
	mtime.tm_hour = atoi(datetime.substr(11, 2).c_str());
	mtime.tm_min = atoi(datetime.substr(14, 2).c_str());
	mtime.tm_sec = atoi(datetime.substr(17, 2).c_str());
	time_t ntime = mktime(&mtime);
	if ( ntime == -1)
		return false;
	char c_date[12];
	sprintf_s(c_date,12,"%04d-%02d-%02d",mtime.tm_year+1900,mtime.tm_mon+1,mtime.tm_mday);
	char c_time[12];
	sprintf_s(c_time,12,"%02d:%02d:%02d",mtime.tm_hour,mtime.tm_min,mtime.tm_sec);
	return ( (s_date == std::string(c_date)) && (s_time == std::string(c_time)) );
}


/*
 * Convert a localtime ISO datetime string to UTC
 */
std::string EvohomeClient::local_to_utc(std::string local_time)
{
	if (local_time.size() <  19)
		return "";
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		time_t now = time(0);
		struct tm utime;
		gmtime_r(&now, &utime);
		utime.tm_isdst = -1;
		tzoffset = (int)difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(local_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(local_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(local_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(local_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(local_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(local_time.substr(17, 2).c_str()) + tzoffset;
	mktime(&ltime);
	if (lastDST == -1)
		lastDST = ltime.tm_isdst;
	else if ((lastDST != ltime.tm_isdst) && (lastDST != -1)) // DST changed - must recalculate timezone offset
	{
		ltime.tm_hour -= (ltime.tm_isdst - lastDST);
		lastDST = ltime.tm_isdst;
		tzoffset = -1;
	}
	char until[22];
	sprintf_s(until,22,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return std::string(until);
}


/*
 * Convert a UTC ISO datetime string to localtime
 */
std::string EvohomeClient::utc_to_local(std::string utc_time)
{
	if (utc_time.size() <  19)
		return "";
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		time_t now = time(0);
		struct tm utime;
		gmtime_r(&now, &utime);
		utime.tm_isdst = -1;
		tzoffset = (int)difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str()) - tzoffset;
	mktime(&ltime);
	if (lastDST == -1)
		lastDST = ltime.tm_isdst;
	else if ((lastDST != ltime.tm_isdst) && (lastDST != -1)) // DST changed - must recalculate timezone offset
	{
		ltime.tm_hour += (ltime.tm_isdst - lastDST);
		lastDST = ltime.tm_isdst;
		tzoffset = -1;
	}
	char until[40];
	sprintf_s(until,40,"%04d-%02d-%02dT%02d:%02d:%02dA",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return std::string(until);
}


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/

/* 
 * Set the system mode
 * Throws std::invalid_argument from (web) send_receive_data
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
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureControlSystem/" << systemId << "/mode";
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
	std::string s_res;
	try
	{
		s_res = put_receive_data(url.str(), data.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res.find("\"id\""))
		return true;
	return false;
}



/* 
 * Override a zone's target temperature
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::set_temperature(std::string zoneId, std::string temperature, std::string time_until)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/heatSetpoint";
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
	std::string s_res;
	try
	{
		s_res = put_receive_data(url.str(), data.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_temperature(std::string zoneId, std::string temperature)
{
	return set_temperature(zoneId, temperature, "");
}


/* 
 * Cancel a zone's target temperature override
 * Throws std::invalid_argument from (web) send_receive_data
 */
bool EvohomeClient::cancel_temperature_override(std::string zoneId)
{
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/temperatureZone/" << zoneId << "/heatSetpoint";
	std::string s_data = "{\"HeatSetpointValue\":0.0,\"SetpointMode\":0,\"TimeUntil\":null}";
	std::string s_res;
	try
	{
		s_res = put_receive_data(url.str(), s_data, evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res.find("\"id\""))
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
 * Throws std::invalid_argument from (web) send_receive_data
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
	std::stringstream url;
	url << "/WebAPI/emea/api/v1/domesticHotWater/" << dhwId << "/state";
	std::string s_res;
	try
	{
		s_res = put_receive_data(url.str(), data.str(), evoheader);
	}
	catch (...)
	{
		throw;
	}
	if (s_res.find("\"id\""))
		return true;
	return false;
}
bool EvohomeClient::set_dhw_mode(std::string dhwId, std::string mode)
{
	return set_dhw_mode(dhwId, mode, "");
}

