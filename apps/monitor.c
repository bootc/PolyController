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
#include <init.h>
#include "monitor.h"

//#include <stdio.h>
//#include <avr/pgmspace.h>

#if CONFIG_APPS_DHCP
#include "dhcp.h"
#endif
//#include "shell/shell.h"

#include "drivers/wallclock.h"

PROCESS(monitor_process, "Monitor");
INIT_PROCESS(monitor_process);

static struct etimer heartbeat;

/*
static void log_message_P(PGM_P fmt, ...) {
	va_list argp;

	printf_P(PSTR("\r\x1b[2K\x1b[01;32m"));

	va_start(argp, fmt);
	vfprintf_P(stdout, fmt, argp);
	va_end(argp);

	printf_P(PSTR("\x1b[00m\n"));

	shell_prompt_P(PSTR("Contiki> "));
}
*/

PROCESS_THREAD(monitor_process, ev, data) {
	PROCESS_BEGIN();

	// Initialise timer
	etimer_set(&heartbeat, CLOCK_SECOND / 2);

	while (1) {
		PROCESS_WAIT_EVENT();

#if CONFIG_APPS_DHCP
		if (ev == dhcp_event) {
			if (dhcp_status.configured) {
				PORTD |= _BV(PIND7);
			}
			else {
				PORTD &= ~_BV(PIND7);
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
				PORTD ^= _BV(PIND6);
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&monitor_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

