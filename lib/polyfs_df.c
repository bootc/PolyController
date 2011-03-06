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
#include <stdio.h>
#include <string.h>
#include "drivers/dataflash.h"

#include "polyfs_df.h"

#ifdef CONFIG_LIB_POLYFS_DF_MAXFS
#define MAXFS CONFIG_LIB_POLYFS_DF_MAXFS
#else
#define MAXFS 2
#endif

struct pfsdf_info {
	uint32_t offset;
	uint32_t bytes;
};

static struct pfsdf_info info[MAXFS];

static int pfsdf_read(polyfs_fs_t *fs, void *ptr,
	uint32_t offset, uint32_t bytes)
{
	struct pfsdf_info *iptr = fs->userptr;
	int err;
	uint8_t sreg;

	// Check the inputs are in range
	if (offset >= iptr->bytes) {
		return -1;
	}
	else if (bytes == 0) {
		return 0;
	}
	else if (offset + bytes >= iptr->bytes) {
		bytes = iptr->bytes - offset;
	}

	// Make sure the flash is ready
	err = dataflash_read_status(&sreg);
	if ((err) || (sreg & DATAFLASH_SREG_BUSY)) {
		return -1;
	}

	// Read the dataflash
	return dataflash_read_data(ptr, iptr->offset + offset, bytes);
}

int pfsdf_open(polyfs_fs_t *fs, uint32_t offset, uint32_t size) {
	struct pfsdf_info *iptr = NULL;

	// Sanity check
	if (size < sizeof(struct polyfs_super)) {
		return -1;
	}

	// Find a free pfsdf_info member
	for (int i = 0; i < MAXFS; i++) {
		if (info[i].bytes == 0) {
			iptr = &info[i];
			break;
		}
	}

	// Set up the info structure
	iptr->offset = offset;
	iptr->bytes = size;

	// Set up the polyfs_fs_t structure
	fs->fn_read = pfsdf_read;
	fs->userptr = iptr;

	// Open the filesystem
	int err = polyfs_fs_open(fs);
	if (err) {
		// Clear the info structure
		iptr->offset = iptr->bytes = 0;

		// Clear the polyfs_fs_t
		fs->fn_read = NULL;
		fs->userptr = NULL;

		return err;
	}

	return 0;
}

int pfsdf_close(polyfs_fs_t *fs) {
	// Sanity check
	if (fs->fn_read != pfsdf_read) {
		return -1;
	}

	// Clear up the info structure
	struct pfsdf_info *iptr = fs->userptr;
	iptr->offset = iptr->bytes = 0;
	fs->userptr = fs->fn_read = NULL;

	return 0;
}

