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

class CAudioManager
{
public:
	CAudioManager() : hot_mic(false) {}
	~CAudioManager() {}

	void RecordMicThread();
	void PlayAMBEDataThread();
	void DSVTPacket2Audio(const SDSVT &dvst);
	const SDSVT *Audio2Network();

	// the ambe device is protected so it can be public
	CDV3000U AMBEDevice;

private:
	// data
	std::atomic<bool> hot_mic;
	std::atomic<unsigned> stream_size;
	CAudioQueue audio_queue;
	CAMBEQueue ambe_queue;
	CSequenceQueue a2d_queue, d2a_queue;
	std::mutex audio_mutex, ambe_mutex, a2d_mutex, d2a_mutex;
	std::future<void> r1, r2, r3, p1, p2, p3;
	// methods
	bool audio_is_empty();
	bool ambe_is_empty();
	void microphone2audioqueue();
	void audioqueue2ambedevice();
	void ambedevice2ambequeue();
	void ambequeue2ambedevice();
	void ambedevice2audioqueue();
	void play_audio_queue();
};
