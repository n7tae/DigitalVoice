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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include <iostream>
#include <fstream>
#include <cstring>

#include "Configure.h"

CConfigure cfg;

bool GetCfgDirectory(std::string &dir)
{
	const char *homedir = getenv("HOME");

	if (NULL == homedir)
		homedir = getpwuid(getuid())->pw_dir;

	if (NULL == homedir) {
		std::cerr << "ERROR: could not find a home directory!" << std::endl;
		return true;
	}
	dir.assign(homedir);
	dir.append("/.config/qndv/");
	return false;
}

void CConfigure::SetDefaultValues()
{
	data.sCallsign.clear();
	data.sName.clear();
	data.sStation.clear();
	data.sMessage.clear();
	data.sLocation[0].clear();
	data.sLocation[1].clear();
	data.sLinkAtStart.clear();
	data.sURL.assign("https://github.com/n7tae/DigitalVoice");
	data.bUseMyCall = false;
	data.bMaintainLink = false;
	data.bDPlusEnable  = false;
	data.iBaudRate = 460800;
	data.fLatitude = data.fLongitude = 0.0;
}

void CConfigure::ReadData()
{
	SetDefaultValues();
	std::string path;
	if (GetCfgDirectory(path))
		return;

	path.append("qndv.cfg");

	std::ifstream cfg(path.c_str(), std::ifstream::in);
	if (! cfg.is_open()) {	// this should not happen!
		std::cerr << "Error opening " << path << '!' << std::endl;
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
		if (*val && '\'')	// value is a quoted string
			val = strtok(val, "'");
		//if (0==strlen(val)) continue;	// skip lines with no values

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
		} else if (0 == strcmp(key, "ReportStation")) {
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
			data.fLatitude = std::stod(val);
		} else if (0 == strcmp(key, "Longitude")) {
			data.fLongitude = std::stod(val);
		} else if (0 == strcmp(key, "Location1")) {
			data.sLocation[0].assign(val);
		} else if (0 == strcmp(key, "Location2")) {
			data.sLocation[1].assign(val);
		} else if (0 == strcmp(key, "URL")) {
			data.sURL.assign(val);
		} else if (0 == strcmp(key, "LinkAtStart")) {
			data.sLinkAtStart.assign(val);
		}
	}
	cfg.close();
}

void CConfigure::WriteData()
{

	std::string path;
	if (GetCfgDirectory(path))
		return;
	path.append("/qndv.cfg");

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
	file << "Latitude=" << data.fLatitude << std::endl;
	file << "Longitude=" << data.fLongitude << std::endl;
	file << "Location1='" << data.sLocation[0] << "'" << std::endl;
	file << "Location2='" << data.sLocation[1] << "'" << std::endl;
	file << "ReportStation=" << (data.bMaintainLink ? "true" : "false") << std::endl;

	file.close();
}

void CConfigure::CopyFrom(const CFGDATA &from)
{
	data.sCallsign.assign(from.sCallsign);
	data.sLocation[0].assign(from.sLocation[0]);
	data.sLocation[1].assign(from.sLocation[1]);
	data.sMessage.assign(from.sMessage);
	data.sName.assign(from.sName);
	data.sStation.assign(from.sStation);
	data.sURL.assign(from.sURL);
	data.bDPlusEnable = from.bDPlusEnable;
	data.bMaintainLink = from.bMaintainLink;
	data.bUseMyCall = from.bUseMyCall;
	data.iBaudRate = from.iBaudRate;
	data.eNetType = from.eNetType;
	data.fLatitude = from.fLatitude;
	data.fLongitude = from.fLongitude;
}

void CConfigure::CopyTo(CFGDATA &to)
{
	to.sCallsign.assign(data.sCallsign);
	to.sLocation[0].assign(data.sLocation[0]);
	to.sLocation[1].assign(data.sLocation[1]);
	to.sMessage.assign(data.sMessage);
	to.sName.assign(data.sName);
	to.sStation.assign(data.sStation);
	to.sURL.assign(data.sURL);
	to.bDPlusEnable = data.bDPlusEnable;
	to.bMaintainLink = data.bMaintainLink;
	to.bUseMyCall = data.bUseMyCall;
	to.iBaudRate = data.iBaudRate;
	to.eNetType = data.eNetType;
	to.fLatitude = data.fLatitude;
	to.fLongitude = data.fLongitude;
}
