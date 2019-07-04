/*
 *   Copyright (C) 2014 by Jonathan Naylor G4KLX and John Hays K7VE
 *   Copyright 2016 by Jeremy McDermond (NH6Z)
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
 *   Adapted by K7VE from G4KLX dv3000d
 */

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <cerrno>

#include <netinet/in.h>

#include "DV3000U.h"

CDV3000U::CDV3000U() : fd(-1)
{
}

CDV3000U::~CDV3000U()
{
	CloseDevice();
}

bool CDV3000U::checkResponse(PDV3K_PACKET p, unsigned char response)
{
	if(p->start_byte != DV3K_START_BYTE || p->header.packet_type != DV3K_TYPE_CONTROL || p->field_id != response)
		return true;

	return false;
}

bool CDV3000U::IsOpen()
{
	return fd >= 0;
}

std::string CDV3000U::GetDevicePath()
{
	return devicepath;
}

std::string CDV3000U::GetVersion()
{
	return version;
}

std::string CDV3000U::GetProductID()
{
	return productid;
}

void CDV3000U::FindandOpen(int baudrate, Eencoding type)
{
	bool rval = true;
	char device[16];

	for (int i=0; rval && i<32; i++) {
		sprintf(device, "/dev/ttyUSB%d", i);

		if (access(device, R_OK | W_OK) != 0)
			continue;

		rval = OpenDevice(device, baudrate, type);
	}

	if (rval)
		devicepath.clear();
	else
		devicepath.assign(device);
}

bool CDV3000U::SetBaudRate(int baudrate)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) != 0) {
		std::cerr << "AMBEserver tcgetattr: " << strerror(errno) << std::endl;
		close(fd);
		return true;
	}

	//  Input speed = output speed
	cfsetispeed(&tty, B0);

	switch(baudrate) {
		case 230400:
			cfsetospeed(&tty, B230400);
			break;
		case 460800:
			cfsetospeed(&tty, B460800);
			break;
		default:
			std::cerr << "AMBEserver: unsupported baud rate " << baudrate << std::endl;
			close(fd);
			return true;
	}

	tty.c_lflag    &= ~(ECHO | ECHOE | ICANON | IEXTEN | ISIG);
	tty.c_iflag    &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF | IXANY);
	tty.c_cflag    &= ~(CSIZE | CSTOPB | PARENB);
	tty.c_cflag    |= CS8 | CRTSCTS;
	tty.c_oflag    &= ~(OPOST);
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 1;

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		std::cerr << "AMBEserver: tcsetattr: " << strerror(errno) << std::endl;
		close(fd);
		return true;
	}
	return false;
}

bool CDV3000U::OpenDevice(char *ttyname, int baudrate, Eencoding dvtype)
{
	fd = open(ttyname, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) {
		std::cerr << "AMBEserver: error when opening " << ttyname << ": " << strerror(errno) << std::endl;
		return true;
	}

	if (SetBaudRate(baudrate))
		return true;

	if (initDV3K(dvtype)) {
		close(fd);
		return true;
	}

	return false;
}

