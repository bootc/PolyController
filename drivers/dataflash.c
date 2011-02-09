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
#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <init.h>

#include "dataflash.h"
#include "spi.h"

#define CMD_RD_ARRAY 0x0b
#define CMD_RD_ARRAY_LF 0x03
#define CMD_ERASE_BLK_4K 0x20
#define CMD_ERASE_BLK_32K 0x52
#define CMD_ERASE_BLK_64K 0xd8
#define CMD_ERASE_CHIP 0x60
#define CMD_WR_PAGE 0x02
#define CMD_WR_EN 0x06
#define CMD_WR_DIS 0x04
#define CMD_SECTOR_PROT 0x36
#define CMD_SECTOR_UNPROT 0x39
#define CMD_RD_PROT 0x3c
#define CMD_RD_SREG 0x05
#define CMD_WR_SREG 0x01
#define CMD_RD_MFR_DEV_ID 0x9f

#define MFR_CONT_CODE 0x7f
#define MFR_CT_ATMEL 0x00
#define MFR_ID_ATMEL 0x1f
#define DEV_ID_AT26DF081A_P1 0x45
#define DEV_ID_AT26DF081A_P2 0x01

// size of dataflash in bytes
#define FLASH_SIZE 1048576

#define WR_PAGE_SIZE ((uint32_t)1 << 8)
#define WR_PAGE_MASK (~(WR_PAGE_SIZE - 1))
#define SECTOR_32K_SIZE ((uint32_t)1 << 15)
#define SECTOR_32K_MASK (~(SECTOR_32K_SIZE - 1))
#define SECTOR_64K_SIZE ((uint32_t)1 << 16)
#define SECTOR_64K_MASK (~(SECTOR_64K_SIZE - 1))

static const dataflash_sector_t sectors[] PROGMEM = {
	{ 0x00000, 0x0ffff }, //  0: 64K
	{ 0x10000, 0x1ffff }, //  1: 64K
	{ 0x20000, 0x2ffff }, //  2: 64K
	{ 0x30000, 0x3ffff }, //  3: 64K
	{ 0x40000, 0x4ffff }, //  4: 64K
	{ 0x50000, 0x5ffff }, //  5: 64K
	{ 0x60000, 0x6ffff }, //  6: 64K
	{ 0x70000, 0x7ffff }, //  7: 64K
	{ 0x80000, 0x8ffff }, //  8: 64K
	{ 0x90000, 0x9ffff }, //  9: 64K
	{ 0xa0000, 0xaffff }, // 10: 64K
	{ 0xb0000, 0xbffff }, // 11: 64K
	{ 0xc0000, 0xcffff }, // 12: 64K
	{ 0xd0000, 0xdffff }, // 13: 64K
	{ 0xe0000, 0xeffff }, // 14: 64K
	{ 0xf0000, 0xf3fff }, // 15: 16K
	{ 0xf4000, 0xf5fff }, // 16:  8K
	{ 0xf6000, 0xf7fff }, // 17:  8K
	{ 0xf8000, 0xfffff }, // 18: 32K
};

static struct {
	int inited : 1;
} status;

// Set up SPI and assert CS
static inline void dev_assert(void) {
	spi_init();
	CONFIG_DRIVERS_DATAFLASH_PORT |= _BV(CONFIG_DRIVERS_DATAFLASH_CS);
}

// Release CS and release SPI
static inline void dev_release(void) {
	CONFIG_DRIVERS_DATAFLASH_PORT |= _BV(CONFIG_DRIVERS_DATAFLASH_CS);
	spi_release();
}

static inline void send_address(uint32_t addr) {
	// Send 3 bytes (assume little endian)
	for (int x = 2; x >= 0; x--) {
		spi_rw(((uint8_t *)&addr)[x]);
	}
}

static void dataflash_init(void) {
	// Make sure CS is pulled high (release device)
	CONFIG_DRIVERS_DATAFLASH_DDR |= _BV(CONFIG_DRIVERS_DATAFLASH_CS);
	CONFIG_DRIVERS_DATAFLASH_PORT |= _BV(CONFIG_DRIVERS_DATAFLASH_CS);

	// read device ID
	dataflash_id_t id;
	dataflash_read_id(&id, NULL, 0);

	// check device ID matches what we expect
	if ((id.num_cont != MFR_CT_ATMEL) ||
		(id.mfr_id != MFR_ID_ATMEL) ||
		(id.devid1 != DEV_ID_AT26DF081A_P1) ||
		(id.devid2 != DEV_ID_AT26DF081A_P2))
	{
		return;
	}

	// save state
	status.inited = 1;

	return;
}

