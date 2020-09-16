/*
 *   Copyright (C) 2010 by Scott Lawson KI4LKF
 *   Copyright (C) 2015,2018-2020 by Thomas A. Early N7TAE
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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <regex.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <arpa/inet.h>
#include <netdb.h>

#include <iostream>
#include <fstream>
#include <future>
#include <exception>
#include <utility>
#include <thread>
#include <chrono>

#include "DPlusAuthenticator.h"
#include "QnetLink.h"

#define LINK_VERSION "QnetLink-417"
#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

CQnetLink::CQnetLink()
{
	keep_running = true;
	memset(&tracing, 0, sizeof(struct tracing_tag));
	old_sid = 0U;
}

CQnetLink::~CQnetLink()
{

}

bool CQnetLink::resolve_rmt(const char *name, const unsigned short port, CSockAddress &addr)
{
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *rp;
	bool found = false;

	memset(&hints, 0x00, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	int rc = getaddrinfo(name, NULL, &hints, &res);
	if (rc != 0) {
		fprintf(stderr, "getaddrinfo return error code %d for [%s]\n", rc, name);
		return false;
	}

	for (rp=res; rp!=NULL; rp=rp->ai_next) {
		if ((AF_INET==rp->ai_family || AF_INET6==rp->ai_family) && SOCK_DGRAM==rp->ai_socktype) {
            if (AF_INET == rp->ai_family) {
                char saddr[INET_ADDRSTRLEN];
                struct sockaddr_in *addr4 = (struct sockaddr_in *)rp->ai_addr;
                if (inet_ntop(rp->ai_family, &addr4->sin_addr, saddr, INET_ADDRSTRLEN)) {
                    addr.Initialize(rp->ai_family, port, saddr);
                    found = true;
                    break;
                }
            } else if (AF_INET6 == rp->ai_family) {
                char saddr[INET6_ADDRSTRLEN];
                struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)rp->ai_addr;
                if (inet_ntop(rp->ai_family, &addr6->sin6_addr, saddr, INET6_ADDRSTRLEN)) {
                    addr.Initialize(rp->ai_family, port, saddr);
                    found = true;
                    break;
                }
            }
		}
	}
	freeaddrinfo(res);

    if (found && strcmp(name, addr.GetAddress())) {
        printf("Node address %s on port %u resolved to %s\n", name, port, addr.GetAddress());
    }

	return found;
}

/* compute checksum */
void CQnetLink::calcPFCS(unsigned char *packet, int len)
{
	unsigned short crc_tabccitt[256] = {
		0x0000,0x1189,0x2312,0x329b,0x4624,0x57ad,0x6536,0x74bf,0x8c48,0x9dc1,0xaf5a,0xbed3,0xca6c,0xdbe5,0xe97e,0xf8f7,
		0x1081,0x0108,0x3393,0x221a,0x56a5,0x472c,0x75b7,0x643e,0x9cc9,0x8d40,0xbfdb,0xae52,0xdaed,0xcb64,0xf9ff,0xe876,
		0x2102,0x308b,0x0210,0x1399,0x6726,0x76af,0x4434,0x55bd,0xad4a,0xbcc3,0x8e58,0x9fd1,0xeb6e,0xfae7,0xc87c,0xd9f5,
		0x3183,0x200a,0x1291,0x0318,0x77a7,0x662e,0x54b5,0x453c,0xbdcb,0xac42,0x9ed9,0x8f50,0xfbef,0xea66,0xd8fd,0xc974,
		0x4204,0x538d,0x6116,0x709f,0x0420,0x15a9,0x2732,0x36bb,0xce4c,0xdfc5,0xed5e,0xfcd7,0x8868,0x99e1,0xab7a,0xbaf3,
		0x5285,0x430c,0x7197,0x601e,0x14a1,0x0528,0x37b3,0x263a,0xdecd,0xcf44,0xfddf,0xec56,0x98e9,0x8960,0xbbfb,0xaa72,
		0x6306,0x728f,0x4014,0x519d,0x2522,0x34ab,0x0630,0x17b9,0xef4e,0xfec7,0xcc5c,0xddd5,0xa96a,0xb8e3,0x8a78,0x9bf1,
		0x7387,0x620e,0x5095,0x411c,0x35a3,0x242a,0x16b1,0x0738,0xffcf,0xee46,0xdcdd,0xcd54,0xb9eb,0xa862,0x9af9,0x8b70,
		0x8408,0x9581,0xa71a,0xb693,0xc22c,0xd3a5,0xe13e,0xf0b7,0x0840,0x19c9,0x2b52,0x3adb,0x4e64,0x5fed,0x6d76,0x7cff,
		0x9489,0x8500,0xb79b,0xa612,0xd2ad,0xc324,0xf1bf,0xe036,0x18c1,0x0948,0x3bd3,0x2a5a,0x5ee5,0x4f6c,0x7df7,0x6c7e,
		0xa50a,0xb483,0x8618,0x9791,0xe32e,0xf2a7,0xc03c,0xd1b5,0x2942,0x38cb,0x0a50,0x1bd9,0x6f66,0x7eef,0x4c74,0x5dfd,
		0xb58b,0xa402,0x9699,0x8710,0xf3af,0xe226,0xd0bd,0xc134,0x39c3,0x284a,0x1ad1,0x0b58,0x7fe7,0x6e6e,0x5cf5,0x4d7c,
		0xc60c,0xd785,0xe51e,0xf497,0x8028,0x91a1,0xa33a,0xb2b3,0x4a44,0x5bcd,0x6956,0x78df,0x0c60,0x1de9,0x2f72,0x3efb,
		0xd68d,0xc704,0xf59f,0xe416,0x90a9,0x8120,0xb3bb,0xa232,0x5ac5,0x4b4c,0x79d7,0x685e,0x1ce1,0x0d68,0x3ff3,0x2e7a,
		0xe70e,0xf687,0xc41c,0xd595,0xa12a,0xb0a3,0x8238,0x93b1,0x6b46,0x7acf,0x4854,0x59dd,0x2d62,0x3ceb,0x0e70,0x1ff9,
		0xf78f,0xe606,0xd49d,0xc514,0xb1ab,0xa022,0x92b9,0x8330,0x7bc7,0x6a4e,0x58d5,0x495c,0x3de3,0x2c6a,0x1ef1,0x0f78
	};

	unsigned short crc_dstar_ffff = 0xffff;
	unsigned short tmp;
	short int low, high;

	if (len == 56) {
		low = 15;
		high = 54;
	} else if (len == 58) {
		low = 17;
		high = 56;
	} else
		return;

	for (short int i=low; i<high ; i++) {
		unsigned short short_c = 0x00ff & (unsigned short)packet[i];
		tmp = (crc_dstar_ffff & 0x00ff) ^ short_c;
		crc_dstar_ffff = (crc_dstar_ffff >> 8) ^ crc_tabccitt[tmp];
	}
	crc_dstar_ffff =  ~crc_dstar_ffff;
	tmp = crc_dstar_ffff;

	if (len == 56) {
		packet[54] = (unsigned char)(crc_dstar_ffff & 0xff);
		packet[55] = (unsigned char)((tmp >> 8) & 0xff);
	} else {
		packet[56] = (unsigned char)(crc_dstar_ffff & 0xff);
		packet[57] = (unsigned char)((tmp >> 8) & 0xff);
	}
	return;
}

void CQnetLink::ToUpper(std::string &s)
{
	for (auto it=s.begin(); it!=s.end(); it++)
		if (islower(*it))
			*it = toupper(*it);
}

