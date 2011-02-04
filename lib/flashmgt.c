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

#include "flashmgt.h"

struct flashmgt_partition {
	uint32_t start;
	uint32_t len;
};

struct flashmgt_status {
	uint8_t pri : 1;
	uint8_t pending : 1;
};

static struct flashmgt_partition part[] = {
	{ .start = CONFIG_FLASHMGT_P1_START, .len = CONFIG_FLASHMGT_P1_LEN },
	{ .start = CONFIG_FLASHMGT_P2_START, .len = CONFIG_FLASHMGT_P2_LEN },
};

struct flashmgt_status flashmgt_status_ee EEMEM;
struct flashmgt_status flashmgt_status;
polyfs_fs_t flashmgt_pfs;

int flashmgt_init(void) {
	int ret;

	// Nullify in case we run into trouble
	polyfs_cfs_fs = NULL;

	// FIXME: read EEPROM to work out primary partition
	eeprom_read_block(&flashmgt_status, &flashmgt_status_ee,
		sizeof(flashmgt_status));
	int pri = 0;

	// Try to open the filesystem
	ret = pfsdf_open(&flashmgt_pfs, part[pri].start, part[pri].len);
	if (ret < 0) {
		return ret;
	}

	// Set up the CFS FS pointer
	polyfs_cfs_fs = &flashmgt_pfs;

	return 0;
}



