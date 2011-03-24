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
#include "shell.h"
#include "apps/syslog.h"

#include <string.h>
#include <avr/pgmspace.h>

PROCESS(shell_log_process, "log");
SHELL_COMMAND(shell_log_command,
	"log", "log: send something to syslog",
	&shell_log_process);
INIT_SHELL_COMMAND(shell_log_command);

PROCESS_THREAD(shell_log_process, ev, data) {
	char *msg;

	PROCESS_BEGIN();

	msg = data;
	if (msg == NULL || strlen(msg) == 0) {
		shell_output_P(&shell_log_command,
			PSTR("Usage: log <message>\n"));
		PROCESS_EXIT();
	}

	syslog_P(LOG_MAKEPRI(LOG_USER, LOG_INFO), PSTR("%s"), msg);

	PROCESS_END();
}

