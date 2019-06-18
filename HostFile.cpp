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

#include <fstream>
#include <cstring>

#include "HostFile.h"

void CHostFile::Open(const char *filename, unsigned short defaultport)
{
	ClearMap();
	std::ifstream hostfile(filename, std::ifstream::in);
	if (hostfile.is_open()) {
		while (! hostfile.eof()) {
			const char *ws = " \t\r\n";
			char line[128];
			hostfile.getline(line, 128);
			char *key = strtok(line, ws);
			if (key && *key!='#') {
				char *address = strtok(NULL, ws);
				if (address) {
					unsigned short port;
					char *p = strtok(NULL, ws);
					if (p && isdigit(*p))
						port = std::stoul(p);
					else
						port = defaultport;
					SDATA *pData = new SDATA;
					pData->address.assign(address);
					pData->port = port;
					hostmap[key] = pData;
				}
			}
		}
		hostfile.close();
	}
}

CHostFile::~CHostFile()
{
	ClearMap();
}

void CHostFile::ClearMap()
{
	for (auto it=hostmap.begin(); it!=hostmap.end(); it++)
		delete it->second;
	hostmap.clear();
}
