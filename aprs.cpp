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

#include "aprs.h"

void CAPRS::StartThread()
{
	if (APRS_ENABLE) {	// start the beacon thread
		try {
			aprs_future = std::async(std::launch::async, &CAPRS::APRSBeaconThread, this);
		} catch (const std::exception &e) {
			fprintf(stderr, "Failed to start the APRSBeaconThread. Exception: %s\n", e.what());
		}
		if (aprs_future.valid())
			log.SendLog("APRS beacon thread started\n");
	}
}

void CAPRS::FinishThread()
{
	// thread clean-up
	if (APRS_ENABLE) {
		if (aprs_future.valid())
			aprs_future.get();
	}
}

// This is called when header comes in from repeater
void CAPRS::SelectBand(unsigned short streamID)
{
	/* lock on the streamID */
	aprs_streamID.streamID = streamID;
	// aprs_streamID.last_time = 0;

	Reset();
	return;
}

// This is called when data(text) comes in from repeater
// Parameter buf is either:
//              12 bytes(packet from repeater was 29 bytes) or
//              15 bytes(packet from repeater was 32 bytes)
// Paramter seq is the byte at pos# 16(counting from zero) in the repeater data
void CAPRS::ProcessText(unsigned short streamID, unsigned char seq, unsigned char *buf)
{
	unsigned char aprs_data[200];
	char aprs_buf[1024];
	time_t tnow = 0;


	if (streamID != aprs_streamID.streamID) {
		printf("ERROR in aprs_process_text: streamID is invalid\n");
		return;
	}

	if ((seq & 0x40) == 0x40)
		return;

	if ((seq & 0x1f) == 0x00) {
		SyncIt();
		return;
	}

	bool done = WriteData(buf + 9);
	if (!done)
		return;

	unsigned int aprs_len = GetData(aprs_data, 200);
	aprs_data[aprs_len] = '\0';

	time(&tnow);
	if ((tnow - aprs_streamID.last_time) < 30)
		return;

	if (aprs_sock.GetFD() == -1)
		return;

	char *p = strchr((char*)aprs_data, ':');
	if (!p) {
		Reset();
		return;
	}
	*p = '\0';


	char *hdr = (char *)aprs_data;
	char *aud = p + 1;
	if (strchr(hdr, 'q') != NULL)
		return;

	p = strchr(aud, '\r');
	*p = '\0';

	sprintf(aprs_buf, "%s,qAR,%s:%s\r\n", hdr, m_rptr->mod.call.c_str(), aud);
	// printf("GPS-A=%s", aprs_buf);
    int rc = aprs_sock.Write((unsigned char *)aprs_buf, strlen(aprs_buf));
	if (rc == -1) {
		if ((errno == EPIPE) ||
		        (errno == ECONNRESET) ||
		        (errno == ETIMEDOUT) ||
		        (errno == ECONNABORTED) ||
		        (errno == ESHUTDOWN) ||
		        (errno == EHOSTUNREACH) ||
		        (errno == ENETRESET) ||
		        (errno == ENETDOWN) ||
		        (errno == ENETUNREACH) ||
		        (errno == EHOSTDOWN) ||
		        (errno == ENOTCONN)) {
			printf("CAPRS::ProcessText(): APRS_HOST closed connection, error=%d\n",errno);
			aprs_sock.Close();
		} else /* if it is WOULDBLOCK, we will not go into a loop here */
			printf("CAPRS::ProcessText(): send error=%d\n", errno);
	}

	time(&aprs_streamID.last_time);

	return;
}

void CAPRS::Init()
{
	/* Initialize the statistics on the APRS packets */
	for (short int rptr_idx = 0; rptr_idx < 3; rptr_idx++) {
		aprs_pack.al = al_none;
		aprs_pack.data[0] = '\0';
		aprs_pack.len = 0;
		aprs_pack.buf[0] = '\0';
		aprs_pack.sl = sl_first;
		aprs_pack.is_sent = false;
	}

	for (short int i = 0; i < 3; i++) {
		aprs_streamID.streamID = 0;
		aprs_streamID.last_time = 0;
	}

	compute_aprs_hash();

}

