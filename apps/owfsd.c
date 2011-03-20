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

#include <stdlib.h>
#include <string.h>
#include <contiki-net.h>
#include <init.h>
#include <util/delay.h>
#include "syslog.h"
#include "drivers/ds2482.h"

#include <stdio.h>

#define OWFSD_PORT 15862
#define OW_BUFLEN 64

#ifndef CONFIG_APPS_OWFSD_MAX_CONNS
#define MAX_CONNS (UIP_CONNS / 2)
#else /* CONFIG_APPS_OWFSD_MAX_CONNS */
#define MAX_CONNS CONFIG_APPS_OWFSD_MAX_CONNS
#endif /* CONFIG_APPS_OWFSD_MAX_CONNS */

struct owfsd_state {
	struct timer timer;
	struct psock sock;
	struct pt pt;
	uint8_t buf_in[OW_BUFLEN];
	uint8_t buf_out[OW_BUFLEN];
	uint8_t len;
	uint8_t cmd;
};

static uint8_t conns_free = MAX_CONNS;

PROCESS(owfsd_process, "owfsd");
INIT_PROCESS(owfsd_process);

static PT_THREAD(read_bytes(struct psock *sin, uint8_t len)) {
	PSOCK_BEGIN(sin);

	if (len > OW_BUFLEN) {
		printf_P(PSTR("owfsd: read length overrun\n"));
		uip_abort();
		PSOCK_CLOSE_EXIT(sin);
	}

	// Read exactly len bytes
	sin->bufsize = len;
	PSOCK_READBUF(sin);

	// Check length
	if (PSOCK_DATALEN(sin) != len) {
		printf_P(PSTR("owfsd: read len fail\n"));
		uip_abort();
		PSOCK_CLOSE_EXIT(sin);
	}

	PSOCK_END(sin);
}

static PT_THREAD(send_response(struct owfsd_state *s)) {
	PSOCK_BEGIN(&s->sock);

	// Send the response
	s->sock.bufsize = OW_BUFLEN;
	PSOCK_SEND(&s->sock, s->buf_out, s->len + 1);

	PSOCK_END(&s->sock);
}

static int cmd_reset(struct owfsd_state *s) {
	// Reset the bus
	int ret = ow_reset();
	if (ret < 0) {
		printf_P(PSTR("owfsd: bus reset failed\n"));
		return -1;
	}

	return 0;
}

static int cmd_byte(struct owfsd_state *s) {
	// Read/write bytes
	int ret = ow_block(&s->buf_out[2], s->len - 1);
	if (ret) {
		printf_P(PSTR("owfsd: touch byte failed\n"));
		return -1;
	}

	return 0;
}

static int cmd_bit(struct owfsd_state *s) {
	// Loop through the data buffer touching bits
	for (int i = 0; i < s->len - 1; i++) {
		int ret = ow_touch_bit(s->buf_out[i + 2]);
		if (ret < 0) {
			printf_P(PSTR("owfsd: touch bit failed\n"));
			return ret;
		}

		s->buf_out[i + 2] = ret ? 0xff : 0x00;
	}

	return 0;
}

static int cmd_search(struct owfsd_state *s) {
	ow_search_t src;

	// Set up the fields
	memcpy(src.rom_no, &s->buf_out[2], sizeof(src.rom_no));
	src.last_discrepancy = s->buf_out[10] & 0xff;
	src.last_family_discrepancy = 0;
	src.last_device_flag = 0;
	// FIXME: alarm search

	int ret = ow_search_next(&src);
	if (ret < 0) {
		printf_P(PSTR("owfsd: search failed\n"));
	}
	else if (ret == 0) {
		memset(&s->buf_out[2], 0, sizeof(ow_addr_t));
		s->buf_out[10] = 0xff; // no devices on bus
	}
	else {
		memcpy(&s->buf_out[2], src.rom_no, sizeof(src.rom_no));
		if (src.last_device_flag) {
			s->buf_out[10] = 0xfe;
		}
		else {
			s->buf_out[10] = src.last_discrepancy;
		}
	}

	return 0;
}

static int cmd_byte_pu(struct owfsd_state *s) {

	uint8_t delay = s->buf_out[2];
	uint8_t byte = s->buf_out[3];

	int ret = ow_write_byte_power(byte);
	if (ret) {
		return -1;
	}

	while (delay--) {
		// FIXME: find a better way of doing this!
		_delay_ms(500);
	}

	ret = ow_level_std();
	if (ret) {
		return -1;
	}

	return 0;
}

