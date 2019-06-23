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

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

#include <iostream>

#include "AudioManager.h"

CAudioManager AudioManager;

void CAudioManager::RecordMicThread()
{
	hot_mic = true;
	record_mic_thread = std::async(std::launch::async, &CAudioManager::record_mic, this);
}

bool CAudioManager::record_mic()
{
	// Open PCM device for recording (capture).
	snd_pcm_t *handle;
	int rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		std::cerr << "unable to open pcm device: " << snd_strerror(rc) << std::endl;
		return true;
	}

	// Allocate a hardware parameters object.
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);

	// Fill it in with default values.
	snd_pcm_hw_params_any(handle, params);

	// Set the desired hardware parameters.

	// Interleaved mode
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	// Signed 16-bit little-endian format
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	// One channels (mono)
	snd_pcm_hw_params_set_channels(handle, params, 1);

	// 8000 samples/second
	snd_pcm_hw_params_set_rate(handle, params, 8000, 0);

	// Set period size to 160 frames.
	snd_pcm_uframes_t frames = 160;
	snd_pcm_hw_params_set_period_size(handle, params, frames, 0);

	// Write the parameters to the driver
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		std::cerr << "unable to set hw parameters: " << snd_strerror(rc) << std::endl;
		return true;
	}

	while (hot_mic) {
		short audio_buffer[frames];
		rc = snd_pcm_readi(handle, audio_buffer, frames);
		if (rc == -EPIPE) {
			// EPIPE means overrun
			std::cerr << "overrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr << "error from readi: " << snd_strerror(rc) << std::endl;
		} else if (rc != int(frames)) {
			std::cerr << "short readi, read " << rc << " frames" << std::endl;
		}
		unsigned char ambe_buffer[9];
		if (AMBEDevice.EncodeAudio(audio_buffer, ambe_buffer))
			continue;
		ambe_mutex.lock();
		ambe_queue.Push(CAMBEFrame(ambe_buffer));
		ambe_mutex.unlock();
	}
	//std::cerr << "snd_pcm_drain\n";
	//snd_pcm_drain(handle);
	snd_pcm_close(handle);
	return false;
}

void CAudioManager::PlayAMBEDataThread()
{
	hot_mic = false;
	if (record_mic_thread.get())
		return;	// there was trouble with the recording

	std::async(std::launch::async, &CAudioManager::decode_ambe, this);
	std::cout << "CAudioManager::decode_ambe launched\n";
	std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
	std::cout << "CAudioManager::play_audio_queue launched\n";
}

void CAudioManager::play_audio_queue()
{
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	// Open PCM device for playback.
	snd_pcm_t *handle;
	int rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		std::cerr << "unable to open pcm device: " << snd_strerror(rc) << std::endl;
		exit(1);
	}

	// Allocate a hardware parameters object.
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	// Set the desired hardware parameters.

	// Interleaved mode
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	// Signed 16-bit little-endian format
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);

	// One channels (mono)
	snd_pcm_hw_params_set_channels(handle, params, 1);

	// 8000 samples/second sampling rate
	snd_pcm_hw_params_set_rate(handle, params, 8000, 0);

	// Set period size to 32 frames.
	snd_pcm_uframes_t frames = 160;
	snd_pcm_hw_params_set_period_size(handle, params, frames, 0);
	//snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

	// Write the parameters to the driver
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		std::cerr << "unable to set hw parameters: " << snd_strerror(rc) << std::endl;
		exit(1);
	}

	// Use a buffer large enough to hold one period
	snd_pcm_hw_params_get_period_size(params, &frames, 0);

	while (! is_audio_empty()) {
		//short audio_buffer[frames];
		//CAMBEFrame frame = mic_ambe_data.Pop();
		//if (AMBEDevice.DecodeData(frame.GetData(), audio_buffer))
		//	continue;

		audio_mutex.lock();
		CAudioFrame frame = audio_queue.Pop();
		audio_mutex.unlock();
		rc = snd_pcm_writei(handle, frame.GetData(), frames);
		if (rc == -EPIPE) {
			// EPIPE means underrun
			std::cerr << "underrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr <<  "error from writei: " << snd_strerror(rc) << std::endl;
		}  else if (rc != int(frames)) {
			std::cerr << "short write, write " << rc << " frames" << std::endl;
		}
	}

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

void CAudioManager::decode_ambe()
{
	while (! is_ambe_empty()) {
		short audio_buffer[160];
		ambe_mutex.lock();
		CAMBEFrame frame = ambe_queue.Pop();
		ambe_mutex.unlock();
		AMBEDevice.DecodeData(frame.GetData(), audio_buffer);
		audio_mutex.lock();
		audio_queue.Push(CAudioFrame(audio_buffer));
		audio_mutex.unlock();
	}
}

bool CAudioManager::is_audio_empty()
{
	audio_mutex.lock();
	bool ret = audio_queue.Empty();
	audio_mutex.unlock();
	return ret;
}

bool CAudioManager::is_ambe_empty()
{
	ambe_mutex.lock();
	bool ret = ambe_queue.Empty();
	ambe_mutex.unlock();
	return ret;
}
