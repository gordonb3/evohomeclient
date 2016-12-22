/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Main app for interfacing with Evohome from Domoticz
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <time.h>
#include <stdlib.h>
#include "../lib/domoticzclient.h"
#include "../lib/evohomeclient.h"


#ifndef MYNAME
#define MYNAME "evo-client"
#endif

#ifndef MYPATH
#define MYPATH "/"
#endif

#ifndef HWNAME
#define HWNAME "evohome"
#endif

#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif

#ifndef SCHEDULE_CACHE
#define SCHEDULE_CACHE "schedules.json"
#endif

#ifndef LOCKFILE
#ifdef _WIN32
#define LOCKFILE "evo-lastup"
#else
#define LOCKFILE "/tmp/evo-lastup"
#endif
#endif

#ifndef LOCKSECONDS
#define LOCKSECONDS 60
#endif

#define SETMODE_SCRIPT "--set-mode {status}"
#define SETDHW_SCRIPT "--set-dhw {deviceid} {mode} {state} {until}"
#define SETTEMP_SCRIPT "--set-temp {deviceid} {mode} {setpoint} {until}"

#define HARDWARE_TYPE "40"

#define CONTROLLER_SUBTYPE "Evohome"
#define CONTROLLER_SUBTYPE_ID "69"

#define HOTWATER_SUBTYPE "Hot Water"
#define HOTWATER_SUBTYPE_ID "71"

#define ZONE_SUBTYPE "Zone"
#define ZONE_SUBTYPE_ID "70"


#ifdef _WIN32
#define localtime_r(timep, result) localtime_s(result, timep)
#define gmtime_r(timep, result) gmtime_s(result, timep)
#endif

using namespace std;

time_t now;
int tzoffset, newzone;
bool createdev, updatedev, reloadcache, dobackup, verbose, dolog;
std::string command, backupfile, configfile, scriptfullname, s_ERROR, s_WARN, logfile, s_LOG;
std::map<std::string,std::string> evoconfig;
map<int,std::string> parameters;
ofstream flog;

void init_globals()
{
	evoconfig["hwname"] = HWNAME;
	configfile = CONF_FILE;
	scriptfullname = MYPATH MYNAME;

	now = time(0);
	tzoffset = -1;
	newzone = 0;
	createdev = false;
	updatedev = true;
	reloadcache = false;
	dobackup = true;
	verbose = false;
	command = "update";

	s_ERROR = "ERROR: ";
	s_WARN = "WARNING: ";

	dolog = false;
	logfile = "";
	s_LOG = "";
}


void usage(std::string mode)
{
	if (mode == "badparm")
	{
		cerr << "Bad parameter\n";
		exit(1);
	}
	if (mode == "missingfile")
	{
		cerr << "You need to supply a backupfile\n";
		exit(1);
	}
	if (mode == "short")
	{
		cout << "Usage: " << MYNAME << " [-hikuv] [-c file] [-b|-r file]\n";
		cout << "Type \"" << MYNAME << " --help\" for more help\n";
		exit(0);
	}
	cout << "Usage: " << MYNAME << " [OPTIONS]\n";
	cout << endl;
	cout << "  -h, --help                display this help and exit\n";
	cout << "  -i, --init                create Evohome devices in Domoticz\n";
	cout << "  -k, --keepdevs            do not update device settings - only status\n";
	cout << "  -u, --utc                 don't do localtime conversions\n";
	cout << "  -b, --backup  <FILE>      create a schedules backup\n";
	cout << "  -r, --restore <FILE>      restore a schedules backup\n";
	cout << "  -v, --verbose             print a lot of information\n";
	cout << "  -c, --conf=FILE           use FILE for server settings and credentials\n";
	cout << "  -l, --log=FILE            use FILE for logging (implies -v)\n";
	cout << "      --reload-cache        reload the schedules cache from the Evohome server\n";
	cout << "      --set-mode <PARMS>    set system mode\n";
	cout << "      --set-temp <PARMS>    set or cancel temperature override\n";
	cout << "      --set-dhw  <PARMS>    set or cancel domestic hot water override\n";
	cout << endl;
	cout << "Parameters for set arguments:\n";
	cout << " --set-mode <system mode> [time until] [+duration]\n";
	cout << " --set-temp <zone id> <setpoint mode> <target temperature> [time until] [+duration]\n";
	cout << " --set-dhw  <zone id> <dhw status> [time until] [+duration]\n";
	cout << endl;
	exit(0);
}


void print_out(std::string message)
{
	if (dolog)
		flog << message << endl;
	cout << message << endl;
}


void print_err(std::string message)
{
	if (dolog)
		flog << message << endl;
	cerr << message << endl;
}


