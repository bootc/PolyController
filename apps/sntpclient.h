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

#ifndef __SNTPCLIENT_H__
#define __SNTPCLIENT_H__

// How often to refresh the local time offset (in seconds)
#define SNTP_RESYNC_INTERVAL	600

typedef struct {
	int			running : 1;
	int			synchronised : 1;
	int			offset_valid : 1;
	uint32_t	offset_seconds;
} sntp_status_t;

extern sntp_status_t sntp_status;
extern process_event_t sntp_event;

// seconds since NTP epoch
uint32_t sntp_seconds(void);

PROCESS_NAME(sntp_process);

#endif
