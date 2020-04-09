/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Type definitions for devices in Evohome API
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#pragma once
#include <vector>
#include <string>
#include "jsoncpp/json.h"


namespace evohome {
  namespace device {

	typedef struct _sZone // also used for Domestic Hot Water
	{
		std::string szZoneId;
		std::string szSystemId;
		std::string szGatewayId;
		std::string szLocationId;
		Json::Value *jInstallationInfo;
		Json::Value *jStatus;
		Json::Value schedule;
	} zone;

	typedef struct _sTemperatureControlSystem
	{
		std::string szSystemId;
		std::string szGatewayId;
		std::string szLocationId;
		Json::Value *jInstallationInfo;
		Json::Value *jStatus;
		std::vector<evohome::device::zone> zones;
		std::vector<evohome::device::zone> dhw;
	} temperatureControlSystem;

	typedef struct _sGateway
	{
		std::string szGatewayId;
		std::string szLocationId;
		Json::Value *jInstallationInfo;
		Json::Value *jStatus;
		std::vector<evohome::device::temperatureControlSystem> temperatureControlSystems;
	} gateway;

	typedef struct _sLocation
	{
		std::string szLocationId;
		Json::Value *jInstallationInfo;
		Json::Value *jStatus;
		std::vector<evohome::device::gateway> gateways;
	} location;

  }; // namespace device

}; // namespace evohome