int dataflash_read_id(dataflash_id_t *id, uint8_t *extinfo, uint8_t bufsz) {
	uint8_t val;
	id->num_cont = 0;

	// Start talking
	dev_assert();

	// Read manufacturer and device ID codes
	spi_rw(CMD_RD_MFR_DEV_ID);

	// Read manufacturer ID code
	do {
		val = spi_rw(0x00);

		// continuation byte
		if (val == MFR_CONT_CODE) {
			id->num_cont++;
		}
		// faulty SPI
		else if (val == 0xff) {
			dev_release();
			return -1;
		}
		else {
			id->mfr_id = val;
		}
	} while (val == MFR_CONT_CODE);

	// Read device ID code
	id->devid1 = spi_rw(0x00);
	id->devid2 = spi_rw(0x00);

	// Read extended device ID string length
	id->extinfo_len = spi_rw(0x00);

	// Read the extended info string into the supplied buffer
	uint8_t read = bufsz < id->extinfo_len ? bufsz : id->extinfo_len;
	while (read--) {
		*extinfo++ = spi_rw(0x00);
	}

	// All done
	dev_release();

	return 0;
}

int dataflash_sector_from_addr(uint32_t addr, dataflash_sector_t *sector) {
	// Loop through the table of sectors
	for (int i = 0; i < (sizeof(sectors) / sizeof(dataflash_sector_t)); i++) {
		// Copy the array entry into the buffer
		memcpy_P(sector, &sectors[i], sizeof(*sector));

		// Is this the entry we're after?
		if ((sector->start <= addr) && 
			(sector->end >= addr))
		{
			return 0;
		}
	}

	return -1;
}

int dataflash_sector_by_idx(uint8_t idx, dataflash_sector_t *sector) {
	// Check the index isn't too large
	if (idx >= (sizeof(sectors) / sizeof(dataflash_sector_t))) {
		return -1;
	}

	// Copy the entry over
	memcpy_P(sector, &sectors[idx], sizeof(*sector));
	return 0;
}

int dataflash_read_status(uint8_t *sreg) {
	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_RD_SREG);

	// Read status
	*sreg = spi_rw(0x00);

	// All done
	dev_release();

	return 0;
}

int dataflash_write_status(uint8_t sreg) {
	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_WR_SREG);

	// Send new value
	spi_rw(sreg);

	// All done
	dev_release();

	return 0;
}

int dataflash_wait_ready(void) {
	uint8_t sreg;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_RD_SREG);

	// Read status
	do {
		sreg = spi_rw(0x00);
	} while (sreg & DATAFLASH_SREG_BUSY);

	// All done
	dev_release();

	return 0;
}

int dataflash_read_data(void *buf, uint32_t offset, uint32_t bytes) {
	uint8_t *cbuf = (uint8_t *)buf;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Sanity-check the inputs
	if (offset >= FLASH_SIZE) {
		return -1;
	}
	else if (bytes == 0) {
		return 0;
	}
	else if (offset + bytes >= FLASH_SIZE) {
		bytes = FLASH_SIZE - offset;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_RD_ARRAY_LF); // "low frequency" is 33 MHz or less

	// Send address
	send_address(offset);

	// Read data
	for (uint32_t i = 0; i < bytes; i++) {
		*(cbuf++) = spi_rw(0x00);
	}

	// All done
	dev_release();

	return bytes;
}

int dataflash_write_enable(void) {
	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_WR_EN);

	// All done
	dev_release();

	return 0;
}

int dataflash_write_disable(void) {
	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_WR_DIS);

	// All done
	dev_release();

	return 0;
}

