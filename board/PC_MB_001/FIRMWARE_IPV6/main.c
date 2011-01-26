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
#include <dev/serial-line.h>

#include <stdio.h>
#include <avr/pgmspace.h>

#include "board.h"
#include "apps/network.h"
#include "apps/sntpclient.h"
#include "apps/monitor.h"
#include "apps/serial-shell.h"
#include "apps/shell/shell-ps.h"
#include "apps/shell/shell-netstat.h"

PROCINIT(
	&etimer_process,
	&tcpip_process,
	&network_process,
	&sntp_process,
	&serial_line_process,
	&monitor_process);

int main(void) {
	board_init();
	clock_init();

	sei();

	/* Initialize drivers and event kernel */
	process_init();
	procinit_init();

	serial_line_init();
	serial_shell_init();
	shell_ps_init();
	shell_netstat_init();
	monitor_init();

	while (1) {
		// Run processes
		process_run();
	}

	return 0;
}

#if LOG_CONF_ENABLED
void log_message(const char *part1, const char *part2) {
	printf_P(PSTR("%s%s\n"), part1 ? part1 : "", part2 ? part2 : "");
}
#endif

