/*
 * Copyright (c) 2016 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Json client for 'Old' US Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#ifndef _EvohomeV1Client
#define _EvohomeV1Client

#include <vector>
#include <string>
#include "jsoncpp/json.h"

#include "evohome/devices.hpp"


class EvohomeClient
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
 *	Logins to the Evohome API stay valid indefinitely as long as	*
 *	you make your next call within 15 minutes after the previous	*
 *	one.								*
 *									*
 *	The Evohome portal sets a limit on the number of sessions that	*
 *	any given user can have. Thus if you are building a single run	*
 *	application you should use the provided save and load functions	*
 *	for the authentication tokens.					*
 *									*
 *	If load_auth_from_file() finds an expired authentication token  *
 *	it will return false and you need to login again.		*
 *									*
 *	UPDATE December 2019						*
 *	Automatic session renewal no longer works and the manual	*
 *	session renewal method that was added to the library to counter	*
 *	that issue also proved not to allow the session to live past 	*
 *	15 minutes from submitting credentials.	The added method has	*
 *	been renamed to is_session_valid() and I suggest you call it	*
 *	before attempting to retrieve any other data.			*
 *									*
 ************************************************************************/

	bool login(const std::string &user, const std::string &password);
	bool save_auth_to_file(const std::string &szFilename);
	bool load_auth_from_file(const std::string &szFilename);

	bool is_session_valid();

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
 *	Evohome overrides						*
 *									*
 ************************************************************************/

	bool set_system_mode(const std::string szLocationId, const int mode, const std::string szDateUntil = "");
	bool set_system_mode(const std::string szLocationId, const std::string szMode, const std::string szDateUntil = "");

	bool set_temperature(const std::string szZoneId, const std::string temperature, const std::string szTimeUntil = "");
	bool cancel_temperature_override(const std::string szZoneId);

	bool set_dhw_mode(const std::string szDHWId, const std::string szMode,const  std::string szTimeUntil = "");
	bool cancel_dhw_override(const std::string szDHWId);




	std::string get_zone_temperature(const std::string szLocationId, const std::string szZoneId, const int numDecimals = 1);
	std::string get_zone_temperature(const int locationIdx, const int zoneIdx, const int numDecimals = 1);
	std::string get_zone_temperature(const int locationIdx, const int gatewayIdx, const int zoneIdx, const int numDecimals = 1);


/************************************************************************
 *									*
 *	Simple tests							*
 *									*
 *	is_single_heating_system() returns true if your full		*
 *	installation contains just one temperature control system.	*
 *	When true, your locationIdx and gatewayIdx will both be 0	*
 *									*
 *	has_dhw returns true if the installation at the specified	*
 *	location contains a domestic hot water device.			*
 *									*
 ************************************************************************/

	bool is_single_heating_system();
	bool has_dhw(const int locationIdx, const int gatewayIdx = 0);


/************************************************************************
 *									*
 *	Object locators							*
 *									*
 *									*
 *									*
 ************************************************************************/

	int get_location_index(const std::string szLocationId);
	int get_zone_index(const int locationIdx, const std::string szZoneId);
	int get_zone_index(const int locationIdx, const int gatewayIdx, const std::string szZoneId);



/************************************************************************
 *									*
 *	Class construct							*
 *									*
 ************************************************************************/

	EvohomeClient();
	~EvohomeClient();
	void cleanup();



private:
	void init();

	void get_gateways(const int locationIdx);
	void get_temperatureControlSystems(const int locationIdx, const int gatewayIdx);
	void get_devices(const int locationIdx, const int gatewayIdx);


private:
	Json::Value m_jFullInstallation;
	std::string m_szUserId;
	std::string m_szSessionId;
	time_t m_tLastWebCall;
	std::vector<std::string> m_vEvoHeader;
	std::string m_szLastError;
	std::string m_szResponse;
};

#endif
