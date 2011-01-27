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

#ifndef __POLYFS_DF_H__
#define __POLYFS_DF_H__

#include <polyfs.h>

/*
 * PolyFS over dataflash
 */


/**
 * Open a PolyFS filesystem stored on dataflash.
 *
 * This function sets up the read function in the polyfs_fs_t structure with a
 * function that reads from dataflash, and calls polyfs_fs_open() to read the
 * superblock.
 *
 * polyfs_init() and dataflash_init() must have been called beforehand. The read
 * function uses the fs->userptr pointer to keep its own data - do not overwrite
 * this.
 *
 * fs: pointer to a polyfs_fs_t data structure.
 * offset: memory offset in flash to the start of the filesystem.
 * size: maximum size in bytes of the filesystem in flash.
 */
int pfsdf_open(polyfs_fs_t *fs, uint32_t offset, uint32_t size);

/**
 * Close a PolyFS on dataflash filesystem and clean up resources.
 *
 * This function cleans up some internal pfsdf resources used when opening a
 * filesystem.
 */
int pfsdf_close(polyfs_fs_t *fs);

#endif
