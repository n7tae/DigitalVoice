#pragma once

/*
 *   Copyright 2016 by Thomas Early N7TAE
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
 *
 */

#include <string>
#include <sstream>

#define DV3K_TYPE_CONTROL 0x00U
#define DV3K_TYPE_AMBE 0x01U
#define DV3K_TYPE_AUDIO 0x02U

#define DV3K_START_BYTE 0x61U
#define DV3K_CONTROL_RATEP 0x0AU
#define DV3K_CONTROL_PRODID 0x30U
#define DV3K_CONTROL_VERSTRING 0x31U
#define DV3K_CONTROL_RESET 0x33U
#define DV3K_CONTROL_READY 0x39U

#define dv3k_packet_size(a) int(1 + sizeof((a).header) + ntohs((a).header.payload_length))

#pragma pack(push, 1)
struct dv3k_packet {
	unsigned char start_byte;
	struct {
		unsigned short payload_length;
		unsigned char packet_type;
	} header;
	unsigned char field_id;
	union {
		struct {
			union {
				char prodid[16];
				unsigned char ratep[12];
				char version[48];
				short chanfmt;
			} data;
		} ctrl;
		struct {
			unsigned char num_samples;
			short samples[160];
		} audio;
		struct {
			unsigned char num_bits;
			unsigned char data[9];
		} ambe;
	} payload;
};
#pragma pack(pop)

typedef struct dv3k_packet DV3K_PACKET, *PDV3K_PACKET;
enum class Encoding { dstar, dmr };

class CDV3000U {
public:
	CDV3000U();
	~CDV3000U();
	void FindandOpen(int baudrate, Encoding type);
	bool SetBaudRate(int baudrate);
	bool EncodeAudio(const short *audio, unsigned char *data);
	bool DecodeData(const unsigned char *data, short *audio);
	bool SendAudio(const short *audio);
	bool GetData(unsigned char *data);
	bool SendData(const unsigned char *data);
	bool GetAudio(short *audio);
	void CloseDevice();
	bool IsOpen();
	std::string GetDevicePath();
	std::string GetProductID();
	std::string GetVersion();
private:
	int fd;
	std::string devicepath, productid, version;
	bool OpenDevice(char *ttyname, int baudrate, Encoding dvtype);
	void dump(const char *title, void *data, int length);
	bool getresponse(PDV3K_PACKET packet);
	bool initDV3K(Encoding dvtype);
	bool checkResponse(PDV3K_PACKET responsePacket, unsigned char response);
};
