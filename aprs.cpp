/*
 *   Copyright (C) 2016-2020 by Thomas A. Early N7TAE
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

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include <thread>
#include <chrono>
#include <future>
#include <cmath>
#include <string>

#include "aprs.h"
#include "Utilities.h"

CAPRS::CAPRS() : keep_running(true) {}

CAPRS::~CAPRS()
{
    CloseSock();
}

void CAPRS::Init()
{
	cfg.CopyTo(cfgdata);
	last_time = 0;

	if (cfgdata.bAPRSEnable) {	// start the beacon thread
		try {
			aprs_future = std::async(std::launch::async, &CAPRS::APRSBeaconThread, this);
		} catch (const std::exception &e) {
			log.SendLog("Failed to start the APRSBeaconThread. Exception: %s\n", e.what());
		}
		if (aprs_future.valid())
			log.SendLog("APRS beacon thread started\n");
	}
}

void CAPRS::Close()
{
	keep_running = false;
	FinishThread();
	CloseSock();
}

void CAPRS::FinishThread()
{
	// thread clean-up
	if (cfgdata.bAPRSEnable) {
		if (aprs_future.valid())
			aprs_future.get();
	}
}

void CAPRS::UpdateUser()
{
	if (! cfgdata.bAPRSEnable || -1==aprs_sock.GetFD())
		return;

	char aprs_buf[1024];
	time_t tnow = time(NULL);

	if ((tnow - last_time) < 30)
		return;	// it hasn't even been 30 seconds

	if (0.0==cfgdata.dLatitude && 0.0==cfgdata.dLongitude)
		return;	// nothing to report!

	double fracp, intp;
	fracp = modf(fabs(cfgdata.dLatitude), &intp);
	const double lat = intp * 100.0 + fracp * 60.0;
	const char latc = (cfgdata.dLatitude >= 0.0) ? 'N' : 'S';
	fracp = modf(fabs(cfgdata.dLongitude), &intp);
	const double lon = intp * 100.0 + fracp * 60.0;
	const char lonc = (cfgdata.dLongitude >= 0.0) ? 'E' : 'W';

	sprintf(aprs_buf, "%s>API51,qAR,%s-%c:!%.2f%c/%.2f%c[/\r\n", trim_copy(cfgdata.sCallsign).c_str(), trim_copy(cfgdata.sStation).c_str(), cfgdata.cModule, lat, latc, lon, lonc);
	log.SendLog("GPS-A=%s", aprs_buf);
    int rc = aprs_sock.Write((unsigned char *)aprs_buf, strlen(aprs_buf));
	if (rc == -1) {
		if ((errno == EPIPE) || (errno == ECONNRESET) || (errno == ETIMEDOUT) || (errno == ECONNABORTED) || (errno == ESHUTDOWN) || (errno == EHOSTUNREACH) || (errno == ENETRESET) || (errno == ENETDOWN) || (errno == ENETUNREACH) || (errno == EHOSTDOWN) || (errno == ENOTCONN)) {
			log.SendLog("CAPRS::ProcessText(): APRS_HOST closed connection, error=%d\n", errno);
			aprs_sock.Close();
		} else /* if it is WOULDBLOCK, we will not go into a loop here */
			log.SendLog("CAPRS::ProcessText(): send error=%d\n", errno);
	}

	last_time = tnow;
}

