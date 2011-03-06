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
#include <settings.h>
#include <string.h>

#if CONFIG_WATCHDOG
#include <avr/wdt.h>
#endif

#include <stdio.h> // for debug only

#include "drivers/dataflash.h"
#include "flashmgt.h"

// memory buffer used during CRC check
#define CRC_BUFFER_SIZE 256

struct flashmgt_partition {
	uint32_t start;
	uint32_t end;
};

struct flashmgt_status {
	uint8_t primary : 1;
	uint8_t upgrade_pending : 1;
	uint8_t padding[3];
};

static struct flashmgt_partition part[] = {
	{ .start = CONFIG_FLASHMGT_P1_START, .end = CONFIG_FLASHMGT_P1_END },
	{ .start = CONFIG_FLASHMGT_P2_START, .end = CONFIG_FLASHMGT_P2_END },
};

static struct {
	uint8_t sec_write_ready : 1;
} flags;

static struct flashmgt_status status;
polyfs_fs_t *flashmgt_pfs;
polyfs_fs_t flashmgt_pfs_struct;

static void flashmgt_init(void);
INIT_LIBRARY(flashmgt, flashmgt_init);

static void flashmgt_init(void) {
	int ret;

	// Nullify in case we run into trouble
	flashmgt_pfs = NULL;
	polyfs_cfs_fs = NULL;

	// Make sure the flash chip is ready
	ret = dataflash_wait_ready();
	if (ret) {
		printf_P(PSTR("Wait 1\n"));
		return;
	}

	// Let us change SREG
	ret = dataflash_write_enable();
	if (ret) {
		printf_P(PSTR("Write enable 1\n"));
		return;
	}

	// Clear SPRL so we can change lockbits
	ret = dataflash_write_status(0x3c);
	if (ret) {
		printf_P(PSTR("Write SREG 1\n"));
		return;
	}

	// Let us change SREG
	dataflash_write_enable();
	if (ret) {
		printf_P(PSTR("Write enable 2\n"));
		return;
	}

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
	if (ret) {
		printf_P(PSTR("Write SREG 2\n"));
		return;
	}

	// Check we have some info about the flash blocks
	if (!settings_check(SETTINGS_KEY_FLASHMGT_STATUS, 0)) {
		printf_P(PSTR("Settings check\n"));
		return;
	}

	// Read in the info
	size_t size = sizeof(status);
	ret = settings_get(SETTINGS_KEY_FLASHMGT_STATUS, 0,
		&status, &size);
	if (ret != SETTINGS_STATUS_OK || size != sizeof(status)) {
		printf_P(PSTR("Settings get\n"));
		status.primary = 1; // so that secondary is 0
		return;
	}

	// Try to open the filesystem
	ret = pfsdf_open(&flashmgt_pfs_struct,
		part[status.primary].start,
		part[status.primary].end - part[status.primary].start + 1);
	if (ret < 0) {
		printf_P(PSTR("Filesystem open\n"));
		return;
	}

	// Set up the CFS FS pointer
	flashmgt_pfs = &flashmgt_pfs_struct;
	polyfs_cfs_fs = flashmgt_pfs;

	return;
}

int flashmgt_sec_open(polyfs_fs_t *ptr) {
	int ret;
	int sec = status.primary == 0 ? 1 : 0;

	// Clear the filesystem pointer for good measure
	memset(ptr, 0, sizeof(*ptr));

	// Open the filesystem
	ret = pfsdf_open(ptr,
		part[sec].start,
		part[sec].end - part[sec].start + 1);
	if (ret) {
		return ret;
	}

	return 0;
}

int flashmgt_sec_close(polyfs_fs_t *ptr) {
	int ret;

	// Close the filesystem
	ret = pfsdf_close(ptr);

	return ret;
}

int flashmgt_sec_write_start(void) {
	int ret;
	int sec = status.primary == 0 ? 1 : 0;
	uint32_t addr;

	if (flags.sec_write_ready) {
		printf_P(PSTR("Write already in progress.\n"));
		return -1;
	}

	printf_P(PSTR("Unlocking...\n"));

	// Allow us to change SREG
	ret = dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Clear SPRL, but keep sectors locked
	ret = dataflash_write_status(0x3c);
	if (ret) {
		return ret;
	}

	// Unprotect the sectors
	addr = part[sec].start;
	while (addr <= part[sec].end) {
		dataflash_sector_t sec;

		if (dataflash_sector_from_addr(addr, &sec)) {
			break;
		}

		ret = dataflash_write_enable();
		if (ret) {
			return ret;
		}

		ret = dataflash_unprotect_sector(addr);
		if (ret) {
			printf_P(PSTR("Unprotect failed.\n"));

			// Re-lock everything
			dataflash_write_enable();
			dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
			return ret;
		}

		addr = sec.end + 1;

#if CONFIG_WATCHDOG
		// Poke the watchdog
		wdt_reset();
#endif
	}

	// Allow us to change SREG
	ret = dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Set SPRL, but don't change sector locks
	ret = dataflash_write_status(DATAFLASH_SREG_SPRL | 0x24);
	if (ret) {
		return ret;
	}

	printf_P(PSTR("Erasing: "));

	// Now erase the partition
	addr = part[sec].start;
	while (addr <= part[sec].end) {
		// Let us erase sectors
		ret = dataflash_write_enable();
		if (ret) {
			printf_P(PSTR("Write enable 2 failed.\n"));
			return ret;
		}

		if (addr + DATAFLASH_SECTOR_64K_SIZE - 1 <= part[sec].end) {
			// Erase a 64K sector
			ret = dataflash_erase_64k(addr);
			if (ret) {
				printf_P(PSTR("Erase 64K failed.\n"));
				return -1;
			}
			addr += DATAFLASH_SECTOR_64K_SIZE;
		}
		else if (addr + DATAFLASH_SECTOR_32K_SIZE - 1 <= part[sec].end) {
			// Erase a 32K sector
			if (dataflash_erase_32k(addr)) {
				printf_P(PSTR("Erase 32K failed.\n"));
				return -1;
			}
			addr += DATAFLASH_SECTOR_32K_SIZE;
		}
		else {
			// Erase a 4K sector
			if (dataflash_erase_4k(addr)) {
				printf_P(PSTR("Erase 4K failed.\n"));
				return -1;
			}
			addr += DATAFLASH_SECTOR_4K_SIZE;
		}

		// Wait for the erase to complete
		dataflash_wait_ready();

		printf_P(PSTR("."));

#if CONFIG_WATCHDOG
		// Poke the watchdog
		wdt_reset();
#endif
	}

	printf_P(PSTR(" done!\n"));

	// OK to carry on with writes
	flags.sec_write_ready = 1;

	return 0;
}

