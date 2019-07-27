/*
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

#pragma once

#include <regex.h>
#include <string>
#include <map>
#include <vector>
#include <set>
#include <atomic>
#include <netinet/in.h>

#include "QnetTypeDefs.h"
#include "Random.h"
#include "SockAddress.h"
#include "UnixDgramSocket.h"
#include "Configure.h"

/*** version number must be x.xx ***/
#define CALL_SIZE 8
#define IP_SIZE 15
#define QUERY_SIZE 56
#define MAXHOSTNAMELEN 64
#define TIMEOUT 50
#define LH_MAX_SIZE 39

typedef struct refdvst_tag {
	unsigned char head[2];
	CDSVT dsvt;
} SREFDVST;

typedef struct link_to_remote_g2_tag {
    char to_call[CALL_SIZE + 1];
    CSockAddress addr;
    char from_mod;
    char to_mod;
    short countdown;
    bool is_connected;
    unsigned short in_streamid;  // incoming from remote systems
    unsigned short out_streamid; // outgoing to remote systems
} STOREMOTE;

class CQnetLink {
public:
	// functions
	CQnetLink();
	~CQnetLink();
	bool Init();
	void Process();
	void Shutdown();
	void Link(const char *call, const char to_mod);
	void Unlink();
	std::atomic<bool> keep_running;
private:
	// functions
	void ToUpper(std::string &s);
	void calcPFCS(unsigned char *packet, int len);
	bool Configure();
	bool srv_open();
	void srv_close();
	void print_status_file();
	bool resolve_rmt(const char *name, const unsigned short port, CSockAddress &addr);
	void PlayAudioNotifyThread(char *msg);
	void SendLog(const char *fmt, ...);

	/* configuration data */
	CFGDATA cfgdata;
	std::string owner, to_g2_external_ip, my_g2_link_ip, status_file, qnvoice_file, announce_dir;
	bool only_admin_login, only_link_unlink, qso_details, log_debug, announce;
	unsigned short rmt_xrf_port, rmt_ref_port, rmt_dcs_port, my_g2_link_port, to_g2_external_port;
    int delay_before;
	std::string link_at_startup;
	int rf_inactivity_timer;
	const unsigned char REF_ACK[3] = { 3, 96, 0 };

	char notify_msg[64];

	STOREMOTE to_remote_g2;

	struct tracing_tag {
		unsigned short streamid;
		time_t last_time;	// last time RF user talked
	} tracing;

	// input from remote
	int xrf_g2_sock, ref_g2_sock, dcs_g2_sock;
	CSockAddress fromDst4;

	// unix socket to the audio unit
	CUnixDgramReader AM2Link;
	CUnixDgramWriter Link2AM, LogInput;

	// input from our own local repeater
	struct sockaddr_in fromRptr;

	fd_set fdset;
	struct timeval tv;

	// Used to validate incoming donglers
	regex_t preg;

	unsigned char queryCommand[QUERY_SIZE];

	// this is used for the "dashboard and qso_details" to avoid processing multiple headers
	unsigned short old_sid;

	CRandom Random;
};
