/*
 *   Copyright (c) 2019-2020 by Thomas A. Early N7TAE
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
#include <iomanip>
#include <fstream>
#include <cstring>

#include "Configure.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

void CConfigure::SetDefaultValues()
{
	data.sCallsign.clear();
	data.sName.clear();
	data.sStation.clear();
	data.sMessage.clear();
	data.sLocation[0].clear();
	data.sLocation[2].clear();
	data.sLinkAtStart.clear();
	data.sURL.assign("https://github.com/n7tae/DigitalVoice");
	data.bUseMyCall = false;
	data.bDPlusEnable  = false;
	data.eMode = EMode::routing;
	data.iBaudRate = 460800;
	data.dLatitude = data.dLongitude = 0.0;
	data.cModule = 'A';
	data.sAudioIn.assign("default");
	data.sAudioOut.assign("default");
	data.bAPRSEnable = true;
	data.sAPRSServer.assign("rotate.aprs2.net");
	data.usAPRSPort = 14580U;
	data.iAPRSInterval = 40;
	data.bGPSDEnable = false;
	data.sGPSDServer.assign("localhost");
	data.usGPSDPort = 2947U;
}

void CConfigure::ReadData()
{
	std::string path(CFG_DIR);
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
		if (! val)
			continue;

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
		} else if (0 == strcmp(key, "Mode")) {
			if (0 == strcmp(val, "Route"))
				data.eMode = EMode::routing;
			else
				data.eMode = EMode::linking;
		} else if (0 == strcmp(key, "Latitude")) {
			data.dLatitude = std::stod(val);
		} else if (0 == strcmp(key, "Longitude")) {
			data.dLongitude = std::stod(val);
		} else if (0 == strcmp(key, "Location1")) {
			data.sLocation[0].assign(val);
		} else if (0 == strcmp(key, "Location2")) {
			data.sLocation[1].assign(val);
		} else if (0 == strcmp(key, "URL")) {
			data.sURL.assign(val);
		} else if (0 == strcmp(key, "LinkAtStart")) {
			data.sLinkAtStart.assign(val);
		} else if (0 == strcmp(key, "Module")) {
			data.cModule = *val;
		} else if (0 == strcmp(key, "AudioInput")) {
			data.sAudioIn.assign(val);
		} else if (0 == strcmp(key, "AudioOutput")) {
			data.sAudioOut.assign(val);
		} else if (0 == strcmp(key, "APRSEnable")) {
			data.bAPRSEnable = IS_TRUE(*val);
		} else if (0 == strcmp(key, "APRSServer")) {
			data.sAPRSServer.assign(val);
		} else if (0 == strcmp(key, "APRSPort")) {
			data.usAPRSPort = std::stoul(val);
		} else if (0 == strcmp(key, "APRSInterval")) {
			data.iAPRSInterval = std::stoi(val);
		} else if (0 == strcmp(key, "GPSDEnable")) {
			data.bGPSDEnable = IS_TRUE(*val);
		} else if (0 == strcmp(key, "GPSDServer")) {
			data.sGPSDServer.assign(val);
		} else if (0 == strcmp(key, "GPSDPort")) {
			data.usGPSDPort = std::stoul(val);
		}
	}
	cfg.close();
}

void CConfigure::WriteData()
{

	std::string path(CFG_DIR);
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
	file << "Mode=" << ((data.eMode == EMode::routing) ? "Route" : "Link") << std::endl;
	file << "DPlusEnable=" << (data.bDPlusEnable ? "true" : "false") << std::endl;
	file << "LinkAtStart='" << data.sLinkAtStart << "'" << std::endl;
	file << "BaudRate=" << data.iBaudRate << std::endl;
	file << "Latitude=" << std::setprecision(9) << data.dLatitude << std::endl;
	file << "Longitude=" << std::setprecision(9) << data.dLongitude << std::endl;
	file << "Location1='" << data.sLocation[0] << "'" << std::endl;
	file << "Location2='" << data.sLocation[1] << "'" << std::endl;
	file << "AudioInput='" << data.sAudioIn << "'" << std::endl;
	file << "AudioOutput='" << data.sAudioOut << "'" << std::endl;
	file << "APRSEnable=" << (data.bAPRSEnable ? "true" : "false") << std::endl;
	file << "APRSServer='" << data.sAPRSServer << "'" << std::endl;
	file << "APRSPort=" << data.usAPRSPort << std::endl;
	file << "APRSInterval=" << data.iAPRSInterval << std::endl;
	file << "GPSDEnable=" << (data.bGPSDEnable ? "true" : "false") << std::endl;
	file << "GPSDServer=" << data.sGPSDServer << std::endl;
	file << "GPSDPort=" << data.usGPSDPort << std::endl;

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
	data.cModule = from.cModule;
	data.sURL.assign(from.sURL);
	data.bDPlusEnable = from.bDPlusEnable;
	data.sLinkAtStart.assign(from.sLinkAtStart);
	data.bUseMyCall = from.bUseMyCall;
	data.iBaudRate = from.iBaudRate;
	data.eNetType = from.eNetType;
	data.eMode = from.eMode;
	data.dLatitude = from.dLatitude;
	data.dLongitude = from.dLongitude;
	data.sAudioIn.assign(from.sAudioIn);
	data.sAudioOut.assign(from.sAudioOut);
	data.bAPRSEnable = from.bAPRSEnable;
	data.sAPRSServer.assign(from.sAPRSServer);
	data.usAPRSPort = from.usAPRSPort;
	data.iAPRSInterval = from.iAPRSInterval;
	data.bGPSDEnable = from.bGPSDEnable;
	data.sGPSDServer.assign(from.sGPSDServer);
	data.usGPSDPort = from.usGPSDPort;
}

void CConfigure::CopyTo(CFGDATA &to)
{
	to.sCallsign.assign(data.sCallsign);
	to.sLocation[0].assign(data.sLocation[0]);
	to.sLocation[1].assign(data.sLocation[1]);
	to.sMessage.assign(data.sMessage);
	to.sName.assign(data.sName);
	to.sStation.assign(data.sStation);
	to.cModule = data.cModule;
	to.sURL.assign(data.sURL);
	to.bDPlusEnable = data.bDPlusEnable;
	to.sLinkAtStart.assign(data.sLinkAtStart);
	to.bUseMyCall = data.bUseMyCall;
	to.iBaudRate = data.iBaudRate;
	to.eNetType = data.eNetType;
	to.eMode = data.eMode;
	to.dLatitude = data.dLatitude;
	to.dLongitude = data.dLongitude;
	to.sAudioIn.assign(data.sAudioIn);
	to.sAudioOut.assign(data.sAudioOut);
	to.bAPRSEnable = data.bAPRSEnable;
	to.sAPRSServer.assign(data.sAPRSServer);
	to.usAPRSPort = data.usAPRSPort;
	to.iAPRSInterval = data.iAPRSInterval;
	to.bGPSDEnable = data.bGPSDEnable;
	to.sGPSDServer.assign(data.sGPSDServer);
	to.usGPSDPort = data.usGPSDPort;
}

bool CConfigure::IsOkay()
{
	bool station = (data.sStation.size() > 0);
	bool module = isalpha(data.cModule);
	bool call = (data.sCallsign.size() > 0);
	bool audio = (data.sAudioIn.size()>0 && data.sAudioOut.size()>0);
	return (station && module && call && audio);
}

const CFGDATA *CConfigure::GetData()
{
	return &data;
}
