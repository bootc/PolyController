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

#ifndef __TIMESYNC_H__
#define __TIMESYNC_H__

#include "drivers/wallclock.h"

// How often to refresh the local time offset (in seconds)
#define SNTP_RESYNC_INTERVAL	600

typedef struct {
	int					running : 1;
	int					synchronised : 1;
} timesync_status_t;

extern timesync_status_t timesync_status;
extern process_event_t timesync_event;

void timesync_schedule_resync(void);
int timesync_set_time(const wallclock_time_t *time);

PROCESS_NAME(timesync_process);

#endif