int dataflash_protect_sector(uint32_t addr) {
	int err;
	uint8_t sreg;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Check the address is within bounds
	if (addr >= FLASH_SIZE) {
		return -1;
	}

	// Get the current device status
	err = dataflash_read_status(&sreg);
	if (err) {
		return err;
	}

	// Check that WEL is set
	if (!(sreg & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Check that SPRL isn't set
	if (sreg & DATAFLASH_SREG_SPRL) {
		// Disable writes (turn off WEL)
		dataflash_write_disable();
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_SECTOR_PROT);

	// Send address
	send_address(addr);

	// All done
	dev_release();

	return 0;
}

int dataflash_unprotect_sector(uint32_t addr) {
	int err;
	uint8_t sreg;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Check the address is within bounds
	if (addr >= FLASH_SIZE) {
		return -1;
	}

	// Get the current device status
	err = dataflash_read_status(&sreg);
	if (err) {
		return err;
	}

	// Check that WEL is set
	if (!(sreg & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Check that SPRL isn't set
	if (sreg & DATAFLASH_SREG_SPRL) {
		// Disable writes (turn off WEL)
		dataflash_write_disable();
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_SECTOR_UNPROT);

	// Send address
	send_address(addr);

	// All done
	dev_release();

	return 0;
}

int dataflash_read_protection(uint32_t addr, uint8_t *value) {
	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Check the address is within bounds
	if (addr >= FLASH_SIZE) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_RD_PROT);

	// Send address
	send_address(addr);

	// Read response
	*value = spi_rw(0x00);

	// All done
	dev_release();

	return 0;
}

int dataflash_erase_4k(uint32_t addr) {
	int err;
	uint8_t temp;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Check the address is within bounds
	if (addr >= FLASH_SIZE) {
		return -1;
	}

	// Read sector protection information
	err = dataflash_read_protection(addr, &temp);
	if (err) {
		return err;
	}

	// Check that the sector isn't protected
	if (temp != 0x00) {
		dataflash_write_disable();
		return -1;
	}

	// Get the current device status
	err = dataflash_read_status(&temp);
	if (err) {
		return err;
	}

	// Check that WEL is set
	if (!(temp & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_ERASE_BLK_4K);

	// Send address
	send_address(addr);

	// All done
	dev_release();

	return 0;
}

int dataflash_erase_32k(uint32_t addr) {
	int err;
	uint8_t temp;
	uint32_t start = addr & SECTOR_32K_MASK;
	uint32_t end = start + SECTOR_32K_SIZE;
	dataflash_sector_t sector;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Check the address is within bounds
	if (addr >= FLASH_SIZE) {
		return -1;
	}

	// Find the sector information for the address
	dataflash_sector_from_addr(start, &sector);

	// Go through all the sectors in this erase block
	do {
		// Read sector protection information
		err = dataflash_read_protection(sector.start, &temp);
		if (err) {
			return err;
		}

		// Check that the sector isn't protected
		if (temp != 0x00) {
			dataflash_write_disable();
			return -1;
		}

		// Get the next sector info
		if (dataflash_sector_from_addr(sector.end + 1, &sector)) {
			break;
		}
	} while (sector.end < end);

	// Get the current device status
	err = dataflash_read_status(&temp);
	if (err) {
		return err;
	}

	// Check that WEL is set
	if (!(temp & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_ERASE_BLK_32K);

	// Send address
	send_address(start);

	// All done
	dev_release();

	return 0;
}

int dataflash_erase_64k(uint32_t addr) {
	int err;
	uint8_t temp;
	uint32_t start = addr & SECTOR_64K_MASK;
	uint32_t end = start + SECTOR_64K_SIZE;
	dataflash_sector_t sector;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Check the address is within bounds
	if (addr >= FLASH_SIZE) {
		return -1;
	}

	// Find the sector information for the address
	dataflash_sector_from_addr(start, &sector);

	// Go through all the sectors in this erase block
	do {
		// Read sector protection information
		err = dataflash_read_protection(sector.start, &temp);
		if (err) {
			return err;
		}

		// Check that the sector isn't protected
		if (temp != 0x00) {
			dataflash_write_disable();
			return -1;
		}

		// Get the next sector info
		if (dataflash_sector_from_addr(sector.end + 1, &sector)) {
			break;
		}
	} while (sector.end < end);

	// Get the current device status
	err = dataflash_read_status(&temp);
	if (err) {
		return err;
	}

	// Check that WEL is set
	if (!(temp & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_ERASE_BLK_64K);

	// Send address
	send_address(start);

	// All done
	dev_release();

	return 0;
}

int dataflash_erase_chip(void) {
	int err;
	uint8_t sreg;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Get the current device status
	err = dataflash_read_status(&sreg);
	if (err) {
		return err;
	}

	// Check that WEL is set
	if (!(sreg & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Check that none of the sectors are protected
	if (sreg & DATAFLASH_SREG_SWP0) {
		dataflash_write_disable();
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_ERASE_CHIP);

	// All done
	dev_release();

	return 0;
}

int dataflash_write_data(void *buf, uint32_t addr, uint8_t bytes) {
	uint8_t *cbuf = (uint8_t *)buf;
	uint32_t page_start = addr & WR_PAGE_MASK;
	uint32_t page_end = page_start + WR_PAGE_SIZE;
	int err;
	uint8_t temp;

	// Make sure init has been called
	if (!status.inited) {
		return -1;
	}

	// Sanity-check the inputs
	if (addr >= FLASH_SIZE) {
		return -1;
	}
	else if (bytes == 0) {
		return 0;
	}
	else if (addr + bytes >= FLASH_SIZE) {
		bytes = FLASH_SIZE - bytes;
	}

	// Clamp write size to the end of the write page
	if (addr + bytes > page_end) {
		bytes = page_end - addr;
	}

	// Read protection status
	err = dataflash_read_protection(addr, &temp);
	if (err) {
		return err;
	}

	// Check protection status
	if (temp != 0x00) {
		dataflash_write_disable();
		return -1;
	}

	// Read status register
	err = dataflash_read_status(&temp);
	if (err) {
		return -1;
	}

	// Check for WEL
	if (!(temp & DATAFLASH_SREG_WEL)) {
		return -1;
	}

	// Start talking
	dev_assert();

	// Send command
	spi_rw(CMD_WR_PAGE);

	// Send address
	send_address(addr);

	// Write data
	for (uint16_t i = 0; i < bytes; i++) {
		spi_rw(*(cbuf++));
	}

	// All done
	dev_release();

	return bytes;
}

INIT_DRIVER(dataflash, dataflash_init);

