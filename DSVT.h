#pragma once
/*
 *   Copyright 2017-2019 by Thomas Early, N7TAE
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

// for the g2 external port and between QnetGateway programs
#pragma pack(push, 1)
class CDSVT
{
public:
	unsigned char title[4];	//  0   "DSVT"
	unsigned char config;	//  4   0x10 is hdr 0x20 is vasd
	unsigned char flaga[3];	//  5   zeros
	unsigned char id;		//  8   0x20
	unsigned char flagb[3];	//  9   0x0 0x1 (A:0x3 B:0x1 C:0x2)
	unsigned short streamid;// 12
	unsigned char ctrl;		// 14   hdr: 0x80 vsad: framecounter (mod 21)
	union {
		struct {                    // index
			unsigned char flag[3];  // 15
			unsigned char rpt1[8];	// 18
			unsigned char rpt2[8];  // 26
			unsigned char urcall[8];// 34
			unsigned char mycall[8];// 42
			unsigned char sfx[4];   // 50
			unsigned char pfcs[2];  // 54
		} hdr;						// total 56
		struct {
			unsigned char voice[9]; // 15
			unsigned char text[3];  // 24
		} vasd;	// voice and slow data total 27
	};
};
#pragma pack(pop)

#pragma pack(push, 1)
class CREFDSVT
{
public:
	unsigned char head[2];
	CDSVT dsvt;
};
#pragma pack(pop)

#pragma pack(push, 1)
class CLinkFamily
{
public:
    char title[4];
    int family;
};
#pragma pack(pop)
