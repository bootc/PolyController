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

#include <time.h>
#include <avr/pgmspace.h>

PROCESS(shell_uptime_process, "uptime");
SHELL_COMMAND(uptime_command,
	"uptime", "uptime: show system time since boot",
	&shell_uptime_process);
INIT_SHELL_COMMAND(uptime_command);

PROCESS_THREAD(shell_uptime_process, ev, data) {
	PROCESS_BEGIN();

	uint32_t time = clock_seconds();

	uint8_t sec  = time % 60;
	uint8_t min  = (time / 60) % 60;
	uint8_t hour = (time / 3600) % 24;
	uint16_t day  = time / 86400;

	shell_output_P(&uptime_command,
		PSTR("Uptime: %d days, %02d:%02d:%02d\n"),
		day, hour, min, sec);

	PROCESS_END();
}