void CAPRS::Open()
{
	char snd_buf[512];
	char rcv_buf[512];
    while (aprs_sock.Open(cfgdata.sAPRSServer, AF_UNSPEC, std::to_string(cfgdata.usAPRSPort))) {
        log.SendLog("Failed to open %s, retry in 10 seconds...\n", cfgdata.sAPRSServer.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

	/* login to aprs */
	//sprintf(snd_buf, "user %s pass %d vers QnetGateway 9 UDP 5 ", OWNER.c_str(), m_rptr->aprs_hash);
	sprintf(snd_buf, "user %s pass %d vers QnetGateway-9 ", trim_copy(cfgdata.sStation).c_str(), compute_aprs_hash());

	//log.SendLog("APRS Login command:[%s]\n", snd_buf);
	strcat(snd_buf, "\r\n");

	while (true) {
        int rc = aprs_sock.Write((unsigned char *)snd_buf, strlen(snd_buf));
		if (rc < 0) {
			if (errno == EWOULDBLOCK) {
                aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} else {
				log.SendLog("APRS Login command failed, error=%d\n", errno);
				break;
			}
		} else {
			// log.SendLog("APRS Login command sent\n");
			break;
		}
	}
    aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
	//log.SendLog("APRS Login returned: %s", rcv_buf);
	return;
}

void CAPRS::CloseSock()
{
	if (cfgdata.bAPRSEnable) {
		if (aprs_sock.GetFD() != -1) {
			aprs_sock.Close();
			log.SendLog("Closed APRS\n");
		}
	}
}

void CAPRS::APRSBeaconThread()
{
	char snd_buf[512];
	char rcv_buf[512];
	time_t tnow = 0;

//	struct sigaction act;

	/*
	   Every 20 seconds, the remote APRS host sends a KEEPALIVE packet-comment
	   on the TCP/APRS port.
	   If we have not received any KEEPALIVE packet-comment after 5 minutes
	   we must assume that the remote APRS host is down or disappeared
	   or has dropped the connection. In these cases, we must re-connect.
	   There are 3 keepalive packets in one minute, or every 20 seconds.
	   In 5 minutes, we should have received a total of 15 keepalive packets.
	*/
	short THRESHOLD_COUNTDOWN = 15;

	time_t last_keepalive_time;
	time(&last_keepalive_time);

	time_t last_beacon_time = 0;
	/* This thread is also saying to the APRS_HOST that we are ALIVE */
	while (keep_running) {
		if (aprs_sock.GetFD() == -1) {
			Open();
			if (aprs_sock.GetFD() == -1)
				sleep(1);
			else
				THRESHOLD_COUNTDOWN = 15;
		}

		time(&tnow);
		if ((tnow - last_beacon_time) > (cfgdata.iAPRSInterval * 60)) {
			if (cfgdata.dLongitude || cfgdata.dLatitude) {
				double fpart, ipart;
				fpart = modf(fabs(cfgdata.dLatitude), &ipart);
				const double lat = 100.0 * ipart + 60.0 * fpart;
				fpart = modf(fabs(cfgdata.dLongitude), &ipart);
				const double lon = 100.0 * ipart + 0.0 * fpart;

				/* send to aprs */
				std::string modcall(cfgdata.sStation);
				trim(modcall);
				modcall.append(1, '-');
				modcall.append(1, cfgdata.cModule);
				sprintf(snd_buf, "%s>APJI23,TCPIP*,qAC,%sS:!%08.2f%cD%08.2f%c&RNG0000 DigitalVoice by_N7TAE",modcall.c_str(),  modcall.c_str(), lat, (cfgdata.dLatitude < 0.0)  ? 'S' : 'N', lon, (cfgdata.dLongitude < 0.0) ? 'W' : 'E');

				// log.SendLog("APRS Beacon =[%s]\n", snd_buf);
				strcat(snd_buf, "\r\n");

				while (keep_running) {
					if (aprs_sock.GetFD() == -1) {
						Open();
						if (aprs_sock.GetFD() == -1)
							sleep(1);
						else
							THRESHOLD_COUNTDOWN = 15;
					} else {
						int rc = aprs_sock.Write((unsigned char *)snd_buf, strlen(snd_buf));
						if (rc < 0) {
							if ((errno == EPIPE) || (errno == ECONNRESET) || (errno == ETIMEDOUT) || (errno == ECONNABORTED) || (errno == ESHUTDOWN) || (errno == EHOSTUNREACH) || (errno == ENETRESET) || (errno == ENETDOWN) || (errno == ENETUNREACH) || (errno == EHOSTDOWN) || (errno == ENOTCONN)) {
								log.SendLog("send_aprs_beacon: APRS_HOST closed connection,error=%d\n", errno);
								aprs_sock.Close();
							} else if (errno == EWOULDBLOCK) {
								std::this_thread::sleep_for(std::chrono::milliseconds(100));
							} else {
								/* Cant do nothing about it */
								log.SendLog("send_aprs_beacon failed, error=%d\n", errno);
								break;
							}
						} else {
							// log.SendLog("APRS beacon sent\n");
							break;
						}
					}
					int rc = aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
					if (rc > 0)
						THRESHOLD_COUNTDOWN = 15;
				}
			}
			int rc = aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
			if (rc > 0)
				THRESHOLD_COUNTDOWN = 15;
			time(&last_beacon_time);
		}
		/*
		   Are we still receiving from APRS host ?
		*/
		int rc = aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
		if (rc < 0) {
			if ((errno == EPIPE) || (errno == ECONNRESET) || (errno == ETIMEDOUT) || (errno == ECONNABORTED) || (errno == ESHUTDOWN) || (errno == EHOSTUNREACH) || (errno == ENETRESET) || (errno == ENETDOWN) || (errno == ENETUNREACH) || (errno == EHOSTDOWN) || (errno == ENOTCONN)) {
				log.SendLog("send_aprs_beacon: recv error: APRS_HOST closed connection,error=%d\n", errno);
				aprs_sock.Close();
			}
		} else if (rc == 0) {
			log.SendLog("send_aprs_beacon: recv: APRS shutdown\n");
			aprs_sock.Close();
		} else
			THRESHOLD_COUNTDOWN = 15;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		/* 20 seconds passed already ? */
		time(&tnow);
		if ((tnow - last_keepalive_time) > 20) {
			/* we should be receving keepalive packets ONLY if the connection is alive */
			if (aprs_sock.GetFD() >= 0) {
				if (THRESHOLD_COUNTDOWN > 0)
					THRESHOLD_COUNTDOWN--;

				if (THRESHOLD_COUNTDOWN == 0) {
					log.SendLog("APRS host keepalive timeout\n");
					aprs_sock.Close();
				}
			}
			/* reset timer */
			time(&last_keepalive_time);
		}
	}
	log.SendLog("APRS beacon thread exiting...\n");
}

int CAPRS::compute_aprs_hash()
{
	int hash = 0x73e2;
	char rptr_sign[9];

	strcpy(rptr_sign, trim_copy(cfgdata.sStation).c_str());
	char *p = rptr_sign;
	short int len = strlen(rptr_sign);

	for (int i=0; i < len; i+=2) {
		hash ^= (*p++) << 8;
		hash ^= (*p++);
	}
	log.SendLog("aprs hash code=[%d] for %s\n", hash, rptr_sign);
	return hash;
}
