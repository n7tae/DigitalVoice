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
#include <map>

// value for the gateway table, the key is the callsign/reflector name
using HOST_DATA = struct host_data_tag {
	std::string address;
	unsigned short port;
};

using HOST_MAP = std::map<std::string, HOST_DATA *>;

class CHostFile
{
public:
	CHostFile();
	~CHostFile();
	int Open(const char *filename, unsigned short defaultport);
	int Transfer(HOST_MAP &tomap, const char *type);
	void Init();
	// data
	HOST_MAP hostmap;
	void ClearMap();
};
