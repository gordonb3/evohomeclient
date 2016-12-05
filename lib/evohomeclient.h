/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for Evohome
 *
 *
 *
 */

#ifndef _EvohomeClient
#define _EvohomeClient

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <curl/curl.h>
#include <json-c/json.h>
#include <map>


struct evo_zone
{
	public:
	std::string locationId;
	std::string gatewayId;
	std::string systemId;
	std::string zoneId;
	json_object *installationInfo;
	json_object *status;
	json_object *schedule;
};

struct evo_temperatureControlSystem
{
	public:
	std::string locationId;
	std::string gatewayId;
	std::string systemId;
	json_object *installationInfo;
	json_object *status;
	std::map<int,evo_zone> zones;
};

struct evo_gateway
{
	public:
	std::string locationId;
	std::string gatewayId;
	json_object *installationInfo;
	json_object *status;
	std::map<int,evo_temperatureControlSystem> temperatureControlSystems;
};


struct evo_location
{
	public:
	std::string locationId;
	json_object *installationInfo;
	json_object *status;
	std::map<int,evo_gateway> gateways;
};


class EvohomeClient
{
	private:
	std::map<std::string,std::string> auth_info;
	std::map<std::string,std::string> account_info;
	struct curl_slist *evoheader;

	std::string send_receive_data(std::string url, curl_slist *header);
	std::string send_receive_data(std::string url, std::string postdata, curl_slist *header);
	std::string put_receive_data(std::string url, std::string postdata, curl_slist *header);

	void init();
	void login(std::string user, std::string password);
	void user_account();

	void get_gateways(int location);
	void get_temperatureControlSystems(int location, int gateway);
	void get_zones(int location, int gateway, int temperatureControlSystem);


	public:
	std::map<int,evo_location> locations;

	EvohomeClient(std::string user, std::string password);
	void cleanup();

	bool full_installation();
	bool get_status(int location);
	bool get_status(std::string locationId);


	evo_location* get_location_by_ID(std::string locationId);
	evo_gateway* get_gateway_by_ID(std::string gatewayId);
	evo_temperatureControlSystem* get_temperatureControlSystem_by_ID(std::string systemId);
	evo_zone* get_zone_by_ID(std::string zoneId);
	evo_temperatureControlSystem* get_zone_temperatureControlSystem(evo_zone* zone);

	bool has_dhw(int location, int gateway, int temperatureControlSystem);
	bool has_dhw(evo_temperatureControlSystem *tcs);
	bool is_single_heating_system();

	bool schedules_backup(std::string filename);
	bool schedules_restore(std::string filename);
	bool read_schedules_from_file(std::string filename);
	bool get_schedule(std::string zoneId);
	bool set_schedule(std::string zoneId, std::string zoneType, json_object *schedule);
	bool set_zone_schedule(std::string zoneId, json_object *schedule);
	bool set_dhw_schedule(std::string zoneId, json_object *schedule);

	std::string get_next_switchpoint(evo_temperatureControlSystem* tcs, int zone);
	std::string get_next_switchpoint(std::string zoneId);
	std::string get_next_switchpoint(json_object *schedule);


	bool set_system_mode(std::string systemId, int mode, std::string date_until);
	bool set_system_mode(std::string systemId, std::string mode, std::string date_until);
	bool set_system_mode(std::string systemId, std::string mode);


	std::string json_get_val(std::string s_json, std::string key);
	std::string json_get_val(json_object *j_json, std::string key);
	std::string json_get_val(std::string s_json, std::string key1, std::string key2);
	std::string json_get_val(json_object *j_json, std::string key1, std::string key2);


	bool set_temperature(std::string zoneId, std::string temperature, std::string time_until);
	bool set_temperature(std::string zoneId, std::string temperature);
	bool cancel_temperature_override(std::string zoneId);

	bool set_dhw_mode(std::string dhwId, std::string mode, std::string time_until);
	bool set_dhw_mode(std::string systemId, std::string mode);

};

#endif
