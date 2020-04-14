/*
 * Copyright (c) 2016-2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
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
/************************************************************************
 *									*
 *	Main storage							*
 *									*
 ************************************************************************/

	std::vector<evohome::device::location> m_vLocations;


/************************************************************************
 *									*
 *	Debug information and errors 					*
 *									*
 ************************************************************************/

	std::string get_last_error();
	std::string get_last_response();


/************************************************************************
 *									*
 *	Evohome authentication						*
 *									*
 *	Logins to the newer (v2) Evohome API stay valid for one hour.	*
 *	After this time the session requires renewal for which you	*
 *	should always first attempt the renew_login() function. Only	*
 *	if that function returns false should you resend credentials.	*
 *									*
 *	The Evohome portal sets a limit on the number of sessions that	*
 *	any given user can have. Thus if you are building a single run	*
 *	application you should use the provided save and load functions	*
 *	for the authentication tokens.					*
 *									*
 *	If load_auth_from_file() finds an expired authentication token  *
 *	it will automatically try to renew it and save the new token	*
 *	back to the file on success.					*
 *									*
 ************************************************************************/

	bool login(const std::string &szUser, const std::string &szPassword);
	bool renew_login();
	bool save_auth_to_file(const std::string &szFilename);
	bool load_auth_from_file(const std::string &szFilename);


/************************************************************************
 *									*
 *	Evohome heating installations retrieval				*
 *									*
 *	full_installation() retrieves the basic information of your	*
 *	registered Evohome installation(s) and prepares the internal	*
 *	structs. Because this information is static, unless you are	*
 *	adding or removing hardware components, you will only need to	*
 *	make this call once at the start of your application.		*
 *									*
 ************************************************************************/

	bool full_installation();


/************************************************************************
 *									*
 *	Evohome system status retrieval					*
 *									*
 *	get_status() retrieves the current (or last known) values from	*
 *	an Evohome location. The location parameter may be specified	*
 *	as either an index (0 if you only have one installation) or	*
 *	the seven digit unique ID assigned to your system as a string.	*
 *									*
 ************************************************************************/

	bool get_status(const unsigned int locationIdx);
	bool get_status(std::string szLocationId);


/************************************************************************
 *									*
 *	Locate Evohome devices						*
 *									*
 *	these functions return pointers to hardware descriptive		*
 *	structs. Reference devices.hpp for their contents.		*
 *									*
 ************************************************************************/

	evohome::device::zone *get_zone_by_ID(std::string szZoneId);
	evohome::device::zone *get_zone_by_Name(std::string szZoneName);
	evohome::device::location *get_location_by_ID(std::string locationIdx);
	evohome::device::gateway *get_gateway_by_ID(std::string gatewayIdx);
	evohome::device::temperatureControlSystem *get_temperatureControlSystem_by_ID(std::string szSystemId);
	evohome::device::temperatureControlSystem *get_zone_temperatureControlSystem(evohome::device::zone *zone);


/************************************************************************
 *									*
 *	Simple tests							*
 *									*
 *	is_single_heating_system() returns true if your full		*
 *	installation contains just one temperature control system.	*
 *	When true, your locationIdx, gatewayIdx and systemIdx will all	*
 *	be 0								*
 *									*
 *	has_dhw returns true if the installation at the specified	*
 *	location contains a domestic hot water device.			*
 *									*
 ************************************************************************/

	bool is_single_heating_system();
	bool has_dhw(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx);
	bool has_dhw(evohome::device::temperatureControlSystem *tcs);
	bool has_dhw(const std::string szSystemId);


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/

	bool set_system_mode(const std::string szSystemId, const unsigned int mode, const std::string szDateUntil = "");
	bool set_system_mode(const std::string szSystemId, const std::string szMode, const std::string szDateUntil = "");

	bool set_temperature(const std::string szZoneId, const std::string temperature, const std::string szTimeUntil = "");
	bool cancel_temperature_override(const std::string szZoneId);

	bool set_dhw_mode(const std::string szDHWId, const std::string szMode, const std::string szTimeUntil = "");


/************************************************************************
 *									*
 *	Schedule handlers						*
 *									*
 ************************************************************************/

	bool schedules_backup(const std::string &szFilename);
	bool schedules_restore(const std::string &szFilename);
	bool load_schedules_from_file(const std::string &szFilename);

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

	std::string request_next_switchpoint(std::string szZoneId);


/************************************************************************
 *									*
 *	Class construct							*
 *									*
 ************************************************************************/

	EvohomeClient2();
	~EvohomeClient2();
	void cleanup();


private:
	void init();
	bool obtain_access_token(const std::string &szCredentials);
	bool get_user_id();

	void get_gateways(const unsigned int locationIdx);
	void get_temperatureControlSystems(const unsigned int locationIdx, const unsigned int gatewayIdx);
	void get_zones(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx);
	void get_dhw(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx);

	bool get_zone_schedule_ex(const std::string szZoneId, const unsigned int zoneType);
	bool set_zone_schedule_ex(const std::string szZoneId, const unsigned int zoneType, Json::Value *jSchedule);

	bool verify_object_path(const unsigned int locationIdx);
	bool verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx);
	bool verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx);
	bool verify_object_path(const unsigned int locationIdx, const unsigned int gatewayIdx, const unsigned int systemIdx, const unsigned int zoneIdx);

	int get_zone_path_ID(const std::string szZoneId);
	evohome::device::path::zone *get_zone_path(const std::string szZoneId);

private:
	Json::Value m_jFullInstallation;

	std::string m_szUserId;
	std::string m_szAccessToken;
	std::string m_szRefreshToken;
	time_t m_tTokenExpirationTime;
	std::vector<std::string> m_vEvoHeader;

	std::string m_szLastError;
	std::string m_szResponse;

	std::vector<evohome::device::path::zone> m_vZonePaths;

};

#endif
