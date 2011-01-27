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

#include "polyfs_cfs.h"

#ifdef CONFIG_LIB_POLYFS_CFS_MAXFDS
#define MAXFDS CONFIG_LIB_POLYFS_CFS_MAXFDS
#else
#define MAXFDS 5
#endif

#define FD_VALID(fd) \
	(((fd) >= 0) && ((fd) < MAXFDS) && (fds[(fd)].inode.offset != 0))
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

struct polyfs_cfs_fd {
	struct polyfs_inode inode;
	uint32_t offset;
};

struct polyfs_cfs_dir {
	struct polyfs_inode parent;
	struct polyfs_inode child;
	uint32_t next;
};

static inline void build_time_checks_dont_call(void) {
	BUILD_BUG_ON(sizeof(struct polyfs_cfs_dir) > sizeof(struct cfs_dir));
}

polyfs_fs_t *polyfs_cfs_fs;
static struct polyfs_cfs_fd fds[MAXFDS];

static int find_free_fd(void) {
	for (int i = 0; i < MAXFDS; i++) {
		// PolyFS files all have an offset > 0
		if (fds[i].inode.offset == 0) {
			return i;
		}
	}

	return -1;
}

int cfs_open(const char *name, int flags) {
	struct polyfs_cfs_fd *fdp;
	int err;

	// Check the fs pointer is set
	if (!polyfs_cfs_fs) {
		return -1;
	}

	// PolyFS is a read-only FS
	if (flags != CFS_READ) {
		return -1;
	}

	// Find ourselves a free fd number
	int fd = find_free_fd();
	if (fd < 0) {
		return -1;
	}

	fdp = &fds[fd];

	// Find the file in the filesystem
	err = polyfs_lookup(polyfs_cfs_fs, name, &fdp->inode);
	if (err) {
		fdp->inode.offset = 0; // make sure this fd is still free
		return -1;
	}

	// Make sure it's a file and not a directory or otherwise
	if (!S_ISREG(POLYFS_16(fds->inode.mode))) {
		fdp->inode.offset = 0; // make sure this fd is still free
		return -1;
	}

	// Set up the fd
	fdp->offset = 0;

	return fd;
}

void cfs_close(int fd) {
	if (FD_VALID(fd)) {
		fds[fd].inode.offset = 0;
		fds[fd].offset = 0;
	}
}

int cfs_read(int fd, void *buf, unsigned int len) {
	struct polyfs_cfs_fd *fdp;

	// Check the fs pointer is set
	if (!polyfs_cfs_fs) {
		return -1;
	}

	if (!FD_VALID(fd)) {
		return -1;
	}

	fdp = &fds[fd];

	// Shorten the read if it would go past the end of file
	if (fdp->offset + len > fdp->inode.size) {
		len = fdp->inode.size - fdp->offset;
	}

	// Forward the read to PolyFS
	len = polyfs_fread(polyfs_cfs_fs, &fdp->inode, buf, fdp->offset, len);
	if (len > 0) {
		fdp->offset += len;
	}

	return len;
}

int cfs_write(int fd, const void *buf, unsigned int len) {
	return -1; // we can't do writes
}

cfs_offset_t cfs_seek(int fd, cfs_offset_t offset, int whence) {
	struct polyfs_cfs_fd *fdp;
	uint32_t new_offset;

	// Check the fs pointer is set
	if (!polyfs_cfs_fs) {
		return -1;
	}

	if (!FD_VALID(fd)) {
		return -1;
	}

	fdp = &fds[fd];

	// Determine the new offset
	if (whence == CFS_SEEK_SET) {
		new_offset = offset;
	}
	else if (whence == CFS_SEEK_END) {
		new_offset = fdp->inode.size + offset;
	}
	else if (whence == CFS_SEEK_CUR) {
		new_offset = fdp->offset + offset;
	}
	else {
		return -1;
	}

	// Make sure it doesn't go past the end of file
	if ((new_offset < 0) || (new_offset > fdp->inode.size)) {
		return -1;
	}

	fdp->offset = new_offset;
	return new_offset;
}

int cfs_remove(const char *name) {
	return -1; // we can't change the filesystem
}

/*
 * We have to implement our own opendir and readdir, as the normal polyfs ones
 * expect different sized structures to what we can supply. Thankfully the code
 * is really rather simple.
 */
int cfs_opendir(struct cfs_dir *dirp, const char *name) {
	struct polyfs_cfs_dir *dir = (struct polyfs_cfs_dir *)dirp;
	int err;

	// Check the fs pointer is set
	if (!polyfs_cfs_fs) {
		return -1;
	}

	// Find the inode in the filesystem
	err = polyfs_lookup(polyfs_cfs_fs, name, &dir->parent);
	if (err) {
		return -1;
	}

	// Make sure it's a directory
	if (!S_ISDIR(POLYFS_16(dir->parent.mode))) {
		return -1;
	}

	dir->next = POLYFS_GET_OFFSET(&dir->parent) << 2;

	return 0;
}

int cfs_readdir(struct cfs_dir *dirp, struct cfs_dirent *dirent) {
	struct polyfs_cfs_dir *dir = (struct polyfs_cfs_dir *)dirp;
	uint32_t start = POLYFS_GET_OFFSET(&dir->parent) << 2;
	uint32_t psize = POLYFS_24(dir->parent.size);
	int len;

	// Check the fs pointer is set
	if (!polyfs_cfs_fs) {
		return -1;
	}

	// Sanity checks
	if ((dir->next < start) || (dir->next > (start + psize))) {
		return -1;
	}

	// Read in the inode
	len = polyfs_cfs_fs->fn_read(polyfs_cfs_fs, &dir->child, dir->next,
		sizeof(dir->child));
	if (len != sizeof(dir->child)) {
		return -1;
	}

	// Work out the length of the filename
	uint8_t namelen = POLYFS_GET_NAMELEN(&dir->child) << 2;
	if (namelen >= sizeof(dirent->name)) {
		namelen = sizeof(dirent->name) - 1;
	}

	// Read in the name
	len = polyfs_cfs_fs->fn_read(polyfs_cfs_fs, dirent->name,
		dir->next + sizeof(dir->child), namelen);
	if (len != namelen) {
		return -1;
	}
	dirent->name[namelen] = '\0';

	// Advance the pointer
	dir->next += sizeof(dir->child) + (POLYFS_GET_NAMELEN(&dir->child) << 2);

	// Check for the end of the directory
	if (dir->next >= (start + psize)) {
		dir->next = 0; // will fail the sanity check above
	}

	dirent->size = dir->child.size;

	return 0;
}

void cfs_closedir(struct cfs_dir *dirp) {
	// no need to do anything
}

