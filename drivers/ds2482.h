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

#ifndef DS2482_H
#define DS2482_H

/**
 * Driver for DS2482 1-wire master.
 */

typedef uint8_t ow_addr_t[8];

typedef struct {
	ow_addr_t	rom_no;
	uint8_t		last_discrepancy;
	uint8_t		last_family_discrepancy;
	uint8_t		last_device_flag;
	uint8_t		crc;
} ow_search_t;

// DS2482 specific functions
int ds2482_detect(uint8_t addr);
int ds2482_reset(void);
int ds2482_channel_select(int channel);

// General 1-wire bus functions
int ow_reset(void);
void ow_write_bit(uint8_t sendbit);
int8_t ow_read_bit(void);
int8_t ow_touch_bit(uint8_t sendbit);
int ow_write_byte(uint8_t sendbyte);
int16_t ow_read_byte(void);
void ow_block(uint8_t *tran_buf, int tran_len);
uint8_t ow_touch_byte(uint8_t sendbyte);

// 1-wire search functions
int ow_first(ow_search_t *s);
int ow_next(ow_search_t *s);
int ow_verify(ow_addr_t s);
void ow_target_setup(ow_search_t *s, uint8_t family);
void ow_family_skip_setup(ow_search_t *s);

// Extended 1-wire functions
int ow_speed(int new_speed);
int ow_level(int new_level);
int ow_read_bit_power(int applyPowerResponse);
int ow_write_byte_power(int sendbyte);

#endif /* DS2482_H */
