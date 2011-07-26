/*
 * Copyright (c) 2008, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: shell-file.c,v 1.14 2010/04/12 13:21:58 nvt-se Exp $
 */

/**
 * \file
 *         File-related shell commands
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include <contiki.h>
#include <cfs/cfs.h>
#include <polyfs.h>
#include <polyfs_cfs.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "shell.h"

#define MAX_FILENAME_LEN POLYFS_MAXPATHLEN
#define MAX_BLOCKSIZE 40

PROCESS(shell_ls_process, "ls");
SHELL_COMMAND(ls_command,
	"ls",
	"ls [dir]: list files in directory",
	&shell_ls_process);
INIT_SHELL_COMMAND(ls_command);

PROCESS(shell_cat_process, "cat");
SHELL_COMMAND(cat_command,
	"cat",
	"cat <file>: print the contents of <file>",
	&shell_cat_process);
INIT_SHELL_COMMAND(cat_command);

static void print_inode(const char *name, const struct polyfs_inode *ino) {
	// Get a char that describes the file type
	char type =
		S_ISREG(ino->mode) ? 'f' :
		S_ISDIR(ino->mode) ? 'd' :
		'?'; // we won't use more than this

	// Print the directory entry
	shell_output_P(&ls_command,
		PSTR("%c %04o %9lu %5u:%-3u %s\n"),
		type,
		ino->mode & ~S_IFMT,
		(uint32_t)ino->size,
		ino->uid,
		ino->gid,
		name);
}

struct shell_ls_process_data {
	struct polyfs_inode dir;
	polyfs_readdir_t rd;
	char name[POLYFS_MAXPATHLEN + 1];
};

PROCESS_THREAD(shell_ls_process, ev, data) {
	int err;
	static struct shell_ls_process_data *d = NULL;
	PROCESS_EXITHANDLER(if (d) { free(d); d = NULL; });
	PROCESS_BEGIN();

	if (d == NULL) {
		d = malloc(sizeof(*d));
	}

	// List root directory if no path provided
	if (!data || strlen(data) == 0) {
		data = "/";
	}

	// Lookup directory
	err = polyfs_lookup(polyfs_cfs_fs, data, &d->dir);
	if (err < 0) {
		shell_output_P(&ls_command,
			PSTR("Cannot lookup path: %s\n"), data);
		PROCESS_EXIT();
	}

	// Print the contents of a directory
	if (S_ISDIR(d->dir.mode)) {
		// Open the directory for reading
		err = polyfs_opendir(polyfs_cfs_fs, &d->dir, &d->rd);
		if (err < 0) {
			shell_output_P(&ls_command,
				PSTR("Cannot read directory\n"));
			PROCESS_EXIT();
		}

		// Loop through reading all files
		while (d->rd.next) {
			// Read the directory entry
			err = polyfs_readdir(&d->rd);
			if (err < 0) {
				shell_output_P(&ls_command,
					PSTR("Readdir failed\n"));
				PROCESS_EXIT();
			}

			// Copy the file name out as a null-terminated string
			d->name[0] = '\0';
			strncat(d->name, (char *)d->rd.name, POLYFS_GET_NAMELEN(&d->rd.inode) << 2);

			// Print the inode
			print_inode(d->name, &d->rd.inode);
		}
	}
	// Not a directory: just show the one entry
	else {
		print_inode(data, &d->dir);
	}

	PROCESS_END();
}

PROCESS_THREAD(shell_cat_process, ev, data) {
	static int fd;
	PROCESS_EXITHANDLER(cfs_close(fd));
	PROCESS_BEGIN();

	if (data == NULL || !strlen(data)) {
		shell_output_P(&cat_command,
			PSTR("Usage: cat <file>\n"));
		PROCESS_EXIT();
	}

	fd = cfs_open(data, CFS_READ);
	if (fd < 0) {
		shell_output_P(&cat_command,
			PSTR("cat: could not open file for reading: %s\n"), data);
	}

	while(1) {
		char buf[MAX_BLOCKSIZE + 1];
		int len;
		struct shell_input *input;

		len = cfs_read(fd, buf, MAX_BLOCKSIZE);
		if (len <= 0) {
			cfs_close(fd);
			PROCESS_EXIT();
		}

		buf[len] = '\0';
		shell_output_P(&cat_command,
			PSTR("%s"), buf);

		process_post(&shell_cat_process, PROCESS_EVENT_CONTINUE, NULL);
		PROCESS_WAIT_EVENT_UNTIL(
			ev == PROCESS_EVENT_CONTINUE ||
			ev == shell_event_input);

		if (ev == shell_event_input) {
			input = data;
			if (input->len1 + input->len2 == 0) {
				cfs_close(fd);
				PROCESS_EXIT();
			}
		}
	}

	PROCESS_END();
}

