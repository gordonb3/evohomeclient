/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for UK/EMEA Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeClient2
#define _EvohomeClient2

#include <vector>
#include <string>
#include "jsoncpp/json.h"
#include "evohome/devices.hpp"


class EvohomeClient2
{
public:
	EvohomeClient2();
	EvohomeClient2(const std::string &szUser, const std::string &szPassword);
	~EvohomeClient2();

	void cleanup();

	bool login(const std::string &szUser, const std::string &szPassword);
	bool renew_login();
	bool save_auth_to_file(const std::string &szFilename);
	bool load_auth_from_file(const std::string &szFilename);
	bool user_account();

	bool full_installation();
	bool get_status(int location);

	bool get_status_by_ID(std::string locationId);

	evohome::device::location *get_location_by_ID(std::string locationId);
	evohome::device::gateway *get_gateway_by_ID(std::string gatewayId);
	evohome::device::temperatureControlSystem *get_temperatureControlSystem_by_ID(std::string systemId);
	evohome::device::temperatureControlSystem *get_zone_temperatureControlSystem(evohome::device::zone *zone);
	evohome::device::zone *get_zone_by_ID(std::string szZoneId);

	bool has_dhw(int location, int gateway, int temperatureControlSystem);
	bool has_dhw(evohome::device::temperatureControlSystem *tcs);
	bool is_single_heating_system();

	bool schedules_backup(const std::string &szFilename);
	bool schedules_restore(const std::string &szFilename);
	bool read_schedules_from_file(const std::string &szFilename);

	bool get_dhw_schedule(const std::string szDHWId);
	bool get_zone_schedule(const std::string szZoneId);

	bool set_dhw_schedule(const std::string szDHWId, Json::Value *jSchedule);
	bool set_zone_schedule(const std::string szZoneId, Json::Value *jSchedule);

	std::string get_next_switchpoint(evohome::device::temperatureControlSystem *tcs, int zone);
	std::string get_next_switchpoint(evohome::device::zone *hz);
	std::string get_next_switchpoint(std::string szZoneId);
	std::string get_next_switchpoint(Json::Value &jSchedule);
	std::string get_next_switchpoint(Json::Value &jSchedule, std::string &szCurrentSetpoint, int force_weekday = -1, bool convert_to_utc = false);

	std::string get_next_utcswitchpoint(evohome::device::temperatureControlSystem *tcs, int zone);
	std::string get_next_utcswitchpoint(evohome::device::zone *hz);
	std::string get_next_utcswitchpoint(std::string szZoneId);
	std::string get_next_utcswitchpoint(Json::Value &jSchedule);
	std::string get_next_utcswitchpoint(Json::Value &jSchedule, std::string &szCurrentSetpoint, int force_weekday = -1);

	bool set_system_mode(const std::string systemId, const int mode, const std::string date_until = "");
	bool set_system_mode(const std::string systemId, const std::string mode, const std::string date_until = "");

	bool set_temperature(std::string szZoneId, std::string temperature, std::string time_until);
	bool set_temperature(std::string szZoneId, std::string temperature);
	bool cancel_temperature_override(std::string szZoneId);

	bool set_dhw_mode(std::string dhwId, std::string mode, std::string time_until);
	bool set_dhw_mode(std::string dhwId, std::string mode);

	std::string request_next_switchpoint(std::string szZoneId);

	bool verify_date(std::string date);
	bool verify_datetime(std::string datetime);
	std::string local_to_utc(std::string local_time);
	std::string utc_to_local(std::string utc_time);

	std::vector<evohome::device::location> m_vLocations;

	std::string get_last_error();

private:
	Json::Value m_jFullInstallation;
	Json::Value m_jFullStatus;

	std::string m_szUserId;
	std::string m_szAccessToken;
	std::string m_szRefreshToken;
	time_t m_tTokenExpirationTime;
	std::vector<std::string> m_vEvoHeader;
	int m_tzoffset;
	int lastDST;

	std::string m_szLastError;

	void init();
	bool obtain_access_token(const std::string &szCredentials);

	void get_gateways(const int location);
	void get_temperatureControlSystems(const int location, const int gateway);
	void get_zones(const int location, const int gateway, const int temperatureControlSystem);
	void get_dhw(const int location, const int gateway, const int temperatureControlSystem);

	bool get_zone_schedule_ex(const std::string szZoneId, const std::string szZoneType);
	bool set_zone_schedule_ex(const std::string szZoneId, const std::string zoneType, Json::Value *jSchedule);

};

#endif
