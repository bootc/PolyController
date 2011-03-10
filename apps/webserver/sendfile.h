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

#ifndef SENDFILE_H_
#define SENDFILE_H_

#include <cfs/cfs.h>

#define SENDFILE_MODE_MASK 0x03
#define SENDFILE_MODE_NORMAL 0x00
#define SENDFILE_MODE_SCRIPT 0x01

struct httpd_state;

// State for the entire sendfile machine
struct sendfile_state {
	uint8_t open : 1;
	uint8_t mode : 2;
	uint8_t reason : 4;
	struct pt pt;
	void *spare;
	LIST_STRUCT(stack);
};

// State for each file in a sendfile stack
struct sendfile_file_state {
	struct sendfile_file_state *next;
	int fd;
	cfs_offset_t fpos;
	int ret;
};

int sendfile_init(struct sendfile_state *s, const char *file, uint8_t mode);
PT_THREAD(sendfile(struct sendfile_state *s, struct httpd_state *hs));
int sendfile_finish(struct sendfile_state *s);

#endif
