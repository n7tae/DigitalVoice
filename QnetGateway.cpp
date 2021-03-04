/*
 *   Copyright (C) 2010 by Scott Lawson KI4LKF
 *   Copyright (C) 2017-2020 by Thomas Early N7TAE
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
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <future>
#include <exception>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>

#include "IRCutils.h"
#include "DStarDecode.h"
#include "QnetGateway.h"

#ifndef CFG_DIR
#define CFG_DIR "/tmp/"
#endif

const std::string GW_VERSION("QnetGateway-417");

int CQnetGateway::FindIndex() const
{
    int index = Index;
    if (index < 0) {
        if (AF_INET == link_family) {
            index = ii[1] ? 1 : 0;
        } else if (AF_INET6 == link_family) {
            index = 0;
        }
    }
    return index;
}

bool CQnetGateway::VoicePacketIsSync(const unsigned char *text)
{
	return *text==0x55U && *(text+1)==0x2DU && *(text+2)==0x16U;
}

void CQnetGateway::UnpackCallsigns(const std::string &str, std::set<std::string> &set, const std::string &delimiters)
{
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);	// Skip delimiters at beginning.
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);	// Find first non-delimiter.

	while (std::string::npos != pos || std::string::npos != lastPos) {
		std::string element = str.substr(lastPos, pos-lastPos);
		if (element.length()>=3 && element.length()<=8) {
			ToUpper(element);
			element.resize(CALL_SIZE, ' ');
			set.insert(element);	// Found a token, add it to the list.
		} else
			fprintf(stderr, "found bad callsign in list: %s\n", str.c_str());
		lastPos = str.find_first_not_of(delimiters, pos);	// Skip delimiters.
		pos = str.find_first_of(delimiters, lastPos);	// Find next non-delimiter.
	}
}

void CQnetGateway::PrintCallsigns(const std::string &key, const std::set<std::string> &set)
{
	log.SendLog("%s = [", key.c_str());
	for (auto it=set.begin(); it!=set.end(); it++) {
		if (it != set.begin())
			log.SendLog(",");
		log.SendLog("%s", (*it).c_str());
	}
	log.SendLog("]\n");
}


void CQnetGateway::set_dest_rptr(std::string &call)
{
	std::list<CLink> linklist;
	if (qnDB.FindLS(pCFGData->cModule, linklist))
		return;

	auto count = linklist.size();
	if (count != 1)
		printf("set_dest_rptr() returned %d link sets\n", int(count));
	if (0 == count)
		return;

	call.assign(linklist.front().callsign);
}

/* compute checksum */
void CQnetGateway::calcPFCS(unsigned char *packet, int len)
{
	const unsigned short crc_tabccitt[256] = {
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
	short int low, high;
	unsigned short tmp;

	switch (len) {
		case 56:
			low = 15;
			high = 54;
			break;
		case 58:
			low = 17;
			high = 56;
			break;
		default:
			return;
	}

	for (unsigned short int i = low; i < high ; i++) {
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

/* process configuration file */
bool CQnetGateway::Configure()
{
	// ircddb
	owner.assign(pCFGData->sStation);
	OWNER = owner;
	ToLower(owner);
	ToUpper(OWNER);
	OWNER.resize(CALL_SIZE, ' ');

	switch (pCFGData->eNetType) {
		case EQuadNetType::ipv4only:
			ircddb[0].ip.assign("ircv4.openquad.net");
			ircddb[0].port = 9007U;
			IRCDDB_PASSWORD[0].clear();
			ircddb[1].ip.clear();
			ircddb[1].port = 0U;
			IRCDDB_PASSWORD[1].clear();
			break;
		case EQuadNetType::ipv6only:
			ircddb[0].ip.assign("ircv6.openquad.net");
			ircddb[0].port = 9007U;
			IRCDDB_PASSWORD[0].clear();
			ircddb[1].ip.clear();
			ircddb[1].port = 0U;
			IRCDDB_PASSWORD[1].clear();
			break;
		case EQuadNetType::dualstack:
			ircddb[0].ip.assign("ircv6.openquad.net");
			ircddb[0].port = 9007U;
			IRCDDB_PASSWORD[0].clear();
			ircddb[1].ip.assign("ircv4.openquad.net");
			ircddb[1].port = 9007U;
			IRCDDB_PASSWORD[1].clear();
			break;
		default:
			ircddb[0].ip.clear();
			ircddb[0].port = 0U;
			IRCDDB_PASSWORD[0].clear();
			ircddb[1].ip.clear();
			ircddb[1].port = 0U;
			IRCDDB_PASSWORD[1].clear();
			break;
	}

	if (ircddb[0].ip.empty()) {
		fprintf(stderr, "IRC networks must be defined\n");
		return true;
	}

	// module
	Rptr.mod.package_version.assign(GW_VERSION+".DigVoice");
	Rptr.mod.frequency = Rptr.mod.offset = 0.0;
	Rptr.mod.range = Rptr.mod.agl = 0.0;
	Rptr.mod.desc1.assign(pCFGData->sLocation[0]);
	Rptr.mod.desc2.append(pCFGData->sLocation[1]);
	Rptr.mod.url.assign(pCFGData->sURL);

	// gateway
	g2_external.ip.assign("ANY_PORT");
	g2_external.port = 40000U;
	g2_ipv6_external.ip.assign("ANY_PORT");
	g2_ipv6_external.port = 9011U;
	GATEWAY_HEADER_REGEN = true;
	GATEWAY_SEND_QRGS_MAP = false;
	Rptr.mod.latitude = pCFGData->dLatitude;
	Rptr.mod.longitude = pCFGData->dLongitude;
	//std::string csv;
	//UnpackCallsigns(csv, findRoute);
	//PrintCallsigns("findRoutes", findRoute);

	// APRS
	Rptr.aprs.ip.assign(pCFGData->sAPRSServer);
	Rptr.aprs.port = pCFGData->usAPRSPort;
	Rptr.aprs_interval = pCFGData->iAPRSInterval;
	Rptr.aprs_filter.clear();

	// log
	LOG_QSO = true;
	LOG_IRC = false;
	LOG_DEBUG = false;

	// file
	std::string path(CFG_DIR);
	path.append("routes.cfg");
	std::ifstream file;
	file.open(path.c_str(), std::ifstream::in);
	if (file.is_open()) {
		char line[128];
		while (file.getline(line, 128)) {
			std::string call(line);
			call.resize(8, ' ');
			findRoute.insert(call.c_str());
		}
		file.close();
	}

	// timing
	TIMING_PLAY_WAIT = 1;
	TIMING_PLAY_DELAY = 19;
	TIMING_TIMEOUT_REMOTE_G2 = 2;
	TIMING_TIMEOUT_LOCAL_RPTR = 1;

	return false;
}

// Create ports
int CQnetGateway::open_port(const SPORTIP *pip, int family)
{
	CSockAddress sin(family, pip->port, pip->ip.c_str());

	int sock = socket(family, SOCK_DGRAM, 0);
	if (0 > sock) {
		log.SendLog("Failed to create socket on %s:%d, errno=%d, %s\n", pip->ip.c_str(), pip->port, errno, strerror(errno));
		return -1;
	}
	fcntl(sock, F_SETFL, O_NONBLOCK);

	if (bind(sock, sin.GetPointer(), sizeof(struct sockaddr_storage)) != 0) {
		log.SendLog("Failed to bind %s:%d, errno=%d, %s\n", pip->ip.c_str(), pip->port, errno, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

/* receive data from the irc server and save it */
void CQnetGateway::GetIRCDataThread(const int i)
{
	std::string user, rptr, gateway, ipaddr;
	IRCDDB_RESPONSE_TYPE type;
	short last_status = 0;

	short threshold = 0;
	bool not_announced = true;
	bool is_quadnet = (std::string::npos != ircddb[i].ip.find(".openquad.net"));
	bool doFind = true;
	while (keep_running) {
		int rc = ii[i]->getConnectionState();
		if (rc > 5 && rc < 8 && is_quadnet) {
			char ch = '\0';
			if (not_announced)
				ch = pCFGData->cModule;
			if (ch) {
				// we need to announce quadnet
				char str[56];
				memset(str, 0, 56);
				snprintf(str, 56, "PLAY%c_connected2network.dat_WELCOME_TO_QUADNET", ch);
				Gate2AM.Write(str, strlen(str)+1);
				not_announced = false;
			}
			if (doFind) {
				log.SendLog("Finding Routes for...\n");
				for (auto it=findRoute.begin(); it!=findRoute.end(); it++) {
					std::this_thread::sleep_for(std::chrono::milliseconds(800));
					log.SendLog("\t'%s'\n", it->c_str());
					ii[i]->findUser(*it);
				}
				doFind = false;
			}
		}
		threshold++;
		if (threshold >= 100) {
			if ((rc == 0) || (rc == 10)) {
				if (last_status != 0) {
					log.SendLog("irc status=%d, probable disconnect...\n", rc);
					last_status = 0;
				}
			} else if (rc == 7) {
				if (last_status != 2) {
					log.SendLog("irc status=%d, probable connect...\n", rc);
					last_status = 2;
				}
			} else {
				if (last_status != 1) {
					log.SendLog("irc status=%d, probable connect...\n", rc);
					last_status = 1;
				}
			}
			threshold = 0;
		}

		while (((type = ii[i]->getMessageType()) != IDRT_NONE) && keep_running) {
			switch (type) {
			case IDRT_PING: {
					std::string rptr, gate, addr;
					ii[i]->receivePing(rptr);
					if (! rptr.empty()) {
						ReplaceChar(rptr, '_', ' ');
						ii[i]->cache.findRptrData(rptr, gate, addr);
						if (addr.empty())
							break;
						CSockAddress to;
						if (addr.npos == rptr.find(':'))
							to.Initialize(AF_INET, (unsigned short)g2_external.port, addr.c_str());
						else
							to.Initialize(AF_INET6, (unsigned short)g2_ipv6_external.port, addr.c_str());
						sendto(g2_sock[i], "PONG", 4, 0, to.GetCPointer(), to.GetSize());
						if (LOG_QSO)
							printf("Sent 'PONG' to %s\n", addr.c_str());
					}
				}
				break;
			default:
				break;
			}	// switch (type)
		}	// while (keep_running)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	log.SendLog("GetIRCDataThread[%i] exiting...\n", i);
	return;
}

/* return codes: 0=OK(found it), 1=TRY AGAIN, 2=FAILED(bad data) */
int CQnetGateway::get_yrcall_rptr_from_cache(const int i, const std::string &call, std::string &rptr, std::string &gate, std::string &addr, char RoU)
{
	switch (RoU) {
		case 'U':
			ii[i]->cache.findUserData(call, rptr, gate, addr);
			if (rptr.empty()) {
				printf("Could not find last heard repeater for user '%s'\n", call.c_str());
				return 1;
			}
			break;
		case 'R':
			rptr.assign(call);
			ii[i]->cache.findRptrData(call, gate, addr);
			break;
		default:
			fprintf(stderr, "ERROR: Invalid Rou of '%c'\n", RoU);
			return 2;
	}
	std::string temp;

	if (rptr.at(7) == 'G') {
		fprintf(stderr, "ERROR: Invalid module %c\n", rptr.at(7));
		return 2;
	}

	if (addr.empty()) {
		printf("Couldn't find IP address for %s\n", ('R' == RoU) ? "repeater" : "user");
		return 1;
	}
	return 0;
}

int CQnetGateway::get_yrcall_rptr(const std::string &call, std::string &rptr, std::string &gate, std::string &addr, char RoU)
// returns 0 if unsuccessful, otherwise returns ii index plus one
{
	int rval[2] = { 1, 1 };
	for (int i=0; i<2; i++) {
		if (ii[i]) {
			rval[i] = get_yrcall_rptr_from_cache(i, call, rptr, gate, addr, RoU);
			if (0 == rval[i])
				return i + 1;
		}
	}

	/* at this point, the data is not in cache */
	for (int i=0; i<2; i++) {
		if (ii[i] && (1 == rval[i])) {
			if (ii[i]->getConnectionState() > 5) {
				// we can try a find
				if (RoU == 'U') {
					printf("User [%s] not in local cache, try again\n", call.c_str());
					/*** YRCALL=KJ4NHFBL ***/
					if (((call.at(6) == 'A') || (call.at(6) == 'B') || (call.at(6) == 'C')) && (call.at(7) == 'L'))
						printf("If this was a gateway link request, that is ok\n");
					if (!ii[i]->findUser(call))
						printf("findUser(%s): Network error\n", call.c_str());
				} else if (RoU == 'R') {
					printf("Repeater [%s] not found\n", call.c_str());
				}
			}
		}
	}
	return 0;
}

bool CQnetGateway::Flag_is_ok(unsigned char flag)
{
	//      normal          break          emr          emr+break
	return 0x00U==flag || 0x08U==flag || 0x20U==flag || 0x28U==flag;
}

void CQnetGateway::ProcessTimeouts()
{
	{
		time_t t_now;
		// any stream going to local repeater timed out?
		if (toRptr.last_time != 0) {
			time(&t_now);
			//   The stream can be from a cross-band, or from a remote system,
			//   so we could use either FROM_LOCAL_RPTR_TIMEOUT or FROM_REMOTE_G2_TIMEOUT
			//   but FROM_REMOTE_G2_TIMEOUT makes more sense, probably is a bigger number
			if ((t_now - toRptr.last_time) > TIMING_TIMEOUT_REMOTE_G2) {
				log.SendLog("Inactivity to local rptr module %c, removing stream id %04x\n", pCFGData->cModule, ntohs(toRptr.streamid));

				// Send end_of_audio to local repeater.
				// Let the repeater re-initialize
				end_of_audio.streamid = toRptr.streamid;
				end_of_audio.ctrl = toRptr.sequence | 0x40;
				if (toRptr.sequence) {
					const unsigned char silence[3] = { 0x70U, 0x4FU, 0x93U };
					memcpy(end_of_audio.vasd.text, silence, 3U);
				} else {
					const unsigned char sync[3] = { 0x55U, 0x2DU, 0x16U };
					memcpy(end_of_audio.vasd.text, sync, 12U);
				}

				Gate2AM.Write(end_of_audio.title, 27);


				toRptr.streamid = 0;
				toRptr.addr.ClearAddress();
				toRptr.last_time = 0;
			}
		}

		/* any stream coming from local repeater timed out ? */
		if (band_txt.last_time != 0) {
			time(&t_now);
			if ((t_now - band_txt.last_time) > TIMING_TIMEOUT_LOCAL_RPTR) {
				/* This local stream never went to a remote system, so trace the timeout */
				if (to_remote_g2.toDstar.AddressIsZero())
					log.SendLog("Inactivity from local rptr module %c, removing stream id %04x\n", pCFGData->cModule, ntohs(band_txt.streamID));

				band_txt.streamID = 0;
				band_txt.flags[0] = band_txt.flags[1] = band_txt.flags[2] = 0x0;
				band_txt.lh_mycall[0] = '\0';
				band_txt.lh_sfx[0] = '\0';
				band_txt.lh_yrcall[0] = '\0';
				band_txt.lh_rpt1[0] = '\0';
				band_txt.lh_rpt2[0] = '\0';

				band_txt.last_time = 0;

				band_txt.txt[0] = '\0';
				band_txt.txt_cnt = 0;

				band_txt.dest_rptr[0] = '\0';

				band_txt.num_dv_frames = 0;
				band_txt.num_dv_silent_frames = 0;
				band_txt.num_bit_errors = 0;
			}
		}

		/* any stream from local repeater to a remote gateway timed out ? */
		if (! to_remote_g2.toDstar.AddressIsZero()) {
			time(&t_now);
			if ((t_now - to_remote_g2.last_time) > TIMING_TIMEOUT_LOCAL_RPTR) {
				log.SendLog("Inactivity from local rptr mod %c, removing stream id %04x\n", pCFGData->cModule, ntohs(to_remote_g2.streamid));

				to_remote_g2.toDstar.Clear();
				to_remote_g2.streamid = 0;
				to_remote_g2.last_time = 0;
			}
		}
	}
}

bool CQnetGateway::ProcessG2Msg(const unsigned char *data, std::string &smrtgrp)
{
	static unsigned int part = 0;
	static char txt[21];
	if ((data[0] != 0x55u) || (data[1] != 0x2du) || (data[2] != 0x16u)) {
		const unsigned char c[3] = {
			static_cast<unsigned char>(data[0] ^ 0x70u),
			static_cast<unsigned char>(data[1] ^ 0x4fu),
			static_cast<unsigned char>(data[2] ^ 0x93u)
		};	// unscramble
		if (part) {
			// we are in a message
			if (part % 2) {
				// this is the second part of the 2-frame pair
				memcpy(txt+(5u*(part/2u)+2u), c, 3);
				if (++part > 7) {
					// we've got everything!
					part = 0;	// now we can start over
					if (0 == strncmp(txt, "VIA SMARTGP ", 12))
						smrtgrp.assign(txt+12);
					if (smrtgrp.size() < 8) {
						// something bad happened
						smrtgrp.empty();
						return false;
					}
					return true;
				}
			} else {	// we'll get here when part[mod] = 2, 4 or 6
				unsigned int sequence = part++ / 2;	// this is the sequency we are expecting, 1, 2 or 3
				if ((sequence | 0x40u) == c[0]) {
					memcpy(txt+(5u*sequence), c+1, 2);	// got it, copy the 2 remainin chars
				} else {
					part = 0;	// unexpected
				}
			}
		} else if (0x40u == c[0]) {
			// start a new message
			memcpy(txt, c+1, 2);
			memset(txt+2, 0, 19);
			part = 1;
		}
	} else {
		part = 0;	// messages will never be spread across a superframe
	}
	return false;
}

void CQnetGateway::ProcessG2(const ssize_t g2buflen, CDSVT &g2buf)
{
	static std::string lhcallsign, lhsfx;
	static bool csroute = false;
	static unsigned char nextctrl = 0U;
	static std::string superframe;
	if ( (g2buflen==56 || g2buflen==27) && 0==memcmp(g2buf.title, "DSVT", 4) && (g2buf.config==0x10 || g2buf.config==0x20) && g2buf.id==0x20) {
		if (g2buflen == 56) {
			/* valid repeater module? */
			if (g2buf.hdr.rpt1[7]==pCFGData->cModule || true) {
				// toRptr is active if a remote system is talking to it or
				// toRptr is receiving data from a cross-band
				if (0==toRptr.last_time && 0==band_txt.last_time && (Flag_is_ok(g2buf.hdr.flag[0]) || 0x01U==g2buf.hdr.flag[0] || 0x40U==g2buf.hdr.flag[0])) {
					superframe.clear();
					if (LOG_QSO) {
						log.SendLog("id=%04x flags=%02x:%02x:%02x ur=%.8s r1=%.8s r2=%.8s my=%.8s/%.4s IP=[%s]:%u\n", ntohs(g2buf.streamid), g2buf.hdr.flag[0], g2buf.hdr.flag[1], g2buf.hdr.flag[2], g2buf.hdr.urcall, g2buf.hdr.rpt1, g2buf.hdr.rpt2, g2buf.hdr.mycall, g2buf.hdr.sfx, fromDstar.GetAddress(), fromDstar.GetPort());
					}

					lhcallsign.assign((const char *)g2buf.hdr.mycall, 8);
					if (memcmp(g2buf.hdr.sfx, "RPTR", 4) && std::regex_match(lhcallsign.c_str(), preg)) {
						lhsfx.assign((const char *)g2buf.hdr.sfx, 4);
						std::string  reflector((const char *)g2buf.hdr.urcall, 8);
						if (0 == reflector.compare("CQCQCQ  "))
							set_dest_rptr(reflector);
						else if (0 == reflector.compare(OWNER))
							reflector.assign("CSRoute");
						qnDB.UpdateLH(lhcallsign.c_str(), lhsfx.c_str(), pCFGData->cModule, reflector.c_str());
					}

					Gate2AM.Write(g2buf.title, 56);
					nextctrl = 0U;

					// save the header
					memcpy(toRptr.saved_hdr.title, g2buf.title, 56);
					toRptr.saved_addr = fromDstar;

					/* This is the active streamid */
					toRptr.streamid = g2buf.streamid;
					toRptr.addr = fromDstar;;

					/* time it, in case stream times out */
					time(&toRptr.last_time);

					toRptr.sequence = g2buf.ctrl;
					csroute = (0 == memcmp(g2buf.hdr.urcall, pCFGData->sCallsign.c_str(), pCFGData->sCallsign.size()));
				}
			}
		} else {	// g2buflen == 27
			/* find out which repeater module to send the data to */
			/* streamid match ? */
			if (toRptr.streamid == g2buf.streamid && toRptr.addr == fromDstar) {
				if (LOG_DEBUG) {
					const unsigned int ctrl = g2buf.ctrl & 0x1FU;
					if (VoicePacketIsSync(g2buf.vasd.text)) {
						if (superframe.size() > 65U) {
							log.SendLog("Frame[%c]: %s\n", pCFGData->cModule, superframe.c_str());
							superframe.clear();
						}
						const char *ch = "#abcdefghijklmnopqrstuvwxyz";
						superframe.append(1, (ctrl<27U) ? ch[ctrl] : '%' );
					} else {
						const char *ch = "!ABCDEFGHIJKLMNOPQRSTUVWXYZ";
						superframe.append(1, (ctrl<27U) ? ch[ctrl] : '*' );
					}
				}

				int diff = int(0x1FU & g2buf.ctrl) - int(nextctrl);
				if (diff) {
					if (diff < 0)
						diff += 21;
					if (diff < 6) {	// fill up to 5 missing voice frames
						if (LOG_DEBUG)
							printf("Inserting %d missing voice frame(s)\n", diff - 1);
						CDSVT dsvt;
						memcpy(dsvt.title, g2buf.title, 14U);	// everything but the ctrl and voice data
						const unsigned char silence[9] = { 0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U };
						memcpy(dsvt.vasd.voice, silence, 9U);
						while (diff-- > 0) {
							dsvt.ctrl = nextctrl++;
							nextctrl %= 21U;
							if (dsvt.ctrl) {
								const unsigned char text[3] = { 0x70U, 0x4FU, 0x93U };
								memcpy(dsvt.vasd.text, text, 3U);
							} else {
								const unsigned char sync[3] = { 0x55U, 0x2DU, 0x16U };
								memcpy(dsvt.vasd.text, sync, 3U);
							}
							Gate2AM.Write(dsvt.title, 27);
						}
					} else {
					    if (LOG_DEBUG) {
        					printf("Missing %d packets from the voice stream, resetting\n", diff);
						}
						nextctrl = g2buf.ctrl;
					}
				}

				if ((nextctrl == (0x1FU & g2buf.ctrl)) || (0x40U & g2buf.ctrl)) {
					// no matter what, we will send this on if it is the closing frame
					if (0x40U & g2buf.ctrl) {
						g2buf.ctrl = (nextctrl | 0x40U);
					} else {
						g2buf.ctrl = nextctrl;
						nextctrl = (nextctrl + 1U) % 21U;
					}
					Gate2AM.Write(g2buf.title, 27);
					std::string smartgroup;
					if (ProcessG2Msg(g2buf.vasd.text, smartgroup))
						qnDB.UpdateLH(lhcallsign.c_str(), lhsfx.c_str(), pCFGData->cModule, smartgroup.c_str());
				} else {
					if (LOG_DEBUG)
						fprintf(stderr, "Ignoring packet because its ctrl=0x%02xU and nextctrl=0x%02xU\n", g2buf.ctrl, nextctrl);
				}

				/* timeit */
				time(&toRptr.last_time);

				toRptr.sequence = g2buf.ctrl;

				/* End of stream ? */
				if (g2buf.ctrl & 0x40U) {
					/* clear the saved header */
					memset(toRptr.saved_hdr.title, 0U, 56U);
					toRptr.saved_addr.ClearAddress();

					toRptr.last_time = 0;
					toRptr.streamid = 0;
					toRptr.addr.ClearAddress();
					if (LOG_DEBUG && superframe.size()) {
						log.SendLog("Final[%c]: %s\n", pCFGData->cModule, superframe.c_str());
						superframe.clear();
					}
					if (LOG_QSO)
						log.SendLog("id=%04x END\n", ntohs(g2buf.streamid));

					if (csroute) {
						char play[56];
						snprintf(play, 56, "PLAY%c_ringing.dat_CALLSIGN_ROUTE", pCFGData->cModule);
						Gate2AM.Write(play, strlen(play)+1);
						csroute = false;
					}
				}
			}

			/* no match ? */
			if (GATEWAY_HEADER_REGEN) {
				/* check if this a continuation of audio that timed out */

				if (0U == (g2buf.ctrl & 0x40)) {	// only do this if it's not the last voice packet
					/* for which repeater this stream has timed out ?  */
					if (toRptr.saved_hdr.streamid == g2buf.streamid && toRptr.saved_addr == fromDstar) {
						/* repeater module is inactive ?  */
						if (toRptr.last_time==0 && band_txt.last_time==0) {
							log.SendLog("Re-generating header for streamID=%04x\n", ntohs(g2buf.streamid));

							/* re-generate/send the header */
							Gate2AM.Write(toRptr.saved_hdr.title, 56);

							/* send this audio packet to repeater */
							Gate2AM.Write(g2buf.title, 27);

							/* make sure that any more audio arriving will be accepted */
							toRptr.streamid = g2buf.streamid;
							toRptr.addr = fromDstar;

							/* time it, in case stream times out */
							time(&toRptr.last_time);

							toRptr.sequence = g2buf.ctrl;

						}
					}
				}
			}
		}
	}
}

void CQnetGateway::ProcessAudio(const CDSVT *packet)
{
	CDSVT dsvt;
	memcpy(dsvt.title, packet, (packet->config==0x10U) ? 56 : 27);

	if (0 == memcmp(dsvt.title, "DSVT", 4)) {
		if (dsvt.id==0x20U && (dsvt.config==0x10U || dsvt.config==0x20U) ) {
			if (dsvt.config==0x10U) {
				if (LOG_QSO)
					log.SendLog("id=%04x start RPTR flag0=%02x ur=%.8s r1=%.8s r2=%.8s my=%.8s/%.4s\n", ntohs(dsvt.streamid), dsvt.hdr.flag[0],  dsvt.hdr.urcall, dsvt.hdr.rpt1, dsvt.hdr.rpt2, dsvt.hdr.mycall, dsvt.hdr.sfx);

				if (0==memcmp(dsvt.hdr.rpt1, OWNER.c_str(), 7) && Flag_is_ok(dsvt.hdr.flag[0])) {

					if (dsvt.hdr.rpt1[7] == pCFGData->cModule) {
                        vPacketCount = 0;
                        Index = -1;

						/* Initialize the LAST HEARD data for the band */

						band_txt.streamID = dsvt.streamid;

						memcpy(band_txt.flags, dsvt.hdr.flag, 3);

						memcpy(band_txt.lh_mycall, dsvt.hdr.mycall, 8);
						band_txt.lh_mycall[8] = '\0';

						memcpy(band_txt.lh_sfx, dsvt.hdr.sfx, 4);
						band_txt.lh_sfx[4] = '\0';

						memcpy(band_txt.lh_yrcall, dsvt.hdr.urcall, 8);
						band_txt.lh_yrcall[8] = '\0';

						memcpy(band_txt.lh_rpt1, dsvt.hdr.rpt1, 8);
						band_txt.lh_rpt1[8] = '\0';

						memcpy(band_txt.lh_rpt2, dsvt.hdr.rpt2, 8);
						band_txt.lh_rpt2[8] = '\0';

						time(&band_txt.last_time);

						band_txt.txt[0] = '\0';
						band_txt.txt_cnt = 0;
						band_txt.sent_key_on_msg = false;

						band_txt.dest_rptr[0] = '\0';

						/* try to process GPS mode: GPRMC and ID */
						band_txt.temp_line[0] = '\0';
						band_txt.temp_line_cnt = 0;
						band_txt.gprmc[0] = '\0';
						band_txt.gpid[0] = '\0';
						band_txt.gps_last_time = 0;

						band_txt.num_dv_frames = 0;
						band_txt.num_dv_silent_frames = 0;
						band_txt.num_bit_errors = 0;
					}
				}

				/* Is MYCALL valid ? */
				std::string call((const char *)dsvt.hdr.mycall, 8);

				bool mycall_valid = std::regex_match(call, preg);

				if (! mycall_valid)
					fprintf(stderr, "MYCALL [%s] failed IRC expression validation\n", call.c_str());

				if ( mycall_valid &&
						memcmp(dsvt.hdr.urcall, "XRF", 3) &&	// not a reflector
						memcmp(dsvt.hdr.urcall, "REF", 3) &&
						memcmp(dsvt.hdr.urcall, "DCS", 3) &&
						memcmp(dsvt.hdr.urcall, "XLX", 3) &&
						dsvt.hdr.urcall[0]!=' ' && 				// must have something
						memcmp(dsvt.hdr.urcall, "CQCQCQ", 6) )	// urcall is NOT CQCQCQ
				{
					std::string user, rptr, gate, addr;
					if ( dsvt.hdr.urcall[0]=='/' &&							// repeater routing!
							0==memcmp(dsvt.hdr.rpt1, OWNER.c_str(), 7) &&	// rpt1 this repeater
							(dsvt.hdr.rpt1[7]==pCFGData->cModule) &&					// with a valid module
							0==memcmp(dsvt.hdr.rpt2, OWNER.c_str(), 7) && 	// rpt2 is this repeater
							dsvt.hdr.rpt2[7]=='G' &&						// local Gateway
							Flag_is_ok(dsvt.hdr.flag[0]) )
					{
						if (memcmp(dsvt.hdr.urcall+1, OWNER.c_str(), 6)) {	// the value after the slash is NOT this repeater
							if (dsvt.hdr.rpt1[7] == pCFGData->cModule) {
								/* one radio user on a repeater module at a time */
								if (to_remote_g2.toDstar.AddressIsZero()) {
									/* YRCALL=/repeater + mod */
									/* YRCALL=/KJ4NHFB */

									user.assign((char *)dsvt.hdr.urcall, 1, 6);
									user.append(" ");
									user.append(dsvt.hdr.urcall[7], 1);
									if (isspace(user.at(7)))
										user[7] = 'A';

									Index = get_yrcall_rptr(user, rptr, gate, addr, 'R');
									if (Index--) { /* it is a repeater */
										//std::string from = OWNER.substr(0, 7);
										//from.append(1, pCFGData->cModule);
										//ii[Index]->sendPing(user, from);
										to_remote_g2.streamid = dsvt.streamid;
										if (addr.npos == addr.find(':') && af_family[Index] == AF_INET6)
											fprintf(stderr, "ERROR: IP returned from cache is IPV4 but family is AF_INET6!\n");
										to_remote_g2.toDstar.Initialize(af_family[Index], (uint16_t)((af_family[Index]==AF_INET6) ? g2_ipv6_external.port : g2_external.port), addr.c_str());

										/* set rpt1 */
										memset(dsvt.hdr.rpt1, ' ', 8);
										memcpy(dsvt.hdr.rpt1, rptr.c_str(), 8);
										/* set rpt2 */
										memset(dsvt.hdr.rpt2, ' ', 8);
										memcpy(dsvt.hdr.rpt2, gate.c_str(), 8);
										/* set yrcall, can NOT let it be slash and repeater + module */
										memcpy(dsvt.hdr.urcall, "CQCQCQ  ", 8);

										/* set PFCS */
										calcPFCS(dsvt.title, 56);

										// The remote repeater has been set, lets fill in the dest_rptr
										// so that later we can send that to the LIVE web site
										band_txt.dest_rptr.assign((const char *)dsvt.hdr.rpt1, 8);
										band_txt.dest_rptr[CALL_SIZE] = '\0';

										// send to remote gateway
										for (int j=0; j<5; j++)
											sendto(g2_sock[Index], dsvt.title, 56, 0, to_remote_g2.toDstar.GetPointer(), to_remote_g2.toDstar.GetSize());

										log.SendLog("id=%04x zone route to [%s]:%u ur=%.8s r1=%.8s r2=%.8s my=%.8s/%.4s\n",
										ntohs(dsvt.streamid), to_remote_g2.toDstar.GetAddress(), to_remote_g2.toDstar.GetPort(),
										dsvt.hdr.urcall, dsvt.hdr.rpt1, dsvt.hdr.rpt2, dsvt.hdr.mycall, dsvt.hdr.sfx);

										time(&(to_remote_g2.last_time));
									}
								}
							}
						}
					}
					else if (memcmp(dsvt.hdr.urcall, OWNER.c_str(), 7) &&		// urcall is not this repeater
							0==memcmp(dsvt.hdr.rpt1, OWNER.c_str(), 7) &&		// rpt1 is this repeater
							(dsvt.hdr.rpt1[7]==pCFGData->cModule) &&						// mod is proper
							0==memcmp(dsvt.hdr.rpt2, OWNER.c_str(), 7) &&		// rpt2 is this repeater
							dsvt.hdr.rpt2[7]=='G' &&							// local Gateway
							Flag_is_ok(dsvt.hdr.flag[0])) {

						user.assign((const char *)dsvt.hdr.urcall, 8);

                        if (dsvt.hdr.rpt1[7] == pCFGData->cModule) {
						    Index = get_yrcall_rptr(user, rptr, gate, addr, 'U');
                            if (Index--) {
                                /* destination is a remote system */
                                if (0 != gate.compare(0, 7, OWNER, 0, 7)) {

									/* one radio user on a repeater module at a time */
									if (to_remote_g2.toDstar.AddressIsZero()) {
										if (std::regex_match(user, preg)) {
											// don't send a ping to a routing group
											std::string from = OWNER.substr(0, 7);
											from.append(1, pCFGData->cModule);
											ii[Index]->sendPing(gate, from);
										}
										/* set the destination */
										to_remote_g2.streamid = dsvt.streamid;
										if (addr.npos == addr.find(':') && af_family[Index] == AF_INET6)
											fprintf(stderr, "ERROR: IP returned from cache is IPV4 but family is AF_INET6!\n");
										to_remote_g2.toDstar.Initialize(af_family[Index], (uint16_t)((af_family[Index]==AF_INET6) ? g2_ipv6_external.port : g2_external.port), addr.c_str());

										/* set rpt1 */
										memcpy(dsvt.hdr.rpt1, rptr.c_str(), 8);
										/* set rpt2 */
										memcpy(dsvt.hdr.rpt2, gate.c_str(), 8);
										/* set PFCS */
										calcPFCS(dsvt.title, 56);

										// The remote repeater has been set, lets fill in the dest_rptr
										// so that later we can send that to the LIVE web site
										band_txt.dest_rptr.assign((const char *)dsvt.hdr.rpt1, 8);

										/* send to remote gateway */
										for (int j=0; j<5; j++)
											sendto(g2_sock[Index], dsvt.title, 56, 0, to_remote_g2.toDstar.GetPointer(), to_remote_g2.toDstar.GetSize());

										//printf("Callsign route to [%s]:%u id=%04x my=%.8s/%.4s ur=%.8s rpt1=%.8s rpt2=%.8s\n", to_remote_g2.toDstar.GetAddress(), to_remote_g2.toDstar.GetPort(), ntohs(dsvt.streamid), dsvt.hdr.mycall, dsvt.hdr.sfx, dsvt.hdr.urcall, dsvt.hdr.rpt1, dsvt.hdr.rpt2);

										time(&(to_remote_g2.last_time));
									}
								}
							}
						    else
						    {
							    if ('L' != dsvt.hdr.urcall[7]) // as long as this doesn't look like a linking command
								    playNotInCache = true; // we need to wait until user's transmission is over
                            }
						}
					}
				}
			}
			else
			{	// recvlen is 27
				{
					if (band_txt.streamID == dsvt.streamid) {
						time(&band_txt.last_time);

						if (dsvt.ctrl & 0x40) {	// end of voice data
							if (! band_txt.sent_key_on_msg) {
								band_txt.txt[0] = '\0';
								if (memcmp(band_txt.lh_yrcall, "CQCQCQ", 6) == 0) {
									set_dest_rptr(band_txt.dest_rptr);
								}
                                int x = FindIndex();
								if (x >= 0)
									ii[x]->sendHeardWithTXMsg(band_txt.lh_mycall, band_txt.lh_sfx, band_txt.lh_yrcall, band_txt.lh_rpt1, band_txt.lh_rpt2, band_txt.flags[0], band_txt.flags[1], band_txt.flags[2], band_txt.dest_rptr, band_txt.txt);
								band_txt.sent_key_on_msg = true;
							}
							// send the "key off" message, this will end up in the openquad.net Last Heard webpage.
                            int index = Index;
                            if (index < 0) {
                                if (AF_INET == link_family) {
                                    index = ii[1] ? 1 : 0;
                                } else if (AF_INET6 == link_family) {
                                    index = 0;
                                }
                            }
							if (index >= 0)
								ii[index]->sendHeardWithTXStats(band_txt.lh_mycall, band_txt.lh_sfx, band_txt.lh_yrcall, band_txt.lh_rpt1, band_txt.lh_rpt2, band_txt.flags[0], band_txt.flags[1], band_txt.flags[2], band_txt.num_dv_frames, band_txt.num_dv_silent_frames, band_txt.num_bit_errors);

							if (playNotInCache) {
								// Not in cache, please try again!
								char str[56];
								memset(str, 0, 56);
								snprintf(str, 56, "PLAY%c_notincache.dat_NOT_IN_CACHE", band_txt.lh_rpt1[7]);
								Gate2AM.Write(str, strlen(str)+1);
								playNotInCache = false;
							}

							band_txt.streamID = 0;
							band_txt.flags[0] = band_txt.flags[1] = band_txt.flags[2] = 0;
							band_txt.lh_mycall[0] = '\0';
							band_txt.lh_sfx[0] = '\0';
							band_txt.lh_yrcall[0] = '\0';
							band_txt.lh_rpt1[0] = '\0';
							band_txt.lh_rpt2[0] = '\0';
							band_txt.last_time = 0;
							band_txt.txt[0] = '\0';
							band_txt.txt_cnt = 0;
							band_txt.dest_rptr.clear();
							band_txt.num_dv_frames = 0;
							band_txt.num_dv_silent_frames = 0;
							band_txt.num_bit_errors = 0;
						}
						else
						{	// not the end of the voice stream
							int ber_data[3];
							int ber_errs = decode.Decode(dsvt.vasd.voice, ber_data);
							if (ber_data[0] == 0xf85)
								band_txt.num_dv_silent_frames++;
							band_txt.num_bit_errors += ber_errs;
							band_txt.num_dv_frames++;
						}
					}
                  	vPacketCount++;
				}

				/* find out if data must go to the remote G2 */
				if (to_remote_g2.streamid==dsvt.streamid && Index>=0) {
					sendto(g2_sock[Index], dsvt.title, 27, 0, to_remote_g2.toDstar.GetPointer(), to_remote_g2.toDstar.GetSize());

					time(&(to_remote_g2.last_time));

					/* Is this the end-of-stream */
					if (dsvt.ctrl & 0x40) {
						to_remote_g2.toDstar.Clear();
						to_remote_g2.streamid = 0;
						to_remote_g2.last_time = 0;
					}
				}

				if (LOG_QSO && dsvt.ctrl&0x40U)
					log.SendLog("id=%04x END RPTR\n", ntohs(dsvt.streamid));
			}
		}
	}
}

void CQnetGateway::AddFDSet(int &max, int newfd, fd_set *set)
{
	if (newfd > max)
		max = newfd;
	FD_SET(newfd, set);
}

/* run the main loop for QnetGateway */
void CQnetGateway::Process()
{
	std::future<void> irc_data_future[2];
	for (int i=0; i<2; i++) {
		if (ii[i]) {
			try {	// start the IRC read thread
				irc_data_future[i] = std::async(std::launch::async, &CQnetGateway::GetIRCDataThread, this, i);
			} catch (const std::exception &e) {
				fprintf(stderr, "Failed to start GetIRCDataThread[%d]. Exception: %s\n", i, e.what());
				keep_running = false;
			}
			if (keep_running)
				log.SendLog("get_irc_data thread[%d] started\n", i);

			ii[i]->kickWatchdog(GW_VERSION);
		}
	}

	while (keep_running) {
		ProcessTimeouts();

		// wait 20 ms max
		int max_nfds = 0;
		fd_set fdset;
		FD_ZERO(&fdset);
		if (g2_sock[0] >= 0)
			AddFDSet(max_nfds, g2_sock[0], &fdset);
		if (g2_sock[1] >= 0)
			AddFDSet(max_nfds, g2_sock[1], &fdset);
		AddFDSet(max_nfds, AM2Gate.GetFD(), &fdset);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 20000; // 20 ms
		(void)select(max_nfds + 1, &fdset, 0, 0, &tv);

		// process packets coming from remote G2
		for (int i=0; i<2; i++) {
			if (g2_sock[i] < 0)
				continue;
			if (keep_running && FD_ISSET(g2_sock[i], &fdset)) {
				CDSVT dsvt;
				socklen_t fromlen = sizeof(struct sockaddr_storage);
				ssize_t g2buflen = recvfrom(g2_sock[i], dsvt.title, 56, 0, fromDstar.GetPointer(), &fromlen);
				if (LOG_QSO && 4==g2buflen && 0==memcmp(dsvt.title, "PONG", 4)) {
					log.SendLog("Got a pong from [%s]:%u\n", fromDstar.GetAddress(), fromDstar.GetPort());
				} else {
					ProcessG2(g2buflen, dsvt);
				}
				FD_CLR(g2_sock[i], &fdset);
			}
		}

		// process packets coming from the audio module
		while (keep_running && FD_ISSET(AM2Gate.GetFD(), &fdset)) {
			CDSVT packet;
			AM2Gate.Read(packet.title, 56);
			ProcessAudio(&packet);
			FD_CLR(AM2Gate.GetFD(), &fdset);
		}
	}

	for (int i=0; i<2; i++) {
		if (ii[i])
			irc_data_future[i].get();
	}
}

void CQnetGateway::qrgs_and_maps()
{
	std::string rptrcall = OWNER;
	rptrcall.resize(CALL_SIZE-1);
	rptrcall.append(1, pCFGData->cModule);
	for (int j=0; j<2; j++) {
		if (ii[j]) {
			if (Rptr.mod.latitude || Rptr.mod.longitude || Rptr.mod.desc1.length() || Rptr.mod.url.length())
				ii[j]->rptrQTH(rptrcall, Rptr.mod.latitude, Rptr.mod.longitude, Rptr.mod.desc1, Rptr.mod.desc2, Rptr.mod.url, Rptr.mod.package_version);
			if (Rptr.mod.frequency)
				ii[j]->rptrQRG(rptrcall, Rptr.mod.frequency, Rptr.mod.offset, Rptr.mod.range, Rptr.mod.agl);
		}
	}
}

bool CQnetGateway::Init(CFGDATA *pData)
{
	pCFGData = pData;
	keep_running = true;
	// unix sockets
	Gate2AM.SetUp("gate2am");
	if (AM2Gate.Open("am2gate"))
		return true;

	//setvbuf(stdout, (char *)NULL, _IOLBF, 0);

	/* Used to validate MYCALL input */
	try {
		preg = std::regex("^(([1-9][A-Z])|([A-PR-Z][0-9])|([A-PR-Z][A-Z][0-9]))[0-9A-Z]*[A-Z][ ]*[ A-RT-Z]$", std::regex::extended);
	} catch (std::regex_error &e) {
		log.SendLog("Regular expression error: %s\n", e.what());
		return true;
	}

	band_txt.streamID = 0;
	memset(band_txt.flags, 0, 3);
	memset(band_txt.lh_mycall, 0, 9);
	memset(band_txt.lh_sfx, 0, 5);
	memset(band_txt.lh_yrcall, 0, 9);
	memset(band_txt.lh_rpt1, 0, 9);
	memset(band_txt.lh_rpt2, 0, 9);
	band_txt.last_time = 0;
	memset(band_txt.txt, 0, 64);   // Only 20 are used
	band_txt.txt_cnt = 0;
	band_txt.sent_key_on_msg = false;
	band_txt.dest_rptr.clear();
	memset(band_txt.temp_line, 0, 256);
	band_txt.temp_line_cnt = 0U;
	memset(band_txt.gprmc, 0, 256);
	memset(band_txt.gpid, 0, 256);
	band_txt.gps_last_time = 0;
	band_txt.num_dv_frames = 0;
	band_txt.num_dv_silent_frames = 0;
	band_txt.num_bit_errors = 0;

	/* process configuration file */
	if ( Configure() ) {
		log.SendLog("Failed to process the configuration\n");
		return true;
	}

	// open the database
	std::string fname(CFG_DIR);
	fname.append("qn.db");
	if (qnDB.Open(fname.c_str()))
		return true;

	playNotInCache = false;

	/* build the repeater callsigns for aprs */
	Rptr.mod.call = OWNER;
	auto n = Rptr.mod.call.size();
	while (isspace(Rptr.mod.call.at(n-1)))
		n--;
	Rptr.mod.call.resize(n);

	Rptr.mod.call += '-';
	Rptr.mod.call += pCFGData->cModule;
	Rptr.mod.band = "DV";
	log.SendLog("Repeater callsign: [%s]\n", Rptr.mod.call.c_str());

	for (int j=0; j<2; j++) {
		if (ircddb[j].ip.empty())
			continue;
		log.SendLog("connecting to %s at %u\n", ircddb[j].ip.c_str(), ircddb[j].port);
		ii[j] = new CIRCDDB(ircddb[j].ip, ircddb[j].port, owner, IRCDDB_PASSWORD[j], GW_VERSION.c_str());
		if (! ii[j]->open()) {
			log.SendLog("%s open failed\n", ircddb[j].ip.c_str());
			return true;
		}

		int rc = ii[j]->getConnectionState();
		log.SendLog("Waiting for %s connection status of 2\n", ircddb[j].ip.c_str());
		int i = 0;
		while (rc < 2) {
			log.SendLog("%s status=%d\n", ircddb[j].ip.c_str(), rc);
			if (rc < 2) {
				i++;
				sleep(5);
			} else
				break;

			if (!keep_running)
				break;

			if (i > 5) {
				log.SendLog("We can not wait any longer for %s...\n", ircddb[j].ip.c_str());
				break;
			}
			rc = ii[j]->getConnectionState();
		}
		switch (ii[j]->GetFamily()) {
		case AF_INET:
			log.SendLog("IRC server is using IPV4\n");
			af_family[j] = AF_INET;
			break;
		case AF_INET6:
			log.SendLog("IRC server is using IPV6\n");
			af_family[j] = AF_INET6;
			break;
		default:
			log.SendLog("%s server is using unknown protocol! Shutting down...\n", ircddb[j].ip.c_str());
			return true;
		}
	}
		/* udp port 40000 must open first */
	if (ii[0]) {
		SPORTIP *pip = (AF_INET == af_family[0]) ? &g2_external : & g2_ipv6_external;
		g2_sock[0] = open_port(pip, af_family[0]);
		if (0 > g2_sock[0]) {
			log.SendLog("Can't open %s:%d for %s\n", pip->ip.c_str(), pip->port, ircddb[0].ip.c_str());
			return true;
		}
		if (ii[1] && (af_family[0] != af_family[1])) {	// we only need to open a second port if the family for the irc servers are different!
			SPORTIP *pip = (AF_INET == af_family[1]) ? &g2_external : & g2_ipv6_external;
			g2_sock[1] = open_port(pip, af_family[1]);
			if (0 > g2_sock[1]) {
				log.SendLog("Can't open %s:%d for %s\n", pip->ip.c_str(), pip->port, ircddb[1].ip.c_str());
				return true;
			}
		}
	} else if (ii[1]) {
		SPORTIP *pip = (AF_INET == af_family[1]) ? &g2_external : & g2_ipv6_external;
		g2_sock[1] = open_port(pip, af_family[1]);
		if (0 > g2_sock[1]) {
			log.SendLog("Can't open %s:%d for %s\n", pip->ip.c_str(), pip->port, ircddb[1].ip.c_str());
			return true;
		}
	}

	// the repeater modules run on these ports
	memset(toRptr.saved_hdr.title, 0, 56);
	toRptr.saved_addr.Clear();

	toRptr.streamid = 0;
	toRptr.addr.Clear();

	toRptr.last_time = 0;

	toRptr.sequence = 0x0;

	/*
	   Initialize the end_of_audio that will be sent to the local repeater
	   when audio from remote G2 has timed out
	*/
	memset(end_of_audio.title, 0U, 27U);
	memcpy(end_of_audio.title, "DSVT", 4U);
	end_of_audio.id = end_of_audio.config = 0x20U;
	const unsigned char silence[9] = { 0x9EU, 0x8DU, 0x32U, 0x88U, 0x26U, 0x1AU, 0x3FU, 0x61U, 0xE8U };
	memcpy(end_of_audio.vasd.voice, silence, 9U);

	/* to remote systems */
	to_remote_g2.toDstar.Clear();
	to_remote_g2.streamid = 0;
	to_remote_g2.last_time = 0;

	log.SendLog("QnetGateway...entering processing loop\n");

	if (GATEWAY_SEND_QRGS_MAP)
		qrgs_and_maps();
	return false;
}

CQnetGateway::CQnetGateway()
{
	keep_running = false;
	ii[0] = ii[1] = NULL;
}

CQnetGateway::~CQnetGateway()
{
	for (int i=0; i<2; i++) {
		if (g2_sock[i] >= 0) {
			close(g2_sock[i]);
			printf("Closed G2_EXTERNAL_PORT %d\n", i);
		}
		if (ii[i]) {
			ii[i]->close();
			delete ii[i];
		}
	}

	printf("QnetGateway exiting\n");
}
