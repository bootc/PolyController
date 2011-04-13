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

#ifndef _POLYFS_FS_SB
#define _POLYFS_FS_SB

/*
 * Polyfs is based on cramfs with disk format changes.
 * This file is based on cramfs_fs_sb.h with only minor modifications.
 */

/*
 * polyfs super-block data in memory
 */
struct polyfs_sb_info {
	struct polyfs_info fsid;
	uint32_t size;
	uint32_t flags;
};

#endif
