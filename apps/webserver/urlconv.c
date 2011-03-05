/*
 * Copyright (c) 2010, Kajtar Zsolt <soci@c64.rulez.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * Author: Kajtar Zsolt <soci@c64.rulez.org>
 *
 * $Id: urlconv.c,v 1.4 2010-09-29 09:35:56 oliverschmidt Exp $
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h> // for debug only
#include <avr/pgmspace.h>

#include "urlconv.h"

#ifndef __AVR__
#define PSTR(x) (x)
#define printf_P(...) printf(__VA_ARGS__)
#endif

/*---------------------------------------------------------------------------*/
/* URL to filename conversion
 *
 * normalizes path by removing "/./"
 * interprets "/../" and calculates path accordingly
 * resulting path is always absolute
 * replaces multiple slashes with a single one
 * replaces "%AB" notation with characters
 * strips "#fragment" and "?query" from end
 *
 * MAXLEN is including trailing zero!
 * input and output is ASCII
 */
void urlconv_tofilename(char *out, const char *in, unsigned char maxlen) {
	int idx = 0;

	if (maxlen == 0) {
		return;
	}
	else {
		maxlen--;
	}

	printf_P(PSTR("IN:%s "), in);

	// Output path is always absolute
	out[idx++] = '/';

	while (*in && idx <= maxlen) {
		int len;

		// Work out the length of this path element
		char *term = strchrnul(in, '/');
		len = term - in;

		if (len == 0 || (len == 1 && in[0] == '.')) {
			// noop segment (/ or ./) so skip it
		}
		else if (len == 2 && in[0] == '.' && in[1] == '.') {
			// backpath (../)

			if (idx == 1) {
				// attempt to move above root (noop)
			}
			else {
				// crop the prior segment
				do {
					--idx;
				} while (idx && out[idx - 1] != '/');
			}
		}
		else {
			// An actual segment, append it to the destination path

			if (*term) {
				len++;
			}

			int i;
			for (i = 0; i < len; i++) {
				if (idx >= maxlen) {
					break;
				}
				else if (in[i] == '%') {
					char c;
					uint8_t hex1 = (in[++i] | 0x20) ^ 0x30;  // ascii only!

					if (hex1 > 0x50 && hex1 < 0x57)
						hex1 -= 0x47;
					else if (hex1 > 9)
						break;  // invalid hex

					c = (in[++i] | 0x20) ^ 0x30;  // ascii only!

					if (c > 0x50 && c < 0x57)
						c -= 0x47;
					else if (c > 9)
						break;  // invalid hex

					c |= hex1 << 4;

					out[idx++] = c;
				}
				else if (in[i] == '#' || in[i] == '?') {
					break;
				}
				else {
					out[idx++] = in[i];
				}
			}

			if (i != len) {
				break;
			}

			//memcpy(&out[idx], in, len);
			//idx += len;
		}

		// Skip to the next path segment
		if (*term) {
			term++;
		}
		in = term;
	}

	// Null terminate the output
	out[idx] = '\0';

	printf_P(PSTR("OUT:%s\n"), out);
}

