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
#include <dev/serial-line.h>

#include <stdio.h>
#include <avr/io.h>
#include <init.h>

#include "serial.h"
#include "drivers/uart.h"


PROCESS(serial_process, "Serial");
INIT_PROCESS(serial_process);

static void serial_init(void) {
#define BAUD CONFIG_UART0_BAUD
#include <util/setbaud.h>
	uart_init(
		(UBRRH_VALUE << 8) |
		(UBRRL_VALUE << 0) |
		(USE_2X ? 0x8000 : 0));

#if CONFIG_UART1_BAUD
#undef BAUD
#define BAUD CONFIG_UART1_BAUD
#include <util/setbaud.h>
	uart1_init(
		(UBRRH_VALUE << 8) |
		(UBRRL_VALUE << 0) |
		(USE_2X ? 0x8000 : 0));
#endif
}

static void pollhandler(void) {
	process_poll(&serial_process);

	while (1) {
		// Read a character
		uint16_t c = uart_getc();

		// Buffer is empty?
		if (c & UART_NO_DATA) {
			break;
		}

		uart_putc(c);

		// Send it into contiki
		serial_line_input_byte(c);

		// Handle '\r' and add an extra '\n'
		if ((c & 0xff) == '\r') {
			uart_putc('\n');
			serial_line_input_byte('\n');
		}
	};
}

static int serial_putc(char c, FILE *stream) {
	if (c == '\n') {
		uart_putc('\r');
	}

	uart_putc(c);

	return 0;
}

static FILE uart_stream =
    FDEV_SETUP_STREAM(serial_putc, NULL, _FDEV_SETUP_WRITE);

PROCESS_THREAD(serial_process, ev, data) {
	PROCESS_POLLHANDLER(pollhandler());
	PROCESS_BEGIN();

	serial_init();
	serial_line_init();

    stdout = &uart_stream;
	process_poll(&serial_process);

	PROCESS_WAIT_UNTIL(ev == PROCESS_EVENT_EXIT);

	PROCESS_END();
}

