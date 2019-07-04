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
#include <iostream>

#include "HostFile.h"

// globals
extern bool GetCfgDirectory(std::string &);

CHostFile::CHostFile()
{
}

CHostFile::~CHostFile()
{
	ClearMap();
}

int CHostFile::Open(const char *filename, unsigned short defaultport)
{
	ClearMap();
	std::ifstream hostfile(filename, std::ifstream::in);
	if (hostfile.is_open()) {
		char line[128];
		while (hostfile.getline(line, 128)) {
			const char *ws = " \t\r\n";
			char *key = strtok(line, ws);
			if (key && *key!='#') {
				char *address = strtok(NULL, ws);
				if (address) {
					std::string kstr(key);
					kstr.resize(8, ' ');
					unsigned short port;
					char *p = strtok(NULL, ws);
					if (p && isdigit(*p))
						port = std::stoul(p);
					else
						port = defaultport;
					HOST_DATA *pData = new HOST_DATA;
					pData->address.assign(address);
					pData->port = port;
					hostmap[kstr] = pData;
				}
			}
		}
		hostfile.close();
		return int(hostmap.size());
	}
	return 0;
}

void CHostFile::ClearMap()
{
	for (auto it=hostmap.begin(); it!=hostmap.end(); it++)
		delete it->second;
	hostmap.clear();
}

int CHostFile::Transfer(HOST_MAP &tomap, const char *type)
{
	int count = 0;
	for (auto it=hostmap.begin(); it!=hostmap.end(); ) {
		if (it->first.compare(0, 3, type))
			it++;
		else {
			tomap[it->first] = it->second;
			it = hostmap.erase(it);
			count++;
		}
	}
	return count;
}

void CHostFile::Init()
{
	ClearMap();
	std::string path;
	if (GetCfgDirectory(path)) {
		std::cerr << "Can't find a home directory!" << std::endl;
		return;
	}

	std::string name(path + "XLX_Hosts.txt");
	CHostFile hosts;
	int xlx=0, xrf=0, dcs=0, ref=0, custom=0;
	if (hosts.Open(name.c_str(), 30001U))
		xlx = hosts.Transfer(hostmap, "XLX");

	name.assign(path + "DExtra_Hosts.txt");
	if (hosts.Open(name.c_str(), 30001U))
		xrf = hosts.Transfer(hostmap, "XRF");

	name.assign(path + "DCS_Hosts.txt");
	if (hosts.Open(name.c_str(), 30051U))
		dcs = hosts.Transfer(hostmap, "DCS");

	name.assign(path + "DPlus_Hosts.txt");
	if (hosts.Open(name.c_str(), 20001U))
		ref = hosts.Transfer(hostmap, "REF");

	name.assign(path + "MyHosts.txt");
	if (hosts.Open(name.c_str(), 20001)) {
		for (auto it=hosts.hostmap.begin(); it!=hosts.hostmap.end(); ) {
			custom++;
			hostmap[it->first] = it->second;
			it = hosts.hostmap.erase(it);
		}
	}

	hosts.ClearMap();
	std::cout << "Host map contains:\n\t" << xlx << " XLX_Hosts.txt\n\t" << xrf << " DExtraHosts.txt\n\t" << dcs << " DCS_Hosts.txt\n\t" << ref << " DPlus_Hosts.txt\n\t" << custom << " MyHosts.txt" << std::endl;
}
