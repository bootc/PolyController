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

#if CONFIG_LIB_LZO
#include <minilzo/minilzo.h>
#endif

#include "polyfs.h"

#if !defined(CONFIG_LIB_POLYFS_DEBUG)
#define PRINTF(fmt, ...)
#define PRINTF1(fmt);
#elif defined(__AVR__)
#include <avr/pgmspace.h>
#define PRINTF(fmt, ...) printf_P(PSTR(fmt), __VA_ARGS__)
#define PRINTF1(fmt) printf_P(PSTR(fmt))
#else
#include <sys/stat.h>
#define PRINTF(fmt, ...) printf(fmt, __VA_ARGS__)
#define PRINTF1(fmt) printf(fmt)
#endif

// MIN for 32-bit uints
static inline uint32_t min(uint32_t a, uint32_t b);

// Read a raw buffer from underlying storage
static int read_storage(polyfs_fs_t *fs, void *ptr,
	uint32_t offset, uint16_t bytes);
// Read a uint32_t from underlying storage and adjust byte order
static inline int read_storage_uint32(polyfs_fs_t *fs,
	uint32_t *ptr, uint32_t offset);

// Read the filesystem superblock
static int read_super(polyfs_fs_t *fs);

// Calculate a CRC-32 checksum
static uint32_t crc32(uint32_t crc, uint8_t *buffer, uint32_t length);

int polyfs_init(void) {
	int err = 0;

#if CONFIG_LIB_LZO
	err = lzo_init();
	if (err != LZO_E_OK) {
		PRINTF1("LZO init failed\n");
		return -1;
	}
#endif

	return err;
}

int polyfs_fs_open(polyfs_fs_t *fs) {
	int err;

	// Make sure the user has supplied a read function
	if (fs->fn_read == NULL) {
		return -1;
	}

	// Read in the superblock
	err = read_super(fs);
	if (err) {
		return err;
	}

	return 0;
}

int polyfs_check_crc(polyfs_fs_t *fs, void *temp, uint16_t tempsize) {
	uint32_t crc = crc32(0, NULL, 0);
	uint32_t size = 0;
	uint32_t read_crc = 0;
	uint32_t offset = 0;
	int ret;

	while (1) {
		// Read a block of FS data
		ret = read_storage(fs, temp, offset, tempsize);
		if (ret < 0) {
			return ret;
		}
		else if (ret == 0) {
			break;
		}

		// If we've just read the superblock, extract some info
		if (offset == 0) {
			struct polyfs_super *super = (struct polyfs_super *)temp;
			size = POLYFS_32(super->size);
			read_crc = POLYFS_32(super->fsid.crc);

			// Wipe the CRC from the block we read so the calculation is valid
			super->fsid.crc = POLYFS_32(crc32(0, NULL, 0));
		}

		offset += ret;

		// Reached the end of the filesystem
		if (offset > size) {
			crc = crc32(crc, temp, ret - (offset - size));
			break;
		}

		crc = crc32(crc, temp, ret);
	}

	if (crc != read_crc) {
		return -1;
	}

	return 0;
}

