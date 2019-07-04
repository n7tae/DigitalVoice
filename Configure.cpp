/*
 *   Copyright (c) 2019 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <iostream>
#include <fstream>
#include <cstring>

#include "Configure.h"

extern bool GetCfgDirectory(std::string &dir);

void CConfigure::SetDefaultValues()
{
	data.sCallsign.clear();
	data.sName.clear();
	data.sStation.clear();
	data.sMessage.clear();
	data.sLocation.clear();
	data.sLinkAtStart.clear();
	data.sURL.assign("https://github.com/n7tae/DigitalVoice");
	data.bUseMyCall = false;
	data.bMaintainLink = false;
	data.bDPlusEnable  = false;
	data.iBaudRate = 460800;
	data.dLatitude = data.dLongitude = 0.0;
	data.cModule = 'A';
}

void CConfigure::ReadData()
{
	std::string path;
	if (GetCfgDirectory(path)) {
		SetDefaultValues();
		return;
	}

	path.append("qdv.cfg");

	std::ifstream cfg(path.c_str(), std::ifstream::in);
	if (! cfg.is_open()) {
		SetDefaultValues();
		return;
	}

	char line[128];
	while (cfg.getline(line, 128)) {
		char *key = strtok(line, "=");
		if (!key)
			continue;	// skip empty lines
		if (0==strlen(key) || '#'==*key)
			continue;	// skip comments
		char *val = strtok(NULL, "\t\r");
		if (!val)
			continue;
		if (*val == '\'')	// value is a quoted string
			val = strtok(val, "'");

		if (0 == strcmp(key, "MyCall")) {
			data.sCallsign.assign(val);
		} else if (0 == strcmp(key, "MyName")) {
			data.sName.assign(val);
		} else if (0 == strcmp(key, "StationCall")) {
			data.sStation.assign(val);
		} else if (0 == strcmp(key, "UseMyCall")) {
			data.bUseMyCall = IS_TRUE(*val);
		} else if (0 == strcmp(key, "Message")) {
			data.sMessage.assign(val);
		} else if (0 == strcmp(key, "DPlusEnable")) {
			data.bDPlusEnable = IS_TRUE(*val);
		} else if (0 == strcmp(key, "MaintainLink")) {
			data.bMaintainLink = IS_TRUE(*val);
		} else if (0 == strcmp(key, "BaudRate")) {
			data.iBaudRate = (0 == strcmp(val, "460800")) ? 460800 : 230400;
		} else if (0 == strcmp(key, "QuadNetType")) {
			if (0 == strcmp(val, "None"))
				data.eNetType = EQuadNetType::norouting;
			else if (0 == strcmp(val, "IPv6"))
				data.eNetType = EQuadNetType::ipv6only;
			else if (0 == strcmp(val, "Dual"))
				data.eNetType = EQuadNetType::dualstack;
			else
				data.eNetType = EQuadNetType::ipv4only;
		} else if (0 == strcmp(key, "Latitide")) {
			data.dLatitude = std::stod(val);
		} else if (0 == strcmp(key, "Longitude")) {
			data.dLongitude = std::stod(val);
		} else if (0 == strcmp(key, "Location")) {
			data.sLocation.assign(val);
		} else if (0 == strcmp(key, "URL")) {
			data.sURL.assign(val);
		} else if (0 == strcmp(key, "LinkAtStart")) {
			data.sLinkAtStart.assign(val);
		} else if (0 == strcmp(key, "Module")) {
			data.cModule = *val;
		}
	}
	cfg.close();
}

void CConfigure::WriteData()
{

	std::string path;
	if (GetCfgDirectory(path))
		return;
	path.append("qdv.cfg");

	// directory exists, now make the file
	std::ofstream file(path.c_str(), std::ofstream::out | std::ofstream::trunc);
	if (! file.is_open()) {
		std::cerr << "ERROR: could not open " << path << '!' << std::endl;
		return;
	}
	file << "#Generated Automatically, DO NOT MANUALLY EDIT!" << std::endl;
	file << "MyCall=" << data.sCallsign << std::endl;
	file << "MyName=" << data.sName << std::endl;
	file << "StationCall=" << data.sStation << std::endl;
	file << "Module=" << data.cModule << std::endl;
	file << "UseMyCall=" << (data.bUseMyCall ? "true" : "false") << std::endl;
	file << "Message='" << data.sMessage << "'" << std::endl;
	file << "QuadNetType=";
	if (data.eNetType == EQuadNetType::ipv6only)
		file << "IPv6";
	else if (data.eNetType == EQuadNetType::dualstack)
		file << "Dual";
	else if (data.eNetType == EQuadNetType::norouting)
		file << "None";
	else
		file << "IPv4";
	file << std::endl;
	file << "DPlusEnable=" << (data.bDPlusEnable ? "true" : "false") << std::endl;
	file << "LinkAtStart='" << data.sLinkAtStart << "'" << std::endl;
	file << "BaudRate=" << data.iBaudRate << std::endl;
	file << "Latitude=" << data.dLatitude << std::endl;
	file << "Longitude=" << data.dLongitude << std::endl;
	file << "Location='" << data.sLocation << "'" << std::endl;
	file << "MaintainLink=" << (data.bMaintainLink ? "true" : "false") << std::endl;

	file.close();
}

void CConfigure::CopyFrom(const CFGDATA &from)
{
	data.sCallsign.assign(from.sCallsign);
	data.sLocation.assign(from.sLocation);
	data.sMessage.assign(from.sMessage);
	data.sName.assign(from.sName);
	data.sStation.assign(from.sStation);
	data.cModule = from.cModule;
	data.sURL.assign(from.sURL);
	data.bDPlusEnable = from.bDPlusEnable;
	data.sLinkAtStart.assign(from.sLinkAtStart);
	data.bMaintainLink = from.bMaintainLink;
	data.bUseMyCall = from.bUseMyCall;
	data.iBaudRate = from.iBaudRate;
	data.eNetType = from.eNetType;
	data.dLatitude = from.dLatitude;
	data.dLongitude = from.dLongitude;
}

void CConfigure::CopyTo(CFGDATA &to)
{
	to.sCallsign.assign(data.sCallsign);
	to.sLocation.assign(data.sLocation);
	to.sMessage.assign(data.sMessage);
	to.sName.assign(data.sName);
	to.sStation.assign(data.sStation);
	to.cModule = data.cModule;
	to.sURL.assign(data.sURL);
	to.bDPlusEnable = data.bDPlusEnable;
	to.sLinkAtStart.assign(data.sLinkAtStart);
	to.bMaintainLink = data.bMaintainLink;
	to.bUseMyCall = data.bUseMyCall;
	to.iBaudRate = data.iBaudRate;
	to.eNetType = data.eNetType;
	to.dLatitude = data.dLatitude;
	to.dLongitude = data.dLongitude;
}