bool read_evoconfig()
{
	ifstream myfile (configfile.c_str());
	if ( myfile.is_open() )
	{
		stringstream key,val;
		bool isKey = true;
		bool quoted = false;
		string line;
		unsigned int i;
		while ( getline(myfile,line) )
		{
			if ( (line[0] == '#') || (line[0] == ';') )
				continue;
			for (i = 0; i < line.length(); i++)
			{
				if ( (line[i] == '\'') || (line[i] == '"') )
				{
					quoted = ( ! quoted );
					continue;
				}
				if (line[i] == 0x0d)
					continue;
				if ( (line[i] == ' ') && ( ! quoted ) )
					continue;
				if (line[i] == '=')
				{
					isKey = false;
					continue;
				}
				if (isKey)
					key << line[i];
				else
				{
					if ( (line[i] == ' ') && (key.str() == "srt") )
						print_err("WARNING: space detected in 'srt' setting. Controlling Evohome from Domoticz will likely not function");
					val << line[i];
				}
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


void startlog(std::string fname)
{
	logfile = fname;
	flog.open(logfile.c_str(), ios::out | ios::app);
	if (flog.is_open())
	{
		dolog = true;
		struct tm ltime;
		localtime_r(&now, &ltime);
		char c_until[40];
		sprintf(c_until, "%04d-%02d-%02d %02d:%02d:%02d", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
		flog << MYNAME << " start: " << c_until << endl;
		return;
	}
	cerr << "WARNING: cannot open logfile '" << logfile << "'\n";
	dolog = false;
	verbose = true;
}


void log(std::string message)
{
	if (dolog)
	{
		flog << message << endl;
		return;
	}
	if (verbose)
		cout << message << endl;
}


void stoplog()
{
	if (!dolog)
		return;
	flog << "--" << endl;
	flog.close();
}


void exit_error(std::string message)
{
	print_err(message);
	stoplog();
	exit(1);
}


void touch_lockfile()
{
	ofstream myfile;
	myfile.open (LOCKFILE, ios::out | ios::trunc); 
	if ( myfile.is_open() )
	{
		time_t now = time(0) + LOCKSECONDS;
		myfile << (unsigned long)now;
		myfile.close();
	}
}


void check_lock_status()
{
	time_t now = time(0);
	ifstream myfile (LOCKFILE);
	if ( myfile.is_open() )
	{
		string line;
		getline(myfile,line);
		myfile.close();

		if ( (unsigned long)now < strtoul(line.c_str(),0,10) )
		{
			stringstream ss;
			ss << "Update not allowed at this time - please try again after " << LOCKSECONDS << " seconds";
			print_err(ss.str());
			stoplog();
			exit(0);
		}
		else
			if( remove(LOCKFILE) != 0 )
				print_err("Error deleting lockfile");
	}
	touch_lockfile(); // don't overload the server
}


EvohomeClient::temperatureControlSystem* select_temperatureControlSystem(EvohomeClient &eclient)
{
	int location = 0;
	int gateway = 0;
	int temperatureControlSystem = 0;
	bool is_unique_heating_system = false;
	if ( evoconfig.find("location") != evoconfig.end() ) {
		log(s_LOG+"using location from "+configfile);
		int l = eclient.locations.size();
		location = atoi(evoconfig["location"].c_str());
		if (location > l)
			exit_error(s_ERROR+"the Evohome location specified in "+configfile+" cannot be found");
		is_unique_heating_system = ( (eclient.locations[location].gateways.size() == 1) &&
						(eclient.locations[location].gateways[0].temperatureControlSystems.size() == 1)
						);
	}
	if ( evoconfig.find("gateway") != evoconfig.end() ) {
		log(s_LOG+"using gateway from "+configfile);
		int l = eclient.locations[location].gateways.size();
		gateway = atoi(evoconfig["gateway"].c_str());
		if (gateway > l)
			exit_error(s_ERROR+"the Evohome gateway specified in "+configfile+" cannot be found");
		is_unique_heating_system = (eclient.locations[location].gateways[gateway].temperatureControlSystems.size() == 1);
	}
	if ( evoconfig.find("controlsystem") != evoconfig.end() ) {
		log(s_LOG+"using controlsystem from "+configfile);
		int l = eclient.locations[location].gateways[gateway].temperatureControlSystems.size();
		temperatureControlSystem = atoi(evoconfig["controlsystem"].c_str());
		if (temperatureControlSystem > l)
			exit_error(s_ERROR+"the Evohome temperature controlsystem specified in "+configfile+" cannot be found");
		is_unique_heating_system = true;
	}

	if ( ! is_unique_heating_system)
		return NULL;

	return &eclient.locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
}


std::string get_domoticz_host(std::string url, std::string port)
{
	stringstream ss;
	if (url.substr(0,4) != "http")
		ss << "http://";
	ss << url;
	if (port.length() > 0)
		ss << ":" << port;
	return ss.str();
}


void parse_args(int argc, char** argv) {
	int i=1;
	int p=1;
	std::string word;
	while (i < argc) {
		word = argv[i];
		if (word.length() > 1 && word[0] == '-' && word[1] != '-') {
			for (size_t j=1;j<word.length();j++) {
				if (word[j] == 'h')
					usage("short");
				else if (word[j] == 'i')
					createdev = true;
				else if (word[j] == 'k')
					updatedev = false;
				else if (word[j] == 'u')
					tzoffset = 0;
				else if (word[j] == 'b')
				{
					if (j+1 < word.length())
						usage("badparm");
					command = "backup";
					dobackup = true;
					i++;
					if (i >= argc)
						usage("missingfile");
					backupfile = argv[i];
				}
				else if (word[j] == 'r')
				{
					if (j+1 < word.length())
						usage("badparm");
					command = "backup";
					dobackup = false;
					i++;
					if (i >= argc)
						usage("missingfile");
					backupfile = argv[i];
				}
				else if (word[j] == 'v')
					verbose = true;
				else if (word[j] == 'c')
				{
					if (j+1 < word.length())
						usage("badparm");
					i++;
					if (i >= argc)
						usage("badparm");
					configfile = argv[i];
				}
				else if (word[j] == 'l')
				{
					if (j+1 < word.length())
						usage("badparm");
					i++;
					if (i >= argc)
						usage("badparm");
					startlog(argv[i]);
				}
				else
					usage("badparm");
			}
		}
		else if (word == "--help")
			usage("long");
		else if (word == "--init")
			createdev = true;
		else if (word == "--keepdevs")
			updatedev = false;
		else if (word == "--utc")
			tzoffset = 0;
		else if (word == "--backup")
		{
			command = "backup";
			dobackup = true;
			i++;
			if (i >= argc)
				usage("missingfile");
			backupfile = argv[i];
		}
		else if (word == "--restore")
		{
			command = "backup";
			dobackup = false;
			i++;
			if (i >= argc)
				usage("missingfile");
			backupfile = argv[i];
		}
		else if (word == "--verbose")
			verbose = true;
		else if (word.substr(0,7) == "--conf=")
			configfile = word.substr(7);
		else if (word.substr(0,6) == "--log=")
			startlog(word.substr(6));
		else if (word == "--reload-cache")
			reloadcache = true;
		else if (word.substr(0,6) == "--set-")
		{
			updatedev = false;
			command = word.substr(2);
			i++;
			while ( (i < argc) && (argv[i][0] != '-') )
			{
				parameters[p] = argv[i];
				i++;
				p++;
			}
			continue;
		}
		else
			usage("badparm");
		i++;
	}
	if (createdev)
		updatedev = true;
}


map<std::string, std::string> evo_get_system_data(EvohomeClient::temperatureControlSystem* tcs)
{
	map<std::string, std::string> ret;
	json_object *j_tmp, *j_res;
	if ( ( json_object_object_get_ex(tcs->status, "systemModeStatus", &j_tmp)) && (json_object_object_get_ex( j_tmp, "mode", &j_res)) )
		ret["systemMode"] = json_object_get_string(j_res);
	ret["systemId"] = tcs->systemId;
	if (updatedev)
	{
		if (json_object_object_get_ex(tcs->installationInfo, "modelType", &j_res))
			ret["modelType"] = json_object_get_string(j_res);
	}
	return ret;
}


map<std::string, std::string> evo_get_dhw_data(EvohomeClient::temperatureControlSystem* tcs)
{
	map<std::string, std::string> ret;
	json_object *j_dhw, *j_tmp, *j_res;
	if (json_object_object_get_ex(tcs->status, "dhw", &j_dhw))
	{
		ret["until"] = "";
		if (json_object_object_get_ex(j_dhw, "dhwId", &j_res))
			ret["dhwId"] = json_object_get_string(j_res);
		if ( (json_object_object_get_ex(tcs->status, "temperatureStatus", &j_tmp)) && (json_object_object_get_ex( j_tmp, "temperature", &j_res)) )
			ret["temperature"] = json_object_get_string(j_res);
		if ( json_object_object_get_ex(tcs->status, "stateStatus", &j_tmp))
		{
			if (json_object_object_get_ex(j_tmp, "state", &j_res))
				ret["state"] = json_object_get_string(j_res);
			if (json_object_object_get_ex(j_tmp, "mode", &j_res))
				ret["mode"] = json_object_get_string(j_res);
			if ( (ret["mode"] == "TemporaryOverride") && (json_object_object_get_ex(j_tmp, "until", &j_res)) )
				ret["until"] = json_object_get_string(j_res);
		}
	}
	return ret;
}


map<std::string, std::string> evo_get_zone_data(EvohomeClient::temperatureControlSystem* tcs, int zoneindex)
{
	map<std::string, std::string> ret;

	json_object_object_foreach(tcs->zones[zoneindex].status, key, val)
	{
		if ( (strcmp(key, "zoneId") == 0) || (strcmp(key, "name") == 0) )
		{
			ret[key] = json_object_get_string(val);
		}
		else if ( (strcmp(key, "temperatureStatus") == 0) || (strcmp(key, "heatSetpointStatus") == 0) )
		{
			json_object_object_foreach(val, key2, val2)
			{
				ret[key2] = json_object_get_string(val2);
			}
		}
	}
	return ret;
}


std::string int_to_string(int myint)
{
	stringstream ss;
	ss << myint;
	return ss.str();
}


std::string local_to_utc(std::string utc_time)
{
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		struct tm utime;
		gmtime_r(&now, &utime);
		tzoffset = (int)difftime(mktime(&utime), now);
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


std::string utc_to_local(std::string utc_time)
{
	if (tzoffset == -1)
	{
		// calculate timezone offset once
		struct tm utime;
		gmtime_r(&now, &utime);
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
	time_t ntime = mktime(&ltime);
	ntime--; // prevent compiler warning
	char until[40];
	sprintf(until,"%04d-%02d-%02dT%02d:%02d:%02dZ",ltime.tm_year+1900,ltime.tm_mon+1,ltime.tm_mday,ltime.tm_hour,ltime.tm_min,ltime.tm_sec);
	return string(until);
}



// Get Evohome hardware ID from Domoticz
int get_evohome_hardwareId(DomoticzClient &dclient)
{
	int hwid = dclient.get_hwid(HARDWARE_TYPE, evoconfig["hwname"]);
	stringstream ss;
	ss << "got ID '" << hwid << "' for Evohome hardware with name '" << evoconfig["hwname"] << "'";
	log(ss.str());
	if (createdev)
	{
		log("init mode enabled\ncreate hardware in Domoticz");
		if (hwid >= 0)
			print_err("WARNING: hardware device "+evoconfig["hwname"]+" already exists");
		else
			hwid = dclient.create_hardware(HARDWARE_TYPE, evoconfig["hwname"]);
	}

	if (hwid == -1)
		exit_error(s_ERROR+"evohome hardware not found");
	return hwid;
}


// get Evohome devices from Domoticz
void get_evohome_devices(DomoticzClient &dclient, int hwid)
{
	dclient.get_devices(hwid);
	if (createdev)
	{
		int controllers = 0;
		int dhws = 0;
		int zones = 0;
		for (std::map<std::string, domoticz_device>::iterator it=dclient.devices.begin(); it!=dclient.devices.end(); ++it)
		{
			if (it->second.SubType == CONTROLLER_SUBTYPE)
				controllers++;
			else if (it->second.SubType == HOTWATER_SUBTYPE)
				dhws++;
			else if (it->second.SubType == ZONE_SUBTYPE)
				zones++;
			else
				print_err("WARNING: got device with unknown SubType '"+it->second.SubType+"'");
		}
		if (controllers == 0)
		{
			log("create evohome controller in Domoticz");
			dclient.create_evohome_device(hwid, CONTROLLER_SUBTYPE_ID);
		}
		if (dhws == 0)
		{
			log("create hot water device in Domoticz");
			dclient.create_evohome_device(hwid, HOTWATER_SUBTYPE_ID);
		}
		int i;
		for (i = zones; i < 12; i++)
		{
			log("create heating device in Domoticz");
			dclient.create_evohome_device(hwid, ZONE_SUBTYPE_ID);
		}

		if ( (controllers == 0) || (dhws == 0) || (zones < 12) )
			dclient.get_devices(hwid);
	}
}


void update_system(DomoticzClient &dclient, map<std::string,std::string> systemdata)
{
	std::string idx="";
	if ( updatedev && (dclient.devices.find(systemdata["systemId"]) == dclient.devices.end()) )
	{
		int _newzone = 0;
		std::string s_newzone = int_to_string(_newzone);
		while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != CONTROLLER_SUBTYPE) )
		{
			_newzone++;
			s_newzone = int_to_string(_newzone);
		}
		if (dclient.devices.find(s_newzone) == dclient.devices.end())
			print_err("WARNING: there does not appear to be a Controller device for this hardware in Domoticz");
		else
			idx = dclient.devices[s_newzone].idx;
	}
	else
		idx = dclient.devices[systemdata["systemId"]].idx;
	if ( (updatedev) && (idx != "") )
	{
		stringstream sms;
		sms << scriptfullname << " " << SETMODE_SCRIPT;
		if (configfile != CONF_FILE)
			sms << " -c " << configfile;
		log(s_LOG+" - change Evohome system name to '"+systemdata["modelType"]+"'\n - change setmode script path to '"+sms.str()+"'");
		dclient.update_system_dev(idx, systemdata["systemId"], systemdata["modelType"], sms.str());
	}
	if (idx != "")
	{
		log(s_LOG+" - change Evohome system status to '"+systemdata["systemMode"]+"'");
		dclient.update_system_mode(dclient.devices[systemdata["systemId"]].idx, systemdata["systemMode"]);
	}
}


void update_dhw(DomoticzClient &dclient, map<std::string,std::string> dhwdata)
{
	std::string idx="";
	if ( updatedev && (dclient.devices.find(dhwdata["dhwId"]) == dclient.devices.end()) )
	{
		int _newzone = 0;
		std::string s_newzone = int_to_string(_newzone);
		while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != HOTWATER_SUBTYPE) )
		{
			_newzone++;
			s_newzone = int_to_string(_newzone);
		}
		if (dclient.devices.find(s_newzone) == dclient.devices.end())
			print_err("WARNING: can't register new Hot Water device because you have no free zones available for this hardware in Domoticz");
		else
			idx = dclient.devices[s_newzone].idx;
	}
	else
		idx = dclient.devices[dhwdata["dhwId"]].idx;
	if ( (updatedev) && (idx != "") )
	{
		stringstream sms;
		sms << scriptfullname << " " << SETDHW_SCRIPT;
		if (configfile != CONF_FILE)
			sms << " -c " << configfile;
		log(s_LOG+" - change hotwater script path to '"+sms.str()+"'");
		dclient.update_zone_dev(idx, dhwdata["dhwId"], "Hot Water", sms.str());
	}
	if (idx != "")
	{
		log(s_LOG+" - change status of Hot Water device: temperature = "+dhwdata["temperature"]+", state = "+dhwdata["state"]+", mode = "+dhwdata["mode"]+", until = "+dhwdata["until"]);
		dclient.update_zone_status(idx, dhwdata["temperature"], dhwdata["state"], dhwdata["mode"], dhwdata["until"]);
	}
}


void update_zone(DomoticzClient &dclient, map<std::string,std::string> zonedata)
{
	std::string idx="";
	if ( updatedev && (dclient.devices.find(zonedata["zoneId"]) == dclient.devices.end()) )
	{
		std::string s_newzone = int_to_string(newzone);
		while ( (dclient.devices.find(s_newzone) != dclient.devices.end()) && (dclient.devices[s_newzone].SubType != ZONE_SUBTYPE) )
		{
			newzone++;
			s_newzone = int_to_string(newzone);
		}
		if (dclient.devices.find(s_newzone) == dclient.devices.end())
			cerr << "WARNING: can't register new Evohome zone because you have no free zones available for this hardware in Domoticz\n";
		else
			idx = dclient.devices[s_newzone].idx;
		newzone++;
	}
	else
		idx = dclient.devices[zonedata["zoneId"]].idx;
	if ( (updatedev) && (idx != "") )
	{
		stringstream sms;
		sms << scriptfullname << " " << SETTEMP_SCRIPT;
		if (configfile != CONF_FILE)
			sms << " -c " << configfile;
		log(s_LOG+" - set name of zone device "+idx+" to '"+zonedata["name"]+"'\n - change zone script path to '"+sms.str()+"'");
		dclient.update_zone_dev(idx, zonedata["zoneId"], zonedata["name"], sms.str());
	}
	if (idx != "")
	{
		log(s_LOG+" - change status of zone "+zonedata["name"]+": temperature = "+zonedata["temperature"]+", setpoint = "+zonedata["targetTemperature"]+", mode = "+zonedata["setpointMode"]+", until = "+zonedata["until"]);
		dclient.update_zone_status(idx, zonedata["temperature"], zonedata["targetTemperature"], zonedata["setpointMode"], zonedata["until"]);
	}
}


void read_schedules(EvohomeClient &eclient)
{
	/* retrieving schedules is painfully slow as we can only fetch them one zone at a time.
	 * luckily schedules do not change very often, so we can use a local cache
	 */
	if ( reloadcache || ( ! eclient.read_schedules_from_file(SCHEDULE_CACHE) ) )
	{
		log("reloading schedules cache");
		if ( ! eclient.schedules_backup(SCHEDULE_CACHE) )
			exit_error(s_ERROR+"failed to open schedule cache file '"+SCHEDULE_CACHE+"'");
		eclient.read_schedules_from_file(SCHEDULE_CACHE);
	}
	log("read schedules from cache");
}


void cmd_update()
{
	// connect to Domoticz server
	log("connect to Domoticz server");
	DomoticzClient dclient = DomoticzClient(get_domoticz_host(evoconfig["url"], evoconfig["port"]));

	// Get Evohome hardware ID from Domoticz
	int hwid = get_evohome_hardwareId(dclient);

	// get Evohome devices from Domoticz
	get_evohome_devices(dclient, hwid);

	// connect to Evohome server
	log("connect to Evohome server");
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);

	// get Evohome installation info
	log("retrieve Evohome installation info");
	eclient.full_installation();

	// get Evohome heating system
	EvohomeClient::temperatureControlSystem* tcs = NULL;
	if ( evoconfig.find("systemId") != evoconfig.end() )
	{
		log(s_LOG+"using systemId from "+configfile);
 		tcs = eclient.get_temperatureControlSystem_by_ID(evoconfig["systemId"]);
		if (tcs == NULL)
			exit_error(s_ERROR+"the Evohome systemId specified in "+configfile+" cannot be found");
	}
	else if (eclient.is_single_heating_system())
		tcs = &eclient.locations[0].gateways[0].temperatureControlSystems[0];
	else
		select_temperatureControlSystem(eclient);
	if (tcs == NULL)
		exit_error(s_ERROR+"multiple Evohome systems found - don't know which one to use for status");


	// get status for Evohome heating system
	log("retrieve status of Evohome heating system");
	if ( !	eclient.get_status(tcs->locationId) )
		exit_error(s_ERROR+"failed to retrieve status");

	// get schedules
	read_schedules(eclient);

	if (updatedev)
	{
#ifdef _WIN32
		stringstream ss;
		ss << hex;
		if (evoconfig["srt"].substr(0,9) == "script://")
			ss << evoconfig["srt"] << scriptfullname << ".exe";
		else
		{
			int l = evoconfig["srt"].length();
			int i;
			char c;
			ss << "script://";
			for (i = 0; i < l; i++)
			{
				c = evoconfig["srt"][i];
				if (c == '\\')
				{
					ss << '/';
					continue;
				}
				if ( (c == '-') || (c == '_') || (c == '.') || (c == '~') || (c == ':') )
				{
					ss << c;
					continue;
				}
				if ( (c >= 0x30) && (c < 0x3A) )
				{
					ss << c;
					continue;
				}
				if ( ((c|0x20) > 0x60) && ((c|0x20) < 0x7b) )
				{
					ss << c;
					continue;
				}
				if (c < 0x10)
					ss << uppercase << "%0" << int((unsigned char) c) << nouppercase;
				else
					ss << uppercase << '%' << int((unsigned char) c) << nouppercase;
			}
			ss << scriptfullname << ".exe";
		}
		scriptfullname = ss.str();
#else
		stringstream ss;
		if (evoconfig["srt"].substr(0,9) != "script://")
			ss << "script://";
		ss << evoconfig["srt"] << scriptfullname;
		scriptfullname = ss.str();
#endif
	}


	// Update system status
	log("start write of Evohome data to Domoticz:");
	map<std::string, std::string> systemdata = evo_get_system_data(tcs);
	update_system(dclient, systemdata);

	// Update hot water status
	if (eclient.has_dhw(tcs))
	{
		map<std::string, std::string> dhwdata = evo_get_dhw_data(tcs);
		if (dhwdata["until"] == "")
			dhwdata["until"] = local_to_utc(eclient.get_next_switchpoint(tcs, atoi(dhwdata["dhwId"].c_str())));
		update_dhw(dclient, dhwdata);
	}

	// Update zones
	for (std::map<int, EvohomeClient::zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
	{
		std::map<std::string, std::string> zonedata = evo_get_zone_data(tcs, it->first);
		if (zonedata["until"].length() == 0)
			zonedata["until"] = local_to_utc(eclient.get_next_switchpoint(tcs, it->first));
		update_zone(dclient, zonedata);
	}

	log("Done!");

	// cleanup
	eclient.cleanup();
	dclient.cleanup();
}


void cmd_backup_and_restore_schedules()
{
	// connect to Evohome server
	log("connect to Evohome server");
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);

	// retrieve Evohome installation
	log("retrieve Evohome installation info");
	eclient.full_installation();

	if (dobackup)	// backup
	{
		print_out("Start backup of Evohome schedules");
		if ( ! eclient.schedules_backup(backupfile) )
			exit_error(s_ERROR+"failed to open backup file '"+backupfile+"'");
		print_out("Done!");
	}
	else		// restore
	{
		print_out("Start restore of Evohome schedules");
		if ( ! eclient.schedules_restore(backupfile) )
			exit_error(s_ERROR+"failed to open backup file '"+backupfile+"'");
		print_out("Done!");
	}

	eclient.cleanup();
}


std::string format_time(std::string utc_time)
{
	struct tm ltime;
	if (utc_time[0] == '+')
	{
		int minutes = atoi(utc_time.substr(1).c_str());
		gmtime_r(&now, &ltime);
		ltime.tm_min += minutes;
		
	}
	else if (utc_time.length() < 19)
		exit_error(s_ERROR+"bad timestamp value on command line");
	else
	{
		ltime.tm_year = atoi(utc_time.substr(0, 4).c_str()) - 1900;
		ltime.tm_mon = atoi(utc_time.substr(5, 2).c_str()) - 1;
		ltime.tm_mday = atoi(utc_time.substr(8, 2).c_str());
		ltime.tm_hour = atoi(utc_time.substr(11, 2).c_str());
		ltime.tm_min = atoi(utc_time.substr(14, 2).c_str());
		ltime.tm_sec = atoi(utc_time.substr(17, 2).c_str());
	}
	ltime.tm_isdst = -1;
	time_t ntime = mktime(&ltime);
	if ( ntime == -1)
		exit_error(s_ERROR+"bad timestamp value on command line");
	char c_until[40];
	sprintf(c_until, "%04d-%02d-%02dT%02d:%02d:%02dZ", ltime.tm_year+1900, ltime.tm_mon+1, ltime.tm_mday, ltime.tm_hour, ltime.tm_min, ltime.tm_sec);
	return string(c_until);
}


void cancel_temperature_override()
{
	log("connect to Evohome server");
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);

	if ( ! eclient.cancel_temperature_override(parameters[1]) )
		exit_error(s_ERROR+"failed to cancel override for zone "+parameters[1]);

	// get Evohome installation info
	log("retrieve Evohome installation info");
	eclient.full_installation();

	// get Evohome heating system
	EvohomeClient::temperatureControlSystem* tcs = NULL;
	if ( evoconfig.find("systemId") != evoconfig.end() )
	{
		log(s_LOG+"using systemId from "+configfile);
 		tcs = eclient.get_temperatureControlSystem_by_ID(evoconfig["systemId"]);
		if (tcs == NULL)
			exit_error(s_ERROR+"the Evohome systemId specified in "+configfile+" cannot be found");
	}
	else if (eclient.is_single_heating_system())
		tcs = &eclient.locations[0].gateways[0].temperatureControlSystems[0];
	else
		select_temperatureControlSystem(eclient);
	if (tcs == NULL)
		exit_error(s_ERROR+"multiple Evohome systems found - don't know which one to use for status");

	// get schedules
	read_schedules(eclient);

	log("connect to Domoticz server");
	DomoticzClient dclient = DomoticzClient(get_domoticz_host(evoconfig["url"], evoconfig["port"]));

	// Get Evohome hardware ID from Domoticz
	int hwid = get_evohome_hardwareId(dclient);

	// get Evohome devices from Domoticz
	get_evohome_devices(dclient, hwid);

	// update zone
	for (std::map<int, EvohomeClient::zone>::iterator it=tcs->zones.begin(); it!=tcs->zones.end(); ++it)
	{
		if (parameters[1] == it->second.zoneId)
		{
			log("update Domoticz zone status with schedule information");
			std::map<std::string, std::string> zonedata;
			zonedata["idx"] = dclient.devices[it->second.zoneId].idx;
			zonedata["temperature"] = dclient.devices[it->second.zoneId].Temp;
			zonedata["setpointMode"] = "FollowSchedule";
			zonedata["zoneId"] = it->second.zoneId;
			zonedata["name"] = dclient.devices[it->second.zoneId].Name;
			zonedata["until"] = eclient.get_next_switchpoint_ex(tcs->zones[it->first].schedule, zonedata["targetTemperature"]);
			update_zone(dclient, zonedata);
		}
	}
	dclient.cleanup();
	eclient.cleanup();
	log("Done!");
}

void cmd_set_temperature()
{
	if ( (parameters.size() > 1) && (parameters[2] == "0") )
	{
		cancel_temperature_override();
		exit(0);
	}
	if ( (parameters.size() < 3) || (parameters.size() > 4) )
		usage("badparm");

	std::string s_until = "";
	if (parameters.size() == 4)
		s_until = format_time(parameters[4]);


	log("connect to Evohome server");
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);

	log("set target temperature");
	eclient.set_temperature(parameters[1], parameters[3], s_until);

	if (s_until != "")
	{
		// correct UTC until time in Domoticz
		log("connect to Domoticz server");
		DomoticzClient dclient = DomoticzClient(get_domoticz_host(evoconfig["url"], evoconfig["port"]));
		int hwid = dclient.get_hwid(HARDWARE_TYPE, evoconfig["hwname"]);
		stringstream ss;
		ss << "got ID '" << hwid << "' for Evohome hardware with name '" << evoconfig["hwname"] << "'";
		log(ss.str());
		if (hwid == -1)
			exit_error(s_ERROR+"evohome hardware not found");
		dclient.get_devices(hwid);
		std::string idx = dclient.devices[parameters[1]].idx;
		std::string temperature = dclient.devices[parameters[1]].Temp;
		log("correct until time value in Domoticz");
		dclient.update_zone_status(idx, temperature, parameters[3], "TemporaryOverride", s_until);
		dclient.cleanup();
	}

	eclient.cleanup();
	log("Done!");
}


