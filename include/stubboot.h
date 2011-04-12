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
	 * Writes a single flash page at byte address 'page'. 'addr' must point to
	 * a location in RAM that is at least SPM_PAGESIZE bytes long.
	 *
	 * The entire buffer is written to flash. The flash page is erased prior to
	 * writing.
	 *
	 * Interrupts and the watchdog must both be disabled before calling this
	 * routine.
	 */
	void (* write_page)(uint_farptr_t page, const void *addr);

	/*
	 * Updates the bootloader code in flash. 'info' is a pointer to a structure
	 * defined below. The function returns 0 on success or non-zero on failure.
	 *
	 * The function cannot be called from the bootloader that is being updated
	 * for obvious reasons - only call this from APPLICATION CODE! Written pages
	 * are not CRC checked after being written.
	 *
	 * Interrupts and the watchdog must both be disabled before calling this
	 * routine.
	 */
	int (* update_loader)(const struct stubboot_selfupdate_info *info);
};

struct stubboot_selfupdate_info {
	/*
	 * Size of bootloader section to be written. This must be at least 512
	 * bytes long (any sane bootloader is at least that big) and less than
	 * 512 bytes less than the bootloader section size (7680 bytes on
	 * PC-MB-001). It must also be a multiple of SPM_PAGESIZE. Unused bytes at
	 * the end of the buffer should be set to 0xff.
	 */
	uint16_t size;

	/*
	 * CRC-16 of the bootloader data. This is checked within the update_loader()
	 * function and the load is aborted if it does not match. This can be
	 * computed using _crc16_update() from util/crc16.h with an initial value
	 * of 0xffff.
	 */
	uint16_t crc;

	/*
	 * Pointer to a RAM buffer of at least 'size' bytes.
	 */
	void *addr;
};

// Compile-time check of struct size
verify(sizeof(struct stubboot_table) == 8);
verify(sizeof(struct stubboot_selfupdate_info) == 6);

// Function to retrieve stubboot_table without memcpy_PF
void stubboot_read_table(struct stubboot_table *t);

int stubboot_write_page(uint_farptr_t page, const void *addr);
int stubboot_update_loader(const struct stubboot_selfupdate_info *info);

#endif // STUBBOOT_H
