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

#ifndef __FLASHMGT_H__
#define __FLASHMGT_H__

extern polyfs_fs_t *flashmgt_pfs;

int flashmgt_sec_open(polyfs_fs_t *ptr);
int flashmgt_sec_close(polyfs_fs_t *ptr);

#if !CONFIG_IMAGE_BOOTLOADER
int flashmgt_sec_write_start(void);
int flashmgt_sec_write_block(const void *buf, uint32_t offset, uint32_t len);
int flashmgt_sec_write_abort(void);
int flashmgt_sec_write_finish(void);
#endif

#if CONFIG_IMAGE_BOOTLOADER
bool flashmgt_update_pending(void);
int flashmgt_bootload(void);
#endif

#endif

