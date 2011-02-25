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

#include <contiki.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "drivers/wallclock.h"
#include "apps/timesync.h"

#include <time.h>
#include <avr/pgmspace.h>

PROCESS(shell_date_process, "date");
SHELL_COMMAND(shell_date_command,
	"date", "date: show/adjust date/time",
	&shell_date_process);
INIT_SHELL_COMMAND(shell_date_command);

#define DATE_MAXLEN 32

PROCESS_THREAD(shell_date_process, ev, data) {
	struct tm tm;

	PROCESS_BEGIN();

	if ((data == NULL) || (strlen(data) == 0)) {
		char *date = malloc(DATE_MAXLEN);
		if (!date) {
			PROCESS_EXIT();
		}

		gmtime(wallclock_seconds(), &tm);
		strftime_P(date, DATE_MAXLEN, PSTR("%c"), &tm);
		
		shell_output_P(&shell_date_command,
			PSTR("%s"), date);

		free(date);
	}
	else if (strcmp_P(data, PSTR("--sync")) == 0) {
		if (timesync_status.running) {
			timesync_schedule_resync();
			PROCESS_WAIT_EVENT_UNTIL(ev == timesync_event);
			shell_output_P(&shell_date_command,
				PSTR("Time was adjusted."));
		}
		else {
			shell_output_P(&shell_date_command,
				PSTR("TimeSync not running."));
		}
	}
	else {
		shell_output_P(&shell_date_command,
			PSTR("Usage: date [--sync]"));
	}

	PROCESS_END();
}

