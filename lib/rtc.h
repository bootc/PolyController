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

#ifndef __RTC_H__
#define __RTC_H__

/*
 * RTC library functions
 */

#define NTP_TO_UNIX_OFFSET 2208988800UL
#define NTP_TO_UNIX(time) ((time) - NTP_TO_UNIX_OFFSET)
#define UNIX_TO_NTP(time) ((time) + NTP_TO_UNIX_OFFSET)

struct rtc_time {
	uint8_t		sec;	// 00-59
	uint8_t		min;	// 00-59
	uint8_t		hour;	// 00-23
	uint8_t		mday;	// 01-<days in month>
	uint8_t		mon;	// 00-11
	uint16_t	year;	// 1970-
	uint8_t		wday;	// 0-6, 0=Sunday
};


/**
 * Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59. 
 */
uint32_t mktime(
	const uint16_t year, const uint8_t mon,
	const uint8_t day, const uint8_t hour,
	const uint8_t min, const uint8_t sec);

/*
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
void rtc_time_to_tm(uint32_t time, struct rtc_time *tm);

/*
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 */
uint32_t rtc_tm_to_time(const struct rtc_time *tm);

/*
 * Does the rtc_time represent a valid date/time?
 */
int rtc_valid_tm(const struct rtc_time *tm);

#endif
