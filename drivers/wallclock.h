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

#ifndef WALLCLOCK_H
#define WALLCLOCK_H

/**
 * Driver for wall-clock seconds using 32.768KHz clock output from an RTC.
 *
 * This driver uses Timer/Counter 2 in asynchronous mode to keep time using
 * the 32.768KHz clock output from an RTC connected to TOSC1 on the ATmega.
 */

typedef struct {
	uint32_t sec;
	uint16_t frac : 12;
} wallclock_time_t;


void wallclock_init(void);
void wallclock_set(wallclock_time_t time);
void wallclock_get(wallclock_time_t *time);

#endif /* WALLCLOCK_H */
