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
#include <sys/log.h>

#include <stdio.h>
#include <avr/pgmspace.h>
#if CONFIG_WATCHDOG
#include <avr/wdt.h>
#endif

#include <init.h>
#include <board.h>
#include <apps/serial.h>

int main(void) {
	// Basic board init
	board_init();

	// Start the main clock
	clock_init();

	// Enable interrupts
	sei();

	// Set up printf as early as possible (boot messages)
	serial_init();

	// Print boot message
	printf_P(PSTR("\n"));
	printf_P(PSTR("PolyController " CONFIG_BOARD " " CONFIG_IMAGE
		" v" CONFIG_VERSION "\n"));
	printf_P(PSTR("\n"));

	// Initialise everything else
	init_doinit();

#if CONFIG_WATCHDOG
	// Set up the watchdog
	wdt_enable(CONFIG_WATCHDOG_TIMEOUT);
#endif

	printf_P(PSTR("\n"));

	while (1) {
#if CONFIG_WATCHDOG
		// Poke the watchdog
		wdt_reset();
#endif

		// Run processes
		process_run();
	}

	return 0;
}

static int init_contiki(void) {
	process_init();
	return 0;
}

INIT_LIBRARY(contiki, init_contiki);
INIT_PROCESS(etimer_process);

#if LOG_CONF_ENABLED
void log_message(const char *part1, const char *part2) {
	printf_P(PSTR("%s%s\n"), part1 ? part1 : "", part2 ? part2 : "");
}
#endif

