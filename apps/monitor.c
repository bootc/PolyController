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
#include <time.h>
#include <strftime.h>
#include "monitor.h"

#include <stdio.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <sys/stimer.h>

#include "network.h"
#if CONFIG_APPS_DHCP
#include "dhcp.h"
#endif
#include "shell/shell.h"
#if CONFIG_APPS_SYSLOG
#include "syslog.h"
#endif
#include "timesync.h"

#include "drivers/wallclock.h"

PROCESS(monitor_process, "Monitor");
INIT_PROCESS(monitor_process);

static struct etimer heartbeat;

static void log_message_P(PGM_P fmt, ...) {
	va_list argp;

	printf_P(PSTR("\r\x1b[2K\x1b[01;32m"));

	va_start(argp, fmt);
#if CONFIG_APPS_SYSLOG
	vsyslog_P(LOG_MAKEPRI(LOG_USER, LOG_INFO), fmt, argp);
#endif
	vfprintf_P(stdout, fmt, argp);
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
			if (net_flags.link) {
				log_message_P(PSTR("NET: Link UP, %S-%S, %Sconfigured"),
						net_flags.speed_100m ? PSTR("100M") : PSTR("10M"),
						net_flags.full_duplex ? PSTR("FDX") : PSTR("HDX"),
						net_flags.configured ? PSTR("") : PSTR("not "));
			}
			else {
				log_message_P(PSTR("NET: Link DOWN"));
			}
		}
#if CONFIG_APPS_DHCP
		else if (ev == dhcp_event) {
			log_message_P(PSTR("DHCP: %Srunning, %Sconfigured"),
				dhcp_status.running ? PSTR("") : PSTR("not "),
				dhcp_status.configured ? PSTR("") : PSTR("not "));

			if (dhcp_status.configured) {
				PORTD |= _BV(PIND7);

				const struct dhcpc_state *s = dhcp_status.state;

				log_message_P(PSTR("DHCP: Got addr %d.%d.%d.%d/%d.%d.%d.%d (exp %lds)"),
					uip_ipaddr_to_quad(&s->ipaddr),
					uip_ipaddr_to_quad(&s->netmask),
					uip_ntohs(s->lease_time[0]) * 65536ul +
					uip_ntohs(s->lease_time[1]));
				log_message_P(PSTR("DHCP: Default route %d.%d.%d.%d"),
					uip_ipaddr_to_quad(&s->default_router));
				log_message_P(PSTR("DHCP: DNS server %d.%d.%d.%d"),
					uip_ipaddr_to_quad(&s->dnsaddr));
			}
			else {
				PORTD &= ~_BV(PIND7);
			}
		}
#endif
#if CONFIG_APPS_TIMESYNC
		else if (ev == timesync_event) {
			log_message_P(PSTR("TimeSync: %Srunning, %Sin sync"),
				timesync_status.running ? PSTR("") : PSTR("not "),
				timesync_status.synchronised ? PSTR("") : PSTR("not "));

			wallclock_time_t time;
			struct tm tm;
			char *ts = malloc(64);
			if (!ts) {
				continue;
			}

			wallclock_get(&time);
			gmtime(time.sec, &tm);

			uint32_t ms = ((uint32_t)time.frac * 1000) >> 12;

			strftime_P(ts, 64, PSTR("%c"), &tm);
			log_message_P(PSTR("Wallclock: Time is %s; %lums"),
				ts, ms);
			free(ts);
		}
#endif
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

