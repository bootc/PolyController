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

#include <stubboot.h>
#include <util/atomic.h>

#define VER_MAJOR 0x01

static struct stubboot_table table;

void stubboot_read_table(struct stubboot_table *t) {
	for (uint8_t i = 0; i < sizeof(*t); i++) {
		((uint8_t *)t)[i] = pgm_read_byte_far(STUBBOOT_ADDR + i);
	}
}

int stubboot_write_page(uint_farptr_t page, const void *addr) {
	// Read the table if we haven't yet
	if (!table.write_page) {
		stubboot_read_table(&table);
		if (!table.write_page) {
			return -1;
		}
	}

	// Check the major version number
	if (table.ver_major != 0x01) {
		return -1;
	}

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// Call the function
		table.write_page(page, addr);
	}

	return 0;
}

#if !CONFIG_IMAGE_BOOTLOADER
int stubboot_update_loader(const struct stubboot_selfupdate_info *info) {
	// Read the table if we haven't yet
	if (!table.update_loader) {
		stubboot_read_table(&table);
		if (!table.update_loader) {
			return -1;
		}
	}

	// Check the major version number
	if (table.ver_major != 0x01) {
		return -1;
	}

	int ret;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// Call the function
		ret = table.update_loader(info);
	}

	return ret;
}
#endif

