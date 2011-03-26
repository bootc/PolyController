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

#include <stdint.h>
#include <polyfs.h>
#include <polyfs_df.h>
#if CONFIG_LIB_POLYFS_CFS
#include <polyfs_cfs.h>
#endif
#include <init.h>
#include <settings.h>
#include <string.h>

#if CONFIG_WATCHDOG
#include <avr/wdt.h>
#endif

#if CONFIG_IMAGE_BOOTLOADER
#include <avr/boot.h>
#include <avr/interrupt.h>
#endif

#include "drivers/dataflash.h"
#include "flashmgt.h"

// memory buffer used during CRC check
#define CRC_BUFFER_SIZE 256

// We use the same buffer for SPM in the bootloader
#if CRC_BUFFER_SIZE < SPM_PAGESIZE && CONFIG_IMAGE_BOOTLOADER
#undef CRC_BUFFER_SIZE
#define CRC_BUFFER_SIZE SPM_PAGESIZE
#endif

struct flashmgt_partition {
	uint32_t start;
	uint32_t end;
};

struct flashmgt_status {
	uint8_t primary : 1;
	uint8_t update_pending : 1;
	uint8_t padding[3];
};

static struct flashmgt_partition part[] = {
	{ .start = CONFIG_FLASHMGT_P1_START, .end = CONFIG_FLASHMGT_P1_END },
	{ .start = CONFIG_FLASHMGT_P2_START, .end = CONFIG_FLASHMGT_P2_END },
};

#if !CONFIG_IMAGE_BOOTLOADER
static struct {
	uint8_t sec_write_ready : 1;
} flags;
#endif

static struct flashmgt_status status;
#if CONFIG_LIB_POLYFS_CFS
polyfs_fs_t *flashmgt_pfs;
polyfs_fs_t flashmgt_pfs_struct;
#endif

static int flashmgt_init(void);
INIT_LIBRARY(flashmgt, flashmgt_init);

static int flashmgt_init(void) {
	int ret;

#if CONFIG_LIB_POLYFS_CFS
	// Nullify in case we run into trouble
	flashmgt_pfs = NULL;
	polyfs_cfs_fs = NULL;
#endif

	// Make sure the flash chip is ready
	ret = dataflash_wait_ready();
	if (ret) {
		return ret;
	}

	// Let us change SREG
	ret = dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Clear SPRL so we can change lockbits
	ret = dataflash_write_status(0x3c);
	if (ret) {
		return ret;
	}

	// Let us change SREG
	dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
	if (ret) {
		return ret;
	}

	// Check we have some info about the flash blocks
	if (!settings_check(SETTINGS_KEY_FLASHMGT_STATUS, 0)) {
		return -1;
	}

	// Read in the info
	size_t size = sizeof(status);
	ret = settings_get(SETTINGS_KEY_FLASHMGT_STATUS, 0,
		&status, &size);
	if (ret != SETTINGS_STATUS_OK || size != sizeof(status)) {
		status.primary = 1; // so that secondary is 0
		return ret;
	}

#if CONFIG_LIB_POLYFS_CFS
	// Try to open the filesystem
	ret = pfsdf_open(&flashmgt_pfs_struct,
		part[status.primary].start,
		part[status.primary].end - part[status.primary].start + 1);
	if (ret < 0) {
		return ret;
	}

	// Set up the CFS FS pointer
	flashmgt_pfs = &flashmgt_pfs_struct;
	polyfs_cfs_fs = flashmgt_pfs;
#endif

	return 0;
}

