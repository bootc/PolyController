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

#include <stdlib.h>
#include <stdbool.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include <board.h>
#include <init.h>
#include <polyfs.h>
#include <flashmgt.h>
#include "drivers/uart.h"

uint8_t mcusr_mirror __attribute__ ((section (".noinit")));

typedef void (*bootloader_jump_type)(void) __attribute__((__noreturn__));
bootloader_jump_type app_start_real = (bootloader_jump_type)0x0000;

void early_init(void)
	__attribute__((naked))
	__attribute__((section(".init3")));

void early_init(void) {
	// Save MCUSR and clear it
	mcusr_mirror = MCUSR;
	MCUSR = 0;

	// Disable the watchdog
	wdt_disable();
}

static inline void app_start(void) __attribute__((__noreturn__));
static inline void app_start(void) {
	// Make sure interrupts go to the application section
	uint8_t mcucr = MCUCR;
	MCUCR = mcucr | _BV(IVCE);
	MCUCR = mcucr & ~_BV(IVSEL);

	// Start the app
	app_start_real();
}

static void reboot(void) __attribute__((__noreturn__));
static void reboot(void) {
	// Reset via watchdog
	wdt_enable(WDTO_15MS);
	while (1);
}

int main(void) {
	bool delay_boot = true;
	bool start_app = true;
	bool rescue = false;

	// Basic board init
	board_init();

	// Check for watchdog or JTAG reset
	if (mcusr_mirror & (_BV(WDRF) | _BV(JTRF))) {
		delay_boot = false;
	}

	// Check if the application area has some code in it
	if (pgm_read_byte(app_start_real) == 0xff) {
		start_app = false;
		rescue = true;
	}

	// Check if there's a code update lined up
	if (flashmgt_update_pending()) {
		start_app = false;
		rescue = false;
	}

	// Start the app now if we're not delaying or upgrading
	if (start_app && !delay_boot) {
		app_start();
	}

	// Move interrupt vectors to bootloader section
	uint8_t mcucr = MCUCR;
	MCUCR = mcucr | _BV(IVCE);
	MCUCR = mcucr | _BV(IVSEL);

	// Enable interrupts
	sei();

	// Initialise serial
#define BAUD CONFIG_UART0_BAUD
#include <util/setbaud.h>
	uart_init(
		(UBRRH_VALUE << 8) |
		(UBRRL_VALUE << 0) |
		(USE_2X ? 0x8000 : 0));

	// Clear screen
	uart_puts("\r\n\x1b[H\x1b[J");

	// Print boot message
	uart_puts("PolyController " CONFIG_BOARD " " CONFIG_IMAGE "\r\n\r\n");

	// Initialise everything else
	init_doinit();

	// Delay boot?
	if (delay_boot) {
		// Do a little LED dance on the diag port while things settle
		for (int i = 0; i <= 7; i++) {
			PORTA |= _BV(i);
			_delay_ms(125);
		}
		for (int i = 0; i <= 7; i++) {
			PORTA &= ~_BV(i);
			_delay_ms(125);
		}
	}

	// Check to see if we need to update the MCU flash
	if (flashmgt_update_pending()) {
		uart_puts("Applying code update. Please wait...\r\n");

		// Run the flashmgmt bootloader code
		int ret = flashmgt_bootload();
		if (ret) {
			uart_puts("Code update failed!\r\n");
		}
		else {
			uart_puts("Code has been updated.\r\n");
		}
	}
	else if (rescue) {
		// FIXME: Some sort of rescue mode?
		uart_puts("Rescue mode not yet implemented. Hanging.\r\n");
		while (1) {
			PORTA = 0xaa;
			_delay_ms(200);
			PORTA = 0x55;
			_delay_ms(200);
		};
	}

	// Reset ourselves
	reboot();
}

