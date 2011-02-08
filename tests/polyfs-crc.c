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
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "polyfs.h"

polyfs_fs_t fs;

static int read_bs(polyfs_fs_t *fs, void *ptr,
	uint32_t offset, uint32_t bytes)
{
	FILE *fsbs = (FILE *)fs->userptr;

	if (fseek(fsbs, offset, SEEK_SET)) {
		return -1;
	}

	return fread(ptr, 1, bytes, fsbs);
}

static int run_tests(const char *file) {
	int err;

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

	// check the CRC
	void *temp = malloc(1024);
	assert(temp != NULL);
	err = polyfs_check_crc(&fs, temp, 1024);
	assert(err == 0);
	free(temp);

	// close the backing store
	fclose(fsbs);

	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printf("Usage: %s <file.pfs>\n", argv[0]);
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

	return run_tests(argv[1]);
}

