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

#include "DV3000U.h"
#include "TemplateClasses.h"
#include "QnetTypeDefs.h"
#include "Random.h"

using PacketQueue = CTQueue<CDVST>;

enum class E_PTT_Type { echo, gateway, link };

class CAudioManager
{
public:
	CAudioManager() : hot_mic(false), gate_sid_in(0U), link_sid_in(0U) {}
	~CAudioManager() {}

	void RecordMicThread(E_PTT_Type for_who, const std::string &urcall);
	void PlayAMBEDataThread();	// for Echo
	void Gateway2AudioMgr(const CDVST &dvst);
	void Link2AudioMgr(const CDVST &dvst);
	bool GatewayQueueIsReady();
	bool LinkQueueIsReady();
	void GetPacket4Gateway(CDVST &packet);
	void GetPacket4Link(CDVST &packet);

	// the ambe device is well protected so it can be public
	CDV3000U AMBEDevice;

private:
	// data
	std::atomic<bool> hot_mic;
	std::atomic<unsigned short> gate_sid_in, link_sid_in;
	CAudioQueue audio_queue;
	CAMBEQueue ambe_queue;
	PacketQueue gateway_queue, link_queue;
	CSequenceQueue a2d_queue, d2a_queue;
	std::mutex audio_mutex, ambe_mutex, a2d_mutex, d2a_mutex, gateway_mutex, link_mutex;
	std::future<void> r1, r2, r3, p1, p2, p3;
	// helpers
	CRandom random;
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
	void play_audio_queue();
	void makeheader(CDVST &c, const std::string &urcall);
};
