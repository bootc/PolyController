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

#include "drivers/ds2482.h"
#include "shell.h"

#define DS18B20_CONV_TIMEOUT (2 * CLOCK_SECOND)

PROCESS(shell_owtest_process, "owtest");
SHELL_COMMAND(owtest_command,
	"owtest", "owtest: test 1-wire bus",
	&shell_owtest_process);
INIT_SHELL_COMMAND(owtest_command);

static struct pt ow_pt;
static ow_addr_t sensor;
static struct timer timeout;

static PT_THREAD(read_temp(struct pt *pt)) {
	uint8_t scratch[9];
	uint8_t crc = 0;

	PT_BEGIN(pt);

	// Reset the bus and send MATCH ROM
	ow_reset();
	ow_write_byte(0x55); // Match ROM
	for (int i = 0; i < sizeof(ow_addr_t); i++) {
		ow_write_byte(sensor[i]);
	}

	// Start temperature conversion
	ow_write_byte(0x44);

	// Conversion timeout
	timer_set(&timeout, DS18B20_CONV_TIMEOUT);
	while (1) {
		// Read a bit from the bus
		int ret = ow_read_bit();

		// Check for stop conditions
		if (ret == 1) {
			printf_P(PSTR("Conversion done!\n"));
			break;
		}
		else if (ret == -1) {
			printf_P(PSTR("Read bit failed.\n"));
			PT_EXIT(pt);
		}
		else if (timer_expired(&timeout)) {
			printf_P(PSTR("Conversion has taken too long. Giving up.\n"));
			PT_EXIT(pt);
		}

		// Poll the process and yield the thread
		process_poll(&shell_owtest_process);
		PT_YIELD(pt);
	}

	// Reset and MATCH ROM again
	ow_reset();
	ow_write_byte(0x55); // Match ROM
	for (int i = 0; i < sizeof(ow_addr_t); i++) {
		ow_write_byte(sensor[i]);
	}

	// Read the scratch pad
	ow_write_byte(0xBE); // Read Scratch Pad
	for (int i = 0; i < sizeof(scratch); i++){
		scratch[i] = ow_read_byte();
		crc = _crc_ibutton_update(crc, scratch[i]);
	}

	// Make sure the CRC is valid
	if (crc) {
		printf_P(PSTR("CRC check failed!\n"));
		PT_EXIT(pt);
	}

	// Convert temperature to floating point
	uint16_t rawtemp = scratch[0] | (scratch[1] << 8);
	float temp = (float)rawtemp / 16.0F;

	printf_P(
		PSTR("Scratchpad: %02x%02x %02x%02x %02x %02x%02x%02x %02x\n"),
		scratch[8], scratch[7], // temperature
		scratch[6], scratch[5], // TH,TL alarm thresholds
		scratch[4], // config
		scratch[3], scratch[2], scratch[1], // reserved
		scratch[0]); // CRC

	printf_P(PSTR("Temp Float: %0.2f\n"), temp);

	PT_END(pt);
}

PROCESS_THREAD(shell_owtest_process, ev, data) {
	int err;
	ow_search_t s;

	PROCESS_BEGIN();

	err = ds2482_detect(0x30);
	if (err) {
		printf_P(PSTR("Detect failed.\n"));
		PROCESS_EXIT();
	}

	err = ow_reset();
	if (err < 0) {
		printf_P(PSTR("Bus reset failed.\n"));
		PROCESS_EXIT();
	}
	else if (err == 0) {
		printf_P(PSTR("No presence detected.\n"));
		PROCESS_EXIT();
	}

	err = ow_search_first(&s);
	do {
		if (err < 0) {
			printf_P(PSTR("Search error: %d\n"), err);
			PROCESS_EXIT();
		}
		else if (err == 0) {
			printf_P(PSTR("No devices found.\n"));
			break;
		}

		printf_P(PSTR("Found: %02x.%02x%02x%02x%02x%02x%02x\n"),
			s.rom_no[0], // family code
			s.rom_no[1], s.rom_no[2], s.rom_no[3],
			s.rom_no[4], s.rom_no[5], s.rom_no[6]); // address

		if (s.rom_no[0] == 0x28) {
			memcpy(sensor, s.rom_no, sizeof(sensor));
		}

		err = ow_search_next(&s);
	} while (err != 0);

	printf_P(PSTR("Search complete.\n"));

	PROCESS_PT_SPAWN(&ow_pt, read_temp(&ow_pt));

	PROCESS_END();
}

