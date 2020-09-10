/*---------------------------------------------------------------------------*\

  FILE........: c2dec.c
  AUTHOR......: David Rowe
  DATE CREATED: 23/8/2010

  Decodes a file of bits to a file of raw speech samples using codec2.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2010 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <memory>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "codec2.h"

void print_help(char* argv[]);

int main(int argc, char *argv[])
{
	int            mode=0;
	FILE          *fin;
	FILE          *fout;


	if (argc < 4)
		print_help(argv);

	if (strcmp(argv[2], "-")  == 0) fin = stdin;
	else if ( (fin = fopen(argv[2],"rb")) == NULL )
	{
		fprintf(stderr, "Error opening input bit file: %s: %s.\n", argv[2], strerror(errno));
		exit(1);
	}

	if (strcmp(argv[3], "-") == 0) fout = stdout;
	else if ( (fout = fopen(argv[3],"wb")) == NULL )
	{
		fprintf(stderr, "Error opening output speech file: %s: %s.\n", argv[3], strerror(errno));
		exit(1);
	}

	// If we got here, we need to honor the command line mode
	if (strcmp(argv[1],"3200") == 0)
		mode = 3200;
	else if (strcmp(argv[1],"1600") == 0)
		mode = 1600;
	else
	{
		fprintf(stderr, "Error in mode: %s.  Must be 3200 or 1600\n", argv[1]);
		exit(1);
	}


	CCodec2 cc2(mode == 3200);
	int nsam = cc2.codec2_samples_per_frame();
	int nbit = cc2.codec2_bits_per_frame();
	short buf[nsam];
	int nbyte = (nbit + 7) / 8;
	unsigned char bits[nbyte];

	auto ret = (fread(bits, sizeof(char), nbyte, fin) == (size_t)nbyte);

	while(ret)
	{
		cc2.codec2_decode(buf, bits);
		fwrite(buf, sizeof(short), nsam, fout);

		//if this is in a pipeline, we probably don't want the usual
		//buffering to occur

		if (fout == stdout) fflush(stdout);
		if (fin == stdin) fflush(stdin);

		ret = (fread(bits, sizeof(char), nbyte, fin) == (size_t)nbyte);
	}
	fclose(fin);
	fclose(fout);

	return 0;
}

void print_help(char* argv[])
{
	int i;
	const char *option_parameters;
	fprintf(stderr, "\nc2dec - Codec 2 decoder and bit error simulation program\n"
			"usage: %s 3200|1600 InputFile OutputRawFile\n\n"
			"Options:\n", argv[0]);
	exit(1);
}
