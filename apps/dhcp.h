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

#ifndef __DHCP_H__
#define __DHCP_H__

#include "net/dhcpc.h"

typedef struct {
	const struct dhcpc_state *	state;
	int							running : 1;
	int							configured : 1;
} dhcp_status_t;

extern process_event_t dhcp_event;
extern dhcp_status_t dhcp_status;

PROCESS_NAME(dhcp_process);

#endif
