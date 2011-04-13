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
#include <settings.h>
#include "drivers/uart.h"

#if CONFIG_LIB_OPTIBOOT
#include <optiboot.h>
#endif

uint8_t mcusr_mirror __attribute__ ((section (".noinit")));

typedef void (*bootloader_jump_type)(void) __attribute__((__noreturn__));
bootloader_jump_type app_start_real = (bootloader_jump_type)0x0000;

typedef enum {
	BOOT_MODE_APP = 0,		// Immediately launch the application
	BOOT_MODE_DELAY = 1,	// Let things settle, then watchdog reboot
	BOOT_MODE_UPDATE = 2,	// Apply software upgrade from FLASH
	BOOT_MODE_RESCUE = 3,	// Arduino-style bootloader
	BOOT_MODE_WIPE = 4,		// Erase EEPROM settings
} boot_mode_t;

void early_init(void)
	__attribute__((naked))
	__attribute__((section(".init3")));

void early_init(void) {
	// Make extra-certain interrupts are disabled
	cli();

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

#define JPORT PORTD
#define JDDR DDRD
#define JPIN PIND

static inline int check_jumper_lo(uint8_t pa, uint8_t pb) {
	// Set pa output low, pb to input w/ pullup
	JDDR = (JDDR & ~_BV(pb)) | _BV(pa);
	JPORT = (JPORT & ~_BV(pa)) | _BV(pb);

	_delay_us(10);

	if (JPIN & _BV(pb)) {
		return 0;
	}
	else {
		return 1;
	}
}

static inline int check_jumper_hi(uint8_t pa, uint8_t pb) {
	// Set pa output high, pb to input w/o pullup
	JDDR = (JDDR & ~_BV(pb)) | _BV(pa);
	JPORT = (JPORT & ~_BV(pb)) | _BV(pa);

	_delay_us(10);

	if (JPIN & _BV(pb)) {
		return 1;
	}
	else {
		return 0;
	}
}

static int check_jumper(uint8_t pa, uint8_t pb) {
	uint8_t tries = 3;

	do {
		if (!check_jumper_lo(pa, pb)) {
			break;
		}
		if (!check_jumper_hi(pa, pb)) {
			break;
		}
		if (!check_jumper_lo(pb, pa)) {
			break;
		}
		if (!check_jumper_hi(pb, pa)) {
			break;
		}
	} while (--tries);

	return (tries == 0);
}

static uint8_t check_jumpers(void) {
	uint8_t jumpers = 0;

	// Save port state
	uint8_t port = JPORT;
	uint8_t ddr = JDDR;

	// Vertical jumpers
	if (check_jumper(PIND2, PIND3)) {
		jumpers |= 0x01;
	}
	if (check_jumper(PIND4, PIND5)) {
		jumpers |= 0x02;
	}
	// Horizontal jumpers
	if (check_jumper(PIND2, PIND4)) {
		jumpers |= 0x04;
	}
	if (check_jumper(PIND3, PIND5)) {
		jumpers |= 0x08;
	}

	// Restore port state
	JPORT = port;
	JDDR = ddr;

	return jumpers;
}

int main(void) {
	// Default to delayed boot mode
	boot_mode_t mode = BOOT_MODE_DELAY;

	// Basic board init
	board_init();

	// Check for watchdog or JTAG reset
	if (mcusr_mirror & (_BV(WDRF) | _BV(JTRF))) {
		mode = BOOT_MODE_APP;
	}

	// Check if the application area has some code in it
	if (pgm_read_byte(app_start_real) == 0xff) {
		mode = BOOT_MODE_RESCUE;
	}

	// Check if there's a code update lined up
	if (flashmgt_update_pending()) {
		mode = BOOT_MODE_UPDATE;
	}

	// Check for jumpers which will force a boot mode
	uint8_t jumpers = check_jumpers();
	if (jumpers) {
		if (jumpers == 0x01) {
			mode = BOOT_MODE_RESCUE;
		}
		else if (jumpers == 0x02) {
			mode = BOOT_MODE_WIPE;
		}
/*
		else if (jumpers == 0x03) {
		}
		else if (jumpers == 0x04) {
		}
		else if (jumpers == 0x08) {
		}
		else if (jumpers == 0x0C) {
		}
*/
	}

	// Start the app now if we're not delaying or upgrading
	if (mode == BOOT_MODE_APP) {
		app_start();
	}

	// Flash diagnostic LEDs
	for (int i = 0; i < 4; i++) {
		CONFIG_DIAG_PORT = 0x80;
		_delay_ms(125);
		CONFIG_DIAG_PORT = mode | 0x80;
		_delay_ms(125);
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

	// Enter rescue mode
	if (mode == BOOT_MODE_RESCUE) {
		// Start our hacked optiboot loader
		optiboot();

		// Reboot when finished
		reboot();
	}

	// Clear screen
	uart_puts("\r\n\x1b[H\x1b[J");

	// Print boot message
	uart_puts("PolyController " CONFIG_BOARD " " CONFIG_IMAGE
		" v" CONFIG_VERSION "\r\n\r\n");

	// Initialise peripherals & libraries
	init_doinit();

	if (mode == BOOT_MODE_DELAY) {
		// No need to do anything, just reboot
		uart_txwait();
		reboot();
	}
	else if (mode == BOOT_MODE_UPDATE) {
		uart_puts("Applying code update. Please wait...\r\n");

		// Run the flashmgmt bootloader code
		int ret = flashmgt_bootload();
		if (ret) {
			uart_puts("Code update failed!\r\n");
		}
		else {
			uart_puts("Code has been updated.\r\n");
		}

		// Reboot
		uart_txwait();
		reboot();
	}
	else if (mode == BOOT_MODE_WIPE) {
		uart_puts("Erasing all settings. Please wait...\r\n");

		// Wipe EEPROM
		settings_wipe();

		uart_puts("Settings have been erased.\r\n"
			"*** Perform a firmware upgrade to ensure "
			"correct operation. ***\r\n"
			"Remove jumper and cycle power to continue.\r\n");
	}
	else {
		uart_puts("Unknown boot mode (bootloader internal error).\r\n");
	}

	uart_txwait();
	abort();
}

