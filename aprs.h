#pragma once

/*
 *   Copyright (C) 2016,2020 by Thomas A. Early N7TAE
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
#include <future>
#include <atomic>

#include "TypeDefs.h"
#include "TCPReaderWriterClient.h"
#include "QnetLog.h"

enum aprs_level { al_none, al_$1, al_$2, al_c1, al_r1, al_c2, al_csum1, al_csum2, al_csum3, al_csum4, al_data, al_end };

enum slow_level { sl_first, sl_second };

class CAPRS {
public:
	// functions
	CAPRS();
	~CAPRS();
	std::atomic<bool> keep_running;
	SRPTR *m_rptr;
	void SelectBand(unsigned short streamID);
	void ProcessText(unsigned short streamID, unsigned char seq, unsigned char *buf);
	void Open(const std::string OWNER);
	void Init();
	void CloseSock();
	void StartThread();
	void FinishThread();
	void APRSBeaconThread();

	// classes
	CTCPReaderWriterClient aprs_sock;

	// data
	bool APRS_ENABLE;


private:
	// data
	struct {
		aprs_level al;
		unsigned char data[300];
		unsigned int len;
		unsigned char buf[6];
		slow_level sl;
		bool is_sent;
	} aprs_pack;
 	std::future<void> aprs_future;
	// lock down a stream per band
	struct {
		unsigned short streamID;
		time_t last_time;
	} aprs_streamID;
	int aprs_interval;
	std::string OWNER;
	SRPTR Rptr;
	SBANDTXT band_txt;

	// classes
	CQnetLog log;

	// functions
	bool WriteData(unsigned char *data);
	void SyncIt();
	void Reset();
	unsigned int GetData(unsigned char *data, unsigned int len);
	bool AddData(unsigned char *data);
	bool CheckData();
	unsigned int CalcCRC(unsigned char* buf, unsigned int len);
	void compute_aprs_hash();
	void gps_send();
	bool verify_gps_csum(char *gps_text, char *csum_text);
	void build_aprs_from_gps_and_send();
	bool validate_csum(SBANDTXT &bt, bool is_gps);
};
