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
#include "monitor.h"

#include "network.h"
#if CONFIG_APPS_DHCP
#include "dhcp.h"
#endif
#include "shell/shell.h"
#include "timesync.h"

#include "drivers/wallclock.h"

#include <stdio.h>
#include <avr/pgmspace.h>
#include <sys/stimer.h>

PROCESS(monitor_process, "Monitor");

static struct etimer heartbeat;

static void log_message_P(PGM_P fmt, ...) {
	va_list argp;
	uint16_t len;

	printf_P(PSTR("\r\x1b[2K\x1b[01;32m"));

	va_start(argp, fmt);
	len = vfprintf_P(stdout, fmt, argp);
	va_end(argp);

	printf_P(PSTR("\x1b[00m\n"));

	shell_prompt_P(PSTR("Contiki> "));
}

PROCESS_THREAD(monitor_process, ev, data) {
	PROCESS_BEGIN();

	// Initialise timer
	etimer_set(&heartbeat, CLOCK_SECOND / 2);

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == net_link_event) {
			log_message_P(PSTR("\rNET: Link %S"),
				net_flags.link ? PSTR("UP") : PSTR("DOWN"));
		}
#if CONFIG_APPS_DHCP
		else if (ev == dhcp_event) {
			log_message_P(PSTR("\rDHCP: %Srunning, %Sconfigured"),
				dhcp_status.running ? PSTR("") : PSTR("not "),
				dhcp_status.configured ? PSTR("") : PSTR("not "));

			if (dhcp_status.configured) {
				PORTD |= _BV(PIND7);

				const struct dhcpc_state *s = dhcp_status.state;

				log_message_P(PSTR("DHCP: Got address %d.%d.%d.%d"),
					uip_ipaddr_to_quad(&s->ipaddr));
				log_message_P(PSTR("DHCP: Got netmask %d.%d.%d.%d"),
					uip_ipaddr_to_quad(&s->netmask));
				log_message_P(PSTR("DHCP: Got DNS server %d.%d.%d.%d"),
					uip_ipaddr_to_quad(&s->dnsaddr));
				log_message_P(PSTR("DHCP: Got default router %d.%d.%d.%d"),
					uip_ipaddr_to_quad(&s->default_router));
				log_message_P(PSTR("DHCP Lease expires in %ld seconds."),
					uip_ntohs(s->lease_time[0]) * 65536ul +
					uip_ntohs(s->lease_time[1]));
			}
			else {
				PORTD &= ~_BV(PIND7);
			}
		}
#endif
		else if (ev == timesync_event) {
			log_message_P(PSTR("\rTimeSync: %Srunning, %Sin sync"),
				timesync_status.running ? PSTR("") : PSTR("not "),
				timesync_status.synchronised ? PSTR("") : PSTR("not "));

			wallclock_time_t time;
			wallclock_get(&time);
			uint32_t ms = ((uint32_t)time.frac * 1000) >> 12;
			log_message_P(PSTR("Wallclock: Time is %lu.%04lus"),
				time.sec, ms);
		}
		else if (ev == PROCESS_EVENT_TIMER) {
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

