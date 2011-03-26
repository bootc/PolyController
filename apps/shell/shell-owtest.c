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
#include <stdio.h>
#include <string.h>
#include <util/crc16.h>
#include <util/delay.h>

#include <onewire.h>

#include "drivers/ds2482.h"
#include "shell.h"

#define DS18B20_CONV_TIMEOUT (2 * CLOCK_SECOND)

PROCESS(shell_owtest_process, "owtest");
SHELL_COMMAND(owtest_command,
	"owtest", "owtest: test 1-wire bus",
	&shell_owtest_process);
INIT_SHELL_COMMAND(owtest_command);

static struct pt ow_pt;
static ow_search_t search;
static struct timer timeout;

static PT_THREAD(read_temp(struct pt *pt, const ow_addr_t *addr)) {
	int err;
	uint8_t scratch[9];
	uint8_t crc = 0;

	PT_BEGIN(pt);

	// Reset the bus
	err = ow_reset();
	if (err != 1) {
		shell_output_P(&owtest_command, PSTR("Reset failed.\n"));
		PT_EXIT(pt);
	}

	// Match ROM
	err = ow_write_byte(0x55);
	if (err) {
		shell_output_P(&owtest_command, PSTR("Match ROM failed\n"));
		PT_EXIT(pt);
	}
	for (int i = 0; i < sizeof(*addr); i++) {
		err = ow_write_byte(addr->u[i]);
		if (err) {
			shell_output_P(&owtest_command, PSTR("Match ROM failed\n"));
			PT_EXIT(pt);
		}
	}

	// Start temperature conversion
	err = ow_write_byte(0x44);
	if (err) {
		shell_output_P(&owtest_command, PSTR("Convert T failed\n"));
		PT_EXIT(pt);
	}

	// Conversion timeout
	timer_set(&timeout, DS18B20_CONV_TIMEOUT);
	while (1) {
		_delay_ms(10); // for good measure

		// Read a bit from the bus
		int ret = ow_read_bit();

		// Check for stop conditions
		if (ret == 1) {
			break;
		}
		else if (ret == -1) {
			shell_output_P(&owtest_command, PSTR("Read status failed.\n"));
			PT_EXIT(pt);
		}
		else if (timer_expired(&timeout)) {
			shell_output_P(&owtest_command, PSTR("Conversion has taken too long. Giving up.\n"));
			PT_EXIT(pt);
		}

		// Poll the process and yield the thread
		process_poll(&shell_owtest_process);
		PT_YIELD(pt);
	}

	// Reset and MATCH ROM again
	err = ow_reset();
	if (err != 1) {
		shell_output_P(&owtest_command, PSTR("Reset failed.\n"));
		PT_EXIT(pt);
	}

	// Match ROM
	err = ow_write_byte(0x55);
	if (err) {
		shell_output_P(&owtest_command, PSTR("Match ROM failed\n"));
		PT_EXIT(pt);
	}
	for (int i = 0; i < sizeof(*addr); i++) {
		err = ow_write_byte(addr->u[i]);
		if (err) {
			shell_output_P(&owtest_command, PSTR("Match ROM failed\n"));
			PT_EXIT(pt);
		}
	}

	// Read the scratch pad
	err = ow_write_byte(0xBE);
	if (err) {
		shell_output_P(&owtest_command, PSTR("Read scratch pad failed\n"));
		PT_EXIT(pt);
	}

	for (int i = 0; i < sizeof(scratch); i++) {
		err = ow_read_byte();
		if (err < 0) {
			shell_output_P(&owtest_command, PSTR("Read byte failed\n"));
			PT_EXIT(pt);
		}

		scratch[i] = err;
		crc = _crc_ibutton_update(crc, scratch[i]);
	}

	// Make sure the CRC is valid
	if (crc) {
		shell_output_P(&owtest_command, PSTR("CRC check failed!\n"));
		PT_EXIT(pt);
	}

	// Convert temperature to floating point
	int16_t rawtemp = scratch[0] | (scratch[1] << 8);
	float temp = (float)rawtemp * 0.0625;

	shell_output_P(&owtest_command, 
		PSTR("Scratchpad: %02x%02x %02x%02x %02x %02x%02x%02x %02x\n"),
		scratch[0], scratch[1], // temperature
		scratch[2], scratch[3], // TH,TL alarm thresholds
		scratch[4], // config
		scratch[5], scratch[6], scratch[7], // reserved
		scratch[8]); // CRC

	shell_output_P(&owtest_command, PSTR("Reading: %0.2fC\n"), temp);

	PT_END(pt);
}

PROCESS_THREAD(shell_owtest_process, ev, data) {
	int err;

	PROCESS_BEGIN();

	// Attempt to acquire 1-Wire lock
	while (!ow_lock()) {
		PROCESS_PAUSE();
	}

	// Reset the bus
	err = ow_reset();
	if (err < 0) {
		shell_output_P(&owtest_command, PSTR("Bus reset failed.\n"));
		PROCESS_EXIT();
	}
	else if (err == 0) {
		shell_output_P(&owtest_command, PSTR("No presence detected.\n"));
		PROCESS_EXIT();
	}

	// Start the search
	err = ow_search_first(&search, 0);
	do {
		if (err < 0) {
			shell_output_P(&owtest_command, PSTR("Search error: %d\n"), err);
			PROCESS_EXIT();
		}
		else if (err == 0) {
			shell_output_P(&owtest_command, PSTR("No devices found.\n"));
			break;
		}

		// Print search result
		shell_output_P(&owtest_command,
			PSTR("Found: %02x.%02x%02x%02x%02x%02x%02x\n"),
			search.rom_no.family, // family code
			search.rom_no.id[0], search.rom_no.id[1], search.rom_no.id[2],
			search.rom_no.id[3], search.rom_no.id[4], search.rom_no.id[5]);

		// If it's a DS18B20, read it
		if (search.rom_no.family == 0x28) {
			shell_output_P(&owtest_command, PSTR("Reading temperature...\n"));
			PROCESS_PT_SPAWN(&ow_pt, read_temp(&ow_pt, &search.rom_no));
		}

		// If we found the last device on the bus, break out of the loop
		if (search.last_device_flag) {
			break;
		}

		// Find the next device on the bus
		err = ow_search_next(&search);
	} while (1);

	// Relinquish bus lock
	ow_unlock();

	shell_output_P(&owtest_command, PSTR("Search complete.\n"));

	PROCESS_END();
}