/* process configuration file */
bool CQnetLink::Configure()
{
	owner.assign(pCFGData->sStation);
	owner.resize(CALL_SIZE, ' ');

	link_at_startup.assign(pCFGData->sLinkAtStart);

	my_g2_link_ip.assign("0.0.0.0");
	rmt_ref_port = 20001U;
	rmt_xrf_port = 30001U;
	rmt_dcs_port = 30051U;
	announce = true;
	qso_details = true;
	log_debug = false;

	std::string homedir(CFG_DIR);
	announce_dir.assign(homedir + "announce");
	qnvoice_file.assign(homedir + "qnvoicefile.txt");

	return false;
}

/* create our server */
bool CQnetLink::srv_open()
{
	struct sockaddr_in sin;
	short i;

	/* create our XRF gateway socket */
	xrf_g2_sock = socket(PF_INET,SOCK_DGRAM,0);
	if (xrf_g2_sock == -1) {
		fprintf(stderr, "Failed to create gateway socket for XRF,errno=%d\n",errno);
		return false;
	}
	fcntl(xrf_g2_sock,F_SETFL,O_NONBLOCK);

	memset(&sin,0,sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(my_g2_link_ip.c_str());
	sin.sin_port = htons(rmt_xrf_port);
	if (bind(xrf_g2_sock,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "Failed to bind gateway socket on port %d for XRF, errno=%d\n", rmt_xrf_port, errno);
		close(xrf_g2_sock);
		xrf_g2_sock = -1;
		return false;
	}

	/* create the dcs socket */
	dcs_g2_sock = socket(PF_INET,SOCK_DGRAM,0);
	if (dcs_g2_sock == -1) {
		fprintf(stderr, "Failed to create gateway socket for DCS,errno=%d\n", errno);
		close(xrf_g2_sock);
		xrf_g2_sock = -1;
		return false;
	}
	fcntl(dcs_g2_sock,F_SETFL,O_NONBLOCK);

	/* socket for REF */
	ref_g2_sock = socket(PF_INET,SOCK_DGRAM,0);
	if (ref_g2_sock == -1) {
		fprintf(stderr, "Failed to create gateway socket for REF, errno=%d\n", errno);
		close(dcs_g2_sock);
		dcs_g2_sock = -1;
		close(xrf_g2_sock);
		xrf_g2_sock = -1;
		return false;
	}
	fcntl(ref_g2_sock,F_SETFL,O_NONBLOCK);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(my_g2_link_ip.c_str());
	sin.sin_port = htons(rmt_ref_port);
	if (bind(ref_g2_sock,(struct sockaddr *)&sin,sizeof(struct sockaddr_in)) != 0) {
		fprintf(stderr, "Failed to bind gateway socket on port %d for REF, errno=%d\n", rmt_ref_port, errno);
		close(dcs_g2_sock);
		dcs_g2_sock = -1;
		close(xrf_g2_sock);
		xrf_g2_sock = -1;
		close(ref_g2_sock);
		ref_g2_sock = -1;
		return false;
	}

	/* create our gateway unix sockets */
	Link2AM.SetUp("link2am");
	LogInput.SetUp("log_input");
	if (AM2Link.Open("am2link")) {
		close(dcs_g2_sock);
		dcs_g2_sock = -1;
		close(xrf_g2_sock);
		xrf_g2_sock = -1;
		close(ref_g2_sock);
		ref_g2_sock = -1;
		AM2Link.Close();
		return false;
	}

	/* initialize all remote links */
	for (i = 0; i < 3; i++) {
		to_remote_g2.to_call[0] = '\0';
        to_remote_g2.addr.Clear();
		to_remote_g2.from_mod = ' ';
		to_remote_g2.to_mod = ' ';
		to_remote_g2.countdown = 0;
		to_remote_g2.is_connected = false;
		to_remote_g2.in_streamid = 0x0;
		to_remote_g2.out_streamid = 0x0;
	}
	return true;
}

/* destroy our server */
void CQnetLink::srv_close()
{
	if (xrf_g2_sock != -1) {
		close(xrf_g2_sock);
		printf("Closed rmt_xrf_port\n");
	}

	if (dcs_g2_sock != -1) {
		close(dcs_g2_sock);
		printf("Closed rmt_dcs_port\n");
	}

	AM2Link.Close();

	if (ref_g2_sock != -1) {
		close(ref_g2_sock);
		printf("Closed rmt_ref_port\n");
	}

	return;
}

/* find the repeater IP by callsign and link to it */
void CQnetLink::Link(const char *call, const char to_mod)
{
	char link_request[519];
	printf("Link request to %s to module %c\n", call, to_mod);
	memset(link_request, 0, sizeof(link_request));

    to_remote_g2.addr.Clear();
    to_remote_g2.countdown = 0;
    to_remote_g2.from_mod = '\0';
    to_remote_g2.in_streamid = 0;
    to_remote_g2.is_connected = false;
    to_remote_g2.out_streamid = 0;
    to_remote_g2.to_call[0] = '\0';
    to_remote_g2.to_mod = '\0';

	strcpy(to_remote_g2.to_call, call);
	to_remote_g2.to_mod = to_mod;

	std::string address;
	unsigned short port;
	if (qnDB.FindGW(call, address, port)) {
		sprintf(notify_msg, "%c_gatewaynotfound.dat_GATEWAY_NOT_FOUND", pCFGData->cModule);
		log.SendLog("%s not found in gwy list\n", call);
		return;
	}

	if (address.size() < 7) {
		std::cerr << "IP address is too short!" << std::endl;
		return;
	}
	bool ok = resolve_rmt(address.c_str(), port, to_remote_g2.addr);
	if (!ok) {
		log.SendLog("Call %s is host %s but could not resolve to IP\n", call, address.c_str());
		to_remote_g2.addr.Clear();
		to_remote_g2.countdown = 0;
		to_remote_g2.from_mod = '\0';
		to_remote_g2.in_streamid = 0;
		to_remote_g2.is_connected = false;
		to_remote_g2.out_streamid = 0;
		to_remote_g2.to_call[0] = '\0';
		to_remote_g2.to_mod = '\0';
		return;
	}

	strcpy(to_remote_g2.to_call, call);
	to_remote_g2.from_mod = pCFGData->cModule;
	to_remote_g2.to_mod = to_mod;
	to_remote_g2.countdown = TIMEOUT;
	to_remote_g2.is_connected = false;
	to_remote_g2.in_streamid= 0x0;

	/* is it XRF? */
	if (port == rmt_xrf_port) {
		strcpy(link_request, owner.c_str());
		link_request[8] = pCFGData->cModule;
		link_request[9] = to_mod;
		link_request[10] = '\0';

		log.SendLog("Sending link request from mod %c to link with: [%s] mod %c\n", to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.to_mod);

		for (int j=0; j<5; j++)
			sendto(xrf_g2_sock, link_request, CALL_SIZE + 3, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
	} else if (port == rmt_dcs_port) {
		strcpy(link_request, owner.c_str());
		link_request[8] = pCFGData->cModule;
		link_request[9] = to_mod;
		link_request[10] = '\0';
		memcpy(link_request + 11, to_remote_g2.to_call, 8);
		strcpy(link_request + 19, "<table border=\"0\" width=\"95%\"><tr><td width=\"4%\"><img border=\"0\" src=g2ircddb.jpg></td><td width=\"96%\"><font size=\"2\"><b>REPEATER</b> QnetGateway v1.0+</font></td></tr></table>");

		log.SendLog("Sending link request from mod %c to link with: [%s] mod %c\n", to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.to_mod);
		sendto(dcs_g2_sock, link_request, 519, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
	} else if (port == rmt_ref_port) {
		log.SendLog("Sending link command from mod %c to: [%s] mod %c\n", to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.to_mod);

		queryCommand[0] = 5;
		queryCommand[1] = 0;
		queryCommand[2] = 24;
		queryCommand[3] = 0;
		queryCommand[4] = 1;

		sendto(ref_g2_sock, queryCommand, 5, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
	}
}

void CQnetLink::Unlink()
{
	char unlink_request[CALL_SIZE + 3];
	char cmd_2_dcs[23];
	if (to_remote_g2.to_call[0]) {
		if (to_remote_g2.addr.GetPort() == rmt_ref_port) {
			/* nothing else is linked there, send DISCONNECT */
			queryCommand[0] = 5;
			queryCommand[1] = 0;
			queryCommand[2] = 24;
			queryCommand[3] = 0;
			queryCommand[4] = 0;
			sendto(ref_g2_sock, queryCommand, 5, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
		} else if (to_remote_g2.addr.GetPort() == rmt_xrf_port) {
			strcpy(unlink_request, owner.c_str());
			unlink_request[8] = to_remote_g2.from_mod;
			unlink_request[9] = ' ';
			unlink_request[10] = '\0';

			for (int j=0; j<5; j++)
				sendto(xrf_g2_sock, unlink_request, CALL_SIZE+3, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
		} else {
			strcpy(cmd_2_dcs, owner.c_str());
			cmd_2_dcs[8] = to_remote_g2.from_mod;
			cmd_2_dcs[9] = ' ';
			cmd_2_dcs[10] = '\0';
			memcpy(cmd_2_dcs + 11, to_remote_g2.to_call, 8);

			for (int j=0; j<5; j++)
				sendto(dcs_g2_sock, cmd_2_dcs, 19, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
		}

		log.SendLog("Unlinked from [%s] mod %c\n", to_remote_g2.to_call, to_remote_g2.to_mod);
		sprintf(notify_msg, "%c_unlinked.dat_UNLINKED", to_remote_g2.from_mod);
		qnDB.DeleteLS(to_remote_g2.addr.GetAddress());

		/* now zero out this entry */
		to_remote_g2.to_call[0] = '\0';
		to_remote_g2.addr.Clear();
		to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
		to_remote_g2.countdown = 0;
		to_remote_g2.is_connected = false;
		to_remote_g2.in_streamid = 0x0;
	} else {
		sprintf(notify_msg, "%c_already_unlinked.dat_UNLINKED", pCFGData->cModule);
	}

}

void CQnetLink::Process()
{
	time_t tnow = 0, hb = 0;

	char *space_p = 0;
	char linked_remote_system[CALL_SIZE + 1];

	int max_nfds = 0;

	char tmp1[CALL_SIZE + 1];
	unsigned char dcs_buf[1000];;

	char call[CALL_SIZE + 1];
	char ip[INET6_ADDRSTRLEN + 1];
	bool found = false;

	char cmd_2_dcs[23];
	unsigned char dcs_seq = 0U;
	struct rptr_2_dcs_tag {
		char mycall[9];
		char sfx[5];
		unsigned int dcs_rptr_seq;
	} rptr_2_dcs = {"        ", "    ", 0};

	struct silent_tag {
		time_t lasttime;
		unsigned char ctrl;
		SDSVT dsvt;
	} silent;

	memset(&silent, 0, sizeof(silent));

	char source_stn[9];

	time(&hb);

	if (xrf_g2_sock > max_nfds)
		max_nfds = xrf_g2_sock;
	if (ref_g2_sock > max_nfds)
		max_nfds = ref_g2_sock;
	if (dcs_g2_sock > max_nfds)
		max_nfds = dcs_g2_sock;
	if (AM2Link.GetFD() > max_nfds)
		max_nfds = AM2Link.GetFD();

	printf("xrf=%d, dcs=%d, ref=%d, AudioUnit=%d, MAX+1=%d\n", xrf_g2_sock, dcs_g2_sock, ref_g2_sock, AM2Link.GetFD(), max_nfds + 1);

	// initialize all request links
	if (8 == link_at_startup.size()) {
		log.SendLog("Sleep for 5 sec before link at startup\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::string node(link_at_startup.substr(0, 6));
		node.resize(CALL_SIZE, ' ');
		Link(node.c_str(), link_at_startup.at(7));
	}

	while (keep_running) {
		time(&tnow);
		if (keep_running && (tnow - hb) > 0) {
			/* send heartbeat to connected donglers */

			/* send heartbeat to linked XRF repeaters/reflectors */
			if (to_remote_g2.addr.GetPort() == rmt_xrf_port)
				sendto(xrf_g2_sock, owner.c_str(), CALL_SIZE+1, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());

			/* send heartbeat to linked DCS reflectors */
			if (to_remote_g2.addr.GetPort() == rmt_dcs_port) {
				strcpy(cmd_2_dcs, owner.c_str());
				cmd_2_dcs[7] = to_remote_g2.from_mod;
				memcpy(cmd_2_dcs + 9, to_remote_g2.to_call, 8);
				cmd_2_dcs[16] = to_remote_g2.to_mod;
				sendto(dcs_g2_sock, cmd_2_dcs, 17, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
			}

			/* send heartbeat to linked REF reflectors */
			if (to_remote_g2.is_connected && to_remote_g2.addr.GetPort()==rmt_ref_port)
				sendto(ref_g2_sock, REF_ACK, 3, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());

			/* check for timeouts from remote */
			if (to_remote_g2.to_call[0] != '\0') {
				if (to_remote_g2.countdown >= 0)
					to_remote_g2.countdown--;

				if (to_remote_g2.countdown < 0) {
					/* maybe remote system has changed IP */
					printf("Unlinked from [%s] mod %c, TIMEOUT...\n", to_remote_g2.to_call, to_remote_g2.to_mod);

					sprintf(notify_msg, "%c_unlinked.dat_UNLINKED_TIMEOUT", to_remote_g2.from_mod);
					qnDB.DeleteLS(to_remote_g2.addr.GetAddress());

					to_remote_g2.to_call[0] = '\0';
					to_remote_g2.addr.Clear();
					to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
					to_remote_g2.countdown = 0;
					to_remote_g2.is_connected = false;
					to_remote_g2.in_streamid = 0x0;
				}
			}

			// check for packet stream timeout
			if (silent.lasttime && ((silent.lasttime - tnow) > 2)) {
				printf("VoiceStream timeout detected from linked system, closing stream!\n");
				if (silent.ctrl) {
					const unsigned char silence[3] = { 0x70U, 0x4FU, 0x93U };
					memcpy(silent.dsvt.vasd.text, silence, 3U);
				} else {
					const unsigned char sync[3] = { 0x55U, 0x2DU, 0x16U };
					memcpy(silent.dsvt.vasd.voice, sync, 3U);
				}
				Link2AM.Write(silent.dsvt.title, 27);
				to_remote_g2.in_streamid = old_sid = 0U;
				silent.lasttime = 0;
			}
			time(&hb);
		}

		// play a qnvoice file if it is specified
		// this could be coming from qnvoice or qngateway (connected2network or notincache)
		std::ifstream voicefile(qnvoice_file.c_str(), std::ifstream::in);
		if (voicefile) {
			if (keep_running) {
				char line[FILENAME_MAX];
				voicefile.getline(line, FILENAME_MAX);
				// trim whitespace
				char *start = line;
				while (isspace(*start))
					start++;
				char *end = start + strlen(start) - 1;
				while (isspace(*end))
					*end-- = (char)0;
				// anthing reasonable left?
				if (strlen(start) > 2)
					PlayAudioNotifyThread(start);
			}
			//clean-up
			voicefile.close();
			remove(qnvoice_file.c_str());
		}

		FD_ZERO(&fdset);
		FD_SET(xrf_g2_sock, &fdset);
		FD_SET(dcs_g2_sock, &fdset);
		FD_SET(ref_g2_sock, &fdset);
		FD_SET(AM2Link.GetFD(), &fdset);
		tv.tv_sec = 0;
		tv.tv_usec = 20000;
		(void)select(max_nfds + 1, &fdset, 0, 0, &tv);

		bool is_packet = false;
		SDSVT dsvt;

		if (keep_running && FD_ISSET(xrf_g2_sock, &fdset)) {
			socklen_t fromlen = sizeof(struct sockaddr_in);
			unsigned char buf[100];
			int length = recvfrom(xrf_g2_sock, buf, 100, 0, fromDst4.GetPointer(), &fromlen);

			strncpy(ip, fromDst4.GetAddress(), INET6_ADDRSTRLEN);
			ip[INET6_ADDRSTRLEN] = '\0';
			memcpy(call, buf, CALL_SIZE);
			call[CALL_SIZE] = '\0';

			/* A packet of length (CALL_SIZE + 1) is a keepalive from a repeater/reflector */
			/* If it is from a dongle, it is either a keepalive or a request to connect */

			if (length == (CALL_SIZE + 1)) {
				found = false;
				/* Find out if it is a keepalive from a repeater */
				if (fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_xrf_port) {
					found = true;
					if (!to_remote_g2.is_connected) {
						tracing.last_time = time(NULL);

						to_remote_g2.is_connected = true;
						printf("Connected from: %.*s\n", length - 1, buf);
						qnDB.UpdateLS(to_remote_g2.addr.GetAddress(), to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.from_mod, tracing.last_time);

						strcpy(linked_remote_system, to_remote_g2.to_call);
						space_p = strchr(linked_remote_system, ' ');
						if (space_p)
							*space_p = '\0';
						sprintf(notify_msg, "%c_linked.dat_LINKED_%s_%c", to_remote_g2.from_mod, linked_remote_system, to_remote_g2.to_mod);

					}
					to_remote_g2.countdown = TIMEOUT;
				}
			} else if (length == (CALL_SIZE + 6)) {
				/* A packet of length (CALL_SIZE + 6) is either an ACK or a NAK from repeater-reflector */
				/* Because we sent a request before asking to link */

				if ((fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_xrf_port)) {
					if (0==memcmp(buf + 10, "ACK", 3) && to_remote_g2.from_mod==buf[8]) {
						if (!to_remote_g2.is_connected) {
							tracing.last_time = time(NULL);

							to_remote_g2.is_connected = true;
							printf("Connected from: [%s] %c\n", to_remote_g2.to_call, to_remote_g2.to_mod);
							qnDB.UpdateLS(to_remote_g2.addr.GetAddress(), to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.from_mod, tracing.last_time);

							strcpy(linked_remote_system, to_remote_g2.to_call);
							space_p = strchr(linked_remote_system, ' ');
							if (space_p)
								*space_p = '\0';
							sprintf(notify_msg, "%c_linked.dat_LINKED_%s_%c", to_remote_g2.from_mod, linked_remote_system, to_remote_g2.to_mod);
						}
					} else if (0==memcmp(buf + 10, "NAK", 3) && to_remote_g2.from_mod==buf[8]) {
						printf("Link module %c to [%s] %c is rejected\n", to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.to_mod);

						sprintf(notify_msg, "%c_failed_link.dat_FAILED_TO_LINK", to_remote_g2.from_mod);
						qnDB.DeleteLS(to_remote_g2.addr.GetAddress());
						to_remote_g2.to_call[0] = '\0';
						to_remote_g2.addr.Clear();
						to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
						to_remote_g2.countdown = 0;
						to_remote_g2.is_connected = false;
						to_remote_g2.in_streamid = 0x0;
					}
				}
			} else if ((length==56 || length==27) && 0==memcmp(buf, "DSVT", 4) && (buf[4]==0x10 || buf[4]==0x20) && buf[8]==0x20) {
				/* reset countdown and protect against hackers */

				found = false;
				if ((fromDst4 == to_remote_g2.addr) && (to_remote_g2.addr.GetPort() == rmt_xrf_port)) {
					to_remote_g2.countdown = TIMEOUT;
					found = true;
				}

				memcpy(dsvt.title, buf, length);	// copy to struct

				/* process header */
				if ((length == 56) && found) {
					memset(source_stn, ' ', 9);
					source_stn[8] = '\0';

					/* some bad hotspot programs out there using INCORRECT flag */
					if (dsvt.hdr.flag[0]==0x40U || dsvt.hdr.flag[0]==0x48U || dsvt.hdr.flag[0]==0x60U || dsvt.hdr.flag[0]==0x68U) dsvt.hdr.flag[0] -= 0x40;

					/* A reflector will send to us its own RPT1 */
					/* A repeater will send to us our RPT1 */

					if (fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_xrf_port) {
						/* it is a reflector, reflector's rpt1 */
						if (0==memcmp(dsvt.hdr.rpt1, to_remote_g2.to_call, 7) && dsvt.hdr.rpt1[7]==to_remote_g2.to_mod) {
							memcpy(dsvt.hdr.rpt1, owner.c_str(), CALL_SIZE);
							dsvt.hdr.rpt1[7] = to_remote_g2.from_mod;
							memcpy(dsvt.hdr.urcall, "CQCQCQ  ", 8);

							memcpy(source_stn, to_remote_g2.to_call, 8);
							source_stn[7] = to_remote_g2.to_mod;
						} else {
							/* it is a repeater, our rpt1 */
							if (memcmp(dsvt.hdr.rpt1, owner.c_str(), CALL_SIZE-1) && dsvt.hdr.rpt1[7]==to_remote_g2.from_mod) {
								memcpy(source_stn, to_remote_g2.to_call, 8);
								source_stn[7] = to_remote_g2.to_mod;
							}
						}
					}

					/* somebody's crazy idea of having a personal callsign in RPT2 */
					/* we must set it to our gateway callsign */
					memcpy(dsvt.hdr.rpt2, owner.c_str(), CALL_SIZE);
					dsvt.hdr.rpt2[7] = 'G';
					calcPFCS(dsvt.title, 56);
					/* At this point, all data have our RPT1 and RPT2 */

					/* are we sure that RPT1 is our system? */
					if (0==memcmp(dsvt.hdr.rpt1, owner.c_str(), CALL_SIZE-1) && (pCFGData->cModule == dsvt.hdr.rpt1[7])) {
						/* Last Heard */
						if (old_sid != dsvt.streamid) {
							if (qso_details) {
								log.SendLog("START from xrf: streamID=%04x, flags=%02x:%02x:%02x, my=%.8s, sfx=%.4s, ur=%.8s, r1=%.8s, r2=%.8s, %d bytes IP=%s, source=%.8s\n", ntohs(dsvt.streamid), dsvt.hdr.flag[0], dsvt.hdr.flag[1], dsvt.hdr.flag[2], dsvt.hdr.mycall, dsvt.hdr.sfx, dsvt.hdr.urcall, dsvt.hdr.rpt1, dsvt.hdr.rpt2, length, fromDst4.GetAddress(), source_stn);
							}

							old_sid = dsvt.streamid;
						}

						/* relay header to the audio manager */
						//Link2AM.Write(dsvt.title, 56);
						is_packet = true;

					}
				} else if (found) {	// length is 27
					if ((dsvt.ctrl & 0x40) != 0) {
						if (old_sid == dsvt.streamid) {
							if (qso_details)
								log.SendLog("END   from xrf: streamID=%04x, %d bytes from IP=%s\n", ntohs(dsvt.streamid), length, fromDst4.GetAddress());
							old_sid = 0x0;
						}
					}

					/* relay data to the audio manager */
					//Link2AM.Write(dsvt.title, 27);
					is_packet = true;
				}
			}
			FD_CLR (xrf_g2_sock,&fdset);
		}

		if (keep_running && FD_ISSET(ref_g2_sock, &fdset)) {
			socklen_t fromlen = sizeof(struct sockaddr_storage);
			unsigned char buf[100];
			int length = recvfrom(ref_g2_sock, buf, 100, 0, fromDst4.GetPointer(), &fromlen);

			strncpy(ip, fromDst4.GetAddress(), INET6_ADDRSTRLEN+1);
			ip[INET_ADDRSTRLEN] = '\0';

			found = false;

			if (fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_ref_port) {
				found = true;
				if (length==5 && buf[0]==5 && buf[1]==0 && buf[2]==24 && buf[3]==0 && buf[4]==1) {
					printf("Connected to call %s\n", to_remote_g2.to_call);
					queryCommand[0] = 28;
					queryCommand[1] = 192;
					queryCommand[2] = 4;
					queryCommand[3] = 0;

					memcpy(queryCommand + 4, owner.c_str(), CALL_SIZE);
					for (int j=11; j>3; j--) {
						if (queryCommand[j] == ' ')
							queryCommand[j] = '\0';
						else
							break;
					}
					memset(queryCommand + 12, '\0', 8);
					memcpy(queryCommand + 20, "DV019999", 8);

					// ATTENTION: I should ONLY send once for each distinct
					// remote IP, so  get out of the loop immediately
					sendto(ref_g2_sock, queryCommand, 28, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
				}
				else if (length==8 && buf[0]==8 && buf[1]==192 && buf[2]==4 && buf[3]==0) {
					if (buf[4]== 79 && buf[5]==75 && buf[6]==82) {
						if (!to_remote_g2.is_connected) {
							to_remote_g2.is_connected = true;
							to_remote_g2.countdown = TIMEOUT;
							printf("Login OK to call %s mod %c\n", to_remote_g2.to_call, to_remote_g2.to_mod);
							tracing.last_time = time(NULL);
							qnDB.UpdateLS(to_remote_g2.addr.GetAddress(), to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.from_mod, tracing.last_time);

							strcpy(linked_remote_system, to_remote_g2.to_call);
							space_p = strchr(linked_remote_system, ' ');
							if (space_p)
								*space_p = '\0';
							sprintf(notify_msg, "%c_linked.dat_LINKED_%s_%c", to_remote_g2.from_mod, linked_remote_system, to_remote_g2.to_mod);
						}
					} else if (buf[4]==70 && buf[5]==65 && buf[6]==73 && buf[7]==76) {
						printf("Login failed to call %s mod %c\n", to_remote_g2.to_call, to_remote_g2.to_mod);

						sprintf(notify_msg, "%c_failed_link.dat_FAILED_TO_LINK", to_remote_g2.from_mod);

						to_remote_g2.to_call[0] = '\0';
						to_remote_g2.addr.Clear();
						to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
						to_remote_g2.countdown = 0;
						to_remote_g2.is_connected = false;
						to_remote_g2.in_streamid = 0x0;
					} else if (buf[4]==66 && buf[5]==85 && buf[6]==83 && buf[7]==89) {
						printf("Busy or unknown status from call %s mod %c\n", to_remote_g2.to_call, to_remote_g2.to_mod);

						sprintf(notify_msg, "%c_failed_link.dat_FAILED_TO_LINK", to_remote_g2.from_mod);

						to_remote_g2.to_call[0] = '\0';
						to_remote_g2.addr.Clear();
						to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
						to_remote_g2.countdown = 0;
						to_remote_g2.is_connected = false;
						to_remote_g2.in_streamid = 0x0;
					}
				}
				else if (length==24 && buf[0]==24 && buf[1]==192 && buf[2]==3 && buf[3]==0)
					to_remote_g2.countdown = TIMEOUT;

				else if (length == 3)
					to_remote_g2.countdown = TIMEOUT;
			}

			if ((length==58 || length==29 || length==32) && 0==memcmp(buf + 2, "DSVT", 4) && (buf[6]==0x10 || buf[6]==0x20) && buf[10]==0x20) {
				/* Is it one of the donglers or repeaters-reflectors */
				found = false;
				if (fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_ref_port) {
					to_remote_g2.countdown = TIMEOUT;
					found = true;
				}

				memcpy(dsvt.title, buf+2, length);	// copy to struct

				if (length==58 && found) {
					memset(source_stn, ' ', 9);
					source_stn[8] = '\0';

					/* some bad hotspot programs out there using INCORRECT flag */
					if (dsvt.hdr.flag[0]==0x40U || dsvt.hdr.flag[0]==0x48U || dsvt.hdr.flag[0]==0x60U || dsvt.hdr.flag[0]==0x68U)
						dsvt.hdr.flag[0] -= 0x40U;

					/* A reflector will send to us its own RPT1 */
					/* A repeater will send to us its own RPT1 */
					/* A dongler will send to us our RPT1 */

					/* It is from a repeater-reflector, correct rpt1, rpt2 and re-compute pfcs */
					if (fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_ref_port &&
							(
								(0==memcmp(dsvt.hdr.rpt1, to_remote_g2.to_call, 7) && dsvt.hdr.rpt1[7]==to_remote_g2.to_mod)  ||
								(0==memcmp(dsvt.hdr.rpt2, to_remote_g2.to_call, 7) && dsvt.hdr.rpt2[7]==to_remote_g2.to_mod)
							)) {
						memcpy(dsvt.hdr.rpt1, owner.c_str(), CALL_SIZE);
						dsvt.hdr.rpt1[7] = to_remote_g2.from_mod;
						memcpy(dsvt.hdr.urcall, "CQCQCQ  ", CALL_SIZE);

						memcpy(source_stn, to_remote_g2.to_call, CALL_SIZE);
						source_stn[7] = to_remote_g2.to_mod;

					}

					/* somebody's crazy idea of having a personal callsign in RPT2 */
					/* we must set it to our gateway callsign */
					memcpy(dsvt.hdr.rpt2, owner.c_str(), CALL_SIZE);
					dsvt.hdr.rpt2[7] = 'G';
					calcPFCS(dsvt.title, 56);

					/* At this point, all data have our RPT1 and RPT2 */
					/* are we sure that RPT1 is our system? */
					if (0==memcmp(dsvt.hdr.rpt1, owner.c_str(), CALL_SIZE-1) && (dsvt.hdr.rpt1[7]==pCFGData->cModule)) {
						/* Last Heard */
						if (old_sid != dsvt.streamid) {
							if (qso_details) {
								log.SendLog("START from ref: streamID=%04x, flags=%02x:%02x:%02x, my=%.8s, sfx=%.4s, ur=%.8s, rpt1=%.8s, rpt2=%.8s, %d bytes fromIP=%s, source=%.8s\n", ntohs(dsvt.streamid), dsvt.hdr.flag[0], dsvt.hdr.flag[1], dsvt.hdr.flag[2], dsvt.hdr.mycall, dsvt.hdr.sfx, dsvt.hdr.urcall, dsvt.hdr.rpt1, dsvt.hdr.rpt2, length, fromDst4.GetAddress(), source_stn);
							}
							// put user into tmp1
							memcpy(tmp1, dsvt.hdr.mycall, 8);
							tmp1[8] = '\0';
							old_sid = dsvt.streamid;
						}

						/* send the data to the audio manager */
						//Link2AM.Write(rdsvt.dsvt.title, 56);
						is_packet = true;
					}
				} else if (found) {
					if (dsvt.ctrl & 0x40U) {
						if (old_sid == dsvt.streamid) {
							if (qso_details)
								log.SendLog("END   from ref: streamID=%04x, %d bytes from IP=%s\n", ntohs(dsvt.streamid), length, fromDst4.GetAddress());

							old_sid = 0U;
						}
					}

					/* send the data to the audio mgr */
					// Link2AM.Write(rdsvt.dsvt.title, 27);
					is_packet = true;
				}
			}
			FD_CLR (ref_g2_sock,&fdset);
		}

		if (keep_running && FD_ISSET(dcs_g2_sock, &fdset)) {
			socklen_t fromlen = sizeof(struct sockaddr_storage);
			int length = recvfrom(dcs_g2_sock, dcs_buf, 1000, 0, fromDst4.GetPointer(), &fromlen);

			strncpy(ip, fromDst4.GetAddress(), INET6_ADDRSTRLEN);
			ip[INET6_ADDRSTRLEN] = '\0';

			/* header, audio */
			if (dcs_buf[0]=='0' && dcs_buf[1]=='0' && dcs_buf[2]=='0' && dcs_buf[3]=='1') {
				if (length == 100) {
					memset(source_stn, ' ', 9);
					source_stn[8] = '\0';

					/* find out our local module */
					if (to_remote_g2.is_connected && fromDst4==to_remote_g2.addr && 0==memcmp(dcs_buf + 7, to_remote_g2.to_call, 7) && to_remote_g2.to_mod==dcs_buf[14]) {
						memcpy(source_stn, to_remote_g2.to_call, 8);
						source_stn[7] = to_remote_g2.to_mod;
						/* Last Heard */
						if (memcmp(&old_sid, dcs_buf + 43, 2)) {
							if (qso_details)
								printf("START from dcs: streamID=%02x%02x, my=%.8s, sfx=%.4s, ur=%.8s, rpt1=%.8s, rpt2=%.8s, %d bytes fromIP=%s, source=%.8s\n", dcs_buf[44],dcs_buf[43], &dcs_buf[31], &dcs_buf[39], &dcs_buf[23], &dcs_buf[7], &dcs_buf[15], length, fromDst4.GetAddress(), source_stn);
							memcpy(&old_sid, dcs_buf + 43, 2);
						}

						to_remote_g2.countdown = TIMEOUT;

						/* new stream ? */
						if (memcmp(&to_remote_g2.in_streamid, dcs_buf+43, 2)) {
							memcpy(&to_remote_g2.in_streamid, dcs_buf+43, 2);
							dcs_seq = 0xff;

							/* generate our header */
							memcpy(dsvt.title, "DSVT", 4);
							dsvt.config = 0x10;
							dsvt.flaga[0] = dsvt.flaga[1] = dsvt.flaga[2] = 0x00;
							dsvt.id = 0x20;
							dsvt.flagb[0] = 0x00;
							dsvt.flagb[1] = 0x01;
							if (to_remote_g2.from_mod == 'A')
								dsvt.flagb[2] = 0x03;
							else if (to_remote_g2.from_mod == 'B')
								dsvt.flagb[2] = 0x01;
							else
								dsvt.flagb[2] = 0x02;
							memcpy(&dsvt.streamid, dcs_buf+43, 2);
							dsvt.ctrl = 0x80;
							dsvt.hdr.flag[0] = dsvt.hdr.flag[1] = dsvt.hdr.flag[2] = 0x00;
							memcpy(dsvt.hdr.rpt1, owner.c_str(), CALL_SIZE);
							dsvt.hdr.rpt1[7] = to_remote_g2.from_mod;
							memcpy(dsvt.hdr.rpt2, owner.c_str(), CALL_SIZE);
							dsvt.hdr.rpt2[7] = 'G';
							memcpy(dsvt.hdr.urcall, "CQCQCQ  ", 8);
							memcpy(dsvt.hdr.mycall, dcs_buf + 31, 8);
							memcpy(dsvt.hdr.sfx, dcs_buf + 39, 4);
							calcPFCS(dsvt.title, 56);

							/* send the header to the audio mgr */
							//Link2AM.Write(dsvt.title, 56);
							is_packet = true;
						}

						if (0==memcmp(&to_remote_g2.in_streamid, dcs_buf+43, 2) && dcs_seq!=dcs_buf[45]) {
							memcpy(dsvt.title, "DSVT", 4);
							dsvt.config = 0x20;
							dsvt.flaga[0] = dsvt.flaga[1] = dsvt.flaga[2] = 0x00;
							dsvt.id = 0x20;
							dsvt.flagb[0] = 0x00;
							dsvt.flagb[1] = 0x01;
							if (to_remote_g2.from_mod == 'A')
								dsvt.flagb[2] = 0x03;
							else if (to_remote_g2.from_mod == 'B')
								dsvt.flagb[2] = 0x01;
							else
								dsvt.flagb[2] = 0x02;
							memcpy(&dsvt.streamid, dcs_buf+43, 2);
							dsvt.ctrl = dcs_buf[45];
							memcpy(dsvt.vasd.voice, dcs_buf+46, 12);

							/* send the data to the audio mgr */
							//Link2AM.Write(rdsvt.dsvt.title, 27);
							is_packet = true;

							if ((dcs_buf[45] & 0x40) != 0) {
								old_sid = 0x0;

								if (qso_details)
									printf("END   from dcs: streamID=%04x, %d bytes from IP=%s\n", ntohs(dsvt.streamid), length, fromDst4.GetAddress());

								to_remote_g2.in_streamid = 0x0;
								dcs_seq = 0xff;
							}
						}
					}
				}
			} else if (dcs_buf[0]=='E' && dcs_buf[1]=='E' && dcs_buf[2]=='E' && dcs_buf[3]=='E')
				;
			else if (length == 35)
				;
			/* is this a keepalive 22 bytes */
			else if (length == 22) {

				/* It is one of our valid repeaters */
				// DG1HT from owner 8 to 7
				if ((pCFGData->cModule==dcs_buf[17]) && 0==memcmp(dcs_buf + 9, owner.c_str(), CALL_SIZE-1)) {
					/* is that the remote system that we asked to connect to? */
					if (fromDst4==to_remote_g2.addr && to_remote_g2.addr.GetPort()==rmt_dcs_port && 0==memcmp(to_remote_g2.to_call, dcs_buf, 7) && to_remote_g2.to_mod==dcs_buf[7]) {
						if (!to_remote_g2.is_connected) {
							tracing.last_time = time(NULL);

							to_remote_g2.is_connected = true;
							printf("Connected from: %.*s\n", 8, dcs_buf);
							qnDB.UpdateLS(to_remote_g2.addr.GetAddress(), to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.from_mod, tracing.last_time);

							strcpy(linked_remote_system, to_remote_g2.to_call);
							space_p = strchr(linked_remote_system, ' ');
							if (space_p)
								*space_p = '\0';
							sprintf(notify_msg, "%c_linked.dat_LINKED_%s_%c", to_remote_g2.from_mod, linked_remote_system, to_remote_g2.to_mod);
						}
						to_remote_g2.countdown = TIMEOUT;
					}
				}
			} else if (length == 14) {	/* is this a reply to our link/unlink request: 14 bytes */

				/* It is one of our valid repeaters */
				if ((pCFGData->cModule==dcs_buf[8]) && (memcmp(dcs_buf, owner.c_str(), CALL_SIZE) == 0)) {
					/* It is from a remote that we contacted */
					if ((fromDst4==to_remote_g2.addr) && (to_remote_g2.addr.GetPort()==rmt_dcs_port) && (to_remote_g2.from_mod == dcs_buf[8])) {
						if ((to_remote_g2.to_mod == dcs_buf[9]) && (memcmp(dcs_buf + 10, "ACK", 3) == 0)) {
							to_remote_g2.countdown = TIMEOUT;
							if (!to_remote_g2.is_connected) {
								tracing.last_time = time(NULL);

								to_remote_g2.is_connected = true;
								printf("Connected from: %.*s\n", 8, to_remote_g2.to_call);
								qnDB.UpdateLS(to_remote_g2.addr.GetAddress(), to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.from_mod, tracing.last_time);

								strcpy(linked_remote_system, to_remote_g2.to_call);
								space_p = strchr(linked_remote_system, ' ');
								if (space_p)
									*space_p = '\0';
								sprintf(notify_msg, "%c_linked.dat_LINKED_%s_%c", to_remote_g2.from_mod, linked_remote_system, to_remote_g2.to_mod);
							}
						} else if (memcmp(dcs_buf + 10, "NAK", 3) == 0) {
							printf("Link module %c to [%s] %c is unlinked\n", to_remote_g2.from_mod, to_remote_g2.to_call, to_remote_g2.to_mod);

							sprintf(notify_msg, "%c_failed_link.dat_UNLINKED", to_remote_g2.from_mod);
							qnDB.DeleteLS(to_remote_g2.addr.GetAddress());
							to_remote_g2.to_call[0] = '\0';
							to_remote_g2.addr.Clear();
							to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
							to_remote_g2.countdown = 0;
							to_remote_g2.is_connected = false;
							to_remote_g2.in_streamid = 0x0;
						}
					}
				}
			}
			FD_CLR (dcs_g2_sock, &fdset);
		}

		while (keep_running && FD_ISSET(AM2Link.GetFD(), &fdset)) {
			SDSVT dsvt;
			int length = AM2Link.Read(dsvt.title, 56);
			if (0 == memcmp(dsvt.title, "LINK", 4)) {
				if (dsvt.config) {
					char target[9];
					memcpy(target, dsvt.title+4, 8);
					target[8] = '\0';
					char mod = target[7];
					target[7] = ' ';
					Link(target, mod);
				} else {
					Unlink();
				}

			} else if ((length==56 || length==27) && 0==memcmp(dsvt.title,"DSVT", 4U) && dsvt.id==0x20U && (dsvt.config==0x10U || dsvt.config==0x20U)) {

				if (length==56) {
					if (qso_details)
						printf("START from local: streamID=%04x, flags=%02x:%02x:%02x, my=%.8s/%.4s, ur=%.8s, rpt1=%.8s, rpt2=%.8s\n", ntohs(dsvt.streamid), dsvt.hdr.flag[0], dsvt.hdr.flag[1], dsvt.hdr.flag[2], dsvt.hdr.mycall, dsvt.hdr.sfx, dsvt.hdr.urcall, dsvt.hdr.rpt1, dsvt.hdr.rpt2);

					// save mycall
					memcpy(call, dsvt.hdr.mycall, 8);
					call[8] = '\0';

					if (dsvt.hdr.rpt1[7] == pCFGData->cModule) {
						tracing.streamid = dsvt.streamid;
						tracing.last_time = time(NULL);
					}

					if (pCFGData->cModule == dsvt.hdr.rpt1[7]) {
						if (to_remote_g2.is_connected) {
							if (0==memcmp(dsvt.hdr.rpt2, owner.c_str(), 7) && 0==memcmp(dsvt.hdr.urcall, "CQCQCQ", 6) && dsvt.hdr.rpt2[7] == 'G') {
								to_remote_g2.out_streamid = dsvt.streamid;

								if (to_remote_g2.addr.GetPort()==rmt_xrf_port || to_remote_g2.addr.GetPort()==rmt_ref_port) {
									SREFDSVT rdsvt;
									rdsvt.head[0] = 56U;
									rdsvt.head[1] = 0x80U;

									memcpy(rdsvt.dsvt.title, dsvt.title, 56);
									memset(rdsvt.dsvt.hdr.rpt1, ' ', CALL_SIZE);
									memcpy(rdsvt.dsvt.hdr.rpt1, to_remote_g2.to_call, strlen(to_remote_g2.to_call));
									rdsvt.dsvt.hdr.rpt1[7] = to_remote_g2.to_mod;
									memset(rdsvt.dsvt.hdr.rpt2, ' ', CALL_SIZE);
									memcpy(rdsvt.dsvt.hdr.rpt2, to_remote_g2.to_call, strlen(to_remote_g2.to_call));
									rdsvt.dsvt.hdr.rpt2[7] = 'G';
									memcpy(rdsvt.dsvt.hdr.urcall, "CQCQCQ  ", CALL_SIZE);
									calcPFCS(rdsvt.dsvt.title, 56);

									if (to_remote_g2.addr.GetPort() == rmt_xrf_port) {
										/* inform XRF about the source */
										rdsvt.dsvt.flagb[2] = to_remote_g2.from_mod;
										calcPFCS(rdsvt.dsvt.title, 56);
										for (int j=0; j<5; j++)
											sendto(xrf_g2_sock, rdsvt.dsvt.title, 56, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
									} else {
										for (int j=0; j<5; j++)
											sendto(ref_g2_sock, rdsvt.head, 58, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
									}
								} else if (to_remote_g2.addr.GetPort() == rmt_dcs_port) {
									memcpy(rptr_2_dcs.mycall, dsvt.hdr.mycall, CALL_SIZE);
									memcpy(rptr_2_dcs.sfx, dsvt.hdr.sfx, 4);
									rptr_2_dcs.dcs_rptr_seq = 0;
								}
							}
						}
					}
				}
				else { // length == 27

					if (to_remote_g2.is_connected && to_remote_g2.out_streamid==dsvt.streamid) {
						if (to_remote_g2.addr.GetPort()==rmt_xrf_port || to_remote_g2.addr.GetPort()==rmt_ref_port) {

							if (to_remote_g2.addr.GetPort() == rmt_xrf_port) {
								/* inform XRF about the source */
								dsvt.flagb[2] = to_remote_g2.from_mod;

								sendto(xrf_g2_sock, dsvt.title, 27, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
							} else if (to_remote_g2.addr.GetPort() == rmt_ref_port) {
								SREFDSVT rdsvt;
								rdsvt.head[0] = (rdsvt.dsvt.ctrl & 0x40U) ? 32U : 29U;
								rdsvt.head[1] = 0x80U;

								memcpy(rdsvt.dsvt.title, dsvt.title, 27);
								if (32U == rdsvt.head[0])
									memcpy(rdsvt.dsvt.vend.end, endbytes, 6);
								sendto(ref_g2_sock, rdsvt.head, rdsvt.head[0], 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
							}
						} else if (to_remote_g2.addr.GetPort() == rmt_dcs_port) {
							memset(dcs_buf, 0x0, 600);
							dcs_buf[0] = dcs_buf[1] = dcs_buf[2] = '0';
							dcs_buf[3] = '1';
							dcs_buf[4] = dcs_buf[5] = dcs_buf[6] = 0x0;
							memcpy(dcs_buf + 7, to_remote_g2.to_call, 8);
							dcs_buf[14] = to_remote_g2.to_mod;
							memcpy(dcs_buf + 15, owner.c_str(), CALL_SIZE);
							dcs_buf[22] = to_remote_g2.from_mod;
							memcpy(dcs_buf + 23, "CQCQCQ  ", 8);
							memcpy(dcs_buf + 31, rptr_2_dcs.mycall, 8);
							memcpy(dcs_buf + 39, rptr_2_dcs.sfx, 4);
							memcpy(dcs_buf + 43, &dsvt.streamid, 2);
							dcs_buf[45] = dsvt.ctrl;  /* cycle sequence */
							memcpy(dcs_buf + 46, dsvt.vasd.voice, 12);

							dcs_buf[58] = (rptr_2_dcs.dcs_rptr_seq >> 0)  & 0xff;
							dcs_buf[59] = (rptr_2_dcs.dcs_rptr_seq >> 8)  & 0xff;
							dcs_buf[60] = (rptr_2_dcs.dcs_rptr_seq >> 16) & 0xff;

							rptr_2_dcs.dcs_rptr_seq++;

							dcs_buf[61] = 0x01;
							dcs_buf[62] = 0x00;

							sendto(dcs_g2_sock, dcs_buf, 100, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
						}

						if (dsvt.ctrl & 0x40U) {
							to_remote_g2.out_streamid = 0x0;
						}
					}

					if (tracing.streamid == dsvt.streamid) {
						/* update the last time RF user talked */
						tracing.last_time = time(NULL);

						if (dsvt.ctrl & 0x40U) {
							if (qso_details)
								printf("END   from local: streamID=%04x\n", ntohs(dsvt.streamid));
							tracing.streamid = 0x0;
						}
					}
				}
			}
			FD_CLR (AM2Link.GetFD(), &fdset);
		}

		if (keep_running && is_packet) {
			if (0x10U == dsvt.config) {			// a header packet

				memcpy(silent.dsvt.title, dsvt.title, 14); // make a silent packet, just in case it's needed.
				silent.dsvt.config = 0x20U;
				silent.ctrl = 0U;
				const unsigned char silence[9] = { 0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U };
				memcpy(dsvt.vasd.voice, silence, 9);

				silent.ctrl = 0U;	// the expected ctrl value for the first voice packet

				Link2AM.Write(dsvt.title, 56);	// send the header
				time(&silent.lasttime);			// and start the voicestream timeout clock
			} else if (0x20U == dsvt.config) {	// a voice packet
				int diff = int(0x1FU & dsvt.ctrl) - int(silent.ctrl);
				if (diff) {	// maybe add some silent frames
					if (diff < 0)
						diff += 21;
					if (diff < 6) {
						if (log_debug)
							printf("Filling in %d silent frames...\n", diff);
						while (diff-- > 0) {
							silent.dsvt.ctrl = silent.ctrl++;
							silent.ctrl %= 21U;
							if (silent.dsvt.ctrl) {
								const unsigned char silence[3] = { 0x70U, 0x4FU, 0x93U };
								memcpy(dsvt.vasd.text, silence, 3U);
							} else {
								const unsigned char sync[3] = { 0x55U, 0x2DU, 0x16U };
								memcpy(silent.dsvt.vasd.text, sync, 12U);
							}
							Link2AM.Write(silent.dsvt.title, 27);
							time(&silent.lasttime);	// update the timecout clock
						}
					} else {
						if (log_debug)
							printf("Missing %d packets in the voice stream sequence, resetting.\n", diff);
						silent.ctrl = dsvt.ctrl;
					}
				}
				if ((silent.ctrl == (0x1FU & dsvt.ctrl)) || (0x40U & dsvt.ctrl)) {	// only send the packet if it's the correct sequence
																					// or it's the terminating packet
					if (0x40U & dsvt.ctrl) {
						dsvt.ctrl = (silent.ctrl | 0x40U);
						silent.lasttime = 0;	// stop the timeout clock
					} else {
						dsvt.ctrl = silent.ctrl;
						silent.ctrl = (silent.ctrl + 1U) % 21U;
						time(&silent.lasttime);	// update the timecout clock
					}
					Link2AM.Write(dsvt.title, 27);
				} else {
					if (log_debug)
						printf("ignorning packet with ctrl=0x%02x, should be 0x%02x\n", dsvt.ctrl, silent.ctrl);
				}
			}
		}

		if (keep_running && notify_msg[0] && 0x0U == tracing.streamid) {
			PlayAudioNotifyThread(notify_msg);
			notify_msg[0] = '\0';
		}
	}
}

void CQnetLink::PlayAudioNotifyThread(char *msg)
{
	if (! announce)
		return;
	if (strlen(msg) > 51) {
		fprintf(stderr, "Audio Message string too long: %s", msg);
		return;
	}
	SDSVT dsvt;
	memcpy(dsvt.title, "PLAY", 4);
	memcpy(dsvt.title+4, msg, strlen(msg)+1);	// copy the terminating NULL
	Link2AM.Write(dsvt.title, 56);
}

bool CQnetLink::Init(CFGDATA *pData)
{
	pCFGData = pData;
	tzset();
	setvbuf(stdout, (char *)NULL, _IOLBF, 0);


	int rc = regcomp(&preg, "^(([1-9][A-Z])|([A-PR-Z][0-9])|([A-PR-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][ ]*[ A-RT-Z]$", REG_EXTENDED | REG_NOSUB);
	if (rc != 0) {
		printf("The IRC regular expression is NOT valid\n");
		return true;
	}

	notify_msg[0] = '\0';
	to_remote_g2.to_call[0] = '\0';
	to_remote_g2.addr.Clear();
	to_remote_g2.to_mod = to_remote_g2.from_mod = ' ';
	to_remote_g2.countdown = 0;
	to_remote_g2.is_connected = false;
	to_remote_g2.in_streamid = to_remote_g2.out_streamid = 0x0;

	/* process configuration file */
	if (Configure()) {
		log.SendLog("Failed to process config data\n");
		return true;
	}

	std::string dbfile(CFG_DIR);
	dbfile.append("qn.db");
	if (qnDB.Open(dbfile.c_str())) {
		log.SendLog("qnlink failed to open the sqlite database\n");
		return true;}

	/* create our server */
	if (!srv_open()) {
		log.SendLog("qnlink srv_open() failed\n");
		return true;
	}
	return false;
}

void CQnetLink::Shutdown()
{
	char unlink_request[CALL_SIZE + 3];
	char cmd_2_dcs[19];

	/* Clear connections */
	queryCommand[0] = 5;
	queryCommand[1] = 0;
	queryCommand[2] = 24;
	queryCommand[3] = 0;
	queryCommand[4] = 0;
	if (to_remote_g2.to_call[0] != '\0') {
		if (to_remote_g2.addr.GetPort() == rmt_ref_port)
			sendto(ref_g2_sock, queryCommand, 5, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
		else if (to_remote_g2.addr.GetPort() == rmt_xrf_port) {
			strcpy(unlink_request, owner.c_str());
			unlink_request[8] = to_remote_g2.from_mod;
			unlink_request[9] = ' ';
			unlink_request[10] = '\0';
			for (int j=0; j<5; j++)
				sendto(xrf_g2_sock, unlink_request, CALL_SIZE+3, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
		} else {
			strcpy(cmd_2_dcs, owner.c_str());
			cmd_2_dcs[8] = to_remote_g2.from_mod;
			cmd_2_dcs[9] = ' ';
			cmd_2_dcs[10] = '\0';
			memcpy(cmd_2_dcs + 11, to_remote_g2.to_call, 8);

			for (int j=0; j<5; j++)
				sendto(dcs_g2_sock, cmd_2_dcs, 19, 0, to_remote_g2.addr.GetCPointer(), to_remote_g2.addr.GetSize());
		}
	}
	to_remote_g2.to_call[0] = '\0';
	to_remote_g2.addr.Clear();
	to_remote_g2.from_mod = to_remote_g2.to_mod = ' ';
	to_remote_g2.countdown = 0;
	to_remote_g2.is_connected = false;
	to_remote_g2.in_streamid = to_remote_g2.out_streamid = 0x0;

	srv_close();
}
