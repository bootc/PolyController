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

#include <avr/boot.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <util/crc16.h>

#include <stubboot.h>

#define LOADER_SIZE (CONFIG_STUBBOOT_START_ADDR - CONFIG_BOOTLDR_START_ADDR)

// Number of write attempts
#define WRITE_ATTEMPTS 3

static int8_t write_page(uint16_t page, const void *addr);
static int8_t update_loader(uint8_t pages, uint16_t crc, void *addr);

static struct stubboot_table table
	__attribute__((used))
	__attribute__((section("_stubboot_table")))
= {
	.ver_major = CONFIG_VERSION_MAJOR,
	.ver_minor = CONFIG_VERSION_MINOR,
	.ver_patch = CONFIG_VERSION_PATCH,

	.write_page = write_page,
	.update_loader = update_loader,
};

static int8_t write_page(uint16_t page, const void *buf1) {
	uint8_t attempts = 0;
	uint_farptr_t addr = (uint_farptr_t)page * SPM_PAGESIZE;
	const uint8_t *buf = buf1;

	// Disable interrupts, disable watchdog
	__asm__ __volatile__ (
		"cli" "\n\t"
		"sts %0, %1" "\n\t"
		"sts %0, __zero_reg__" "\n\t"
		: /* no outputs */
		: "M" (_SFR_MEM_ADDR(_WD_CONTROL_REG)),
		"r" ((uint8_t)(_BV(_WD_CHANGE_BIT) | _BV(WDE)))
		: "r0"
		);

	// Must be a multiple of page size
	if (page > (FLASHEND / SPM_PAGESIZE)) {
		return -1;
	}

	// Erase the page we're about to write to
	boot_page_erase_safe(addr);
	boot_spm_busy_wait();

retry_write:
	// Check we haven't retried too many times
	if (attempts >= WRITE_ATTEMPTS) {
		return -1;
	}

	// Fill the page buffer
	for (uint16_t i = 0; i < SPM_PAGESIZE; i += 2) {
		// Set up little-endian word.
		uint16_t w = buf[i] | (buf[i + 1] << 8);

		boot_page_fill(addr + i, w);
	}

	// Store buffer in flash page.
	boot_page_write(addr);
	boot_spm_busy_wait();

	// Reenable RWW-section again so we can read the data back.
	boot_rww_enable();

	// Verify the write
	for (uint16_t i = 0; i < SPM_PAGESIZE; i++) {
		if (pgm_read_byte_far(addr + i) != buf[i]) {
			attempts++;
			goto retry_write;
		}
	}

	// Write succeeded
	return attempts;
}

static int8_t update_loader(uint8_t pages, uint16_t crc, void *addr) {
	uint16_t i;
	uint8_t ret = 0;
	const uint8_t *buf = addr;

	// Check the bootloader size isn't too big
	if (pages > (LOADER_SIZE / SPM_PAGESIZE)) {
		return -1;
	}
	// Arbitrary check to see if it isn't too small
	else if (pages < 2) {
		return -1;
	}

	// CRC check the data
	uint16_t crc1 = 0xffff;
	for (i = 0; i < ((uint16_t)pages * SPM_PAGESIZE); i++) {
		crc1 = _crc16_update(crc1, buf[i]);
	}
	if (crc1 != crc) {
		return -1;
	}

	// Update the bootloader
	uint16_t page = CONFIG_BOOTLDR_START_ADDR / SPM_PAGESIZE;
	while (pages--) {
		int ret1 = write_page(page, buf);
		if (ret1 < 0) {
			return ret1;
		}
		else {
			ret += ret1;
		}

		page++;
		buf += SPM_PAGESIZE;
	}

	return ret;
}

