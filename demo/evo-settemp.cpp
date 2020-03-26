/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Demo app for connecting to Evohome
 *
 *
 *
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <cstdlib>
#include <time.h>
#include "evohomeclient2/evohomeclient.h"

#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif

#ifndef SCHEDULE_CACHE
#define SCHEDULE_CACHE "schedules.json"
#endif

using namespace std;

// Include common functions
//#include "evo-common.cpp"


using namespace std;

std::string configfile;
std::map<std::string, std::string> evoconfig;

bool verbose;

std::string ERROR = "ERROR: ";
std::string WARN = "WARNING: ";

std::string backupfile;

time_t now;
int tzoffset=-1;

bool read_evoconfig()
{
	ifstream myfile (configfile.c_str());
	if ( myfile.is_open() )
	{
		stringstream key,val;
		bool isKey = true;
		string line;
		unsigned int i;
		while ( getline(myfile,line) )
		{
			if ( (line[0] == '#') || (line[0] == ';') )
				continue;
			for (i = 0; i < line.length(); i++)
			{
				if ( (line[i] == ' ') || (line[i] == '\'') || (line[i] == '"') || (line[i] == 0x0d) )
					continue;
				if (line[i] == '=')
				{
					isKey = false;
					continue;
				}
				if (isKey)
					key << line[i];
				else
					val << line[i];
			}
			if ( ! isKey )
			{
				string skey = key.str();
				evoconfig[skey] = val.str();
				isKey = true;
				key.str("");
				val.str("");
			}
		}
		myfile.close();
		return true;
	}
	return false;
}


void exit_error(std::string message)
{
	cerr << message << endl;
	exit(1);
}


void usage(std::string mode)
{
	if (mode == "badparm")
	{
		cout << "Bad parameter" << endl;
		exit(1);
	}
	if (mode == "short")
	{
		cout << "Usage: evo-setmode [-hv] [-c file] <evohome mode>" << endl;
		cout << "Type \"evo-setmode --help\" for more help" << endl;
		exit(0);
	}
	cout << "Usage: evo-settemp [OPTIONS] <zoneid> <0|1> <setpoint> [ISO time]" << endl;
	cout << endl;
	cout << "  -v, --verbose           print a lot of information" << endl;
	cout << "  -c, --conf=FILE         use FILE for server settings and credentials" << endl;
	cout << "  -h, --help              display this help and exit" << endl;
	exit(0);
}


void parse_args(int argc, char** argv) {
	int i=1;
	std::string word;
	while (i < argc) {
		word = argv[i];
		if (word.length() > 1 && word[0] == '-' && word[1] != '-') {
			for (size_t j=1;j<word.length();j++) {
				if (word[j] == 'h') {
					usage("short");
					exit(0);
				} else if (word[j] == 'v') {
					verbose = true;
				} else {
					usage("badparm");
					exit(1);
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--verbose") {
			verbose = true;
		} else {
			usage("badparm");
			exit(1);
		}
		i++;
	}
}


std::string int_to_string(int myint)
{
	stringstream ss;
	ss << myint;
	return ss.str();
}


std::string utc_to_local(std::string utc_time)
{
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		struct tm utime;
		gmtime_r(&now, &utime);
		tzoffset = difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str()) - tzoffset;
	time_t ntime = mktime(&ltime);
	ntime--; // prevent compiler warning
	char until[40];
	sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return string(until);
}


map<std::string, std::string> evo_get_zone_data(evohome::device::temperatureControlSystem* tcs, int zoneindex)
{
	map<std::string, std::string> ret;

	ret["until"] = "";

	ret["zoneId"] = (*tcs->zones[zoneindex].jStatus)["zoneId"].asString();
	ret["temperature"] = (*tcs->zones[zoneindex].jStatus)["temperatureStatus"]["temperature"].asString();
	ret["targetTemperature"] = (*tcs->zones[zoneindex].jStatus)["heatSetpointStatus"]["targetTemperature"].asString();
	ret["setpointMode"] = (*tcs->zones[zoneindex].jStatus)["heatSetpointStatus"]["setpointMode"].asString();
	if (ret["setpointMode"] == "TemporaryOverride")
		ret["until"] = (*tcs->zones[zoneindex].jStatus)["heatSetpointStatus"]["until"].asString();
	return ret;
}

std::string local_to_utc(std::string utc_time)
{
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		struct tm utime;
		gmtime_r(&now, &utime);
		tzoffset = difftime(mktime(&utime), now);
	}
	struct tm ltime;
	ltime.tm_isdst = -1;
	ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
	ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
	ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
	ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
	ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
	ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str()) + tzoffset;
	time_t ntime = mktime(&ltime);
	ntime--; // prevent compiler warning
	char until[40];
	sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return string(until);
}


int main(int argc, char** argv)
{
	// get current time
	now = time(0);

	// set defaults
	evoconfig["hwname"] = "evohome";
	configfile = CONF_FILE;


	if ( ! read_evoconfig() )
		exit_error(ERROR+"can't read config file");

	if (verbose)
		cout << "connect to Evohome server\n";
	EvohomeClient2 eclient = EvohomeClient2(evoconfig["usr"],evoconfig["pw"]);

	if (strcmp(argv[2],"0") == 0 ) {
		// cancel override
		if ( ! eclient.cancel_temperature_override(string(argv[1])) )
			exit_error(ERROR+"failed to cancel override for zone "+argv[1]);

		eclient.cleanup();
		return 0;
	}

	std::string s_until = "";
	if (argc == 5)
	{
		// until set
		std::string utc_time = string(argv[4]);
		if (utc_time.length() < 19)
			exit_error(ERROR+"bad timestamp value on command line");
		struct tm ltime;
		ltime.tm_isdst = -1;
		ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
		ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
		ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
		ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
		ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
		ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str());
		time_t ntime = mktime(&ltime);
		if ( ntime == -1)
			exit_error(ERROR+"bad timestamp value on command line");
		char c_until[40];
		sprintf(c_until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
		s_until = string(c_until);
	}

	eclient.set_temperature(string(argv[1]), string(argv[3]), s_until);

	eclient.cleanup();
	return 0;
}