int32_t polyfs_fread(polyfs_fs_t *fs, const struct polyfs_inode *inode,
	void *ptr, uint32_t offset, uint16_t bytes)
{
	int err;

	// the number of bytes we would like to read
	uint16_t read_bytes = bytes;
	// the number of blocks that make up this inode
	uint32_t blocks = (POLYFS_24(inode->size) + POLYFS_BLOCK_SIZE - 1) /
		POLYFS_BLOCK_SIZE;
	// the offset of the data section of this inode (start of block pointers)
	uint32_t inode_offset = POLYFS_GET_OFFSET(inode) << 2;
	// the block number that the offset falls into
	uint16_t block = offset / POLYFS_BLOCK_SIZE;
	// offset of the first block of data (block 0)
	uint32_t start_offset = inode_offset + (blocks * 4);
	// offset of the block pointer
	uint32_t blkptr_offset = inode_offset + (block * 4);
	// length of the compressed data block
	uint32_t compr_len;

	// Make sure we're reading a regular file
	if (!S_ISREG(POLYFS_16(inode->mode))) {
		PRINTF1("inode is not a regular file\n");
		return -1;
	}

	// Check we aren't trying to read past the end of the file
	if (offset > inode->size) {
		PRINTF("offset is too large (%lu >= %lu)\n",
			offset, inode->size);
		return -1;
	}

	// If the input buffer extends beyond the end of the file
	if (offset + read_bytes > inode->size) {
		// reduce the requested read size
		read_bytes = inode->size - offset;
	}

	// If we were asked to read nothing, we can return quickly
	if (read_bytes == 0) {
		return 0;
	}

	// We need to read from a block that's not the first
	if (block) {
		err = read_storage_uint32(fs, &start_offset, blkptr_offset - 4);
		if (err) return err;
	}

	// Find out the length of the data block
	err = read_storage_uint32(fs, &compr_len, blkptr_offset);
	if (err) return err;
	compr_len -= start_offset;

	// Is this a hole in the data?
	if (compr_len == 0) {
		// Find out the size of the hole
		uint32_t bytes_out = min(read_bytes, POLYFS_BLOCK_SIZE);

		// Set the memory and return the size
		memset(ptr, 0, bytes_out);
		return bytes_out;
	}

#if CONFIG_LIB_LZO
	// Deal with an LZO compressed file
	if (fs->sb.flags & POLYFS_FLAG_LZO_COMPRESSION) {
		// Offset must be a multiple of the block size
		if (offset % POLYFS_BLOCK_SIZE) {
			PRINTF1("read offset must be a multiple of block size\n");
			return -1;
		}

		// Must have a large block for in-place decompress
		if (bytes < POLYFS_BLOCK_MAX_SIZE_WITH_OVERHEAD) {
			PRINTF1("buffer size must be > block size + lzo overhead\n");
			return -1;
		}

		// The compressed data needs to be put at the end of the buffer
		uint32_t lzo_offset = bytes - compr_len;
		err = read_storage(fs, ptr + lzo_offset, start_offset, compr_len);
		if (err != compr_len) {
			PRINTF1("could not read entire compressed buffer\n");
			return -1;
		}

		// Let's do the decompression
		err = lzo1x_decompress_safe(ptr + lzo_offset, compr_len,
			ptr, (lzo_uintp)&bytes, NULL);
		if (err != LZO_E_OK || bytes != min(read_bytes, POLYFS_BLOCK_SIZE)) {
			PRINTF("overlap decompression failed: %d, %d == %d\n",
				   err, bytes, min(read_bytes, POLYFS_BLOCK_SIZE));
			return -1;
		}

		return bytes;
	}
#endif

	// Offset within the block to read from
	uint32_t block_offset = offset % POLYFS_BLOCK_SIZE;
	// Don't try to read past the end of the block
	read_bytes = min(POLYFS_BLOCK_SIZE - block_offset, read_bytes);

	// Read from the storage
	return read_storage(fs, ptr, start_offset + block_offset, read_bytes);
}

int polyfs_opendir(polyfs_fs_t *fs, const struct polyfs_inode *parent,
	polyfs_readdir_t *rd)
{
	if (!S_ISDIR(POLYFS_16(parent->mode))) {
		PRINTF1("can only readdir directories");
		return -1;
	}

	// Copy over basic values
	rd->fs = fs;
	rd->parent = parent;

	// Work out the offset of the first dirent inode
	rd->next = POLYFS_GET_OFFSET(parent) << 2;

	return 0;
}

int polyfs_readdir(polyfs_readdir_t *rd) {
	uint32_t start = POLYFS_GET_OFFSET(rd->parent) << 2;
	uint32_t psize = POLYFS_24(rd->parent->size);

	if ((rd->next < start) || (rd->next > (start + psize))) {
		PRINTF1("readdir with invalid next\n");
		return -1;
	}

	// Read in the inode
	int len = read_storage(rd->fs, &rd->inode, rd->next, sizeof(rd->inode));
	if (len != sizeof(rd->inode)) {
		PRINTF1("short read\n");
		return -1;
	}

	// Read in the name
	uint8_t namelen = POLYFS_GET_NAMELEN(&rd->inode) << 2;
	len = read_storage(rd->fs, rd->name, rd->next + sizeof(rd->inode), namelen);
	if (len != namelen) {
		PRINTF1("short read\n");
		return -1;
	}

	// Advance the pointer
	rd->next += sizeof(rd->inode) + namelen;

	// Check for the end of the directory
	if (rd->next >= (start + psize)) {
		rd->next = 0;
	}

	return 0;
}

int polyfs_lookup(polyfs_fs_t *fs, const char *path,
	struct polyfs_inode *inode)
{
	polyfs_readdir_t rd;
	int pathlen = strlen(path);

	// Start at the root inode
	*inode = fs->root;

