/*
 * This file is part of the PolyController firmware source code.
 * Copyright (C) 2011 Chris Boot.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifndef TIME_H_
#define TIME_H_

#include <avr/pgmspace.h>

typedef int32_t time_t;

struct tm {
	uint8_t tm_sec;           /* Seconds. [0-60] (1 leap second) */
	uint8_t tm_min;           /* Minutes. [0-59] */
	uint8_t tm_hour;          /* Hours.   [0-23] */
	uint8_t tm_mday;          /* Day.     [1-31] */
	uint8_t tm_mon;           /* Month.   [0-11] */
	uint8_t tm_year;          /* Year - 1900.  */
	uint8_t tm_wday;          /* Day of week. [0-6] */
	uint16_t tm_yday : 9;          /* Days in year.[0-365] */
};

size_t strftime_P(
	char *s,
	size_t size,
	PGM_P fmt,
	const struct tm *tm);

struct tm *gmtime(time_t time, struct tm *tm);
time_t mktime(const struct tm * const tmp);
int tm_valid(const struct tm *tm);

#endif

