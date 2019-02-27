/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeClient
#define _EvohomeClient

#include <vector>
#include <string>
#include "jsoncpp/json.h"

class EvohomeClient
{
public:
	struct zone
	{
		Json::Value *installationInfo;
		Json::Value *status;
		std::string locationId;
		std::string gatewayId;
		std::string systemId;
		std::string zoneId;
		Json::Value schedule;
	};

	struct temperatureControlSystem
	{
		std::vector<zone> zones;
		std::vector<zone> dhw;
		Json::Value *installationInfo;
		Json::Value *status;
		std::string locationId;
		std::string gatewayId;
		std::string systemId;
	};

	struct gateway
	{
		std::vector<temperatureControlSystem> temperatureControlSystems;
		Json::Value *installationInfo;
		Json::Value *status;
		std::string locationId;
		std::string gatewayId;
	};


	struct location
	{
		std::vector<gateway> gateways;
		Json::Value *installationInfo;
		Json::Value *status;
		std::string locationId;
	};

	EvohomeClient();
	EvohomeClient(std::string user, std::string password);
	~EvohomeClient();
	void cleanup();

	bool login(std::string user, std::string password);
	bool renew_login();
	bool save_auth_to_file(std::string filename);
	bool load_auth_from_file(std::string filename);


	bool full_installation();
	bool get_status(int location);
	bool get_status(std::string locationId);

	location* get_location_by_ID(std::string locationId);
	gateway* get_gateway_by_ID(std::string gatewayId);
	temperatureControlSystem* get_temperatureControlSystem_by_ID(std::string systemId);
	zone* get_zone_by_ID(std::string zoneId);
	temperatureControlSystem* get_zone_temperatureControlSystem(zone* zone);

	bool has_dhw(int location, int gateway, int temperatureControlSystem);
	bool has_dhw(temperatureControlSystem *tcs);
	bool is_single_heating_system();

	bool schedules_backup(std::string filename);
	bool schedules_restore(std::string filename);
	bool read_schedules_from_file(std::string filename);
	bool get_dhw_schedule(std::string dhwId);
	bool get_zone_schedule(std::string zoneId);
	bool get_zone_schedule(std::string zoneId, std::string zoneType);
	bool set_dhw_schedule(std::string dhwId, Json::Value *schedule);
	bool set_zone_schedule(std::string zoneId, Json::Value *schedule);
	bool set_zone_schedule(std::string zoneId, std::string zoneType, Json::Value *schedule);

	std::string get_next_switchpoint(temperatureControlSystem* tcs, int zone);
	std::string get_next_switchpoint(zone* hz);
	std::string get_next_switchpoint(std::string zoneId);
	std::string get_next_switchpoint(Json::Value &schedule);
	std::string get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint);
	std::string get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday);

	std::string get_next_switchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday, bool convert_to_utc);
	std::string get_next_utcswitchpoint(EvohomeClient::temperatureControlSystem* tcs, int zone);
	std::string get_next_utcswitchpoint(zone* hz);
	std::string get_next_utcswitchpoint(std::string zoneId);
	std::string get_next_utcswitchpoint(Json::Value &schedule);
	std::string get_next_utcswitchpoint_ex(Json::Value &schedule, std::string &current_setpoint);
	std::string get_next_utcswitchpoint_ex(Json::Value &schedule, std::string &current_setpoint, int force_weekday);

	bool set_system_mode(std::string systemId, int mode, std::string date_until);
	bool set_system_mode(std::string systemId, int mode);
	bool set_system_mode(std::string systemId, std::string mode, std::string date_until);
	bool set_system_mode(std::string systemId, std::string mode);

	bool set_temperature(std::string zoneId, std::string temperature, std::string time_until);
	bool set_temperature(std::string zoneId, std::string temperature);
	bool cancel_temperature_override(std::string zoneId);

	bool set_dhw_mode(std::string dhwId, std::string mode, std::string time_until);
	bool set_dhw_mode(std::string dhwId, std::string mode);

	std::string request_next_switchpoint(std::string zoneId);

	bool verify_date(std::string date);
	bool verify_datetime(std::string datetime);
	std::string local_to_utc(std::string local_time);
	std::string utc_to_local(std::string utc_time);

	std::vector<location> locations;
	Json::Value j_fi;
	Json::Value j_stat;

private:
	std::string v2uid;
	std::string v2access_token;
	std::string v2refresh_token;
	time_t v2token_expiration_time;
	std::vector<std::string> evoheader;
	int tzoffset;
	int lastDST;

	std::string send_receive_data(std::string url, std::vector<std::string> &header);
	std::string send_receive_data(std::string url, std::string postdata, std::vector<std::string> &header);
	std::string put_receive_data(std::string url, std::string putdata, std::vector<std::string> &header);
	std::string send_receive_data(std::string url, std::string postdata, std::vector<std::string> &header, std::string method);

	void init();
	bool user_account();

	void get_gateways(int location);
	void get_temperatureControlSystems(int location, int gateway);
	void get_zones(int location, int gateway, int temperatureControlSystem);
	void get_dhw(int location, int gateway, int temperatureControlSystem);
};

#endif