int flashmgt_sec_write_block(const void *buf, uint32_t offset, uint32_t len) {
	int sec = status.primary == 0 ? 1 : 0;
	int ret;

	if (!flags.sec_write_ready) {
		return -1;
	}

	// The flash address is the start address of the partition + offset
	offset += part[sec].start;

	while (len) {
		// Enable writes
		ret = dataflash_write_enable();
		if (ret < 0) {
			return ret;
		}

		// Write the data block
		int ret = dataflash_write_data(buf, offset,
			len < DATAFLASH_WR_PAGE_SIZE ? len : DATAFLASH_WR_PAGE_SIZE);

		if (ret < 0) {
			return ret;
		}
		else {
			// Advance the buffer pointer
			offset += ret;
			buf = (uint8_t *)buf + ret;
			len -= ret;
		}

		// Wait for the write to complete
		dataflash_wait_ready();

		printf_P(PSTR("."));

#if CONFIG_WATCHDOG
		// Poke the watchdog
		wdt_reset();
#endif
	}

	return 0;
}

int flashmgt_sec_write_abort(void) {
	int ret;

	// Check if a write was initiated
	if (!flags.sec_write_ready) {
		return 0; // nothing to do
	}

	// Switch off enable flag
	flags.sec_write_ready = 0;

	// Let us change SREG
	ret = dataflash_write_enable();
	if (ret) {
		printf_P(PSTR("Write enable 1\n"));
		return ret;
	}

	// Clear SPRL so we can change lockbits
	ret = dataflash_write_status(0x3c);
	if (ret) {
		printf_P(PSTR("Write SREG 1\n"));
		return ret;
	}

	// Let us change SREG
	dataflash_write_enable();
	if (ret) {
		printf_P(PSTR("Write enable 2\n"));
		return ret;
	}

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
	if (ret) {
		printf_P(PSTR("Write SREG 2\n"));
		return ret;
	}

	// Disable update pending flag
	status.upgrade_pending = 0;

	// Write to settings
	ret = settings_set(SETTINGS_KEY_FLASHMGT_STATUS, &status, sizeof(status));
	if (ret != SETTINGS_STATUS_OK) {
		return -1;
	}

	printf_P(PSTR("\nWRITE ABORTED\n"));

	return 0;
}

int flashmgt_sec_write_finish(void) {
	int ret;
	polyfs_fs_t tempfs;
	void *crcbuf = NULL;

	// Check if a write was initiated
	if (!flags.sec_write_ready) {
		return 0; // nothing to do
	}

	// Switch off enable flag
	flags.sec_write_ready = 0;

	// Disable update pending flag (we'll set it later if things pass muster)
	status.upgrade_pending = 0;

	// Let us change SREG
	ret = dataflash_write_enable();
	if (ret) {
		printf_P(PSTR("Write enable 1\n"));
		return ret;
	}

	// Clear SPRL so we can change lockbits
	ret = dataflash_write_status(0x3c);
	if (ret) {
		printf_P(PSTR("Write SREG 1\n"));
		return ret;
	}

	// Let us change SREG
	dataflash_write_enable();
	if (ret) {
		printf_P(PSTR("Write enable 2\n"));
		return ret;
	}

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
	if (ret) {
		printf_P(PSTR("Write SREG 2\n"));
		return ret;
	}

	// Open the new filesystem so we can check the CRC
	ret = flashmgt_sec_open(&tempfs);
	if (ret) {
		printf_P(PSTR("FS open\n"));
		goto out;
	}

	// Malloc a buffer for the CRC check
	crcbuf = malloc(CRC_BUFFER_SIZE);
	if (!crcbuf) {
		printf_P(PSTR("malloc\n"));
		ret = -1;
		goto out;
	}

	// Check new filesystem CRC
	ret = polyfs_check_crc(&tempfs, crcbuf, CRC_BUFFER_SIZE);
	if (ret) {
		printf_P(PSTR("CRC check\n"));
		goto out;
	}

	// Set status flags
	status.upgrade_pending = 1;

out:
	// Close the filesystem
	flashmgt_sec_close(&tempfs);

	// Free the CRC buffer
	if (crcbuf) {
		free(crcbuf);
	}

	// Write status to settings
	int ret2 = settings_set(SETTINGS_KEY_FLASHMGT_STATUS,
		&status, sizeof(status));
	if (ret2 != SETTINGS_STATUS_OK) {
		printf_P(PSTR("Settings write fail.\n"));
		ret = ret2;
	}

	printf_P(PSTR("\n"));

	return ret;
}

#if CONFIG_IMAGE_BOOTLOADER

int flashmgt_check_pending(void);
int flashmgt_swap_partitions(void);

#endif

