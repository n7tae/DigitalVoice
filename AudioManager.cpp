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
	stream_size = 0U;
	audio_queue.Clear();
	ambe_queue.Clear();
	a2d_queue.Clear();


	r1 = std::async(std::launch::async, &CAudioManager::microphone2audioqueue, this);

	r2 = std::async(std::launch::async, &CAudioManager::audioqueue2ambedevice, this);

	r3 = std::async(std::launch::async, &CAudioManager::ambedevice2ambequeue, this);
}

void CAudioManager::microphone2audioqueue()
{
	// Open PCM device for recording (capture).
	snd_pcm_t *handle;
	int rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		std::cerr << "unable to open pcm device: " << snd_strerror(rc) << std::endl;
		return;
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
		return;
	}

	unsigned count = 0U;
	bool keep_running;
	do {
		short int audio_buffer[frames];
		rc = snd_pcm_readi(handle, audio_buffer, frames);
		//std::cout << "audio:" << count << " hot_mic:" << hot_mic << std::endl;
		if (rc == -EPIPE) {
			// EPIPE means overrun
			std::cerr << "overrun occurred" << std::endl;
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			std::cerr << "error from readi: " << snd_strerror(rc) << std::endl;
		} else if (rc != int(frames)) {
			std::cerr << "short readi, read " << rc << " frames" << std::endl;
		}
		keep_running = hot_mic;
		unsigned char seq = count % 21;
		if (! keep_running)
			seq |= 0x40U;
		CAudioFrame frame(audio_buffer);
		frame.SetSequence(seq);
		audio_mutex.lock();
		audio_queue.Push(frame);
		audio_mutex.unlock();
		count++;
	} while (keep_running);
//	std::cout << count << " frames by microphone2audioqueue\n";
	snd_pcm_drop(handle);
	snd_pcm_close(handle);
	stream_size = count;
}

void CAudioManager::audioqueue2ambedevice()
{
	unsigned char seq = 0U;
	do {
		while (audio_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		audio_mutex.lock();
		CAudioFrame frame(audio_queue.Pop());
		audio_mutex.unlock();
		if (! AMBEDevice.IsOpen())
			return;
		if(AMBEDevice.SendAudio(frame.GetData()))
			break;
		seq = frame.GetSequence();
		a2d_mutex.lock();
		a2d_queue.Push(frame.GetSequence());
		a2d_mutex.unlock();
		//std::cout << "audio2ambedev seq:" << std::hex << unsigned(seq) << std::dec << std::endl;
	} while (0U == (seq & 0x40U));
//	std::cout << "audioqueue2ambedevice is finished\n";
}

void CAudioManager::ambedevice2ambequeue()
{
	unsigned char seq = 0U;
	do {
		unsigned char ambe[9];
		if (! AMBEDevice.IsOpen())
			return;
		if (AMBEDevice.GetData(ambe))
			break;
		CAMBEFrame frame(ambe);
		a2d_mutex.lock();
		seq = a2d_queue.Pop();
		a2d_mutex.unlock();
		frame.SetSequence(seq);
		ambe_mutex.lock();
		ambe_queue.Push(frame);
		ambe_mutex.unlock();
		//std::cout << "ambedev2ambeque seq:" << std::hex << unsigned(seq) << std::dec << std::endl;
	} while (0U == (seq & 0x40U));
//	std::cout << "amebedevice2ambequeue is finished\n";
	return;
}

void CAudioManager::PlayAMBEDataThread()
{
	hot_mic = false;
	r3.get();

	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	p1 = std::async(std::launch::async, &CAudioManager::ambequeue2ambedevice, this);

	p2 = std::async(std::launch::async, &CAudioManager::ambedevice2audioqueue, this);

	p3 = std::async(std::launch::async, &CAudioManager::play_audio_queue, this);
}

void CAudioManager::ambequeue2ambedevice()
{
	unsigned char seq = 0U;
	do {
		while (ambe_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		ambe_mutex.lock();
		CAMBEFrame frame(ambe_queue.Pop());
		ambe_mutex.unlock();
		seq = frame.GetSequence();
		d2a_mutex.lock();
		d2a_queue.Push(seq);
		d2a_mutex.unlock();
		if (! AMBEDevice.IsOpen())
			return;
		if (AMBEDevice.SendData(frame.GetData()))
			return;
	} while (0U == (seq & 0x40U));
//	std::cout << "ambequeue2ambedevice is complete\n";
}

void CAudioManager::ambedevice2audioqueue()
{
	unsigned char seq = 0U;
	do {
		if (! AMBEDevice.IsOpen())
			return;
		short audio[160];
		if (AMBEDevice.GetAudio(audio))
			return;
		CAudioFrame frame(audio);
		d2a_mutex.lock();
		seq = d2a_queue.Pop();
		d2a_mutex.unlock();
		frame.SetSequence(seq);
		audio_mutex.lock();
		audio_queue.Push(frame);
		audio_mutex.unlock();
	} while ( 0U == (seq & 0x40U));
//	std::cout << "ambedevice2audioqueue is complete\n";
}

void CAudioManager::play_audio_queue()
{
	// Open PCM device for playback.
	snd_pcm_t *handle;
	int rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		std::cerr << "unable to open pcm device: " << snd_strerror(rc) << std::endl;
		return;
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
		return;
	}

	// Use a buffer large enough to hold one period
	snd_pcm_hw_params_get_period_size(params, &frames, 0);

	unsigned char seq = 0U;
	do {
		while (audio_is_empty())
			std::this_thread::sleep_for(std::chrono::milliseconds(3));
		audio_mutex.lock();
		CAudioFrame frame(audio_queue.Pop());
		audio_mutex.unlock();
		seq = frame.GetSequence();
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
	} while (0U == (seq & 0x40U));

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

bool CAudioManager::audio_is_empty()
{
	audio_mutex.lock();
	bool ret = audio_queue.Empty();
	audio_mutex.unlock();
	return ret;
}

bool CAudioManager::ambe_is_empty()
{
	ambe_mutex.lock();
	bool ret = ambe_queue.Empty();
	ambe_mutex.unlock();
	return ret;
}
