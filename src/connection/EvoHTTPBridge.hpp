/*
 * Copyright (c) 2020 Gordon Bos <gordon@bosvangennip.nl> All rights reserved.
 *
 * Evohome HTTP bridge class
 *
 *
 * Source code subject to GNU GENERAL PUBLIC LICENSE version 3
 */

#include "HTTPClient.hpp"


class EvoHTTPBridge : public HTTPClient_Base
{
public:
	static bool SafeGET(const std::string &szUrl, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, const long iTimeOut = -1);
	static bool SafePOST(const std::string &szUrl, const std::string &szPostdata, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, const long iTimeOut = -1);
	static bool SafePUT(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, const long iTimeOut = -1);
	static bool SafeDELETE(const std::string &szUrl, const std::string &szPutdata, const std::vector<std::string> &ExtraHeaders, std::string &szResponse, const long iTimeOut = -1);

	static std::string URLEncode(std::string szDecodedString);
	static bool ProcessResponse(std::string &szResponse, const std::vector<std::string> &vHeaderData, const bool bhttpOK);
};


