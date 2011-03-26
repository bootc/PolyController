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
#include <stdio.h>
#include <stdlib.h>
#include <init.h>
#include <alloca.h>
#include <avr/pgmspace.h>
#include "timesync.h"

#include <time.h>
#include <sntp.h>
#include <resolv_helper.h>

#if CONFIG_DRIVERS_DS1307
#include "drivers/ds1307.h"
#endif
#include "drivers/wallclock.h"

#include "apps/network.h"
#if CONFIG_APPS_SYSLOG
#include "apps/syslog.h"
#endif

const char sntp_server_name[] PROGMEM = "tarquin.bootc.net";

PROCESS(timesync_process, "TimeSync");
INIT_PROCESS(timesync_process);

timesync_status_t timesync_status;
process_event_t timesync_event;

static struct resolv_helper_status res;
static struct etimer tmr_periodic;
static struct stimer tmr_resync;

static int init(void) {
#if CONFIG_DRIVERS_DS1307
	int err;
	struct tm tm;
	wallclock_time_t new;
	char date[32];
#endif

	timesync_event = process_alloc_event();
	timesync_status.running = 0;
	timesync_status.synchronised = 0;

#if CONFIG_DRIVERS_DS1307
	// Set up DS1307 RTC
	ds1307_clock_start();
	ds1307_ctl_set(DS1307_OUT_SQW_32768HZ);
#endif

	// Start the wallclock timer
	wallclock_init();

#if CONFIG_DRIVERS_DS1307
	// Get the time from the RTC
	err = ds1307_clock_get(&tm);
	if (err) {
		return err;
	}

	// Set up the new time
	new.sec = mktime(&tm);
	new.frac = 0x7ff; // mid-way through second

	// Set the system time from the RTC
	wallclock_set(&new);

	// Print date retrieved from RTC
	strftime_P(date, sizeof(date), PSTR("%c"), &tm);
	printf_P(PSTR("Date from RTC: %s\n"), date);
#endif

	// Copy the host name into the resolv helper structure
	strncpy_P(res.name, sntp_server_name, sizeof(res.name));

	// Launch the lookup
	resolv_helper_lookup(&res);

	return 0;
}

static void sntp_lookup_sync(void) {
	if (res.state == RESOLV_HELPER_STATE_DONE) {
		sntp_sync(res.ipaddr);
		timesync_status.sync_pending = 0;
		return; // no poll
	}
	else if (res.state == RESOLV_HELPER_STATE_EXPIRED) {
		// Refresh the expired lookup
		resolv_helper_lookup(&res);
	}
	else if (res.state == RESOLV_HELPER_STATE_ERROR) {
		syslog_P(LOG_DAEMON | LOG_ERR,
			PSTR("SNTP host lookup error: %s"),
			res.name);
		return; // no poll
	}

	process_poll(&timesync_process);
}

