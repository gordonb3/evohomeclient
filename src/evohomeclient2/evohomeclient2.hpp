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

	bool get_status(int location);
	bool get_status(std::string locationId);


/************************************************************************
 *									*
 *	Locate Evohome devices						*
 *									*
 *	these functions return pointers to hardware descriptive		*
 *	structs. Reference devices.hpp for their contents.		*
 *									*
 ************************************************************************/

	evohome::device::location *get_location_by_ID(std::string locationId);
	evohome::device::gateway *get_gateway_by_ID(std::string gatewayId);
	evohome::device::temperatureControlSystem *get_temperatureControlSystem_by_ID(std::string szSystemId);
	evohome::device::temperatureControlSystem *get_zone_temperatureControlSystem(evohome::device::zone *zone);
	evohome::device::zone *get_zone_by_ID(std::string szZoneId);


/************************************************************************
 *									*
 *	Simple tests							*
 *									*
 *	is_single_heating_system() returns true if your full		*
 *	installation contains just one temperature control system.	*
 *	When true, your locationId, gatewayId and systemId will all	*
 *	be 0								*
 *									*
 *	has_dhw returns true if the installation at the specified	*
 *	location contains a domestic hot water device.			*
 *									*
 ************************************************************************/

	bool is_single_heating_system();
	bool has_dhw(const int locationId, const int gatewayId, const int systemId);
	bool has_dhw(evohome::device::temperatureControlSystem *tcs);
	bool has_dhw(const std::string szSystemId);


/************************************************************************
 *									*
 *	Evohome overrides						*
 *									*
 ************************************************************************/

	bool set_system_mode(const std::string szSystemId, const int mode, const std::string szDateUntil = "");
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


/************************************************************************
 *									*
 *	Helper functions						*
 *									*
 ************************************************************************/

	bool verify_date(std::string date);
	bool verify_datetime(std::string datetime);
	std::string local_to_utc(std::string local_time);
	std::string utc_to_local(std::string utc_time);


private:
	Json::Value m_jFullInstallation;
	Json::Value m_jFullStatus;

	std::string m_szUserId;
	std::string m_szAccessToken;
	std::string m_szRefreshToken;
	time_t m_tTokenExpirationTime;
	std::vector<std::string> m_vEvoHeader;
	int m_tzoffset;
	int m_lastDST;

	std::string m_szLastError;
	std::string m_szResponse;

	void init();
	bool obtain_access_token(const std::string &szCredentials);
	bool get_user_id();

	void get_gateways(const int location);
	void get_temperatureControlSystems(const int location, const int gateway);
	void get_zones(const int location, const int gateway, const int temperatureControlSystem);
	void get_dhw(const int location, const int gateway, const int temperatureControlSystem);

	bool get_zone_schedule_ex(const std::string szZoneId, const int zoneType);
	bool set_zone_schedule_ex(const std::string szZoneId, const int zoneType, Json::Value *jSchedule);

};

#endif