static PT_THREAD(handle_connection(struct owfsd_state *s)) {
	int ret;
	PT_BEGIN(&s->pt);

	while (1) {
		// Read length byte and command
		PT_WAIT_THREAD(&s->pt, read_bytes(&s->sock, 2));

		// Read in the length and command byte
		s->len = s->buf_in[0];
		s->cmd = s->buf_in[1];

		// Make sure the length isn't too long
		if (s->len >= OW_BUFLEN) {
			printf_P(PSTR("owfsd: too long!\n"));
			uip_abort();
			PT_EXIT(&s->pt);
		}
		else if (s->len == 0) {
			printf_P(PSTR("owfsd: too short!\n"));
			uip_abort();
			PT_EXIT(&s->pt);
		}

		// Read in the data packet
		if (s->len - 1) {
			PT_WAIT_THREAD(&s->pt, read_bytes(&s->sock, s->len - 1));
		}

		// Set up the output buffer
		s->buf_out[0] = s->len;
		s->buf_out[1] = s->cmd;
		memcpy(&s->buf_out[2], s->buf_in, s->len - 1);

		// Act on command
		if (s->cmd == 'R' && s->len == 1) {
			ret = cmd_reset(s);
		}
		else if (s->cmd == 'B' && s->len > 1) {
			ret = cmd_byte(s);
		}
		else if (s->cmd == 'b' && s->len > 1) {
			ret = cmd_bit(s);
		}
		else if (s->cmd == 'A' && s->len == 10) {
			ret = cmd_search(s);
		}
		else if (s->cmd == 'P' && s->len == 3) {
			ret = cmd_byte_pu(s);
		}
		else {
			uip_abort();
			PT_EXIT(&s->pt);
		}

		// Check command result
		if (ret) {
			printf_P(PSTR("owfsd: resetting due to bad return\n"));
			uip_abort();
			PT_EXIT(&s->pt);
		}

		// Send response
		PT_WAIT_THREAD(&s->pt, send_response(s));
	}

	PT_END(&s->pt);
}

static void owfsd_appcall(void *state) {
	struct owfsd_state *s = (struct owfsd_state *)state;

	if (uip_closed() || uip_aborted() || uip_timedout()) {
		if (s != NULL) {
			// Free state data
			free(s);
			conns_free++;
			tcp_markconn(uip_conn, NULL);
		}
	}
	else if (uip_connected()) {
		syslog_P(LOG_DAEMON | LOG_INFO, PSTR("Connection from %d.%d.%d.%d"),
			uip_ipaddr_to_quad(&uip_conn->ripaddr));

		// Allocate a connection if we can
		if (conns_free) {
			s = calloc(1, sizeof(*s));
			conns_free--;
		}
		else {
			s = NULL;
		}

		// Make sure we got some memory
		if (s == NULL) {
			uip_abort();
			syslog_P(LOG_DAEMON | LOG_ERR, PSTR("Could not allocate memory"));
			return;
		}

		// Set up the connection
		tcp_markconn(uip_conn, s);
		timer_set(&s->timer, CLOCK_SECOND * 60);
		PSOCK_INIT(&s->sock, s->buf_in, OW_BUFLEN);
		PT_INIT(&s->pt);

		// Handle the connection
		handle_connection(s);
	}
	else if (s != NULL) {
		if (uip_poll()) {
			if (timer_expired(&s->timer)) {
				uip_abort();

				// Free state data
				free(s);
				s = NULL;
				tcp_markconn(uip_conn, NULL);
				conns_free++;

				syslog_P(LOG_DAEMON | LOG_ERR,
					PSTR("%d.%d.%d.%d: connection timed out"),
					uip_ipaddr_to_quad(&uip_conn->ripaddr));
			}
		}
		else {
			timer_restart(&s->timer);
		}

		if (s) {
			handle_connection(s);
		}
	}
	else {
		uip_abort();
	}
}

PROCESS_THREAD(owfsd_process, ev, data) {
	PROCESS_BEGIN();

	int err = ds2482_detect(0x30);
	if (err) {
		printf_P(PSTR("owfsd: DS2482 detect failed.\n"));
		PROCESS_EXIT();
	}

	tcp_listen(UIP_HTONS(OWFSD_PORT));

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event) {
			owfsd_appcall(data);
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			PROCESS_EXIT();
		}
	}

	PROCESS_END();
}

