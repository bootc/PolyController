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
#include <sys/stimer.h>
#include "sntpclient.h"
#include "lib/sntp.h"
#include "dhcp.h"

#include <stdio.h>
#include <avr/pgmspace.h>

PROCESS(sntp_process, "SNTP");

sntp_status_t sntp_status;
process_event_t sntp_event;

const uip_ipaddr_t sntp_server = { .u8 = { 81,187,55,68 }};

static struct etimer sntp_periodic;
static struct stimer sntp_resync;

PROCESS_THREAD(sntp_process, ev, data) {
	PROCESS_BEGIN();

	sntp_event = process_alloc_event();
	sntp_status.running = 0;
	sntp_status.synchronised = 0;
	sntp_status.offset_valid = 0;
	sntp_status.offset_seconds = 0;

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event) {
			if (sntp_status.running) {
				sntp_appcall(ev, data);
			}
		}
#if CONFIG_APPS_DHCP
		else if (ev == dhcp_event) {
			if (dhcp_status.configured && !sntp_status.running) {
				sntp_status.running = 1;
				sntp_status.synchronised = 0;

				etimer_set(&sntp_periodic, CLOCK_SECOND * 10);
				stimer_set(&sntp_resync, SNTP_RESYNC_INTERVAL);

				sntp_sync(sntp_server);

				process_post(PROCESS_BROADCAST, sntp_event, &sntp_status);
			}
			else if (!dhcp_status.configured && sntp_status.running) {
				sntp_status.running = 0;
				sntp_status.synchronised = 0;

				etimer_stop(&sntp_periodic);

				process_post(PROCESS_BROADCAST, sntp_event, &sntp_status);
			}
		}
#endif
		else if (ev == PROCESS_EVENT_TIMER) {
			if (data == &sntp_periodic && etimer_expired(&sntp_periodic)) {
				etimer_reset(&sntp_periodic);

				if (sntp_status.running && stimer_expired(&sntp_resync)) {
					stimer_reset(&sntp_resync);

					sntp_sync(sntp_server);
				}
			}
			else if (sntp_status.running) {
				sntp_appcall(ev, data);
			} 
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			sntp_status.running = 0;
			sntp_status.synchronised = 0;
			sntp_status.offset_valid = 0;
			sntp_status.offset_seconds = 0;

			process_exit(&sntp_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

void sntp_synced(const struct sntp_hdr *message) {
	if (!message) {
		sntp_status.synchronised = 0;
		process_post(PROCESS_BROADCAST, sntp_event, &sntp_status);
		return;
	}

	if (message->VN == 0 ||
		message->Stratum == 0 ||
		uip_ntohl(message->TxTimestamp[0]) == 0)
	{
		sntp_status.synchronised = 0;
		process_post(PROCESS_BROADCAST, sntp_event, &sntp_status);
		return;
	}

	uint32_t ntptime = uip_ntohl(message->TxTimestamp[0]);
	uint32_t localtime = clock_seconds();

	sntp_status.offset_seconds = ntptime - localtime;
	sntp_status.offset_valid = 1;
	sntp_status.synchronised = 1;

	process_post(PROCESS_BROADCAST, sntp_event, &sntp_status);
}

uint32_t sntp_seconds(void) {
	if (sntp_status.offset_valid) {
		return clock_seconds() + sntp_status.offset_seconds;
	}
	else {
		return 0;
	}
}

