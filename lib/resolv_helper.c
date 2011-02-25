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

#include <contiki-net.h>
#include <string.h>
#include <stdio.h>

#include "resolv_helper.h"

#include "apps/network.h"

#define EXPIRE_TTL 3600

PT_THREAD(resolv_helper(struct resolv_helper_status *st,
	process_event_t ev, void *data))
{
	uip_ipaddr_t *ip = NULL;

	PT_BEGIN(&st->pt);

	PT_WAIT_UNTIL(&st->pt, net_status.configured);

	// Send the query
	resolv_query(st->name);

	while (1) {
		// Wait until we get a query resolved event
		PT_WAIT_UNTIL(&st->pt, ev == resolv_event_found);

		// Check the event has to do with our hostname
		if (strncmp(data, st->name, sizeof(st->name)) == 0) {
			ip = resolv_lookup(st->name);
			break;
		}
	}

	// Check if the resolver managed to find an IP
	if (ip) {
		// Set up the status structure
		st->state = RESOLV_HELPER_STATE_DONE;
		stimer_set(&st->expire, EXPIRE_TTL);
		st->ipaddr = *ip;

		// Wait until the TTL expires
		PT_WAIT_UNTIL(&st->pt, stimer_expired(&st->expire));

		st->state = RESOLV_HELPER_STATE_EXPIRED;
	}
	else {
		st->state = RESOLV_HELPER_STATE_ERROR;
	}

	while (1) {
		PT_YIELD(&st->pt);
	}

	PT_END(&st->pt);
}

void resolv_helper_lookup(struct resolv_helper_status *st) {
	st->state = RESOLV_HELPER_STATE_ASKING;
	PT_INIT(&st->pt);
	resolv_helper(st, PROCESS_EVENT_NONE, NULL);
}

void resolv_helper_appcall(struct resolv_helper_status *st,
	process_event_t ev, void *data)
{
	if (st->state != RESOLV_HELPER_STATE_NEW) {
		resolv_helper(st, ev, data);
	}
}

