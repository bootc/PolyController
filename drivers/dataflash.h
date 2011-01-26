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

#ifndef DATAFLASH_H
#define DATAFLASH_H

typedef struct {
	uint8_t num_cont;
	uint8_t mfr_id;
	uint8_t devid1;
	uint8_t devid2;
	uint8_t extinfo_len;
} dataflash_id_t;

typedef struct {
	uint32_t start;
	uint32_t end;
} dataflash_sector_t;

#define DATAFLASH_SREG_SPRL 0x80
#define DATAFLASH_SREG_SPM 0x40
#define DATAFLASH_SREG_EPE 0x20
#define DATAFLASH_SREG_WPP 0x10
#define DATAFLASH_SREG_SWP1 0x08
#define DATAFLASH_SREG_SWP0 0x04
#define DATAFLASH_SREG_WEL 0x02
#define DATAFLASH_SREG_BUSY 0x01

int dataflash_init(void);
int dataflash_read_id(dataflash_id_t *id, uint8_t *extinfo, uint8_t bufsz);

// For sector protection sectors only
int dataflash_sector_from_addr(uint32_t addr, dataflash_sector_t *sector);
int dataflash_sector_by_idx(uint8_t idx, dataflash_sector_t *sector);

int dataflash_read_status(uint8_t *sreg);
int dataflash_write_status(uint8_t sreg);
int dataflash_wait_ready(void);

int dataflash_read_data(void *buf, uint32_t offset, uint32_t bytes);

int dataflash_write_enable(void);
int dataflash_write_disable(void);
int dataflash_protect_sector(uint32_t addr);
int dataflash_unprotect_sector(uint32_t addr);
int dataflash_read_protection(uint32_t addr, uint8_t *value);

int dataflash_erase_4k(uint32_t addr);
int dataflash_erase_32k(uint32_t addr);
int dataflash_erase_64k(uint32_t addr);
int dataflash_erase_chip(void);

int dataflash_write_data(void *buf, uint32_t addr, uint8_t bytes);

#endif /* DATAFLASH_H */

