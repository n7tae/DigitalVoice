/*
 *   Copyright (C) 2019 by Thomas A. Early N7TAE
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

#include <ctime>
#include <chrono>
#include <cmath>
#include <string>

class CTimer
{
public:
	CTimer() { Restart(); }
	~CTimer() {}
	void Restart() {
		starttime = std::chrono::steady_clock::now();
	}
	double Time() {
		std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - starttime;
		return double(elapsed.count()) * std::chrono::steady_clock::period::num / std::chrono::steady_clock::period::den;
	}

	std::string TimeString()
	{
		std::string ret;
		double t = Time();
		if (t >= 60.0) {
			double minutes = t / 60.0;
			double integer;
			double seconds = 60.0 * modf(minutes, &integer);
			seconds = round(10.0 * seconds) / 10.0;
			ret.assign(std::to_string(int(minutes)));
			ret += " min " + std::to_string(seconds) + " sec";
		} else {
			ret = std::to_string(round(10.0 * t) / 10.0) + " sec";
		}
		return ret;
	}

private:
	std::chrono::steady_clock::time_point starttime;
};
