/*
 *   Copyright (C) 2018-2020 by Thomas Early N7TAE
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

#include <map>
#include <set>
#include <regex>
#include <atomic>

#include "IRCDDB.h"
#include "Packet.h"
#include "TypeDefs.h"
#include "SockAddress.h"
#include "UnixDgramSocket.h"
#include "Configure.h"
#include "QnetDB.h"
#include "DStarDecode.h"
#include "Base.h"

#define MAXHOSTNAMELEN 64
#define CALL_SIZE 8
#define MAX_DTMF_BUF 32

using STOREMOTEG2 = struct gate_to_remote_g2_tag {
	unsigned short streamid;
	CSockAddress toDstar;
	time_t last_time;
};

using STOREPEATER = struct torepeater_tag {
	// help with header re-generation
	SDSVT saved_hdr; // repeater format
	CSockAddress saved_addr;

	unsigned short streamid;
	CSockAddress addr;
	time_t last_time;
	unsigned char sequence;
};

class CQnetGateway : CBase
{
public:
	CQnetGateway();
	~CQnetGateway();
	void Process();
	bool Init(CFGDATA *pData);
	std::atomic<bool> keep_running;

private:
	// configuration data
	const CFGDATA *pCFGData;
    // link type
    int link_family = AF_UNSPEC;
	// network type
	int af_family[2] = { AF_UNSPEC, AF_UNSPEC };
    int Index = -1;

	CQnetDB qnDB;
	CDStarDecode decode;
	CUnixDgramReader AM2Gate;
	CUnixDgramWriter Gate2AM;

	SPORTIP g2_external, g2_ipv6_external, ircddb[2];

	std::string OWNER, owner, IRCDDB_PASSWORD[2];

	bool GATEWAY_SEND_QRGS_MAP, GATEWAY_HEADER_REGEN, playNotInCache;
	bool LOG_DEBUG, LOG_IRC, LOG_QSO;

	int TIMING_PLAY_WAIT, TIMING_PLAY_DELAY, TIMING_TIMEOUT_REMOTE_G2, TIMING_TIMEOUT_LOCAL_RPTR;

	unsigned int vPacketCount = 0;

	std::set<std::string> findRoute;

	// data needed for aprs login and aprs beacon
	// RPTR defined in aprs.h
	SRPTR Rptr;

	SDSVT recbuf; // 56 or 27, max is 56

	// the streamids going to remote Gateways from each local module
	STOREMOTEG2 to_remote_g2;

	// input from remote G2 gateway
	int g2_sock[2] = { -1, -1 };
	CSockAddress fromDstar;

	// Incoming data from remote systems
	// must be fed into our local repeater modules.
	STOREPEATER toRptr;

	SDSVT end_of_audio;

	// send packets to g2_link
	struct sockaddr_in plug;

	// for talking with the irc server
	CIRCDDB *ii[2];

	// text coming from local repeater bands
	SBANDTXT band_txt;

	/* Used to validate MYCALL input */
	std::regex preg;

	pthread_mutex_t irc_data_mutex[2] = PTHREAD_MUTEX_INITIALIZER;

	bool VoicePacketIsSync(const unsigned char *text);
	void AddFDSet(int &max, int newfd, fd_set *set);
	int open_port(const SPORTIP *pip, int family);
	void calcPFCS(unsigned char *packet, int len);
	void GetIRCDataThread(const int i);
	int get_yrcall_rptr_from_cache(const int i, const std::string &call, std::string &rptr, std::string &gate, std::string &addr, char RoU);
	int get_yrcall_rptr(const std::string &call, std::string &rptr, std::string &gate, std::string &addr, char RoU);
	void ProcessTimeouts();
	bool ProcessG2Msg(const unsigned char *data, std::string &smrtgrp);
	void ProcessG2(const ssize_t g2buflen, SDSVT &g2buf);
	void ProcessAudio(const SDSVT *packet);
	bool Flag_is_ok(unsigned char flag);
	void UnpackCallsigns(const std::string &str, std::set<std::string> &set, const std::string &delimiters = ",");
	void PrintCallsigns(const std::string &key, const std::set<std::string> &set);
    int FindIndex() const;

	// read configuration file
	bool Configure();

	void qrgs_and_maps();

	void set_dest_rptr(std::string &call);
};
