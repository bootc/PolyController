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

#ifndef __POLYFS_CFS_H__
#define __POLYFS_CFS_H__

#include <polyfs.h>
#include <cfs/cfs.h>

/*
 * Contiki CFS interface for PolyFS
 *
 * NOTE: Refer to cfs/cfs.h for all the main filesystem manipulation functions.
 */


/**
 * Filesystem reference used by CFS wrapper functions.
 *
 * Set this to point at a valid initialised polyfs_fs_t structure before using
 * any of the CFS wrapper functions, or they will fail.
 */
extern polyfs_fs_t *polyfs_cfs_fs;

#endif
