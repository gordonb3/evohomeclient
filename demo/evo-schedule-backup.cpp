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
#include <cstdlib>
#include <time.h>
#include "evohomeclient2/evohomeclient.h"


#ifndef CONF_FILE
#define CONF_FILE "evoconfig"
#endif

#ifndef BACKUP_FILE
#define BACKUP_FILE "schedules.backup"
#endif



using namespace std;

std::string backupfile;
std::string configfile;
std::map<std::string, std::string> evoconfig;

bool dobackup = true;
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
		cout << "Usage: evo-schedule-backup [-hrv] [-c file] [-f file]" << endl;
		cout << "Type \"evo-schedule-backup --help\" for more help" << endl;
		exit(0);
	}
	cout << "Usage: evo-schedule-backup [OPTIONS]" << endl;
	cout << endl;
	cout << "  -r, --restore           restore a previous backup" << endl;
	cout << "  -v, --verbose           print a lot of information" << endl;
	cout << "  -c, --conf=FILE         use FILE for server settings and credentials" << endl;
	cout << "  -f, --file=FILE         use FILE for backup and restore" << endl;
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
				} else if (word[j] == 'r') {
					dobackup = false;
				} else if (word[j] == 'v') {
					verbose = true;
				} else if (word[j] == 'c') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					configfile = argv[i];
				} else if (word[j] == 'f') {
					if (j+1 < word.length())
						usage("badparm");
					i++;
					backupfile = argv[i];
				} else {
					usage("badparm");
					exit(1);
				}
			}
		} else if (word == "--help") {
			usage("long");
			exit(0);
		} else if (word == "--restore") {
			dobackup = false;
		} else if (word == "--verbose") {
			verbose = true;
		} else if (word.substr(0,7) == "--conf=") {
			configfile = word.substr(7);
		} else if (word.substr(0,7) == "--file=") {
			backupfile = word.substr(7);
		} else {
			usage("badparm");
			exit(1);
		}
		i++;
	}
}


int main(int argc, char** argv)
{

	backupfile = BACKUP_FILE;
	configfile = CONF_FILE;
	parse_args(argc, argv);

// override defaults with settings from config file
	read_evoconfig();

// connect to Evohome server
	if (verbose)
		cout << "connect to Evohome server\n";
	EvohomeClient2 eclient = EvohomeClient2(evoconfig["usr"],evoconfig["pw"]);

// retrieve Evohome installation
	if (verbose)
		cout << "retrieve Evohome installation info\n";
	eclient.full_installation();

	if (dobackup)	// backup
	{
		cout << "Start backup of Evohome schedules\n";
		if ( ! eclient.schedules_backup(backupfile) )
			exit_error(ERROR+"failed to open backup file '"+backupfile+"'");
		cout << "Done!\n";
	}
	else		// restore
	{
		cout << "Start restore of Evohome schedules\n";
		if ( ! eclient.schedules_restore(backupfile) )
			exit_error(ERROR+"failed to open backup file '"+backupfile+"'");
		cout << "Done!\n";
	}

	eclient.cleanup();

	return 0;
}

