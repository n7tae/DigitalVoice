/*
 *   Copyright (c) 2019 by Thomas A. Early N7TAE
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

#include <string>
#include <future>
#include <atomic>
#include <mutex>
#include <vector>

#include "DV3000U.h"
#include "TemplateClasses.h"
#include "QnetTypeDefs.h"
#include "Random.h"
#include "UnixDgramSocket.h"

using PacketQueue = CTQueue<CDSVT>;

enum class E_PTT_Type { echo, gateway, link };

class CAudioManager
{
public:
	CAudioManager();
	~CAudioManager() {}

	void RecordMicThread(E_PTT_Type for_who, const std::string &urcall);
	void PlayAMBEDataThread();	// for Echo
	void Gateway2AudioMgr(const CDSVT &dsvt);
	void Link2AudioMgr(const CDSVT &dsvt);
	void KeyOff();
	void PlayFile(const char *filetoplay);

	// the ambe device is well protected so it can be public
	CDV3000U AMBEDevice;

private:
	// data
	std::atomic<bool> hot_mic, play_file;
	std::atomic<unsigned short> gate_sid_in, link_sid_in;
	CAudioQueue audio_queue;
	CAMBEQueue ambe_queue;
	PacketQueue gateway_queue, link_queue;
	CSequenceQueue a2d_queue, d2a_queue;
	std::mutex audio_mutex, ambe_mutex, a2d_mutex, d2a_mutex, gateway_mutex, link_mutex;
	std::future<void> r1, r2, r3, r4, p1, p2, p3;
	// helpers
	CRandom random;
	std::vector<unsigned long> speak;
	// Unix sockets
	CUnixDgramWriter AM2Gate, AM2Link, LogInput;
	// methods
	void calcPFCS(const unsigned char *packet, unsigned char *pfcs);
	bool audio_is_empty();
	bool ambe_is_empty();
	void microphone2audioqueue();
	void audioqueue2ambedevice();
	void ambedevice2ambequeue();
	void ambequeue2ambedevice();
	void ambedevice2audioqueue();
	void ambedevice2packetqueue(PacketQueue &queue, std::mutex &mtx, const std::string &urcall);
	void packetqueue2link();
	void packetqueue2gate();
	void play_audio_queue();
	void makeheader(CDSVT &c, const std::string &urcall, unsigned char *ut, unsigned char *uh);
	void SlowData(const unsigned count, const unsigned char *ut, const unsigned char *uh, CDSVT &v);
};
