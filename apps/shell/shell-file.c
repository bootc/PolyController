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

#include <stdio.h>
#include <string.h>

#include "shell.h"

#define MAX_FILENAME_LEN 40
#define MAX_BLOCKSIZE 40

PROCESS(shell_ls_process, "ls");
SHELL_COMMAND(ls_command,
	"ls",
	"ls: list files",
	&shell_ls_process);
INIT_SHELL_COMMAND(ls_command);

PROCESS(shell_read_process, "read");
SHELL_COMMAND(read_command,
	"read",
	"read <filename> [offset] [block size]: read from a file, with the offset and the block size as options",
	&shell_read_process);
INIT_SHELL_COMMAND(read_command);

PROCESS_THREAD(shell_ls_process, ev, data) {
	static struct cfs_dir dir;
	static cfs_offset_t totsize;
	struct cfs_dirent dirent;
	PROCESS_BEGIN();

	if(cfs_opendir(&dir, "/") != 0) {
		shell_output_P(&ls_command, PSTR("Cannot open directory"));
	} else {
		totsize = 0;
		while(cfs_readdir(&dir, &dirent) == 0) {
			totsize += dirent.size;
			shell_output_P(&ls_command, PSTR("%lu %s"),
				(unsigned long)dirent.size,
				dirent.name);
		}
		cfs_closedir(&dir);
		shell_output_P(&ls_command, PSTR("Total size: %lu"),
			(unsigned long)totsize);
	}
	PROCESS_END();
}

PROCESS_THREAD(shell_read_process, ev, data) {
	static int fd = 0;
	char *next;
	char filename[MAX_FILENAME_LEN];
	int len;
	int offset = 0;
	static int block_size = MAX_BLOCKSIZE;
	PROCESS_EXITHANDLER(cfs_close(fd));
	PROCESS_BEGIN();

	if(data != NULL) {
		next = strchr(data, ' ');
		if(next == NULL) {
			strncpy(filename, data, sizeof(filename));
		} else {
			len = (int)(next - (char *)data);
			if(len <= 0) {
				shell_output_P(&read_command,
					PSTR("read: filename too short: %s"), data);
				PROCESS_EXIT();
			}
			if(len > MAX_FILENAME_LEN) {
				shell_output_P(&read_command,
					PSTR("read: filename too long: %s"), data);
				PROCESS_EXIT();
			}
			memcpy(filename, data, len);
			filename[len] = 0;

			offset = shell_strtolong(next, NULL);
			next++;
			next = strchr(next, ' ');
			if(next != NULL) {
				block_size = shell_strtolong(next, NULL);
				if(block_size > MAX_BLOCKSIZE) {
					shell_output_P(&read_command,
						PSTR("read: block size too large: %s"), data);
					PROCESS_EXIT();
				}
			}
		}

		fd = cfs_open(filename, CFS_READ);
		cfs_seek(fd, offset, CFS_SEEK_SET);

		if(fd < 0) {
			shell_output_P(&read_command,
				PSTR("read: could not open file for reading: %s"), filename);
		} else {

			while(1) {
				char buf[MAX_BLOCKSIZE];
				int len;
				struct shell_input *input;

				len = cfs_read(fd, buf, block_size);
				if(len <= 0) {
					cfs_close(fd);
					PROCESS_EXIT();
				}
				shell_output_P(&read_command,
					PSTR("%s"), buf);

				process_post(&shell_read_process, PROCESS_EVENT_CONTINUE, NULL);
				PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_CONTINUE ||
						ev == shell_event_input);

				if(ev == shell_event_input) {
					input = data;
					/*    printf("cat input %d %d\n", input->len1, input->len2);*/
					if(input->len1 + input->len2 == 0) {
						cfs_close(fd);
						PROCESS_EXIT();
					}
				}
			}
		}
	}

	PROCESS_END();
}

