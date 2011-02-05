/*
 * Copyright (c) 2004, Adam Dunkels.
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
 * Author: Adam Dunkels <adam@sics.se>
 *
 * $Id: httpd.c,v 1.20 2010-12-14 22:45:22 dak664 Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

#include "contiki-net.h"

#include "webserver.h"
#include "httpd-cgi.h"
#include "http-strings.h"
#include "urlconv.h"

#include "httpd.h"

#ifndef CONFIG_APPS_WEBSERVER_CONNS
#define CONNS UIP_CONNS
#else /* CONFIG_APPS_WEBSERVER_CONNS */
#define CONNS CONFIG_APPS_WEBSERVER_CONNS
#endif /* CONFIG_APPS_WEBSERVER_CONNS */

#define STATE_WAITING 0
#define STATE_OUTPUT  1

#define SEND_PSTR(sock, str) \
	PSOCK_GENERATOR_SEND(sock, send_pstr_gen, (void *)str)

//#define MIN(a, b) ((a) < (b)? (a): (b))

#if 0
MEMB(conns, struct httpd_state, CONNS);
#else
static uint8_t conns_free = CONNS;
#endif

#define ISO_tab     0x09
#define ISO_nl      0x0a
#define ISO_cr      0x0d
#define ISO_space   0x20
#define ISO_bang    0x21
#define ISO_percent 0x25
#define ISO_period  0x2e
#define ISO_slash   0x2f
#define ISO_colon   0x3a

static unsigned short send_file_gen(void *state) {
	struct httpd_state *s = (struct httpd_state *)state;

	// Seek to the offset to send from (this could be different to the fd
	// internal offset due to retransmits)
	int ret = cfs_seek(s->fd, s->fpos, CFS_SEEK_SET);
	if (ret != s->fpos) {
		return 0;
	}

	// Copy file data into uip_appdata
	s->len = cfs_read(s->fd, uip_appdata, UIP_TCP_MSS);
	if (s->len < 0) {
		return 0;
	}

	return s->len;
}

static PT_THREAD(send_file(struct httpd_state *s)) {
	PSOCK_BEGIN(&s->sout);

	do {
		// Send some of the file
		PSOCK_GENERATOR_SEND(&s->sout, send_file_gen, s);

		// Move the offset forwards
		if (s->len > 0) {
			s->fpos += s->len;
		}
	} while(s->len > 0);

	PSOCK_END(&s->sout);
}

static unsigned short send_pstr_gen(void *string) {
	PGM_P str = string;

	// Find out how long the string is
	int len = strlen_P(str);

	// Make sure it's not a stupid length
	if (len > UIP_TCP_MSS) {
		len = UIP_TCP_MSS;
	}

	// Copy the string into the send buffer
	memcpy_P(uip_appdata, str, len);

	return len;
}

static PT_THREAD(send_pstring(struct httpd_state *s, PGM_P str)) {
	PSOCK_BEGIN(&s->sout);
	SEND_PSTR(&s->sout, str);
	PSOCK_END(&s->sout);
}

static unsigned short send_script_chunk_gen(void *state) {
	struct httpd_state *s = (struct httpd_state *)state;

	s->flags.eof = 0;

	// Seek to the offset to send from (this could be different to the fd
	// internal offset due to retransmits)
	int ret = cfs_seek(s->fd, s->fpos, CFS_SEEK_SET);
	if (ret != s->fpos) {
		return 0;
	}

	// Copy file data into uip_appdata
	s->len = cfs_read(s->fd, uip_appdata, UIP_TCP_MSS);
	if (s->len < 0) {
		return 0;
	}

	// Have we reached the end of the file?
	if (s->len == 0) {
		s->flags.eof = 1;
		return 0;
	}

	// If the last char of the buffer is %, shorten the buffer by 1
	if (((char *)uip_appdata)[s->len - 1] == ISO_percent) {
		s->len--;
	}

	// Look for a % sign
	char *tmp = uip_appdata;
	int idx = 0;
	while ((tmp = memchr(tmp, ISO_percent, s->len - idx)) != NULL) {
		// We found a '%'
		idx = (int)tmp - (int)uip_appdata;

		// Is the next char a bang?
		if (*(tmp + 1) == ISO_bang) {
			// %! found, so shorten the send bytes
			s->len = idx;
			break;
		}
	}

	return s->len;
}

static PT_THREAD(send_script_chunk(struct httpd_state *s)) {
	PSOCK_BEGIN(&s->sout);

	// Send some of the file, stopping at %!
	do {
		PSOCK_GENERATOR_SEND(&s->sout, send_script_chunk_gen, s);

		// Move the offset forwards
		if (s->len > 0) {
			s->fpos += s->len;
		}
	} while (s->len > 0);

	PSOCK_END(&s->sout);
}