PROCESS_THREAD(timesync_process, ev, data) {
	PROCESS_BEGIN();

	// Set things up
	init();

	while (1) {
		PROCESS_WAIT_EVENT();

		// Call the resolver
		resolv_helper_appcall(&res, ev, data);

		if (ev == PROCESS_EVENT_POLL) {
			sntp_lookup_sync();
		}
		else if (ev == tcpip_event) {
			if (timesync_status.running) {
				sntp_appcall(ev, data);
			}
		}
		else if (ev == net_event) {
			if (net_status.configured && !timesync_status.running) {
				timesync_status.running = 1;
				timesync_status.sync_pending = 1;
				timesync_status.synchronised = 0;

				etimer_set(&tmr_periodic, CLOCK_SECOND);
				stimer_set(&tmr_resync, SNTP_RESYNC_INTERVAL);

				process_post(PROCESS_BROADCAST, timesync_event,
					&timesync_status);
				syslog_P(LOG_DAEMON | LOG_INFO, PSTR("Starting"));

				process_poll(&timesync_process);
			}
			else if (!net_status.configured && timesync_status.running) {
				timesync_status.running = 0;
				timesync_status.sync_pending = 0;
				timesync_status.synchronised = 0;

				etimer_stop(&tmr_periodic);

				process_post(PROCESS_BROADCAST, timesync_event,
					&timesync_status);
				syslog_P(LOG_DAEMON | LOG_INFO, PSTR("Stopped"));
			}
		}
		else if (ev == PROCESS_EVENT_TIMER) {
			if (data == &tmr_periodic && etimer_expired(&tmr_periodic)) {
				etimer_reset(&tmr_periodic);

				if (timesync_status.running && stimer_expired(&tmr_resync)) {
					stimer_reset(&tmr_resync);

					timesync_status.sync_pending = 1;
					process_poll(&timesync_process);
				}
			}
			else if (timesync_status.running) {
				sntp_appcall(ev, data);
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			timesync_status.running = 0;
			timesync_status.sync_pending = 0;
			timesync_status.synchronised = 0;

#if CONFIG_DRIVERS_DS1307
			// Disable the DS1307 clock output to save power
			ds1307_ctl_set(DS1307_OUT_LOW);
#endif

			process_exit(&timesync_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

void timesync_schedule_resync(void) {
	// Hack the timer
	if (timesync_status.running) {
		tmr_resync.start = clock_seconds() - tmr_resync.interval;
	}
}

int timesync_set_time(const wallclock_time_t *time) {
#if CONFIG_DRIVERS_DS1307
	int err;
	struct tm tm;
	uint32_t cur;
#endif
	wallclock_time_t oldtime;
	int32_t diffms;

	// Get the current wallclock time
	wallclock_get(&oldtime);

	// Update the wallclock
	wallclock_set(time);

	// Work out the time difference
	diffms = ((int32_t)time->sec - (int32_t)oldtime.sec) * 1000;
	diffms += (((int32_t)time->frac - (int32_t)oldtime.frac) * 1000) >> 12;

	// Tell folks about the change
	process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
	syslog_P(LOG_DAEMON | LOG_INFO, PSTR("Clock adjusted by %ldms"),
		diffms);

#if CONFIG_DRIVERS_DS1307
	// Get the current RTC time
	err = ds1307_clock_get(&tm);
	if (err) {
		return err;
	}

	cur = mktime(&tm);

	// If the RTC is 3 or more seconds out, change the RTC
	if (abs(time->sec - cur) >= 3) {
		gmtime(time->sec, &tm);

		err = ds1307_clock_set(&tm);
		if (err) {
			return err;
		}

		// Make sure the clock is running
		ds1307_clock_start();
		ds1307_ctl_set(DS1307_OUT_SQW_32768HZ);

		syslog_P(LOG_DAEMON | LOG_INFO,
			PSTR("RTC adjusted by %ds"), time->sec - cur);
	}
#endif

	return 0;
}

void sntp_synced(const struct sntp_hdr *message) {
	if (!message) {
		timesync_status.synchronised = 0;
		process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
		syslog_P(LOG_DAEMON | LOG_WARNING, PSTR("SNTP timed out"));
		return;
	}

	// Sanity check message
	if (message->VN == 0 ||
		message->Stratum == 0 ||
		uip_ntohl(message->TxTimestamp[0]) == 0)
	{
		timesync_status.synchronised = 0;
		process_post(PROCESS_BROADCAST, timesync_event, &timesync_status);
		syslog_P(LOG_DAEMON | LOG_WARNING, PSTR("Invalid SNTP message"));
		return;
	}

	// Set the new wallclock time
	wallclock_time_t new = {
		.sec = NTP_TO_UNIX(uip_ntohl(message->TxTimestamp[0])),
		.frac = uip_ntohl(message->TxTimestamp[1]) >> 20, // 32 to 12 bit fixed
	};

	// Set our status flags
	timesync_status.synchronised = 1;

	// Update the clock
	timesync_set_time(&new);
}

uint32_t sntp_seconds(void) {
	return UNIX_TO_NTP(wallclock_seconds());
}