bool CDV3000U::initDV3K(Eencoding dvtype)
{
	char prodId[17];
	char versionstr[49];
	DV3K_PACKET responsePacket, ctrlPacket;

    ctrlPacket.start_byte = DV3K_START_BYTE;
    ctrlPacket.header.packet_type = DV3K_TYPE_CONTROL;
    ctrlPacket.header.payload_length = htons(1);
    ctrlPacket.field_id = DV3K_CONTROL_RESET;

	if (write(fd, &ctrlPacket, dv3k_packet_size(ctrlPacket)) == -1) {
		std::cerr << "initDV3k: error writing reset packet: " << strerror(errno) << std::endl;
		return true;
	}

	if (getresponse(&responsePacket)) {
		std::cerr << "initDV3k: error receiving response to reset" << std::endl;
		return true;
	}

	if (checkResponse(&responsePacket, DV3K_CONTROL_READY)) {
	   std::cerr << "initDV3k: invalid response to reset" << std::endl;
	   return true;
	}

	ctrlPacket.field_id = DV3K_CONTROL_PRODID;
	if (write(fd, &ctrlPacket, dv3k_packet_size(ctrlPacket)) == -1) {
		std::cerr << "initDV3k: error writing product id packet: " << strerror(errno) << std::endl;
		return true;
	}

	if (getresponse(&responsePacket)) {
		std::cerr << "initDV3k: error receiving response to product id request" << std::endl;
		return true;
	}

	if (checkResponse(&responsePacket, DV3K_CONTROL_PRODID)) {
	   std::cerr << "initDV3k: invalid response to product id query" << std::endl;
	   return true;
	}
	strncpy(prodId, responsePacket.payload.ctrl.data.prodid, sizeof(prodId));
	productid.assign(prodId);

	ctrlPacket.field_id = DV3K_CONTROL_VERSTRING;
	if (write(fd, &ctrlPacket, dv3k_packet_size(ctrlPacket)) == -1) {
		std::cerr << "initDV3k: error writing version packet: " << strerror(errno) << std::endl;
		return true;
	}

	if (getresponse(&responsePacket)) {
		std::cerr << "initDV3k: error receiving response to version request" << std::endl;
		return true;
	}

	if (checkResponse(&responsePacket, DV3K_CONTROL_VERSTRING)) {
	   std::cerr << "initDV3k: invalid response to version query" << std::endl;
	   return true;
	}
	strncpy(versionstr, responsePacket.payload.ctrl.data.version, sizeof(version));
	version.assign(versionstr);
	std::cout << "Initialized " << prodId << " version " << version << std::endl;

	ctrlPacket.header.payload_length = htons(13);
	ctrlPacket.field_id = DV3K_CONTROL_RATEP;
	const char *typestr;
	const char *dstartype = "DStar";
	const char *dmrtype = "DMR";
	unsigned char DSTAR_RATE_P[] = { 0x01U, 0x30U, 0x07U, 0x63U, 0x40U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x48U };
	unsigned char DMR_RATE_P[]   = { 0x04U, 0x31U, 0x07U, 0x54U, 0x24U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x6FU, 0x48U };
	switch (dvtype) {
		case DSTAR_TYPE:
			memcpy(ctrlPacket.payload.ctrl.data.ratep, DSTAR_RATE_P, 12);
			typestr = dstartype;
			break;
		case DMR_TYPE:
			memcpy(ctrlPacket.payload.ctrl.data.ratep, DMR_RATE_P, 12);
			typestr = dmrtype;
			break;
		default:
			std::cerr << "initDV3K: unknown DV type" << std::endl;
			return true;
	}

	if (-1 == write(fd, &ctrlPacket, dv3k_packet_size(ctrlPacket))) {
		std::cerr << "initDV3K: error writing " << typestr << " control packet" << std::endl;
		return true;
	}

	if (getresponse(&responsePacket)) {
		std::cerr << "initDV3K: error receiving response to " << typestr << " control packet" << std::endl;
		return true;
	}

	if (checkResponse(&responsePacket, DV3K_CONTROL_RATEP)) {
		std::cerr << "intiDV3K: invalid response to " << typestr << " RATE_P control packet" << std::endl;
		return true;
	}

	return false;
}

void CDV3000U::CloseDevice()
{
	if (fd >= 0) {
		close(fd);
		fd = -1;
	}
}

bool CDV3000U::getresponse(PDV3K_PACKET packet)
{
	ssize_t bytesRead;

	// get the start byte
	packet->start_byte = 0U;
	for (unsigned i = 0U; i < sizeof(DV3K_PACKET); ++i) {
		bytesRead = read(fd, packet, 1);
		if (bytesRead == -1) {
			std::cerr << "CDV3000U: Error reading from serial port: " << strerror(errno) << std::endl;
			return true;
		}
		if (packet->start_byte == DV3K_START_BYTE)
			break;
	}
	if (packet->start_byte != DV3K_START_BYTE) {
		std::cerr << "CDV3000U: Couldn't find start byte in serial data" << std::endl;
		return true;
	}

	// get the packet size and type (three bytes)
	ssize_t bytesLeft = sizeof(packet->header);
	ssize_t total = bytesLeft;
	while (bytesLeft > 0) {
		bytesRead = read(fd, ((uint8_t *) &packet->header) + total - bytesLeft, bytesLeft);
		if(bytesRead == -1) {
			std::cout << "AMBEserver: Couldn't read serial data header" << std::endl;
			return true;
		}
		bytesLeft -= bytesRead;
	}

	total = bytesLeft = ntohs(packet->header.payload_length);
    if (bytesLeft > 1 + int(sizeof(packet->payload))) {
        std::cout << "AMBEserver: Serial payload exceeds buffer size: " << int(bytesLeft) << std::endl;
        return true;
    }

    while (bytesLeft > 0) {
        bytesRead = read(fd, ((uint8_t *) &packet->field_id) + total - bytesLeft, bytesLeft);
        if (bytesRead == -1) {
            std::cerr << "AMBEserver: Couldn't read payload: " << strerror(errno) << std::endl;
            return true;
        }

        bytesLeft -= bytesRead;
    }

    return false;
}

