/*
 *   Copyright (C) 2020 by Thomas Early N7TAE
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

#include <ctime>
#include <cstdio>
#include <cstdarg>

#include "QnetLog.h"

CQnetLog::CQnetLog()
{
	LogInput.SetUp("log_input");
}

void CQnetLog::SendLog(const char *fmt, ...)
{
	time_t ltime;
	struct tm tm;
	char buf[256];

	time(&ltime);
	localtime_r(&ltime, &tm);

	std::snprintf(buf ,255,"%d:%02d:%02d: ", tm.tm_hour, tm.tm_min, tm.tm_sec);

	va_list args;
	va_start(args,fmt);
	vsnprintf(buf + strlen(buf), 256 - strlen(buf) -1, fmt, args);
	va_end(args);

	LogInput.Write(buf, strlen(buf)+1);
	return;
}
