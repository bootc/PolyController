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

#ifndef __POLYFS_FS_H
#define __POLYFS_FS_H

/*
 * Polyfs is based on cramfs with disk format changes.
 * This file is based on cramfs_fs.h with only minor modifications.
 */

#if __AVR__
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321
#define __BYTE_ORDER __LITTLE_ENDIAN

#define S_IFMT  00170000
#define S_IFSOCK 0140000
#define S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)

#else
#include <byteswap.h>
#include <endian.h>
#endif

#define POLYFS_MAGIC		POLYFS_32(0x53464350)	/* 'PCFS' */
#define POLYFS_SIGNATURE	"PolyControllerFS"

/*
 * Width of various bitfields in struct polyfs_inode.
 * Primarily used to generate warnings in mkpolyfs.
 */
#define POLYFS_MODE_WIDTH 16
#define POLYFS_UID_WIDTH 16
#define POLYFS_SIZE_WIDTH 24
#define POLYFS_GID_WIDTH 8
#define POLYFS_NAMELEN_WIDTH 6
#define POLYFS_OFFSET_WIDTH 26

/*
 * Since inode.namelen is a unsigned 6-bit number, the maximum polyfs
 * path length is 63 << 2 = 252.
 */
#define POLYFS_MAXPATHLEN (((1 << POLYFS_NAMELEN_WIDTH) - 1) << 2)

/*
 * The Linux CRAMFS block size is 4096 but we can't spare that sort of memory
 * in our AVR. Even 1024 might be a bit much, but any lower and we really start
 * to sacrifice the compression ratio.
 */
#define POLYFS_BLOCK_SIZE 1024
#define POLYFS_BLOCK_MAX_SIZE_WITH_OVERHEAD \
	(POLYFS_BLOCK_SIZE + \
	(POLYFS_BLOCK_SIZE/16) + \
	64 + 3)

/*
 * Reasonably terse representation of the inode data.
 */
struct polyfs_inode {
	uint32_t mode:POLYFS_MODE_WIDTH;
	uint32_t uid:POLYFS_UID_WIDTH;
	/* SIZE for device files is i_rdev */
	uint32_t size:POLYFS_SIZE_WIDTH;
	uint32_t gid:POLYFS_GID_WIDTH;
	/* NAMELEN is the length of the file name, divided by 4 and
           rounded up.  (polyfs doesn't support hard links.) */
	uint32_t namelen:POLYFS_NAMELEN_WIDTH;
	/* OFFSET: For symlinks and non-empty regular files, this
	   contains the offset (divided by 4) of the file data in
	   compressed form (starting with an array of block pointers;
	   see README).  For non-empty directories it is the offset
	   (divided by 4) of the inode of the first file in that
	   directory.  For anything else, offset is zero. */
	uint32_t offset:POLYFS_OFFSET_WIDTH;
};

struct polyfs_info {
	uint32_t crc;
	uint32_t edition;
	uint32_t blocks;
	uint32_t files;
};

/*
 * Superblock information at the beginning of the FS.
 */
struct polyfs_super {
	uint32_t magic;				/* POLYFS_MAGIC */
	uint32_t size;				/* length in bytes */
	uint32_t flags;				/* feature flags */
	uint32_t future;			/* reserved for future use */
	uint8_t signature[16];		/* POLYFS_SIGNATURE */
	struct polyfs_info fsid;	/* unique filesystem info */
	uint8_t name[16];			/* user-defined name */
	struct polyfs_inode root;	/* root inode data */
};

/*
 * Feature flags
 */
#define POLYFS_FLAG_FSID_VERSION_1		0x00000001	/* fsid version #1 */
#define POLYFS_FLAG_SORTED_DIRS			0x00000002	/* sorted dirs */
#define POLYFS_FLAG_HOLES				0x00000004	/* support for holes */
#define POLYFS_FLAG_SHIFTED_ROOT_OFFSET	0x00000008	/* shifted root fs */
#define POLYFS_FLAG_ZLIB_COMPRESSION	0x00000010	/* zlib compression */
#define POLYFS_FLAG_LZO_COMPRESSION		0x00000020	/* LZO compression */

/*
 * Valid values in super.flags.  Currently we refuse to mount
 * if (flags & ~POLYFS_SUPPORTED_FLAGS).  Maybe that should be
 * changed to test super.future instead.
 */
#define POLYFS_SUPPORTED_FLAGS	( 0x000000ff )

/*
 * Since polyfs is little-endian, provide macros to swab the bitfields.
 */

#ifndef __BYTE_ORDER
#if defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
#define __BYTE_ORDER __LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN) && !defined(__LITTLE_ENDIAN)
#define __BYTE_ORDER __BIG_ENDIAN
#else
#error "unable to define __BYTE_ORDER"
#endif
#endif /* not __BYTE_ORDER */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define POLYFS_16(x)	(x)
#define POLYFS_24(x)	(x)
#define POLYFS_32(x)	(x)
#define POLYFS_GET_NAMELEN(x)	((x)->namelen)
#define POLYFS_GET_OFFSET(x)	((x)->offset)
#define POLYFS_SET_OFFSET(x,y)	((x)->offset = (y))
#define POLYFS_SET_NAMELEN(x,y)	((x)->namelen = (y))
#elif __BYTE_ORDER == __BIG_ENDIAN
#warning "__BYTE_ORDER == __BIG_ENDIAN"
#define POLYFS_16(x)	bswap_16(x)
#define POLYFS_24(x)	((bswap_32(x)) >> 8)
#define POLYFS_32(x)	bswap_32(x)
#define POLYFS_GET_NAMELEN(x)	(((uint8_t*)(x))[8] & 0x3f)
#define POLYFS_GET_OFFSET(x)	((POLYFS_24(((uint32_t*)(x))[2] & 0xffffff) << 2) |\
				 ((((uint32_t*)(x))[2] & 0xc0000000) >> 30))
#define POLYFS_SET_NAMELEN(x,y)	(((uint8_t*)(x))[8] = (((0x3f & (y))) | \
						  (0xc0 & ((uint8_t*)(x))[8])))
#define POLYFS_SET_OFFSET(x,y)	(((uint32_t*)(x))[2] = (((y) & 3) << 30) | \
				 POLYFS_24((((y) & 0x03ffffff) >> 2)) | \
				 (((uint32_t)(((uint8_t*)(x))[8] & 0x3f)) << 24))
#else
#error "__BYTE_ORDER must be __LITTLE_ENDIAN or __BIG_ENDIAN"
#endif

#endif /* not __POLYFS_FS_H */