static PT_THREAD(handle_script(struct httpd_state *s)) {
	PT_BEGIN(&s->scriptpt);

	while (1) {
		int ret;
		char *name;
		int include = 0;

		// Send file data up to %!
		PT_WAIT_THREAD(&s->scriptpt, send_script_chunk(s));

		if (s->flags.eof || s->flags.err) {
			break;
		}

		// if we've got this far, we've encountered a script call

		// Seek to just after the %!
		ret = cfs_seek(s->fd, s->fpos + 2, CFS_SEEK_SET);
		if (ret < 0) {
			break;
		}

		// Read the script name and arguments
		ret = cfs_read(s->fd, s->script, sizeof(s->script) - 1);
		if (ret < 0) {
			break;
		}

		// Null-terminate on first LF, or at end if no LF
		name = memchr(s->script, ISO_nl, ret);
		if (name) {
			*name = '\0';
		}
		else {
			s->script[ret] = '\0';
		}

		name = s->script;

		// Will this be an include rather than a "cgi"?
		if (*name == ISO_colon) {
			include = 1;
			name++;
		}

		// Skip leading spaces
		while ((*name == ISO_space) || (*name == ISO_tab)) {
			name++;
		}

		// Handle include/cgi
		if (include) {
			// Save the current file info
			s->savefd = s->fd;
			s->savefpos = s->fpos;
			s->fpos = 0;

			// Open the file to be included (NOTE: not urlconv'd)
			s->fd = cfs_open(name, CFS_READ);
			if (s->fd < 0) {
				SEND_PSTR(&s->sout, PSTR("[ include failed ]"));
			}
			else {
				// Send the included file
				PT_WAIT_THREAD(&s->scriptpt, send_file(s));

				// Clean up
				cfs_close(s->fd);
			}

			// Swap back to the main file
			s->fd = s->savefd;
			s->fpos = s->savefpos;
		}
		else {
			char *args = name;

			// Work out where the script arguments start
			strsep_P(&args, PSTR(" \t"));

			// Run the script
			PT_WAIT_THREAD(&s->scriptpt, httpd_cgi(name)(s, args));
		}
	}

	PT_END(&s->scriptpt);
}

static PT_THREAD(send_headers(struct httpd_state *s, const PGM_P statushdr)) {
	const char *ptr = NULL;
	const PGM_P ptr2 = NULL;

	PSOCK_BEGIN(&s->sout);
	SEND_PSTR(&s->sout, statushdr);

	ptr = strrchr(s->filename, ISO_period);
	if (ptr == NULL) {
		ptr2 = http_content_type_binary;
	}
	else if (strncmp_P(ptr, http_html, 5) == 0) {
		ptr2 = http_content_type_html;
	}
	else if (strncmp_P(ptr, http_shtml, 6) == 0) {
		ptr2 = http_content_type_html;
	}
	else if (strncmp_P(ptr, http_css, 4) == 0) {
		ptr2 = http_content_type_css;
	}
	else if (strncmp_P(ptr, http_png, 4) == 0) {
		ptr2 = http_content_type_png;
	}
	else if (strncmp_P(ptr, http_gif, 4) == 0) {
		ptr2 = http_content_type_gif;
	}
	else if (strncmp_P(ptr, http_jpg, 4) == 0) {
		ptr2 = http_content_type_jpg;
	}
	else {
		ptr2 = http_content_type_plain;
	}

	SEND_PSTR(&s->sout, ptr2);
	PSOCK_END(&s->sout);
}

static PT_THREAD(handle_output(struct httpd_state *s)) {
	PT_BEGIN(&s->outputpt);

	s->fd = cfs_open(s->filename, CFS_READ);

	if (s->fd < 0) {
		strcpy_P(s->filename, PSTR("/notfound.html"));
		s->fd = cfs_open(s->filename, CFS_READ);
		PT_WAIT_THREAD(&s->outputpt, send_headers(s, http_header_404));

		if (s->fd < 0) {
			PT_WAIT_THREAD(&s->outputpt, send_pstring(s, PSTR("not found")));
			uip_close();
			webserver_log_file(&uip_conn->ripaddr, "404 (no notfound.htm)");
			PT_EXIT(&s->outputpt);
		}

		webserver_log_file(&uip_conn->ripaddr, "404 - notfound.htm");
	}
	else {
		PT_WAIT_THREAD(&s->outputpt, send_headers(s, http_header_200));
	}

	char *ptr = strrchr(s->filename, ISO_period);
	if (ptr != NULL && strncmp(ptr, http_shtml, 6) == 0) {
		PT_INIT(&s->scriptpt);
		PT_WAIT_THREAD(&s->outputpt, handle_script(s));
	}
	else {
		PT_WAIT_THREAD(&s->outputpt, send_file(s));
	}

	cfs_close(s->fd);
	s->fd = -1;

	PSOCK_CLOSE(&s->sout);
	PT_END(&s->outputpt);
}