int flashmgt_sec_open(polyfs_fs_t *ptr) {
	int ret;
	int sec = !status.primary;

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

#if !CONFIG_IMAGE_BOOTLOADER
int flashmgt_sec_write_start(void) {
	int ret;
	int sec = !status.primary;
	uint32_t addr;

	if (flags.sec_write_ready) {
		return -1;
	}

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

	// Now erase the partition
	addr = part[sec].start;
	while (addr <= part[sec].end) {
		// Let us erase sectors
		ret = dataflash_write_enable();
		if (ret) {
			return ret;
		}

		if (addr + DATAFLASH_SECTOR_64K_SIZE - 1 <= part[sec].end) {
			// Erase a 64K sector
			ret = dataflash_erase_64k(addr);
			if (ret) {
				return -1;
			}
			addr += DATAFLASH_SECTOR_64K_SIZE;
		}
		else if (addr + DATAFLASH_SECTOR_32K_SIZE - 1 <= part[sec].end) {
			// Erase a 32K sector
			if (dataflash_erase_32k(addr)) {
				return -1;
			}
			addr += DATAFLASH_SECTOR_32K_SIZE;
		}
		else {
			// Erase a 4K sector
			if (dataflash_erase_4k(addr)) {
				return -1;
			}
			addr += DATAFLASH_SECTOR_4K_SIZE;
		}

		// Wait for the erase to complete
		dataflash_wait_ready();

#if CONFIG_WATCHDOG
		// Poke the watchdog
		wdt_reset();
#endif
	}

	// OK to carry on with writes
	flags.sec_write_ready = 1;

	return 0;
}

int flashmgt_sec_write_block(const void *buf, uint32_t offset, uint32_t len) {
	int sec = !status.primary;
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
		return ret;
	}

	// Clear SPRL so we can change lockbits
	ret = dataflash_write_status(0x3c);
	if (ret) {
		return ret;
	}

	// Let us change SREG
	dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
	if (ret) {
		return ret;
	}

	// Disable update pending flag
	status.update_pending = 0;

	// Write to settings
	ret = settings_set(SETTINGS_KEY_FLASHMGT_STATUS, &status, sizeof(status));
	if (ret != SETTINGS_STATUS_OK) {
		return -1;
	}

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
	status.update_pending = 0;

	// Let us change SREG
	ret = dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Clear SPRL so we can change lockbits
	ret = dataflash_write_status(0x3c);
	if (ret) {
		return ret;
	}

	// Let us change SREG
	dataflash_write_enable();
	if (ret) {
		return ret;
	}

	// Set SPRL and lock all sectors
	dataflash_write_status(DATAFLASH_SREG_SPRL | 0x3c);
	if (ret) {
		return ret;
	}

	// Open the new filesystem so we can check the CRC
	ret = flashmgt_sec_open(&tempfs);
	if (ret) {
		goto out;
	}

	// Malloc a buffer for the CRC check
	crcbuf = malloc(CRC_BUFFER_SIZE);
	if (!crcbuf) {
		ret = -1;
		goto out;
	}

	// Check new filesystem CRC
	ret = polyfs_check_crc(&tempfs, crcbuf, CRC_BUFFER_SIZE);
	if (ret) {
		goto out;
	}

	// Set status flags
	status.update_pending = 1;

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
		ret = ret2;
	}

	return ret;
}
#endif

#if CONFIG_IMAGE_BOOTLOADER
static uint8_t buf[CRC_BUFFER_SIZE];

static void boot_program_page(uint32_t page, uint8_t *buf) {
	uint16_t i;
	uint8_t sreg;

	// Disable interrupts.
	sreg = SREG;
	cli();

	boot_page_erase_safe(page);
	boot_spm_busy_wait();

	for (i = 0; i < SPM_PAGESIZE; i += 2) {
		// Set up little-endian word.

		uint16_t w = *buf++;
		w += (*buf++) << 8;

		boot_page_fill(page + i, w);
	}

	// Store buffer in flash page.
	boot_page_write_safe(page);
	boot_spm_busy_wait();

	// Reenable RWW-section again. We need this if we want to jump back
	// to the application after bootloading.
	boot_rww_enable ();

	// Re-enable interrupts (if they were ever enabled).
	SREG = sreg;
}

bool flashmgt_update_pending(void) {
	// Read in the info
	size_t size = sizeof(status);
	int ret = settings_get(SETTINGS_KEY_FLASHMGT_STATUS, 0,
			&status, &size);
	if (ret != SETTINGS_STATUS_OK || size != sizeof(status)) {
		return false;
	}

	return status.update_pending;
}

int flashmgt_bootload(void) {
	int ret;
	polyfs_fs_t tempfs;
	struct polyfs_inode sysimg;

	// Don't do anything unless an update is lined up
	if (!status.update_pending) {
		return 0;
	}

	// Even if the update fails, we clear the pending flag
	status.update_pending = 0;

	// Open the new filesystem so we can check the CRC
	ret = flashmgt_sec_open(&tempfs);
	if (ret) {
		goto out;
	}

	// Check new filesystem CRC
	ret = polyfs_check_crc(&tempfs, buf, CRC_BUFFER_SIZE);
	if (ret) {
		goto out;
	}

	// Look up the system image file
	ret = polyfs_lookup(&tempfs, "/system.bin", &sysimg);
	if (ret) {
		goto out;
	}

	// Loop through the entire file
	uint32_t offset = 0;
	while (offset < POLYFS_24(sysimg.size)) {
		// Read a block from the file
		ret = polyfs_fread(&tempfs, &sysimg, buf, offset,
				CRC_BUFFER_SIZE);
		if (ret < 0) {
			goto out;
		}
		else if (ret == 0) {
			memset(&buf[ret], 0xff, CRC_BUFFER_SIZE - ret);
			break;
		}

		// Write the page
		boot_program_page(offset, buf);

		// Advance the offset
		offset += ret;
	}
	ret = 0;

	// Re-enable the RWW section
	boot_rww_enable();

	// Swap the partitions around
	status.primary = !status.primary;

out:
	// Close the filesystem
	flashmgt_sec_close(&tempfs);

	// Write status to settings
	int ret2 = settings_set(SETTINGS_KEY_FLASHMGT_STATUS,
		&status, sizeof(status));
	if (ret2 != SETTINGS_STATUS_OK) {
		ret = ret2;
	}

	return ret;
}
#endif

