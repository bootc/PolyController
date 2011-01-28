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
#include "timesync.h"

#include "drivers/ds1307.h"
#include "drivers/wallclock.h"
#include "lib/sntp.h"
#include "dhcp.h"

#include <stdio.h>
#include <avr/pgmspace.h>

PROCESS(timesync_process, "SNTP");

timesync_status_t timesync_status;
process_event_t timesync_event;

static const uip_ipaddr_t sntp_server = { .u8 = { 81,187,55,68 }};

static struct etimer tmr_periodic;
static struct stimer tmr_resync;

PROCESS_THREAD(timesync_process, ev, data) {
	PROCESS_BEGIN();

	timesync_event = process_alloc_event();
	timesync_status.running = 0;
	timesync_status.synchronised = 0;
	timesync_status.time_valid = 0;

	// Set up DS1307 RTC
	ds1307_clock_start();
	ds1307_ctl_set(DS1307_OUT_SQW_32768HZ);

	// Start the wallclock timer
	wallclock_init();

	// FIXME: initialise wallclock from RTC clock

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event) {
			if (timesync_status.running) {
				sntp_appcall(ev, data);
			}
		}
#if CONFIG_APPS_DHCP
		// FIXME: this logic should be moved elsewhere
		else if (ev == dhcp_event) {
			if (dhcp_status.configured && !timesync_status.running) {
				timesync_status.running = 1;
				timesync_status.synchronised = 0;

				etimer_set(&tmr_periodic, CLOCK_SECOND * 10);
				stimer_set(&tmr_resync, SNTP_RESYNC_INTERVAL);

				sntp_sync(sntp_server);

				process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
			}
			else if (!dhcp_status.configured && timesync_status.running) {
				timesync_status.running = 0;
				timesync_status.synchronised = 0;

				etimer_stop(&tmr_periodic);

				process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
			}
		}
#endif
		else if (ev == PROCESS_EVENT_TIMER) {
			if (data == &tmr_periodic && etimer_expired(&tmr_periodic)) {
				etimer_reset(&tmr_periodic);

				if (timesync_status.running && stimer_expired(&tmr_resync)) {
					stimer_reset(&tmr_resync);

					sntp_sync(sntp_server);
				}
			}
			else if (timesync_status.running) {
				sntp_appcall(ev, data);
			} 
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			timesync_status.running = 0;
			timesync_status.synchronised = 0;
			timesync_status.time_valid = 0;

			// Disable the DS1307 clock output to save power
			ds1307_ctl_set(DS1307_OUT_LOW);

			process_exit(&timesync_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

void sntp_synced(const struct sntp_hdr *message) {
	if (!message) {
		timesync_status.synchronised = 0;
		process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
		return;
	}

	// Sanity check message
	if (message->VN == 0 ||
		message->Stratum == 0 ||
		uip_ntohl(message->TxTimestamp[0]) == 0)
	{
		timesync_status.synchronised = 0;
		process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
		return;
	}

	// Set the new wallclock time
	wallclock_time_t new = {
		.sec = uip_ntohl(message->TxTimestamp[0]),
		.frac = uip_ntohl(message->TxTimestamp[1]) >> 20, // 32 to 12 bit fixed
	};
	wallclock_set(new);

	// FIXME: Set the new RTC time if necessary

	// Set our status flags
	timesync_status.time_valid = 1;
	timesync_status.synchronised = 1;

	// Tell folks about the sync
	process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
}

uint32_t sntp_seconds(void) {
	if (timesync_status.time_valid) {
		return wallclock_seconds();
	}
	else {
		return 0;
	}
}

