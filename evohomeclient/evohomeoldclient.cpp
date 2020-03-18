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
#include "webclient.h"
#include "evohomeoldclient.h"
#include <stdexcept>


#define EVOHOME_HOST "https://tccna.honeywell.com"

#define SESSION_EXPIRATION_TIME 899


/*
 * Class construct
 */
EvohomeOldClient::EvohomeOldClient()
{
	init();
}
EvohomeOldClient::EvohomeOldClient(std::string user, std::string password)
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
	web_connection_init("old_evohome");
}


/*
 * Cleanup curl web client
 */
void EvohomeOldClient::cleanup()
{
	web_connection_cleanup("old_evohome");
}

/*
 * Execute web query
 * Throws std::invalid_argument
 */
std::string EvohomeOldClient::send_receive_data(std::string url, std::vector<std::string> &header)
{
	return send_receive_data(url, "", header);
}

std::string EvohomeOldClient::send_receive_data(std::string url, std::string postdata, std::vector<std::string> &header)
{
	std::stringstream ss_url;
	ss_url << EVOHOME_HOST << url;
	std::string s_res;
	try
	{
		s_res = web_send_receive_data("old_evohome", ss_url.str(), postdata, header);
	}
	catch (...)
	{
		throw;
	}

	if (s_res.find("<title>") != std::string::npos) // received an HTML page
	{
		std::stringstream ss_err;
		ss_err << "Bad return: ";
		int i = s_res.find("<title>");
		char* html = &s_res[i];
		i = 7;
		char c = html[i];
		while (c != '<')
		{
			ss_err << c;
			i++;
			c = html[i];
		}
		throw std::invalid_argument( ss_err.str() );
	}

	v1lastwebcall = time(NULL);
	return s_res;
}


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 ************************************************************************/


/* 
 * login to evohome web server
 * Throws std::invalid_argument
 */
bool EvohomeOldClient::login(std::string user, std::string password)
{
	std::vector<std::string> lheader;
	lheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");
	lheader.push_back("content-type: application/json");

	std::stringstream pdata;
	pdata << "{'Username': '" << user << "', 'Password': '" << password << "'";
	pdata << ", 'ApplicationId': '91db1612-73fd-4500-91b2-e63b069b185c'}";
	std::string s_res;
	try
	{
		s_res = send_receive_data("/WebAPI/api/Session", pdata.str(), lheader);
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

	Json::Value j_login;
	Json::Reader jReader;
	if (!jReader.parse(s_res.c_str(), j_login))
	{
		throw std::invalid_argument( "Failed to parse server response as JSON" );
		return false;
	}

	std::string szError = "";
	if (j_login.isMember("error"))
		szError = j_login["error"].asString();
	if (j_login.isMember("message"))
		szError = j_login["message"].asString();
	if (!szError.empty())
	{
		std::stringstream ss_err;
		ss_err << "Login to Evohome server failed with message: " << szError;
		throw std::invalid_argument( ss_err.str() );
		return false;
	}

	if (!j_login.isMember("sessionId") || !j_login.isMember("userInfo") || !j_login["userInfo"].isObject() || !j_login["userInfo"].isMember("userID"))
	{
		throw std::invalid_argument( "Login to Evohome server did not return required data" );
		return false;
	}

	v1sessionId = j_login["sessionId"].asString();
	std::stringstream atoken;
	atoken << "sessionId: " << v1sessionId;
	v1uid = j_login["userInfo"]["userID"].asString();

	evoheader.clear();
	evoheader.push_back(atoken.str());
	evoheader.push_back("applicationId: 91db1612-73fd-4500-91b2-e63b069b185c");
	evoheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	return true;
}


/*
 * Save authorization key to a backup file
 */
bool EvohomeOldClient::save_auth_to_file(std::string filename)
{
	std::ofstream myfile (filename.c_str(), std::ofstream::trunc);
	if ( myfile.is_open() )
	{
		Json::Value j_auth;

		j_auth["session_id"] = v1sessionId;
		j_auth["last_use"] = static_cast<int>(v1lastwebcall);
		j_auth["user_id"] = v1uid;

		myfile << j_auth.toStyledString() << "\n";
		myfile.close();
		return true;
	}
	return false;
}


/*
 * Load authorization key from a backup file
 */
bool EvohomeOldClient::load_auth_from_file(std::string filename)
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

	v1sessionId = j_auth["session_id"].asString();
	v1lastwebcall =	static_cast<time_t>(atoi(j_auth["last_use"].asString().c_str()));
	v1uid = j_auth["user_id"].asString();


	if ((time(NULL) - v1lastwebcall) > SESSION_EXPIRATION_TIME)
		return false;

	std::stringstream atoken;
	atoken << "sessionId: " << v1sessionId;

	evoheader.clear();
	evoheader.push_back(atoken.str());
	evoheader.push_back("applicationId: 91db1612-73fd-4500-91b2-e63b069b185c");
	evoheader.push_back("Accept: application/json, application/xml, text/json, text/x-json, text/javascript, text/xml");

	return true;
}


/************************************************************************
 *									*
 *	Evohome heating installations					*
 *									*
 ************************************************************************/

/* 
 * Retrieve evohome installation info
 * Throws std::invalid_argument
 */
bool EvohomeOldClient::full_installation()
{
	locations.clear();
	std::stringstream url;

	url << "/WebAPI/api/locations/?userId=" << v1uid << "&allData=True";

	// evohome 'old' interface does not correctly format the json output
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
	if (!jReader.parse(ss_jdata.str(), j_fi))
	{
		throw std::invalid_argument( "Failed to parse server response as JSON" );
		return false;
	}

	if (!j_fi["locations"].isArray())
		return false;

	size_t l = j_fi["locations"].size();
	for (size_t i = 0; i < l; ++i)
		locations[i].installationInfo = &j_fi["locations"][(int)(i)];

	return true;
}


/* 
 * Extract a zone's temperature from system status
 * Throws std::invalid_argument from full_installation()
 */
std::string EvohomeOldClient::get_zone_temperature(std::string locationId, std::string zoneId, int decimals)
{
	if (locations.size() == 0)
	{
		bool got_fi;
		try
		{
			got_fi = full_installation();
		}
		catch (...)
		{
			throw;
		}
		if (!got_fi)
			return "";
	}

	int multiplier = (decimals >= 2) ? 100:10;

	for (size_t iloc = 0; iloc < locations.size(); iloc++)
	{
		Json::Value *j_loc = locations[iloc].installationInfo;
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

			double v1temp = (*j_dev)["thermostat"]["indoorTemperature"].asDouble();
			if (v1temp > 127) // allow rounding error
				return "128"; // unit is offline

			// limit output to two decimals
			std::stringstream sstemp;
			sstemp << ((floor((v1temp * multiplier) + 0.5) / multiplier) + 0.0001);
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
