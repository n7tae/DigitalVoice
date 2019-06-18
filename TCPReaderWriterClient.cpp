/*
 *   Copyright (C) 2010-2013 by Jonathan Naylor G4KLX
 *   Copyright (C) 2019 by Thomas A. Early N7TAE
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

#include "TCPReaderWriterClient.h"
#include <iostream>
#include <cerrno>
#include <cassert>
#include <cstring>


CTCPReaderWriterClient::CTCPReaderWriterClient(const std::string &address, int family, const std::string &port) :
m_address(address),
m_family(family),
m_port(port),
m_fd(-1)
{
}

CTCPReaderWriterClient::CTCPReaderWriterClient() : m_fd(-1)
{
}

CTCPReaderWriterClient::~CTCPReaderWriterClient()
{
}

bool CTCPReaderWriterClient::Open(const std::string &address, int family, const std::string &port)
{
	m_address	= address;
	m_family	= family;
	m_port		= port;

	return Open();
}

bool CTCPReaderWriterClient::Open()
{
	if (m_fd != -1) {
		std::cerr << "ERROR: port for " << m_address << " is already open!\n"  << std::endl;
		return true;
	}

	if (0 == m_address.size() || 0 == m_port.size() || 0 == std::stoul(m_port)) {
		std::cerr << "ERROR: [" << m_address << "]:" << m_port << "is malformed!" << std::endl;
		return true;
	}

	if (AF_INET!=m_family && AF_INET6!=m_family && AF_UNSPEC!=m_family) {
		std::cerr << "ERROR: family must be AF_INET, AF_INET6 or AF_UNSPEC" << std::endl;
		return true;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	//hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *res;
	int s = EAI_AGAIN;
	int count = 0;
	while (EAI_AGAIN==s and count++<20) {
		// connecting to a server, so we can wait until it's ready
		s = getaddrinfo(m_address.c_str(), m_port.c_str(), &hints, &res);
		if (s && s != EAI_AGAIN) {
			std::cerr << "ERROR: getaddrinfo of " << m_address << ": " << gai_strerror(s) << std::endl;
			return true;
		}
		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

    if (EAI_AGAIN == s) {
       std::cerr << "ERROR getaddrinfo of " << m_address << " failed 20 times" << std::endl;
        return true;
    }

	struct addrinfo *rp;
	for (rp = res; rp != NULL; rp = rp->ai_next) {
		m_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (m_fd == -1)
			continue;

		if (connect(m_fd, rp->ai_addr, rp->ai_addrlen)) {
			Close();
			continue;
		} else {
			char buf[INET6_ADDRSTRLEN];
			void *addr;
			if (AF_INET == rp->ai_family) {
				struct sockaddr_in *addr4 = (struct sockaddr_in *)rp->ai_addr;
				addr = &(addr4->sin_addr);
			} else {
				struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)rp->ai_addr;
				addr = &(addr6->sin6_addr);
			}
			//if (inet_ntop(rp->ai_family, addr, buf, INET6_ADDRSTRLEN))
			// std::cout << "Successfully connected to " << m_address << " at [" << buf << "]:" << m_port << std::endl;
			break;
		}
	}
	freeaddrinfo(res);

	if (rp == NULL) {
		std::cerr << "Could not connect to any system returned by " << m_address << std::endl;
		m_fd = -1;
		return true;
	}

	return false;
}

int CTCPReaderWriterClient::ReadExact(unsigned char *buf, const unsigned int length)
{
	unsigned int offset = 0U;

	do {
		int n = Read(buf + offset, length - offset);
		if (n < 0)
			return n;

		offset += n;
	} while ((length - offset) > 0U);

	return length;
}

int CTCPReaderWriterClient::Read(unsigned char* buffer, const unsigned int length)
{
	assert(buffer != NULL);
	assert(length > 0U);
	assert(m_fd != -1);

	ssize_t len = recv(m_fd, buffer, length, 0);
	if (len <= 0) {
		if (len < 0)
			std::cerr << "Error returned from recv: " << strerror(errno) << std::endl;
		return -1;
	}

	return len;
}

int CTCPReaderWriterClient::ReadLine(std::string& line)
{
	unsigned char c;
	int resultCode;
	int len = 0;
	line = "";

	do
	{
		resultCode = Read(&c, 1);
		if(resultCode == 1) {
			line += c;
			len++;
		}
	} while(c != '\n' && resultCode == 1);

	return resultCode <= 0 ? resultCode : len;
}

bool CTCPReaderWriterClient::Write(const unsigned char *buffer, const unsigned int length)
{
	assert(buffer != NULL);
	assert(length > 0U);
	assert(m_fd != -1);

	ssize_t ret = send(m_fd, (char *)buffer, length, 0);
	if (ret != ssize_t(length)) {
		if (ret < 0)
			std::cerr << "Error returned from send: " << strerror(errno) << std::endl;
		else
			std::cerr << "Error only wrote " << ret << " of " << length << " bytes" << std::endl;
		return true;
	}

	return false;
}

bool CTCPReaderWriterClient::WriteLine(const std::string& line)
{
	std::string lineCopy(line);
	if(lineCopy.size() > 0 && lineCopy.at(lineCopy.size() - 1) != '\n')
		lineCopy.append("\n");

	size_t len = lineCopy.size();
	bool result = true;
	for(size_t i = 0; i < len && result; i++){
		unsigned char c = lineCopy.at(i);
		result = Write(&c , 1);
	}

	return result;
}

void CTCPReaderWriterClient::Close()
{
	if (m_fd != -1) {
		close(m_fd);
		m_fd = -1;
	}
}
