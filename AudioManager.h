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

class CAudioManager
{
public:
	CAudioManager() : hot_mic(false) {}
	~CAudioManager() {}

	void RecordMicThread();
	void PlayAMBEDataThread();

	// the ambe device is protected so it can be public
	CDV3000U AMBEDevice;

private:
	// data
	std::atomic<bool> hot_mic;
	std::future<bool> record_mic_thread;
	CAudioQueue audio_queue;
	CAMBEQueue ambe_queue;
	std::mutex audio_mutex, ambe_mutex;
	// methods
	bool record_mic();
	void play_audio_queue();
	void decode_ambe();
	bool is_audio_empty();
	bool is_ambe_empty();
};