bool CAPRS::WriteData(unsigned char *data)
{
	if (aprs_pack.is_sent)
		return false;

	switch (aprs_pack.sl) {
	case sl_first:
		aprs_pack.buf[0] = data[0] ^ 0x70;
		aprs_pack.buf[1] = data[1] ^ 0x4f;
		aprs_pack.buf[2] = data[2] ^ 0x93;
		aprs_pack.sl = sl_second;
		return false;

	case sl_second:
		aprs_pack.buf[3] = data[0] ^ 0x70;
		aprs_pack.buf[4] = data[1] ^ 0x4f;
		aprs_pack.buf[5] = data[2] ^ 0x93;
		aprs_pack.sl = sl_first;
		break;
	}

	if ((aprs_pack.buf[0] & 0xf0) != 0x30)
		return false;

	return AddData(aprs_pack.buf + 1);

}

void CAPRS::SyncIt()
{
	aprs_pack.sl = sl_first;
	return;
}

void CAPRS::Reset()
{
	aprs_pack.al = al_none;
	aprs_pack.len = 0;
	aprs_pack.sl = sl_first;
	aprs_pack.is_sent = false;

	return;
}

unsigned int CAPRS::GetData(unsigned char *data, unsigned int len)
{
	unsigned int l = aprs_pack.len - 10;

	if (l > len)
		l = len;

	memcpy(data, aprs_pack.data  + 10, l);

	aprs_pack.al = al_none;
	aprs_pack.len = 0;
	aprs_pack.is_sent = true;

	return l;
}

void CAPRS::Open(const std::string OWNER)
{
	char snd_buf[512];
	char rcv_buf[512];
    while (aprs_sock.Open(m_rptr->aprs.ip, AF_UNSPEC, std::to_string(m_rptr->aprs.port))) {
        fprintf(stderr, "Failed to open %s, retry in 10 seconds...\n", m_rptr->aprs.ip.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

	/* login to aprs */
	//sprintf(snd_buf, "user %s pass %d vers QnetGateway 9 UDP 5 ", OWNER.c_str(), m_rptr->aprs_hash);
	sprintf(snd_buf, "user %s pass %d vers QnetGateway-9 ", OWNER.c_str(), m_rptr->aprs_hash);

	/* add the user's filter */
	if (m_rptr->aprs_filter.length()) {
		strcat(snd_buf, "filter ");
		strcat(snd_buf, m_rptr->aprs_filter.c_str());
	}
	//printf("APRS Login command:[%s]\n", snd_buf);
	strcat(snd_buf, "\r\n");

	while (true) {
        int rc = aprs_sock.Write((unsigned char *)snd_buf, strlen(snd_buf));
		if (rc < 0) {
			if (errno == EWOULDBLOCK) {
                aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			} else {
				printf("APRS Login command failed, error=%d\n", errno);
				break;
			}
		} else {
			// printf("APRS Login command sent\n");
			break;
		}
	}
    aprs_sock.Read((unsigned char *)rcv_buf, sizeof(rcv_buf));
	//printf("APRS Login returned: %s", rcv_buf);
	return;
}

bool CAPRS::AddData(unsigned char *data)
{
	for (unsigned int i = 0; i < 5; i++) {
		unsigned char c = data[i];

		if ((aprs_pack.al == al_none) && (c == '$')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_$1;
		} else if ((aprs_pack.al == al_$1) && (c == '$')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_$2;
		} else if ((aprs_pack.al == al_$2) && (c == 'C')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_c1;
		} else if ((aprs_pack.al == al_c1) && (c == 'R')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_r1;
		} else if ((aprs_pack.al  == al_r1) && (c == 'C')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_c2;
		} else if (aprs_pack.al == al_c2) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_csum1;
		} else if (aprs_pack.al == al_csum1) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_csum2;
		} else if (aprs_pack.al == al_csum2) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_csum3;
		} else if (aprs_pack.al == al_csum3) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_csum4;
		} else if ((aprs_pack.al == al_csum4) && (c == ',')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;
			aprs_pack.al = al_data;
		} else if ((aprs_pack.al == al_data) && (c != '\r')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;

			if (aprs_pack.len >= 300) {
				printf("ERROR in aprs_add_data: Expected END of APRS data\n");
				aprs_pack.len = 0;
				aprs_pack.al  = al_none;
			}
		} else if ((aprs_pack.al == al_data) && (c == '\r')) {
			aprs_pack.data[aprs_pack.len] = c;
			aprs_pack.len++;


			bool ok = CheckData();
			if (ok) {
				aprs_pack.al = al_end;
				return true;
			} else {
				printf("BAD checksum in APRS data\n");
				aprs_pack.al  = al_none;
				aprs_pack.len = 0;
			}
		} else {
			aprs_pack.al  = al_none;
			aprs_pack.len = 0;
		}
	}
	return false;
}

