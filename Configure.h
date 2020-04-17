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

#pragma once

#include <string>

#define IS_TRUE(a) (a=='t' || a=='T' || a=='1')

enum class EQuadNetType { ipv4only, ipv6only, dualstack, norouting };
enum class EMode { routing, linking };

using CFGDATA = struct CFGData_struct {
	std::string sCallsign, sName, sStation, sMessage, sLocation[2], sURL, sLinkAtStart, sAudioIn, sAudioOut, sAPRSServer, sGPSDServer;
	bool bUseMyCall, bDPlusEnable, bGPSDEnable, bAPRSEnable;
	EMode eMode;
	int iBaudRate, iAPRSInterval;
	unsigned short usAPRSPort, usGPSDPort;
	EQuadNetType eNetType;
	double dLatitude, dLongitude;
	char cModule;
};

class CConfigure {
public:
	CConfigure() { ReadData(); }
	~CConfigure() {}

	void ReadData();
	void WriteData();
	void CopyFrom(const CFGDATA &);
	void CopyTo(CFGDATA &);
	bool IsOkay();

private:
	// data
	CFGDATA data;
	// methods
	void SetDefaultValues();
};
