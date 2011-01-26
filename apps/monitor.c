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
#include "sntpclient.h"
#include "lib/stack.h"
#include "shell/shell.h"
#include "lib/polyfs.h"

#include <stdio.h>
#include <avr/pgmspace.h>
#include <sys/stimer.h>

#if !defined(RAMSTART)
#define RAMSTART (0x100)
#endif
#if !defined(RAMSIZE)
#define RAMSIZE (RAMEND - RAMSTART)
#endif

PROCESS(monitor_process, "Monitor");

PROCESS(shell_free_process, "free");
SHELL_COMMAND(free_command,
	"free", "free: show memory usage",
	&shell_free_process);

PROCESS(polyfs_test_process, "polyfs test");
SHELL_COMMAND(polyfs_test_command,
	"polyfs", "polyfs: test polyfs capability",
	&polyfs_test_process);

#if PROCESS_CONF_STATS
extern process_num_events_t process_maxevents;
#endif

static struct etimer heartbeat;

void monitor_init(void) {
	shell_register_command(&free_command);
	shell_register_command(&polyfs_test_command);
}

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
				PORTD |= _BV(PD7);

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
				PORTD &= ~_BV(PD7);
			}
		}
#endif
		else if (ev == sntp_event) {
			log_message_P(PSTR("\rSNTP: %Srunning, %Sin sync, offset %Svalid"),
				sntp_status.running ? PSTR("") : PSTR("not "),
				sntp_status.synchronised ? PSTR("") : PSTR("not "),
				sntp_status.offset_valid ? PSTR("") : PSTR("not "));

			if (sntp_status.offset_valid) {
				log_message_P(PSTR("SNTP: Offset is %lus"),
					sntp_status.offset_seconds);
			}
		}
		else if (ev == PROCESS_EVENT_TIMER) {
			if (data == &heartbeat &&
				etimer_expired(&heartbeat))
			{
				etimer_restart(&heartbeat);

				// Toggle heartbeat LED
				PORTD ^= _BV(PD6);
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&monitor_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

PROCESS_THREAD(shell_free_process, ev, data) {
	PROCESS_BEGIN();

	const uint16_t stack_size = ((uint16_t)&__stack - (uint16_t)&_end) + 1;
	uint16_t stack_free = StackCount();

	shell_output_P(&free_command,
		PSTR("           total        used        free"));

	shell_output_P(&free_command,
		PSTR("Stack:      %4d        %4d        %4d"),
		stack_size, stack_size - stack_free, stack_free);

	shell_output_P(&free_command,
		PSTR("Heap:       %4d        %4d        %4d"),
		RAMSIZE - stack_size, RAMSIZE - stack_size, 0);

#if PROCESS_CONF_STATS

	shell_output_P(&free_command, PSTR(""));
	shell_output_P(&free_command, PSTR("Max Events: %d"),
		process_maxevents);
#endif

	PROCESS_END();
}

PROCESS_THREAD(polyfs_test_process, ev, data) {
	PROCESS_BEGIN();

	//polyfs_test();
	shell_output_P(&polyfs_test_command, PSTR("FIXME: unimplemented"));

	PROCESS_END();
}

