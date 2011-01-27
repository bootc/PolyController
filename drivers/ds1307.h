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

#ifndef DS1307_H
#define DS1307_H

/**
 * Driver for DS1307 RTC chip.
 *
 * This driver always uses 24-hour time and encodes/decodes the BCD values used
 * by the chip as necessary - no BCD conversions are required in user code.
 */

typedef struct {
	uint8_t sec;	// 0 to 59
	uint8_t min;	// 0 to 59
	uint8_t hour;	// 0 to 23
} ds1307_time_t;

typedef struct {
	uint8_t dow;	// 1 to 7
	uint8_t day;	// 1 to 31
	uint8_t month;	// 1 to 12
	uint8_t year;	// 0 to 99
} ds1307_date_t;

#define DS1307_ADDR		0xD0
#define DS1307_RAMSTART	0x08
#define DS1307_RAMEND	0x3f

#define DS1307_CTL_RS0	_BV(0)
#define DS1307_CTL_RS1	_BV(1)
#define DS1307_CTL_SQWE	_BV(4)
#define DS1307_CTL_OUT	_BV(7)

#define OUT_SQW_1HZ		(DS1307_CTL_SQWE)
#define OUT_SQW_4096HZ	(DS1307_CTL_SQWE | DS1307_CTL_RS0)
#define OUT_SQW_8192HZ	(DS1307_CTL_SQWE | DS1307_CTL_RS1)
#define OUT_SQW_32768HZ	(DS1307_CTL_SQWE | DS1307_CTL_RS1 | DS1307_CTL_RS0)
#define OUT_LOW			(0x00)
#define OUT_HIGH		(DS1307_CTL_OUT)


int ds1307_clock_start(void);
int ds1307_clock_stop(void);

int ds1307_time_set(ds1307_time_t time);
int ds1307_time_get(ds1307_time_t *time);

int ds1307_date_set(ds1307_date_t date);
int ds1307_date_get(ds1307_date_t *date);

int ds1307_ctl_set(uint8_t ctl);
int ds1307_ctl_get(uint8_t *ctl);

int ds1307_ram_write(void *ptr, uint8_t offset, uint8_t len);
int ds1307_ram_read(void *ptr, uint8_t offset, uint8_t len);

#endif /* DS1307_H */
