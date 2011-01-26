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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

#include "polyfs.h"

polyfs_fs_t fs;

static int do_dir(const struct polyfs_inode *inode, const char *path) {
	polyfs_readdir_t rd;
	int err;
	char name[POLYFS_MAXPATHLEN + 1];

	// Set up the readdir
	err = polyfs_opendir(&fs, inode, &rd);
	assert(err == 0);

	// Let's go
	while (rd.next) {
		// Read the directory entry
		err = polyfs_readdir(&rd);
		assert(err == 0);

		/* "d 0755         0     0:0   /" */

		// Get a char that describes the file type
		char type =
			S_ISREG(rd.inode.mode) ? '-' :
			S_ISDIR(rd.inode.mode) ? 'd' :
			'?'; // we won't use more than this

		// Copy the file name out as a null-terminated string
		name[0] = '\0';
		strncat(name, (char *)rd.name, POLYFS_GET_NAMELEN(&rd.inode) << 2);

		// Print the directory entry
		printf("%c %04o %8d %5d:%-3d %s\n",
			type, rd.inode.mode & 07777,
			rd.inode.size, rd.inode.uid, rd.inode.gid,
			name);
	}

	return 0;
}

static int do_file(const struct polyfs_inode *inode, const char *path) {
	int offset = 0;
	char buffer[POLYFS_BLOCK_MAX_SIZE_WITH_OVERHEAD];

	while (offset < inode->size) {
		int len = polyfs_fread(&fs, inode, buffer, offset, sizeof(buffer));
		assert(len > 0);

		offset += len;

		fwrite(buffer, 1, len, stdout);
	}

	return 0;
}

static int read_bs(polyfs_fs_t *fs, void *ptr,
	uint32_t offset, uint32_t bytes)
{
	FILE *fsbs = (FILE *)fs->userptr;

	if (fseek(fsbs, offset, SEEK_SET)) {
		return -1;
	}

	return fread(ptr, 1, bytes, fsbs);
}

static int run_tests(const char *file, const char *path) {
	int err;
	struct polyfs_inode inode;

	// open the backing store
	FILE *fsbs = fopen(file, "r");
	if (!fsbs) {
		printf("failed to open file: %s\n", file);
		return 1;
	}

	// set up the structure
	fs.userptr = fsbs;
	fs.fn_read = read_bs;

	// initialise
	err = polyfs_init();
	assert(err == 0);

	// open the filesystem
	err = polyfs_fs_open(&fs);
	assert(err == 0);

	// find the inode for the file path
	err = polyfs_lookup(&fs, path, &inode);
	assert(err == 0);

	if (S_ISDIR(POLYFS_16(inode.mode))) {
		do_dir(&inode, path);
	}
	else if (S_ISREG(POLYFS_16(inode.mode))) {
		do_file(&inode, path);
	}
	else {
		int mode = POLYFS_16(inode.mode);
		assert(S_ISDIR(mode) || S_ISREG(mode));
	}

	// close the backing store
	fclose(fsbs);

	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: %s <file.pfs> <path>\n", argv[0]);
		return 1;
	}

	struct stat s;
	int err = stat(argv[1], &s);
	if (err) {
		printf("%s: stat failed: %d\n", argv[0], errno);
		return 1;
	}

	if (!S_ISREG(s.st_mode)) {
		printf("%s: %s is not a regular file\n", argv[0], argv[1]);
		return 1;
	}

	return run_tests(argv[1], argv[2]);
}

