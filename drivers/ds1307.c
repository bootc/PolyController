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

#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>

#include "ds1307.h"
#include "i2c.h"

#define ADDR_SEC	0x00
#define ADDR_MIN	0x01
#define ADDR_HOUR	0x02
#define ADDR_DAY	0x03
#define ADDR_DATE	0x04
#define ADDR_MON	0x05
#define ADDR_YEAR	0x06
#define ADDR_CTL	0x07

#define SEC_CH		_BV(7)
#define HOUR_12		_BV(6)
#define HOUR_PM		_BV(5)

struct ds1307_raw {
	/* 0x00: Seconds */
	uint8_t ch : 1;
	uint8_t sec : 7;

	/* 0x01: Minutes */
	uint8_t min;

	/* 0x02: Hours */
	uint8_t hour;

	/* 0x03: Day of Week */
	uint8_t wday;

	/* 0x04: Day of Month */
	uint8_t mday;

	/* 0x05: Month */
	uint8_t mon;

	/* 0x06: Year */
	uint8_t year;
};

static int read(uint8_t *ptr, uint8_t offset, uint8_t len) {
	int ret;

	// START condition
	ret = i2c_start(DS1307_ADDR + I2C_WRITE);
	if (ret) {
		i2c_stop();
		return -1;
	}

	// Send the pointer address
	ret = i2c_write(offset);
	if (ret) {
		i2c_stop();
		return -1;
	}

	// REPEATED START condition
	ret = i2c_rep_start(DS1307_ADDR + I2C_READ);
	if (ret) {
		i2c_stop();
		return -1;
	}

	// Read all the bytes
	while (len--) {
		*ptr++ = i2c_read(len > 0);
	}

	// STOP condition
	i2c_stop();

	return 0;
}

static int write(uint8_t *ptr, uint8_t offset, uint8_t len) {
	int ret;

	// START condition
	ret = i2c_start(DS1307_ADDR + I2C_WRITE);
	if (ret) {
		i2c_stop();
		return -1;
	}

	// Send the pointer address
	ret = i2c_write(offset);
	if (ret) {
		i2c_stop();
		return -1;
	}

	// Write all the bytes
	while (len--) {
		ret = i2c_write(*ptr++);
		if (ret) {
			i2c_stop();
			return -1;
		}
	}

	// STOP condition
	i2c_stop();

	return 0;

}

static inline int dec2bcd(uint8_t dec) {
	return ((dec / 10 * 16) + (dec % 10));
}

static inline int bcd2dec(uint8_t bcd) {
	return ((bcd / 16 * 10) + (bcd % 16));
}

int ds1307_clock_start(void) {
	uint8_t secs;
	int ret;

	// Read the seconds register
	ret = read(&secs, ADDR_SEC, sizeof(secs));
	if (ret) {
		return -1;
	}

	// Is the clock already running?
	if (!(secs & SEC_CH)) {
		return 0;
	}

	// Clear the CH bit
	secs &= ~SEC_CH;

	// Write back the seconds register
	ret = write(&secs, ADDR_SEC, sizeof(secs));
	if (ret) {
		return -1;
	}

	return 0;
}

int ds1307_clock_stop(void) {
	uint8_t secs;
	int ret;

	// Read the seconds register
	ret = read(&secs, ADDR_SEC, sizeof(secs));
	if (ret) {
		return -1;
	}

	// Is the clock already stopped?
	if (secs & SEC_CH) {
		return 0;
	}

	// Set the CH bit
	secs |= SEC_CH;

	// Write back the seconds register
	ret = write(&secs, ADDR_SEC, sizeof(secs));
	if (ret) {
		return -1;
	}

	return 0;
}

int ds1307_clock_set(const struct rtc_time *tm) {
	int ret;
	struct ds1307_raw raw;

	// Sanity check
	ret = rtc_valid_tm(tm);
	if (ret) {
		return -1;
	}

	// We also check the year >= 2000
	if (tm->year < 2000) {
		return -1;
	}

	// Read the current seconds value
	ret = read((uint8_t *)&raw, ADDR_SEC, 1); // just the first byte is enough
	if (ret) {
		return ret;
	}

	// Update the values, keeping the CH bit intact
	raw.sec = dec2bcd(tm->sec);
	raw.min = dec2bcd(tm->min);
	raw.hour = dec2bcd(tm->hour);
	raw.wday = dec2bcd(tm->wday + 1);
	raw.mday = dec2bcd(tm->mday);
	raw.mon = dec2bcd(tm->mon + 1);
	raw.year = dec2bcd(tm->year - 2000);

	// Write the values back
	ret = write((uint8_t *)&raw, ADDR_SEC, sizeof(raw));
	if (ret) {
		return ret;
	}

	return 0;	
}

int ds1307_clock_get(struct rtc_time *tm) {
	int ret;
	struct ds1307_raw raw;

	// Read the values from the RTC
	ret = read((uint8_t *)&raw, ADDR_SEC, sizeof(raw));
	if (ret) {
		return ret;
	}

	// Convert the values
	tm->sec = bcd2dec(raw.sec);
	tm->min = bcd2dec(raw.min);
	// hour is done below
	tm->mday = bcd2dec(raw.mday);
	tm->mon = bcd2dec(raw.mon - 1);
	tm->year = bcd2dec(raw.year + 2000);
	tm->wday = bcd2dec(raw.wday - 1);

	// Convert hours, taking into account 12-hour mode
	if (raw.hour & HOUR_12) {
		if (raw.hour & HOUR_PM) {
			// 1 - 12 pm is 13 - 00
			tm->hour = bcd2dec(raw.hour & ~(HOUR_12 | HOUR_PM));
			tm->hour += 12;
			tm->hour %= 24;
		}
		else {
			// 1 - 12 am is 01 - 12
			tm->hour = bcd2dec(raw.hour & ~HOUR_12);
		}
	}
	else {
		tm->hour = bcd2dec(raw.hour);
	}

	return 0;
}

int ds1307_ctl_set(uint8_t ctl) {
	return write(&ctl, ADDR_CTL, sizeof(ctl));
}

int ds1307_ctl_get(uint8_t *ctl) {
	return read(ctl, ADDR_CTL, sizeof(*ctl));
}

int ds1307_ram_write(void *ptr, uint8_t offset, uint8_t len) {
	int ret;

	// Sanity check
	if ((offset < DS1307_RAMSTART) ||
		(offset > DS1307_RAMEND))
	{
		return -1;
	}

	// Clamp len
	if (offset + len > DS1307_RAMEND) {
		len = (DS1307_RAMEND - offset) + 1;
	}

	// Write data
	ret = write(ptr, offset, len);
	if (ret) {
		return ret;
	}

	return len;
}

int ds1307_ram_read(void *ptr, uint8_t offset, uint8_t len) {
	int ret;

	// Sanity check
	if ((offset < DS1307_RAMSTART) ||
		(offset > DS1307_RAMEND))
	{
		return -1;
	}

	// Clamp len
	if (offset + len > DS1307_RAMEND) {
		len = (DS1307_RAMEND - offset) + 1;
	}

	// Read data
	ret = read(ptr, offset, len);
	if (ret) {
		return ret;
	}

	return len;
}

