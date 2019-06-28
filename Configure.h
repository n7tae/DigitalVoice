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

using CFGDATA = struct CFGData {
	std::string sCallsign, sName, sStation, sMessage, sLocation[2], sURL, sLinkAtStart;
	bool bUseMyCall, bDPlusEnable, bMaintainLink;
	int iBaudRate;
	EQuadNetType eNetType;
	double fLatitude, fLongitude;
};

class CConfigure {
public:
	CConfigure() {}
	~CConfigure() {}

	void ReadData();
	void WriteData();
	void CopyFrom(const CFGDATA &);
	void CopyTo(CFGDATA &);

private:
	// data
	CFGDATA data;
	// methods
	void SetDefaultValues();
};
