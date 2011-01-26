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

#ifndef __POLYFS_H__
#define __POLYFS_H__

#include <polyfs/polyfs_fs.h>
#include <polyfs/polyfs_fs_sb.h>

typedef struct polyfs_fs polyfs_fs_t;

struct polyfs_fs {
	// Private polyfs data structures (peek but don't poke)
	struct polyfs_sb_info sb;
	struct polyfs_inode root;

	// User-supplied function used to read a block of data from
	// underlying storage. This must be filled-in by the user before calling
	// polyfs_fs_open().
	int (*fn_read)(polyfs_fs_t *fs, void *ptr, uint32_t offset, uint32_t bytes);

	// This pointer can point to some user data which may be useful to
	// fn_read() above.
	void *userptr;
};

typedef struct {
	polyfs_fs_t *fs;
	const struct polyfs_inode *parent;

	uint32_t next; // offset of next inode
	struct polyfs_inode inode; // inode data
	uint8_t name[POLYFS_MAXPATHLEN];
} polyfs_readdir_t;

int polyfs_init(void);
int polyfs_fs_open(polyfs_fs_t *fs);

int32_t polyfs_fread(polyfs_fs_t *fs, const struct polyfs_inode *inode,
	void *ptr, uint32_t offset, uint32_t bytes);

int polyfs_opendir(polyfs_fs_t *fs, const struct polyfs_inode *parent,
	polyfs_readdir_t *rd);
int polyfs_readdir(polyfs_readdir_t *rd);

int polyfs_lookup(polyfs_fs_t *fs, const char *path,
	struct polyfs_inode *inode);

#endif