bool CDV3000U::EncodeAudio(const short *audio, unsigned char *data)
{
	if (SendAudio(audio))
		return true;
	if (GetData(data))
		return true;
	return false;
}

bool CDV3000U::SendAudio(const short *audio)
{
	// Create Audio packet based on input short ints
	DV3K_PACKET p;
	p.start_byte = DV3K_START_BYTE;
	const unsigned short int len = 322;
	p.header.payload_length = htons(len);
	p.header.packet_type = DV3K_TYPE_AUDIO;
	p.field_id = 0U;
	p.payload.audio.num_samples = 160U;
	for (int i=0; i<160; i++)
		p.payload.audio.samples[i] = htons(audio[i]);

	// send audio packet to DV3000
	int size = dv3k_packet_size(p);
	if (write(fd, &p, size) != size) {
		std::cerr << "Error sending audio packet" << std::endl;
		return true;
	}
	return false;
}

bool CDV3000U::GetData(unsigned char *data)
{
	DV3K_PACKET p;
	// read data packet from DV3000
	p.start_byte = 0U;
	if (getresponse(&p))
		return true;
	if (p.start_byte!=DV3K_START_BYTE || htons(p.header.payload_length)!=11 ||
			p.header.packet_type!=DV3K_TYPE_AMBE || p.field_id!=1U ||
			p.payload.ambe.num_bits!=72U) {
		std::cerr << "Error receiving audio packet response" << std::endl;
		dump("Received AMBE", &p, dv3k_packet_size(p));
		return true;
	}

	// copy it to the output
	memcpy(data, p.payload.ambe.data, 9);

	return false;
}

bool CDV3000U::DecodeData(const unsigned char *data, short *audio)
{
	if (SendData(data))
		return true;
	if (GetAudio(audio))
		return true;
	return false;
}

bool CDV3000U::SendData(const unsigned char *data)
{
	// Create data packet
	DV3K_PACKET p;
	p.start_byte = DV3K_START_BYTE;
	p.header.payload_length = htons(11);
	p.header.packet_type = DV3K_TYPE_AMBE;
	p.field_id = 1U;
	p.payload.ambe.num_bits = 72U;
	memcpy(p.payload.ambe.data, data, 9);

	// send data packet to DV3000
	int size = dv3k_packet_size(p);
	if (write(fd, &p, size) != size) {
		std::cerr << "SendData: error sending data packet" << std::endl;
		dump("Received Data", &p, size);
		return true;
	}
	return false;
}

bool CDV3000U::GetAudio(short *audio)
{
	DV3K_PACKET p;
	// read audio packet from DV3000
	p.start_byte = 0U;
	if (getresponse(&p))
		return true;
	if (p.start_byte!=DV3K_START_BYTE || htons(p.header.payload_length)!=322 ||
			p.header.packet_type!=DV3K_TYPE_AUDIO || p.field_id!=0U ||
			p.payload.audio.num_samples!=160U) {
		std::cerr << "GetAudio: unexpected audio packet response" << std::endl;
		int size = dv3k_packet_size(p);
		dump("Received Audio", &p, size);
		return true;
	}

	for (int i=0; i<160; i++)
		audio[i] = ntohs(p.payload.audio.samples[i]);

	return false;
}

void CDV3000U::dump(const char *title, void *pointer, int length)
{
	assert(title != NULL);
	assert(pointer != NULL);

	const unsigned char *data = (const unsigned char *)pointer;

	std::cout << title << std::endl;

	unsigned int offset = 0U;

	while (length > 0) {

		unsigned int bytes = (length > 16) ? 16U : length;

		for (unsigned i = 0U; i < bytes; i++) {
			if (i)
				std::cout << " ";
			std::cout << std::hex << std::setw(2) << std::right << std::setfill('0') << int(data[offset + i]);
		}

		for (unsigned int i = bytes; i < 16U; i++)
			std::cout << "   ";

		std::cout << "   *";

		for (unsigned i = 0U; i < bytes; i++) {
			unsigned char c = data[offset + i];

			if (::isprint(c))
				std::cout << c;
			else
				std::cout << '.';
		}

		std::cout << '*' << std::endl;

		offset += 16U;

		if (length >= 16)
			length -= 16;
		else
			length = 0;
	}
}
