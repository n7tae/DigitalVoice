/*
 *   Copyright (C) 2010-2015 by Jonathan Naylor G4KLX
 *   Copyright (C) 2018-2019 by Thomas A. Early N7TAE
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

#include <string>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iostream>

#include "DPlusAuthenticator.h"

CDPlusAuthenticator::CDPlusAuthenticator(const std::string &loginCallsign, const std::string &address) :
m_loginCallsign(loginCallsign),
m_address(address)
{
	assert(loginCallsign.size());

	Trim(m_loginCallsign);
}

CDPlusAuthenticator::~CDPlusAuthenticator()
{
}

bool CDPlusAuthenticator::Process(std::map<std::string, SDATA *> &gwy_map, const bool reflectors, const bool repeaters)
// return true if everything went okay
{
	int result = client.Open(m_address, AF_UNSPEC, "20001");
	if (result) {
		std::cerr << "DPlus Authorization failed: " << gai_strerror(result) << std::endl;
		return true;
	}
	return authenticate(gwy_map, reflectors, repeaters);
}

bool CDPlusAuthenticator::authenticate(std::map<std::string, SDATA *> &gwy_map, const bool reflectors, const bool repeaters)
{
	unsigned char* buffer = new unsigned char[4096U];
	::memset(buffer, ' ', 56U);

	buffer[0U] = 0x38U;
	buffer[1U] = 0xC0U;
	buffer[2U] = 0x01U;
	buffer[3U] = 0x00U;

	::memcpy(buffer+4, m_loginCallsign.c_str(), m_loginCallsign.size());
	::memcpy(buffer+12, "DV019999", 8);
	::memcpy(buffer+28, "W7IB2", 5);
	::memcpy(buffer+40, "DHS0257", 7);

	if (client.Write(buffer, 56U)) {
		std::cerr << "DPlus auth ERROR: could not write opening phrase" << std::endl;
		client.Close();
		delete[] buffer;
		return true;
	}

	int ret = client.ReadExact(buffer, 2U);

	while (ret == 2) {
		unsigned int len = (buffer[1U] & 0x0FU) * 256U + buffer[0U];
		// Ensure that we get exactly len - 2U bytes from the TCP stream
		ret = client.ReadExact(buffer + 2U, len - 2U);
		if (0 > ret) {
			std::cerr << "DPlus auth ERROR: Problem reading line: " << strerror(errno) << std::endl;
			return true;
		}

		if ((buffer[1U] & 0xC0U) != 0xC0U || buffer[2U] != 0x01U) {
			std::cerr << "DPlus auth ERROR: Invalid packet received from server" << std::endl;
			return true;
		}

		for (unsigned int i = 8U; (i + 25U) < len; i += 26U) {
			std::string address((char *)(buffer + i));
			std::string name((char *)(buffer + i + 16U));

			Trim(address);
			Trim(name);
			name.resize(8, ' ');

			// Get the active flag
			bool active = (buffer[i + 25U] & 0x80U) == 0x80U;

			// An empty name or IP address or an inactive gateway/reflector is not added
			if (address.size()>0U && name.size()>0U && active) {
				//std::cout << "name:" << name << " address:" << address << std::endl;
				if (reflectors && 0==name.compare(0, 3, "REF")) {
					SDATA *pData = new SDATA;
					if (pData) {
						pData->address = address;
						pData->port = 20001U;
						gwy_map[name] = pData;
					}
				} else if (repeaters && name.compare(0, 3, "REF")) {
					SDATA *pData = new SDATA;
					if (pData) {
						pData->address = address;
						pData->port = 20001U;
						gwy_map[name] = pData;
					}
				}
			}
		}

		ret = client.ReadExact(buffer, 2U);
	}

	client.Close();

	delete[] buffer;

	return false;
}

void CDPlusAuthenticator::Trim(std::string &s)
{
	auto it = s.begin();
	while (it!=s.end() && isspace(*it))
		s.erase(it);
	auto rit = s.rbegin();
	while (rit!=s.rend() && isspace(*rit)) {
		s.resize(s.size() - 1);
        rit = s.rbegin();
    }
}