void cmd_set_system_mode()
{
	if ( (parameters.size() == 0) || (parameters.size() > 2) )
		usage("badparm");
	std::string systemId;
	std::string until = "";
	std::string mode = parameters[1];
	if (parameters.size() == 2)
		until = format_time(parameters[2]);
	log("connect to Evohome server");
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);
	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		log(s_LOG+"using systemId from "+configfile);
		systemId = evoconfig["systemId"];
	}
	else
	{
		log("retrieve Evohome installation info");
		eclient.full_installation();

		// set Evohome heating system
		EvohomeClient::temperatureControlSystem* tcs = NULL;
		if (eclient.is_single_heating_system())
			tcs = &eclient.locations[0].gateways[0].temperatureControlSystems[0];
		else
			select_temperatureControlSystem(eclient);
		if (tcs == NULL)
			exit_error(s_ERROR+"multiple Evohome systems found - don't know which one to use");
		systemId = tcs->systemId;
	}
	if ( ! eclient.set_system_mode(systemId, mode, until) )
		exit_error(s_ERROR+"failed to set system mode to "+mode);
	log("updated system status");
	eclient.cleanup();
}


void cmd_set_dhw_state()
{
	if ( (parameters.size() < 2) || (parameters.size() > 3) )
		usage("badparm");
	if ( (parameters[2] != "on") && (parameters[2] != "off") && (parameters[2] != "auto") )
		usage("badparm");

	std::string s_until = "";
	if (parameters.size() == 3)
		s_until = format_time(parameters[3]);

	log("connect to Evohome server");
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);
	log("set domestic hot water state");
	eclient.set_dhw_mode(parameters[1], parameters[2], s_until);
	eclient.cleanup();
	log("Done!");
}


