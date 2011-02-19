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

#include <time.h>

/**
 * Driver for DS1307 RTC chip.
 *
 * This driver always uses 24-hour time and encodes/decodes the BCD values used
 * by the chip as necessary - no BCD conversions are required in user code.
 */

#define DS1307_ADDR		0xD0
#define DS1307_RAMSTART	0x08
#define DS1307_RAMEND	0x3f

#define DS1307_CTL_RS0	_BV(0)
#define DS1307_CTL_RS1	_BV(1)
#define DS1307_CTL_SQWE	_BV(4)
#define DS1307_CTL_OUT	_BV(7)

#define DS1307_OUT_SQW_1HZ		(DS1307_CTL_SQWE)
#define DS1307_OUT_SQW_4096HZ	(DS1307_CTL_SQWE|DS1307_CTL_RS0)
#define DS1307_OUT_SQW_8192HZ	(DS1307_CTL_SQWE|DS1307_CTL_RS1)
#define DS1307_OUT_SQW_32768HZ	(DS1307_CTL_SQWE|DS1307_CTL_RS1|DS1307_CTL_RS0)
#define DS1307_OUT_LOW			(0x00)
#define DS1307_OUT_HIGH			(DS1307_CTL_OUT)


int ds1307_clock_start(void);
int ds1307_clock_stop(void);

int ds1307_clock_set(const struct tm * const tm);
int ds1307_clock_get(struct tm *tm);

int ds1307_ctl_set(uint8_t ctl);
int ds1307_ctl_get(uint8_t *ctl);

int ds1307_ram_write(void *ptr, uint8_t offset, uint8_t len);
int ds1307_ram_read(void *ptr, uint8_t offset, uint8_t len);

#endif /* DS1307_H */