static PT_THREAD(handle_input(struct httpd_state *s)) {
	PSOCK_BEGIN(&s->sin);

	PSOCK_READTO(&s->sin, ISO_space);

	if (strncmp_P((char *)s->inputbuf, http_get, 4) != 0) {
		PSOCK_CLOSE_EXIT(&s->sin);
	}

	PSOCK_READTO(&s->sin, ISO_space);

	if (s->inputbuf[0] != ISO_slash) {
		PSOCK_CLOSE_EXIT(&s->sin);
	}

	if (s->inputbuf[1] == ISO_space) {
		strncpy_P(s->filename, http_index_html, sizeof(s->filename));
	}
	else {
		s->inputbuf[PSOCK_DATALEN(&s->sin) - 1] = 0;
		//strncpy(s->filename, (char *)s->inputbuf, sizeof(s->filename));

		strcpy_P(s->filename, PSTR("/www"));
		int idx = strlen(s->filename);
		urlconv_tofilename(&s->filename[idx], (char *)s->inputbuf, sizeof(s->filename) - idx);
	}

	webserver_log_file(&uip_conn->ripaddr, s->filename);
	s->state = STATE_OUTPUT;

	while (1) {
		PSOCK_READTO(&s->sin, ISO_nl);

		if (strncmp_P((char *)s->inputbuf, http_referer, 8) == 0) {
			s->inputbuf[PSOCK_DATALEN(&s->sin) - 2] = 0;
			webserver_log((char *)s->inputbuf);
		}
	}

	PSOCK_END(&s->sin);
}

static void handle_connection(struct httpd_state *s) {
	handle_input(s);

	if (s->state == STATE_OUTPUT) {
		handle_output(s);
	}
}

void httpd_appcall(void *state) {
	struct httpd_state *s = (struct httpd_state *)state;

	if (uip_closed() || uip_aborted() || uip_timedout()) {
		if (s != NULL) {
			if (s->fd >= 0) {
				cfs_close(s->fd);
				s->fd = -1;
			}
#if 0
			memb_free(&conns, s);
#else
			free(s);
			conns_free++;
#endif
		}
	}
	else if (uip_connected()) {
#if 0
		s = (struct httpd_state *)memb_alloc(&conns);
#else
		if (conns_free) {
			s = malloc(sizeof(*s));
			conns_free--;
		}
		else {
			s = NULL;
		}
#endif

		if (s == NULL) {
			uip_abort();
			webserver_log_file(&uip_conn->ripaddr, "reset (no memory block)");
			return;
		}

		tcp_markconn(uip_conn, s);
		PSOCK_INIT(&s->sin, (uint8_t *)s->inputbuf, sizeof(s->inputbuf) - 1);
		PSOCK_INIT(&s->sout, (uint8_t *)s->inputbuf, sizeof(s->inputbuf) - 1);
		PT_INIT(&s->outputpt);
		s->fd = -1;
		s->fpos = 0;
		s->state = STATE_WAITING;
		timer_set(&s->timer, CLOCK_SECOND * 10);
		handle_connection(s);
	}
	else if (s != NULL) {
		if (uip_poll()) {
			if (timer_expired(&s->timer)) {
				uip_abort();
				if (s->fd >= 0) {
					cfs_close(s->fd);
					s->fd = -1;
				}
#if 0
				memb_free(&conns, s);
#else
				free(s);
				conns_free++;
#endif
				webserver_log_file(&uip_conn->ripaddr, "reset (timeout)");
			}
		}
		else {
			timer_restart(&s->timer);
		}

		handle_connection(s);
	}
	else {
		uip_abort();
	}
}

void httpd_init(void) {
	tcp_listen(UIP_HTONS(80));
#if 0
	memb_init(&conns);
#endif
}

#if UIP_CONF_IPV6
uint8_t httpd_sprint_ip6(uip_ip6addr_t addr, char * result) {
	unsigned char i = 0;
	unsigned char zerocnt = 0;
	unsigned char numprinted = 0;
	char * starting = result;

	*result++='[';
	while (numprinted < 8) {
		if ((addr.u16[i] == 0) && (zerocnt == 0)) {
			while(addr.u16[zerocnt + i] == 0) zerocnt++;

			if (zerocnt == 1) {
				*result++ = '0';
				numprinted++;
				break;
			}

			i += zerocnt;
			numprinted += zerocnt;
		}
		else {
			result += sprintf(result, "%x",
				(unsigned int)(uip_ntohs(addr.u16[i])));
			i++;
			numprinted++;
		}
		if (numprinted != 8) *result++ = ':';
	}

	*result++=']';
	*result=0;
	return (result - starting);
}
#endif /* UIP_CONF_IPV6 */