std::string getpath(std::string filename)
{
#ifdef _WIN32
	stringstream ss;
	unsigned int i;
	for (i = 0; i < filename.length(); i++)
	{
		if (filename[i] == '\\')
			ss << '/';
		else
			ss << filename[i];
	}
	filename = ss.str();
#endif
	std::size_t pos = filename.rfind('/');
	if (pos == std::string::npos)
		return "";
	return filename.substr(0, pos+1);
}


int main(int argc, char** argv)
{
	init_globals();
	parse_args(argc, argv);

	if (command == "update")
		check_lock_status(); // don't overload the server
	else
		touch_lockfile();

	if ( ( ! read_evoconfig() ) && (getpath(configfile) == "") )
	{
		// try to find evoconfig in the application path
		stringstream ss;
		ss << getpath(argv[0]) << configfile;
		configfile = ss.str();
		if ( ! read_evoconfig() )
			exit_error(s_ERROR+"can't read config file");
	}

	if ( ( ! dolog ) && (evoconfig.find("logfile") != evoconfig.end()) )
		startlog(evoconfig["logfile"]);

	if (command == "update")
	{
		cmd_update();
	}

	else if (command == "backup")
	{
		cmd_backup_and_restore_schedules();
	}

	else if (command == "set-mode")
	{
		cmd_set_system_mode();
	}

	else if (command == "set-temp")
	{
		cmd_set_temperature();
	}
	
	else if (command == "set-dhw")
	{
		cmd_set_dhw_state();
	}


	stoplog();
	return 0;
}

