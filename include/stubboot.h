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

#ifndef STUBBOOT_H
#define STUBBOOT_H

#include <avr/pgmspace.h>
#include <verify.h>

#define STUBBOOT_ADDR CONFIG_STUBBOOT_START_ADDR

struct stubboot_selfupdate_info;

struct stubboot_table {
	uint8_t ver_major;
	uint8_t ver_minor;
	uint8_t ver_patch;
	uint8_t padding1;

	/*
	 * Writes a single flash page at *page* address 'page'. 'addr' must point to
	 * a location in RAM that is at least SPM_PAGESIZE bytes long. The page
	 * address can be obtained by dividing the byte address by SPM_PAGESIZE.
	 *
	 * The entire buffer is written to flash. The flash page is erased prior to
	 * writing.
	 *
	 * Interrupts and the watchdog must both be disabled before calling this
	 * routine. They are forcefully disabled before the routine executes and not
	 * restored before returning.
	 *
	 * Returns:
	 *  -1 - on failure
	 *   0 - on success
	 *  >0 - on success after write retries (return is number of retries)
	 */
	int8_t (* write_page)(uint16_t page, const void *addr);

	/*
	 * Updates the bootloader code in flash.
	 *  'pages' - size of the bootloader code in increments of SPM_PAGESIZE.
	 *  'crc'   - 16-bit CRC of the code calculated using _crc16_update() from
	 *            util/crc16.h (initial value 0xffff).
	 *  'addr'  - pointer to RAM memory buffer containing bootloader code.
	 *
	 * If the bootloader code is shorter than (pages * SPM_PAGESIZE) bytes, it
	 * should be padded with 0xff. The entire buffer is considered when
	 * calculating the CRC.
	 *
	 * The function cannot be called from the bootloader that is being updated
	 * for obvious reasons - only call this from APPLICATION CODE! Written pages
	 * are not CRC checked after being written, this must be done by the caller.
	 *
	 * Interrupts and the watchdog must both be disabled before calling this
	 * routine. They are forcefully disabled before the routine executes and not
	 * restored before returning.
	 *
	 * Returns:
	 *  -1 - on failure
	 *   0 - on success
	 *  >0 - on success after write retries (return is number of retries)
	 */
	int8_t (* update_loader)(uint8_t pages, uint16_t crc, void *addr);
};

// Compile-time check of struct size
verify(sizeof(struct stubboot_table) == 8);

// Function to retrieve stubboot_table without memcpy_PF
void stubboot_read_table(struct stubboot_table *t);

int stubboot_write_page(uint16_t page, const void *addr);
int stubboot_update_loader(uint8_t pages, uint16_t crc, void *addr);

#endif // STUBBOOT_H
