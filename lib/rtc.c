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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdint.h>
#include <avr/pgmspace.h>

#include "rtc.h"

/*
 * All of the code here has been cribbed from the Linux kernel. There have been
 * some modifications, generally to reduce memory usage (smaller types,
 * pgmspace usage, that sort of thing).
 */

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)

static const uint8_t rtc_days_in_month[] PROGMEM = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static inline uint8_t is_leap_year(uint16_t year) {
	return (!(year % 4) && (year % 100)) || !(year % 400);
}

static inline uint8_t rtc_month_days(uint8_t month, uint16_t year) {
	return pgm_read_byte(&rtc_days_in_month[month]) +
		(is_leap_year(year) && month == 1);
}

/*
 * Converts Gregorian date to seconds since 1970-01-01 00:00:00.
 * Assumes input in normal date format, i.e. 1980-12-31 23:59:59
 * => year=1980, mon=12, day=31, hour=23, min=59, sec=59.
 *
 * [For the Julian calendar (which was used in Russia before 1917,
 * Britain & colonies before 1752, anywhere else before 1582,
 * and is still in use by some communities) leave out the
 * -year/100+year/400 terms, and add 10.]
 *
 * This algorithm was first published by Gauss (I think).
 *
 * WARNING: this function will overflow on 2106-02-07 06:28:16 on
 * machines where long is 32-bit! (However, as time_t is signed, we
 * will already get problems at other places on 2038-01-19 03:14:08)
 */
uint32_t mktime(
	const uint16_t year0, const uint8_t mon0,
	const uint8_t day, const uint8_t hour,
	const uint8_t min, const uint8_t sec)
{
	uint8_t mon = mon0;
	uint16_t year = year0;

	/* 1..12 -> 11,12,1..10 */
	if (0 >= (int8_t) (mon -= 2)) {
		mon += 12;	/* Puts Feb last since it has leap day */
		year -= 1;
	}

	return ((((uint32_t)
		(year/4 - year/100 + year/400 + 367*mon/12 + day) +
		year*365 - 719499
		)*24 + hour /* now have hours */
		)*60 + min /* now have minutes */
		)*60 + sec; /* finally seconds */
}

/*
 * Convert seconds since 01-01-1970 00:00:00 to Gregorian date.
 */
void rtc_time_to_tm(uint32_t time, struct rtc_time *tm) {
	uint16_t year;
	uint8_t month;
	int32_t days;

	days = time / 86400;
	time %= 86400;

	/* day of the week, 1970-01-01 was a Thursday */
	tm->wday = (days + 4) % 7;

	year = 1970 + days / 365;
	days -= (year - 1970) * 365
		+ LEAPS_THRU_END_OF(year - 1)
		- LEAPS_THRU_END_OF(1970 - 1);
	if (days < 0) {
		year -= 1;
		days += 365 + is_leap_year(year);
	}
	tm->year = year;

	for (month = 0; month < 11; month++) {
		int32_t newdays = days - rtc_month_days(month, year);
		if (newdays < 0)
			break;
		days = newdays;
	}
	tm->mon = month;
	tm->mday = days + 1;

	tm->hour = time / 3600;
	time %= 3600;
	tm->min = time / 60;
	tm->sec = time % 60;
}

/*
 * Convert Gregorian date to seconds since 01-01-1970 00:00:00.
 */
uint32_t rtc_tm_to_time(const struct rtc_time *tm) {
	return mktime(tm->year, tm->mon + 1, tm->mday,
		tm->hour, tm->min, tm->sec);
}

/*
 * Does the rtc_time represent a valid date/time?
 */
int rtc_valid_tm(const struct rtc_time *tm) {
	if (tm->year < 1970
		|| tm->mon >= 12
		|| tm->mday < 1
		|| tm->mday > rtc_month_days(tm->mon, tm->year)
		|| tm->hour >= 24
		|| tm->min >= 60
		|| tm->sec >= 60)
	{
		return -1;
	}

	return 0;
}

