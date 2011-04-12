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

static void write_page(uint_farptr_t page, const void *addr);
static int update_loader(const struct stubboot_selfupdate_info *info);

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

static void write_page(uint_farptr_t page, const void *addr) {
	const uint8_t *buf = addr;

	// Safety net
	cli();
	wdt_disable();

	// Erase the page we're about to write to
	boot_page_erase_safe(page);
	boot_spm_busy_wait();

	// Fill the page buffer
	for (uint16_t i = 0; i < SPM_PAGESIZE; i += 2) {
		// Set up little-endian word.
		uint16_t w = buf[i] | (buf[i + 1] << 8);

		boot_page_fill(page + i, w);
	}

	// Store buffer in flash page.
	boot_page_write_safe(page);
	boot_spm_busy_wait();

	// Reenable RWW-section again. We need this if we want to jump back
	// to the application after bootloading.
	boot_rww_enable();
}

static int update_loader(const struct stubboot_selfupdate_info *info) {
	uint16_t i;

	// Check the bootloader size isn't too big
	if (info->size > LOADER_SIZE) {
		return -1;
	}
	// Arbitrary check to see if it isn't too small
	else if (info->size < 512) {
		return -1;
	}
	else if (info->size % SPM_PAGESIZE) {
		return -1;
	}

	// CRC check the data
	uint16_t crc = 0xffff;
	for (i = 0; i < info->size; i++) {
		crc = _crc16_update(crc, ((uint8_t *)info->addr)[i]);
	}
	if (crc != info->crc) {
		return -1;
	}

	// Update the bootloader
	for (i = 0; i < info->size; i += SPM_PAGESIZE) {
		uint_farptr_t addr = CONFIG_BOOTLDR_START_ADDR + i;
		const uint8_t *buf = ((const uint8_t *)info->addr) + i;

		write_page(addr, buf);
	}

	return 0;
}

