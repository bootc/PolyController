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

#ifndef RESOLV_HELPER_H
#define RESOLV_HELPER_H

#include <contiki-net.h>
#include <sys/stimer.h>

enum resolv_helper_state {
	RESOLV_HELPER_STATE_NEW = 0,
	RESOLV_HELPER_STATE_ASKING,
	RESOLV_HELPER_STATE_DONE,
	RESOLV_HELPER_STATE_EXPIRED,
	RESOLV_HELPER_STATE_ERROR
};

struct resolv_helper_status {
	enum resolv_helper_state state;
	struct pt pt;
	struct stimer expire;
	char name[32];
	uip_ipaddr_t ipaddr;
};

void resolv_helper_lookup(struct resolv_helper_status *st);
void resolv_helper_appcall(struct resolv_helper_status *st,
	process_event_t ev, void *data);

#endif // RESOLV_HELPER_H
