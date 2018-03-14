/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Demo app for connecting to Evohome and Domoticz
 *
 *
 *
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include "../evohomeclient/evohomeclient.h"


#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif


using namespace std;


std::string mode = "";
std::string configfile;
std::map<std::string, std::string> evoconfig;

bool verbose;

std::string ERROR = "ERROR: ";
std::string WARN = "WARNING: ";


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
	cout << "Usage: evo-setmode [OPTIONS] <evohome mode>" << endl;
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
				} else if (word[j] == 'v') {
					verbose = true;
				} else if (word[j] == 'c') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					configfile = argv[i];
				} else {
					usage("badparm");
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--verbose") {
			verbose = true;
		} else if (word.substr(0,7) == "--conf=") {
			configfile = word.substr(7);
		} else if (mode == "") {
			mode = argv[i];
		} else {
			usage("badparm");
		}
		i++;
	}
	if (mode == "")
		usage("short");
}


EvohomeClient::temperatureControlSystem* select_temperatureControlSystem(EvohomeClient &eclient)
{
	int location = 0;
	int gateway = 0;
	int temperatureControlSystem = 0;
	bool is_unique_heating_system = false;
	if ( evoconfig.find("location") != evoconfig.end() ) {
		if (verbose)
			cout << "using location from " << configfile << endl;
		int l = eclient.locations.size();
		location = atoi(evoconfig["location"].c_str());
		if (location > l)
			exit_error(ERROR+"the Evohome location specified in "+configfile+" cannot be found");
		is_unique_heating_system = ( (eclient.locations[location].gateways.size() == 1) &&
						(eclient.locations[location].gateways[0].temperatureControlSystems.size() == 1)
						);
	}
	if ( evoconfig.find("gateway") != evoconfig.end() ) {
		if (verbose)
			cout << "using gateway from " << configfile << endl;
		int l = eclient.locations[location].gateways.size();
		gateway = atoi(evoconfig["gateway"].c_str());
		if (gateway > l)
			exit_error(ERROR+"the Evohome gateway specified in "+configfile+" cannot be found");
		is_unique_heating_system = (eclient.locations[location].gateways[gateway].temperatureControlSystems.size() == 1);
	}
	if ( evoconfig.find("controlsystem") != evoconfig.end() ) {
		if (verbose)
			cout << "using controlsystem from " << configfile << endl;
		int l = eclient.locations[location].gateways[gateway].temperatureControlSystems.size();
		temperatureControlSystem = atoi(evoconfig["controlsystem"].c_str());
		if (temperatureControlSystem > l)
			exit_error(ERROR+"the Evohome temperature controlsystem specified in "+configfile+" cannot be found");
		is_unique_heating_system = true;
	}


	if ( ! is_unique_heating_system)
		return NULL;

	return &eclient.locations[location].gateways[gateway].temperatureControlSystems[temperatureControlSystem];
}


int main(int argc, char** argv)
{
	configfile = CONF_FILE;
	parse_args(argc, argv);
	read_evoconfig();

	if (verbose)
		cout << "connect to Evohome server\n";
	EvohomeClient eclient = EvohomeClient(evoconfig["usr"],evoconfig["pw"]);


	std::string systemId;
	if ( evoconfig.find("systemId") != evoconfig.end() ) {
		if (verbose)
			cout << "using systemId from " << CONF_FILE << endl;
		systemId = evoconfig["systemId"];
	}
	else
	{
		if (verbose)
			cout << "retrieve Evohome installation info\n";
		eclient.full_installation();

		// set Evohome heating system
		EvohomeClient::temperatureControlSystem* tcs = NULL;

		if (eclient.is_single_heating_system())
			tcs = &eclient.locations[0].gateways[0].temperatureControlSystems[0];
		else
			select_temperatureControlSystem(eclient);

		if (tcs == NULL)
			exit_error(ERROR+"multiple Evohome systems found - don't know which one to use");

		systemId = tcs->systemId;
	}


	if ( ! eclient.set_system_mode(systemId,mode) )
		exit_error(ERROR+"failed to set system mode to "+mode);
	
	if (verbose)
		cout << "updated system status to " << mode << endl;

	eclient.cleanup();

	return 0;
}

