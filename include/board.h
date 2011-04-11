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

#ifndef BOARD_H
#define BOARD_H

#include <verify.h>

#define BOARD_INFO_ADDR 0x010

struct board_info {
	uint16_t crc;			// CRC-CCITT of info block
	char model[32];			// Model name, UTF-8, null-padded
	char hw_rev[8];			// Hardware revision, UTF-8, null-padded
	char serial[8];			// Serial number, UTF-8, null-padded
	uint16_t mfr_year;		// Year of manufacture
	uint8_t mfr_month;		// Month of manufacture
	uint8_t mfr_day;		// Day of manufacture
	uint8_t padding[10];	// Reserved for future use, set to 0xff
};

// Compile-time check of struct size
verify(sizeof(struct board_info) == 64);

// Call this to set up IO pins
void board_init(void);

// Board info functions
void board_info_read(struct board_info *info);
int board_info_validate(const struct board_info *info);

#endif // BOARD_H
