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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <contiki-net.h>
#include "sendfile.h"

#include <stdio.h>

#include "httpd.h"
#include "httpd-cgi.h"

#define REASON_NONE 0
#define REASON_EOF 1
#define REASON_ERROR 2
#define REASON_SCRIPT 3

static unsigned short generator(void *state) {
	struct sendfile_state *s = state;
	struct sendfile_file_state *fs = list_head(s->stack);

	// Seek to the offset to send from (this could be different to the fd
	// internal offset due to retransmits)
	cfs_offset_t ret = cfs_seek(fs->fd, fs->fpos, CFS_SEEK_SET);
	if (ret != fs->fpos) {
		s->reason = REASON_ERROR;

		// Because we can't send nothing from this function, we send a single
		// space character. This should be the most neutral when dealing with
		// HTML
		((char *)uip_appdata)[0] = ' ';
		return 1;
	}

	// Copy file data into uip_appdata
	fs->ret = cfs_read(fs->fd, uip_appdata, UIP_TCP_MSS);
	if (fs->ret < 0) {
		s->reason = REASON_ERROR;

		// Because we can't send nothing from this function, we send a single
		// space character. This should be the most neutral when dealing with
		// HTML
		((char *)uip_appdata)[0] = ' ';
		return 1;
	}

	// Extra processing for script mode
	if (s->mode == SENDFILE_MODE_SCRIPT) {
		// If the last char of the buffer is %, shorten the buffer by 1
		if (((char *)uip_appdata)[fs->ret - 1] == '%') {
			fs->ret--;
		}

		// Look for a % sign
		char *tmp = uip_appdata;
		int idx = 0;
		while ((tmp = memchr(tmp, '%', fs->ret - idx)) != NULL) {
			// We found a '%'
			idx = (int)tmp - (int)uip_appdata;

			// Is the next char a bang?
			if (*(tmp + 1) == '!') {
				// %! found, so shorten the send bytes
				fs->ret = idx;
				s->reason = REASON_SCRIPT;
				break;
			}
		}
	}

	if (fs->ret == 0) {
		// Because we can't send nothing from this function, we send a single
		// space character. This should be the most neutral when dealing with
		// HTML
		((char *)uip_appdata)[0] = ' ';
		return 1;
	}

	return fs->ret;
}

static PT_THREAD(send_part(struct sendfile_state *s, struct psock *sock)) {
	struct sendfile_file_state *fs = list_head(s->stack);
	PSOCK_BEGIN(sock);

	// Clear the finish reason
	s->reason = 0;

	while (1) {
		// Check if we've reached the end of file
		cfs_offset_t len = cfs_seek(fs->fd, 0, CFS_SEEK_END);
		if (fs->fpos >= len) {
			s->reason = REASON_EOF;
			break;
		}

		// Send some of the file
		PSOCK_GENERATOR_SEND(sock, generator, s);

		// Move the offset forwards
		if (fs->ret > 0) {
			fs->fpos += fs->ret;
		}

		// Check if the generator flagged a problem
		if (s->reason) {
			break;
		}
	};

	PSOCK_END(sock);
}

/*
 * Open a file and push it onto the stack
 */
static int openfile(struct sendfile_state *s, const char *file) {
	struct sendfile_file_state *f;

	// First try to allocate enough memory to fit the state structure
	f = calloc(1, sizeof(*f));
	if (f == NULL) {
		return -1;
	}

	// Try to open the file
	f->fd = cfs_open(file, CFS_READ);
	if (f->fd < 0) {
		free(f);
		return f->fd;
	}

	// Push it to the stack
	list_push(s->stack, f);

	return 0;
}

/*
 * Pop a file off the stack and close it
 */
static int closefile(struct sendfile_state *s) {
	struct sendfile_file_state *f;

	// Grab the first file off the stack
	f = list_pop(s->stack);
	if (!f) {
		// Nothing left on the stack
		return -1;
	}

	// Close the file
	if (f->fd) {
		cfs_close(f->fd);
	}

	// Free up memory
	free(f);

	return 0;
}

int sendfile_init(struct sendfile_state *s, const char *file, uint8_t mode) {
	// Check for valid mode flags
	if ((mode & SENDFILE_MODE_MASK) != mode) {
		return -1;
	}

	// Init our file stack list
	LIST_STRUCT_INIT(s, stack);

	// Try to open the first file
	int ret = openfile(s, file);
	if (ret < 0) {
		return ret;
	}

	// Set the flags
	s->mode = mode;
	s->open = 1;

	// Set up the protothread
	PT_INIT(&s->pt);

	return 0;
}

PT_THREAD(sendfile(struct sendfile_state *s, struct httpd_state *hs)) {
	PT_BEGIN(&s->pt);

	do {
		// Send part of the file
		PT_WAIT_THREAD(&s->pt, send_part(s, &hs->sock));

		if (s->reason == REASON_EOF) {
			// Close one file on the stack
			closefile(s);

			// Break out the loop if there are no more files
			if (!list_head(s->stack)) {
				break;
			}
		}
		if (s->reason == REASON_ERROR) {
			// Go through and clear all the open files
			while (list_head(s->stack)) {
				closefile(s);
			}
			break;
		}
		else if (s->reason == REASON_SCRIPT) {
			struct sendfile_file_state *fs = list_head(s->stack);
			char *buf = uip_appdata;
			size_t len = 0;
			uint8_t include = 0;

			// Skip the '%!'
			fs->fpos += 2;

			// Seek to that offset
			cfs_seek(fs->fd, fs->fpos, CFS_SEEK_SET);

			// Read into the buffer until we hit a newline or EOF
			while (len < UIP_TCP_MSS) {
				// Read as much as we can
				int err = cfs_read(fs->fd, &buf[len], UIP_TCP_MSS - len);
				if (err < 0) {
					s->reason = REASON_ERROR;
					break;
				}
				else if (err == 0) {
					break;
				}

				len += err;

				// Check if there's a NL in the buffer
				char *nl = memchr(buf, '\n', len);
				if (nl) {
					len = nl - buf;

					// Skip over the CGI call line (and trailing NL)
					fs->fpos += len + 1;
					break;
				}
			}

			// Check for read errors in the above loop
			if (s->reason == REASON_ERROR) {
				break;
			}

			// Now the buffer should contain the CGI call line minus '%!'
			buf[len] = '\0';

			// Check for an include vs. CGI call
			if (*buf == ':') {
				include = 1;
				buf++;
			}

			// Skip leading spaces
			while (isblank(*buf) && *buf) {
				buf++;
			}

			// Handle includes
			if (include) {
				// Push a file open on to the stack
				int err = openfile(s, buf);
				if (err) {
					s->reason = REASON_ERROR;
					break;
				}
			}
			else {
				char *args = buf;

				// Work out where the script arguments start
				strsep_P(&args, PSTR(" \t"));

				// Get a pointer to the CGI function
				s->spare = httpd_cgi(buf);

				// Run the script
				PT_WAIT_THREAD(&s->pt, ((httpd_cgifunction)s->spare)(hs, args));
			}
		}
	} while (1);

	PT_END(&s->pt);
}

int sendfile_finish(struct sendfile_state *s) {
	// Don't do anything on a closed handle
	if (!s->open) {
		return 0;
	}

	// Clear the open flag
	s->open = 0;

	// Go through and clear all the open files
	while (list_head(s->stack)) {
		closefile(s);
	}

	// Make sure our thread is exited
	PT_EXIT(&s->pt);

	// Now check the error flag
	if (s->reason == REASON_ERROR) {
		return -1;
	}

	return 0;
}

