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

#include <stdio.h>
#include <contiki-net.h>
#include <init.h>
#include "monitor.h"

#if CONFIG_APPS_NETWORK
#include "network.h"
#endif

PROCESS(monitor_process, "Monitor");
INIT_PROCESS(monitor_process);

static struct etimer heartbeat;

PROCESS_THREAD(monitor_process, ev, data) {
	PROCESS_BEGIN();

	// Initialise timer
	etimer_set(&heartbeat, CLOCK_SECOND / 2);

	// Print boot message
	printf_P(PSTR("\n\n\n"));
	printf_P(PSTR("PolyController " CONFIG_BOARD "\n"));
	printf_P(PSTR("Image: " CONFIG_IMAGE "\n"));
	printf_P(PSTR("\n"));

	while (1) {
		PROCESS_WAIT_EVENT();

#if CONFIG_APPS_NETWORK
		if (ev == net_event) {
			if (net_status.configured) {
				PORTA |= _BV(PINA1);
			}
			else {
				PORTA &= ~_BV(PINA1);
			}
		}
		else
#endif
		if (ev == PROCESS_EVENT_TIMER) {
			if (data == &heartbeat &&
				etimer_expired(&heartbeat))
			{
				etimer_restart(&heartbeat);

				// Toggle heartbeat LED
				PORTA ^= _BV(PINA0);
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&monitor_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

