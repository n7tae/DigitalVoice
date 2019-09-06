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

template <class T, int N> class CTFrame
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

	CTFrame(const CTFrame<T, N> &from)
	{
		memcpy(data, from.GetData(), N *sizeof(T));
		sequence = from.GetSequence();
	}

	CTFrame<T, N> &operator=(const CTFrame<T, N> &from)
	{
		memcpy(data, from.GetData(), N * sizeof(T));
		sequence = from.GetSequence();
		return *this;
	}

	const T *GetData() const
	{
		return data;
	}

	unsigned char GetSequence() const
	{
		return sequence;
	}

	void SetSequence(unsigned char s)
	{
		sequence = s;
	}

	~CTFrame() {}
private:
	T data[N];
	unsigned char sequence;
};

template <class T> class CTQueue
{
public:
	CTQueue() {}

	~CTQueue()
	{
		Clear();
	}

	void Push(T item)
	{
		queue.push(item);
	}

	T Pop()
	{
		T item = queue.front();
		queue.pop();
		return item;
	}

	bool Empty()
	{
		return queue.empty();
	}

	void Clear()
	{
		while (queue.size())
			queue.pop();
	}

private:
	std::queue<T> queue;
};

using CAMBEFrame = CTFrame<unsigned char, 9>;
using CAMBEQueue = CTQueue<CAMBEFrame>;
using CAudioFrame = CTFrame<short int, 160>;
using CAudioQueue = CTQueue<CAudioFrame>;
using CSequenceQueue = CTQueue<unsigned char>;
