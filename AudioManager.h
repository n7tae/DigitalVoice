/*
 *   Copyright (c) 2019-2020 by Thomas A. Early N7TAE
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
#include "Packet.h"
#include "Random.h"
#include "UnixDgramSocket.h"

using DSVTPacketQueue = CTQueue<CDSVT>;
using M17PacketQueue = CTQueue<M17_IPFrame>;
#define M17CHARACTERS " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."

enum class E_PTT_Type { echo, gateway, link, m17 };

class CMainWindow;

class CAudioManager
{
public:
	CAudioManager();
	bool Init(CMainWindow *);

	void RecordMicThread(E_PTT_Type for_who, const std::string &urcall);
	void PlayEchoDataThread();	// for Echo
	void Gateway2AudioMgr(const CDSVT &dsvt);
	void Link2AudioMgr(const CDSVT &dsvt);
	void M17_2AudioMgr(const M17_IPFrame &m17);
	void KeyOff();
	void PlayFile(const char *filetoplay);
	void QuickKey(const char *urcall);
	void Link(const std::string &linkcmd);
	void EncodeCallsign(uint8_t *out, const std::string &callsign);
	void DecodeCallsign(std::string &cs, const uint8_t *in);

	// the ambe device is well protected so it can be public
	CDV3000U AMBEDevice;

private:
	// data
	std::atomic<bool> hot_mic, play_file;
	std::atomic<unsigned short> gate_sid_in, link_sid_in, m17_sid_in;
	CAudioQueue audio_queue;
	CAmbeDataQueue ambe_queue;
	CC2DataQueue c2_queue;
	DSVTPacketQueue gateway_queue, link_queue;
	CAmbeSeqQueue a2d_queue, d2a_queue;
	std::mutex audio_mutex, ambe_mutex, a2d_mutex, d2a_mutex, gateway_mutex, link_mutex, l2am_mutex;
	std::future<void> r1, r2, r3, r4, p1, p2, p3;
	bool link_open;
	const std::string m17_alphabet{M17CHARACTERS};

	// Unix sockets
	CUnixDgramWriter AM2M17, AM2Gate, AM2Link, LogInput;
	// helpers
	CMainWindow *pMainWindow;
	CRandom random;
	void l2am(const CDSVT &dsvt, const bool shutoff);
	std::vector<unsigned long> speak;
	// methods
	void calcPFCS(const unsigned char *packet, unsigned char *pfcs);
	bool audio_is_empty();
	bool ambe_is_empty();
	bool codec_is_empty();
	void microphone2audioqueue();
	void audioqueue2ambedevice();
	void ambedevice2ambequeue();
	void ambequeue2ambedevice();
	void ambedevice2audioqueue();
	void codec2encode(const bool is_3200);
	void codec2decode(const bool is_3200);
	void codec2m17gateway();
	void ambedevice2packetqueue(DSVTPacketQueue &queue, std::mutex &mtx, const std::string &urcall);
	void packetqueue2link();
	void packetqueue2gate();
	void play_audio_queue();
	void makeheader(CDSVT &c, const std::string &urcall, unsigned char *ut, unsigned char *uh);
	void SlowData(const unsigned count, const unsigned char *ut, const unsigned char *uh, CDSVT &v);
};