	// Main traversal loop
	while (pathlen) {
		int err;
		char *term;
		int len;
		int found = 0;

		// Skip leading slash characters
		while (*path == '/') {
			path++;
			pathlen--;
		}

		// Work out the length of this path element
		term = strchrnul(path, '/');
		len = term - path;

		// We have nothing left to look at
		if (len == 0) {
			break;
		}

		// Start the readdir
		err = polyfs_opendir(fs, inode, &rd);
		if (err) return err;

		// Iterate through the entries
		while (rd.next) {
			int cmp;
			int namelen;

			// Read the directory entry
			err = polyfs_readdir(&rd);
			if (err) return err;

			// Find the length of the name string
			namelen = strnlen((char *)rd.name,
				POLYFS_GET_NAMELEN(&rd.inode) << 2);

			// Compare the names
			cmp = strncmp((char *)rd.name, path, min(len, namelen));

			if (cmp > 0) {
				// Doesn't match and it sorts greater than path
				break; // stop searching
			}
			else if (cmp == 0 && len == namelen) {
				// Name matches!
				found = 1;
				break;
			}
			else {
				// Doesn't match and it sorts less than path
				continue; // keep going
			}
		}

		// We haven't found the subdirectory
		if (!found) {
			return -1;
		}

		// Advance to the next path element
		*inode = rd.inode;
		path += len;
		pathlen -= len;
	}

	// Looks like we found it!
	return 0;
}

static inline uint32_t min(uint32_t a, uint32_t b) {
	return (a < b) ? a : b;
}

static int read_storage(polyfs_fs_t *fs, void *ptr,
	uint32_t offset, uint16_t bytes)
{
	if (fs->fn_read == NULL) {
		PRINTF1("fn_read not set!\n");
		return -1;
	}

	return fs->fn_read(fs, ptr, offset, bytes);
}

static inline int read_storage_uint32(polyfs_fs_t *fs,
	uint32_t *ptr, uint32_t offset)
{
	int num = read_storage(fs, ptr, offset, sizeof(*ptr));
	if (num != sizeof(*ptr)) {
		return -1;
	}

	*ptr = POLYFS_32(*ptr);

	return 0;
}

static int read_super(polyfs_fs_t *fs) {
	struct polyfs_super super;
	int status;
	uint16_t root_offset;

	// Read in the superblock
	status = read_storage(fs, &super, 0, sizeof(super));
	if (status != sizeof(super)) {
		PRINTF("could not read superblock: %d\n", status);
		return (status < 0) ? status : -1;
	}

	// Check magic number
	if (super.magic != POLYFS_32(POLYFS_MAGIC)) {
		PRINTF("superblock magic not found: %08x\n", super.magic);
		return -1;
	}

	// Check flags
	if (POLYFS_32(super.flags) & ~POLYFS_SUPPORTED_FLAGS) {
		PRINTF1("unsupported features\n");
		return -1;
	}

	// Check that the root inode is sane
	if (!S_ISDIR(POLYFS_16(super.root.mode))) {
		PRINTF1("root is not a directory\n");
		return -1;
	}

	// Copy over a few things we're sure about
	fs->sb.flags = POLYFS_32(super.flags);
	fs->sb.size = POLYFS_32(super.size);

	// Check for required flags
	if (!(fs->sb.flags & POLYFS_FLAG_FSID_VERSION_1)) {
		PRINTF1("required flags missing\n");
		return -1;
	}

	// Copy over a few more things
	fs->sb.blocks = POLYFS_32(super.fsid.blocks);
	fs->sb.files = POLYFS_32(super.fsid.files);

	// Work out the root node's offset
	root_offset = POLYFS_GET_OFFSET(&super.root) << 2;

	// Check for sanity
	if (root_offset == 0) {
		PRINTF1("empty filesystem\n");
		return -1;
	}
	else if (!(fs->sb.flags & POLYFS_FLAG_SHIFTED_ROOT_OFFSET) &&
		 (root_offset != sizeof(struct polyfs_super)))
	{
		PRINTF("bad root offset %d\n", root_offset);
		return -1;
	}

	// Check the compression algorithms
#if !CONFIG_LIB_LZO
	if (fs->sb.flags & POLYFS_FLAG_LZO_COMPRESSION) {
		PRINTF1("LZO compression not available\n");
		return -1;
	}
#endif
	if (fs->sb.flags & POLYFS_FLAG_ZLIB_COMPRESSION) {
		PRINTF1("zlib compression not available\n");
		return -1;
	}

	// Copy the root inode info over to the fs structure
	fs->root = super.root;

	return 0;
}

// CCITT CRC-32 (Autodin II) polynomial:
// X32+X26+X23+X22+X16+X12+X11+X10+X8+X7+X5+X4+X2+X+1

static uint32_t crc32(uint32_t crc, uint8_t *buffer, uint32_t length) {
	if (buffer == NULL) {
		return 0;
	}

	crc ^= 0xffffffffUL;

	while (length--) {
		crc = crc ^ *buffer++;
		for (int i = 0; i < 8; i++) {
			if (crc & 1) {
				crc = (crc >> 1) ^ 0xedb88320UL;
			}
			else {
				crc = crc >> 1;
			}
		}
	}

	return crc ^ 0xffffffffUL;
}