bool CAPRS::CheckData()
{
	unsigned int my_sum;
	char buf[5];

	my_sum = CalcCRC(aprs_pack.data + 10, aprs_pack.len - 10);

	sprintf(buf, "%04X", my_sum);

	return (0 == memcmp(buf, aprs_pack.data + 5, 4));
}

unsigned int CAPRS::CalcCRC(unsigned char* buf, unsigned int len)
{
	unsigned int my_crc = 0xffff;

	if (!buf)
		return 0;

	if (len <= 0)
		return 0;

	for (unsigned int j = 0; j < len; j++) {
		unsigned int c = buf[j];

		for (unsigned int i = 0; i < 8; i++) {
			bool xor_val = (((my_crc ^ c) & 0x01) == 0x01);
			my_crc >>= 1;

			if (xor_val)
				my_crc ^= 0x8408;

			c >>= 1;
		}
	}
	return (~my_crc & 0xffff);
}

CAPRS::CAPRS() : keep_running(true)
{
}

CAPRS::~CAPRS()
{
    CloseSock();
}

void CAPRS::CloseSock()
{
	if (APRS_ENABLE) {
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
			Open(OWNER);
			if (aprs_sock.GetFD() == -1)
				sleep(1);
			else
				THRESHOLD_COUNTDOWN = 15;
		}

		time(&tnow);
		if ((tnow - last_beacon_time) > (aprs_interval * 60)) {
			if (Rptr.mod.desc1[0] != '\0') {
				float tmp_lat = fabs(Rptr.mod.latitude);
				float tmp_lon = fabs(Rptr.mod.longitude);
				float lat = floor(tmp_lat);
				float lon = floor(tmp_lon);
				lat = (tmp_lat - lat) * 60.0F + lat  * 100.0F;
				lon = (tmp_lon - lon) * 60.0F + lon  * 100.0F;

				char lat_s[15], lon_s[15];
				if (lat >= 1000.0F)
					sprintf(lat_s, "%.2f", lat);
				else if (lat >= 100.0F)
					sprintf(lat_s, "0%.2f", lat);
				else if (lat >= 10.0F)
					sprintf(lat_s, "00%.2f", lat);
				else
					sprintf(lat_s, "000%.2f", lat);

				if (lon >= 10000.0F)
					sprintf(lon_s, "%.2f", lon);
				else if (lon >= 1000.0F)
					sprintf(lon_s, "0%.2f", lon);
				else if (lon >= 100.0F)
					sprintf(lon_s, "00%.2f", lon);
				else if (lon >= 10.0F)
					sprintf(lon_s, "000%.2f", lon);
				else
					sprintf(lon_s, "0000%.2f", lon);

				/* send to aprs */
				sprintf(snd_buf, "%s>APJI23,TCPIP*,qAC,%sS:!%s%cD%s%c&RNG%04u %s %s",
						Rptr.mod.call.c_str(),  Rptr.mod.call.c_str(),
						lat_s,  (Rptr.mod.latitude < 0.0)  ? 'S' : 'N',
						lon_s,  (Rptr.mod.longitude < 0.0) ? 'W' : 'E',
						(unsigned int)Rptr.mod.range, Rptr.mod.band.c_str(), Rptr.mod.desc1.c_str());

				// printf("APRS Beacon =[%s]\n", snd_buf);
				strcat(snd_buf, "\r\n");

				while (keep_running) {
					if (aprs_sock.GetFD() == -1) {
						Open(OWNER);
						if (aprs_sock.GetFD() == -1)
							sleep(1);
						else
							THRESHOLD_COUNTDOWN = 15;
					} else {
						int rc = aprs_sock.Write((unsigned char *)snd_buf, strlen(snd_buf));
						if (rc < 0) {
							if ((errno == EPIPE) ||
									(errno == ECONNRESET) ||
									(errno == ETIMEDOUT) ||
									(errno == ECONNABORTED) ||
									(errno == ESHUTDOWN) ||
									(errno == EHOSTUNREACH) ||
									(errno == ENETRESET) ||
									(errno == ENETDOWN) ||
									(errno == ENETUNREACH) ||
									(errno == EHOSTDOWN) ||
									(errno == ENOTCONN)) {
								fprintf(stderr, "send_aprs_beacon: APRS_HOST closed connection,error=%d\n", errno);
								aprs_sock.Close();
							} else if (errno == EWOULDBLOCK) {
								std::this_thread::sleep_for(std::chrono::milliseconds(100));
							} else {
								/* Cant do nothing about it */
								fprintf(stderr, "send_aprs_beacon failed, error=%d\n", errno);
								break;
							}
						} else {
							// printf("APRS beacon sent\n");
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
			if ((errno == EPIPE) ||
			        (errno == ECONNRESET) ||
			        (errno == ETIMEDOUT) ||
			        (errno == ECONNABORTED) ||
			        (errno == ESHUTDOWN) ||
			        (errno == EHOSTUNREACH) ||
			        (errno == ENETRESET) ||
			        (errno == ENETDOWN) ||
			        (errno == ENETUNREACH) ||
			        (errno == EHOSTDOWN) ||
			        (errno == ENOTCONN)) {
				fprintf(stderr, "send_aprs_beacon: recv error: APRS_HOST closed connection,error=%d\n", errno);
				aprs_sock.Close();
			}
		} else if (rc == 0) {
			printf("send_aprs_beacon: recv: APRS shutdown\n");
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
					fprintf(stderr, "APRS host keepalive timeout\n");
					aprs_sock.Close();
				}
			}
			/* reset timer */
			time(&last_keepalive_time);
		}
	}
	log.SendLog("APRS beacon thread exiting...\n");
}

void CAPRS::compute_aprs_hash()
{
	short hash = 0x73e2;
	char rptr_sign[9];

	strcpy(rptr_sign, OWNER.c_str());
	char *p = strchr(rptr_sign, ' ');
	if (!p) {
		fprintf(stderr, "Failed to build repeater callsign for aprs hash\n");
		return;
	}
	*p = '\0';
	p = rptr_sign;
	short int len = strlen(rptr_sign);

	for (short int i=0; i < len; i+=2) {
		hash ^= (*p++) << 8;
		hash ^= (*p++);
	}
	printf("aprs hash code=[%d] for %s\n", hash, OWNER.c_str());
	Rptr.aprs_hash = hash;
}

void CAPRS::gps_send()
{
	time_t tnow = time(NULL);
	if ((tnow - band_txt.gps_last_time) < 31)
		return;

	static char old_mycall[9] = { "        " };

	if (band_txt.gprmc[0] == '\0') {
		band_txt.gpid[0] = '\0';
		fprintf(stderr, "missing GPS ID\n");
		return;
	}
	if (band_txt.gpid[0] == '\0') {
		band_txt.gprmc[0] = '\0';
		fprintf(stderr, "Missing GPSRMC\n");
		return;
	}
	if (memcmp(band_txt.gpid, band_txt.lh_mycall, 8) != 0) {
		fprintf(stderr, "MYCALL [%s] does not match first 8 characters of GPS ID [%.8s]\n", band_txt.lh_mycall, band_txt.gpid);
		band_txt.gprmc[0] = '\0';
		band_txt.gpid[0] = '\0';
		return;
	}

	/* if new station, reset last time */
	if (strcmp(old_mycall, band_txt.lh_mycall) != 0) {
		strcpy(old_mycall, band_txt.lh_mycall);
		band_txt.gps_last_time = 0;
	}

	/* do NOT process often */
	time(&tnow);

	printf("GPRMC=[%s]\n", band_txt.gprmc);
	printf("GPS id=[%s]\n",band_txt.gpid);

	if (validate_csum(band_txt, false))	// || validate_csum(band_txt, true))
		return;

	/* now convert GPS into APRS and send it */
	build_aprs_from_gps_and_send();

	band_txt.gps_last_time = tnow;
}

void CAPRS::build_aprs_from_gps_and_send()
{
	char buf[512];
	const char *delim = ",";

	char *saveptr = NULL;

	/*** dont care about the rest */

	strcpy(buf, band_txt.lh_mycall);
	char *p = strchr(buf, ' ');
	if (p) {
		if (band_txt.lh_mycall[7] != ' ') {
			*p = '-';
			*(p + 1) = band_txt.lh_mycall[7];
			*(p + 2) = '>';
			*(p + 3) = '\0';
		} else {
			*p = '>';
			*(p + 1) = '\0';
		}
	} else
		strcat(buf, ">");

	strcat(buf, "APDPRS,DSTAR*,qAR,");
	strcat(buf, Rptr.mod.call.c_str());
	strcat(buf, ":!");

	//GPRMC =
	strtok_r(band_txt.gprmc, delim, &saveptr);
	//time_utc =
	strtok_r(NULL, delim, &saveptr);
	//nav =
	strtok_r(NULL, delim, &saveptr);
	char *lat_str = strtok_r(NULL, delim, &saveptr);
	char *lat_NS = strtok_r(NULL, delim, &saveptr);
	char *lon_str = strtok_r(NULL, delim, &saveptr);
	char *lon_EW = strtok_r(NULL, delim, &saveptr);

	if (lat_str && lat_NS) {
		if ((*lat_NS != 'N') && (*lat_NS != 'S')) {
			fprintf(stderr, "Invalid North or South indicator in latitude\n");
			return;
		}
		if (strlen(lat_str) > 9) {
			fprintf(stderr, "Invalid latitude\n");
			return;
		}
		if (lat_str[4] != '.') {
			fprintf(stderr, "Invalid latitude\n");
			return;
		}
		lat_str[7] = '\0';
		strcat(buf, lat_str);
		strcat(buf, lat_NS);
	} else {
		fprintf(stderr, "Invalid latitude\n");
		return;
	}
	/* secondary table */
	strcat(buf, "/");

	if (lon_str && lon_EW) {
		if ((*lon_EW != 'E') && (*lon_EW != 'W')) {
			fprintf(stderr, "Invalid East or West indicator in longitude\n");
			return;
		}
		if (strlen(lon_str) > 10) {
			fprintf(stderr, "Invalid longitude\n");
			return;
		}
		if (lon_str[5] != '.') {
			fprintf(stderr, "Invalid longitude\n");
			return;
		}
		lon_str[8] = '\0';
		strcat(buf, lon_str);
		strcat(buf, lon_EW);
	} else {
		fprintf(stderr, "Invalid longitude\n");
		return;
	}

	/* Just this symbolcode only */
	strcat(buf, "/");
	strncat(buf, band_txt.gpid + 13, 32);

	// printf("Built APRS from old GPS mode=[%s]\n", buf);
	strcat(buf, "\r\n");

	if (aprs_sock.Write((unsigned char *)buf, strlen(buf))) {
		if ((errno == EPIPE) || (errno == ECONNRESET) || (errno == ETIMEDOUT) || (errno == ECONNABORTED) ||
		    (errno == ESHUTDOWN) || (errno == EHOSTUNREACH) || (errno == ENETRESET) || (errno == ENETDOWN) ||
		    (errno == ENETUNREACH) || (errno == EHOSTDOWN) || (errno == ENOTCONN)) {
			fprintf(stderr, "build_aprs_from_gps_and_send: APRS_HOST closed connection, error=%d\n", errno);
			aprs_sock.Close();
		} else
			fprintf(stderr, "build_aprs_from_gps_and_send: send error=%d\n", errno);
	}
	return;
}

bool CAPRS::verify_gps_csum(char *gps_text, char *csum_text)
{
	short computed_csum = 0;
	char computed_csum_text[16];

	short int len = strlen(gps_text);
	for (short int i=0; i<len; i++) {
		char c = gps_text[i];
		if (computed_csum == 0)
			computed_csum = (char)c;
		else
			computed_csum = computed_csum ^ ((char)c);
	}
	sprintf(computed_csum_text, "%02X", computed_csum);
	// printf("computed_csum_text=[%s]\n", computed_csum_text);

	char *p = strchr(csum_text, ' ');
	if (p)
		*p = '\0';

	if (strcmp(computed_csum_text, csum_text) == 0)
		return true;
	else
		return false;
}

bool CAPRS::validate_csum(SBANDTXT &bt, bool is_gps)
{
	const char *name = is_gps ? "GPS" : "GPRMC";
	char *s = is_gps ? bt.gpid : bt.gprmc;
	char *p = strrchr(s, '*');
	if (!p) {
		// BAD news, something went wrong
		fprintf(stderr, "Missing asterisk before checksum in %s\n", name);
		bt.gprmc[0] = bt.gpid[0] = '\0';
		return true;
	} else {
		*p = '\0';
		// verify csum in GPRMC
		bool ok = verify_gps_csum(s + 1, p + 1);
		if (!ok) {
			fprintf(stderr, "csum in %s not good\n", name);
			bt.gprmc[0] = bt.gpid[0] = '\0';
			return true;
		}
	}
	return false;
}
