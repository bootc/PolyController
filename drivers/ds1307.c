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

int ds1307_time_set(ds1307_time_t time) {
	int ret;
	uint8_t raw[3];

	// Sanity check values
	if ((time.hour > 23) ||
		(time.min > 59) ||
		(time.sec > 59))
	{
		return -1;
	}

	// Read the current seconds value
	ret = read(&raw[0], ADDR_SEC, sizeof(raw[0]));
	if (ret) {
		return ret;
	}

	// Update the values, keeping the CH bit intact
	raw[0] = dec2bcd(time.sec) | (raw[0] & SEC_CH);
	raw[1] = dec2bcd(time.min);
	raw[2] = dec2bcd(time.hour);

	// Write the values back
	ret = write(raw, ADDR_SEC, sizeof(raw));
	if (ret) {
		return ret;
	}

	return 0;
}

int ds1307_time_get(ds1307_time_t *time) {
	int ret;

	// Read the values from the RTC
	ret = read((uint8_t *)time, ADDR_SEC, sizeof(*time));
	if (ret) {
		return ret;
	}

	// Convert the values, ignoring the CH bit
	time->sec = bcd2dec(time->sec & ~SEC_CH);
	time->min = bcd2dec(time->min);

	// Convert hours, taking into account 12-hour mode
	if (time->hour & HOUR_12) {
		if (time->hour & HOUR_PM) {
			// 1 - 12 pm is 13 - 00
			time->hour = bcd2dec(time->hour & ~(HOUR_12 | HOUR_PM));
			time->hour += 12;
			time->hour %= 24;
		}
		else {
			// 1 - 12 am is 01 - 12
			time->hour = bcd2dec(time->hour & ~HOUR_12);
		}
	}
	else {
		time->hour = bcd2dec(time->hour);
	}

	return 0;
}

int ds1307_date_set(ds1307_date_t date) {
	int ret;

	// Sanity check values
	if ((date.year > 99) ||
		(date.month < 1) || (date.month > 12) ||
		(date.day < 1) || (date.day > 31) ||
		(date.dow < 1) || (date.dow > 7))
	{
		return -1;
	}

	// Convert to BCD
	date.year = dec2bcd(date.year);
	date.month = dec2bcd(date.year);
	date.day = dec2bcd(date.day);
	date.dow = dec2bcd(date.dow);

	// Write the values
	ret = write((uint8_t *)&date, ADDR_DAY, sizeof(date));
	if (ret) {
		return -1;
	}

	return 0;
}

int ds1307_date_get(ds1307_date_t *date) {
	int ret;

	// Read the values
	ret = read((uint8_t *)date, ADDR_DAY, sizeof(*date));
	if (ret) {
		return -1;
	}

	// Convert from BCD
	date->year = bcd2dec(date->year);
	date->month = bcd2dec(date->month);
	date->day = bcd2dec(date->day);
	date->dow = bcd2dec(date->dow);

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

