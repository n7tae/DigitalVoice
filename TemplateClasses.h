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

#include <queue>
#include <string>

#include "HostQueue.h"

template <class T, class U, int N> class CTFrame
{
public:
	CTFrame()
	{
		memset(data, 0, N * sizeof(T));
		sequence = 0U;
	}

	CTFrame(const T *from)
	{
		memcpy(data, from, N * sizeof(T));
		sequence = 0U;
	}

	CTFrame(const CTFrame<T, U, N> &from)
	{
		memcpy(data, from.GetData(), N *sizeof(T));
		sequence = from.GetSequence();
	}

	CTFrame<T, U, N> &operator=(const CTFrame<T, U, N> &from)
	{
		memcpy(data, from.GetData(), N * sizeof(T));
		sequence = from.GetSequence();
		return *this;
	}

	const T *GetData() const
	{
		return data;
	}

	U GetSequence() const
	{
		return sequence;
	}

	unsigned int Size() const
	{
		return sizeof(data) / sizeof(T);
	}

	void SetSequence(unsigned char s)
	{
		sequence = s;
	}

private:
	T data[N];
	U sequence;
};

// AMBE
using CAmbeDataFrame = CTFrame<unsigned char, unsigned char, 9>;
using CAmbeDataQueue = CTQueue<CAmbeDataFrame>;
using CAmbeAudioFrame = CTFrame<short int, unsigned char, 160>;
using CAmbeAudioQueue = CTQueue<CAmbeAudioFrame>;
using CUByteSeqQueue = CTQueue<unsigned char>;

// M17
using C3200DataFrame = CTFrame<unsigned char, unsigned short, 16>;
using C3200DataQueue = CTQueue<C3200DataFrame>;
using C1600DataFrame = CTFrame<unsigned char, unsigned short, 8>;
using C1600DataQueue = CTQueue<C1600DataFrame>;
using CM17AudioFrame = CTFrame<short, unsigned short, 320>;
using CM17AudioQueue = CTQueue<CM17AudioFrame>;
using CShortSeqQueue = CTQueue<unsigned short>;
