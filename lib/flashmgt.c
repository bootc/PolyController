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
#include <polyfs.h>
#include <polyfs_df.h>
#include <polyfs_cfs.h>
#include <avr/eeprom.h>
#include <init.h>

#include <stdio.h> // for debug only

#include "drivers/dataflash.h"

#include "flashmgt.h"

struct flashmgt_partition {
	uint32_t start;
	uint32_t end;
};

struct flashmgt_status {
	uint8_t pri : 1;
	uint8_t pending : 1;
};

static struct flashmgt_partition part[] = {
	{ .start = CONFIG_FLASHMGT_P1_START, .end = CONFIG_FLASHMGT_P1_END },
	{ .start = CONFIG_FLASHMGT_P2_START, .end = CONFIG_FLASHMGT_P2_END },
};

//struct flashmgt_status flashmgt_status_ee EEMEM;
//struct flashmgt_status flashmgt_status;
polyfs_fs_t *flashmgt_pfs;
polyfs_fs_t flashmgt_pfs_struct;
static uint8_t sec_write_ready = 0;

static void flashmgt_init(void);

INIT_LIBRARY(flashmgt, flashmgt_init);

static void flashmgt_init(void) {
	int ret;

	// Nullify in case we run into trouble
	flashmgt_pfs = NULL;
	polyfs_cfs_fs = NULL;

	// Make sure the flash chip is ready
	dataflash_wait_ready();

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x60);

	// FIXME: read EEPROM to work out primary partition
//	eeprom_read_block(&flashmgt_status, &flashmgt_status_ee,
//		sizeof(flashmgt_status));
	int pri = 0;

	// Try to open the filesystem
	ret = pfsdf_open(&flashmgt_pfs_struct,
		part[pri].start, part[pri].end - part[pri].start + 1);
	if (ret < 0) {
		return;
	}

	// Set up the CFS FS pointer
	flashmgt_pfs = &flashmgt_pfs_struct;
	polyfs_cfs_fs = flashmgt_pfs;

	return;
}

int flashmgt_sec_open(polyfs_fs_t *ptr);
int flashmgt_sec_close(polyfs_fs_t *ptr);

// FIXME
int flashmgt_sec_write_start(void) {
	int sec = 0;
	uint32_t addr;

	if (sec_write_ready) {
		printf_P(PSTR("Write already in progress.\n"));
		return -1;
	}

	// Clear SPRL, but keep sectors locked
	dataflash_write_enable();
	dataflash_write_status(0x60);

	// Unprotect the sectors
	addr = part[sec].start;
	while (addr <= part[sec].end) {
		dataflash_sector_t sec;

		if (dataflash_sector_from_addr(addr, &sec)) {
			printf_P(PSTR("End of sectors.\n"));
			break;
		}

		if (dataflash_write_enable()) {
			printf_P(PSTR("Write enable failed.\n"));
			return -1;
		}

		if (dataflash_unprotect_sector(addr)) {
			printf_P(PSTR("Unprotect failed.\n"));
			return -1;
		}

		printf_P(PSTR("Unprotected: 0x%06lx\n"), addr);

		addr = sec.end + 1;
	}

	// Now erase the partition
	addr = part[sec].start;
	while (addr <= part[sec].end) {
		printf_P(PSTR("Erasing: 0x%06lx\n"), addr);

		if (dataflash_write_enable()) {
			printf_P(PSTR("Write enable 2 failed.\n"));
			return -1;
		}

		if (addr + DATAFLASH_SECTOR_64K_SIZE - 1 <= part[sec].end) {
			if (dataflash_erase_64k(addr)) {
				printf_P(PSTR("Erase 64K failed.\n"));
				return -1;
			}
			addr += DATAFLASH_SECTOR_64K_SIZE;
		}
		else if (addr + DATAFLASH_SECTOR_32K_SIZE - 1 <= part[sec].end) {
			if (dataflash_erase_32k(addr)) {
				printf_P(PSTR("Erase 32K failed.\n"));
				return -1;
			}
			addr += DATAFLASH_SECTOR_32K_SIZE;
		}
		else {
			if (dataflash_erase_4k(addr)) {
				printf_P(PSTR("Erase 4K failed.\n"));
				return -1;
			}
			addr += DATAFLASH_SECTOR_4K_SIZE;
		}

		dataflash_wait_ready();
	}

	sec_write_ready = 1;

	return 0;
}

// FIXME
int flashmgt_sec_write_block(const void *buf, uint32_t offset, uint32_t len) {
	int sec = 0;
	uint32_t addr;

	if (!sec_write_ready) {
		return -1;
	}

	addr = part[sec].start + offset;

	while (len) {
		if (dataflash_write_enable()) {
			return -1;
		}

		int ret = dataflash_write_data(buf, addr,
			len < DATAFLASH_WR_PAGE_SIZE ? len : DATAFLASH_WR_PAGE_SIZE);

		if (ret < 0) {
			return -1;
		}
		else {
			addr += ret;
			buf = (uint8_t *)buf + ret;
			len -= ret;
		}

		dataflash_wait_ready();
	}

	return 0;
}

int flashmgt_sec_write_abort(void) {
	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x60);

	sec_write_ready = 0;
	return 0; // FIXME
}

int flashmgt_sec_write_finish(void) {
	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x60);

	sec_write_ready = 0;
	return 0; // FIXME
}

#if CONFIG_IMAGE_BOOTLOADER

int flashmgt_check_pending(void);
int flashmgt_swap_partitions(void);

#endif

