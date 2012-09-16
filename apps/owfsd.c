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
#include <onewire.h>
#if CONFIG_APPS_SYSLOG
#include "syslog.h"
#endif
#include "drivers/ds2482.h"

#define OWFSD_PORT 15862

#define CMD_RESET 'R'
#define CMD_BYTES 'B'
#define CMD_BITS 'b'
#define CMD_SEARCH 'A'
#define CMD_BYTE_SPU 'P'
#define CMD_RET_ERROR 'E' // only used to send back to client

#define ERR_OK 0
#define ERR_INVALID 1 // invalid command or request size
#define ERR_BUFSZ 2 // buffer too long, short read, etc...
#define ERR_OWSD 3 // bus short detected
#define ERR_OWERR 4 // general bus fault
#define ERR_NOLOCK 5 // bus access without a lock

#ifndef CONFIG_APPS_OWFSD_MAX_CONNS
#define MAX_CONNS (UIP_CONNS / 2)
#else /* CONFIG_APPS_OWFSD_MAX_CONNS */
#define MAX_CONNS CONFIG_APPS_OWFSD_MAX_CONNS
#endif /* CONFIG_APPS_OWFSD_MAX_CONNS */

#ifndef CONFIG_APPS_OWFSD_BUFFER_SIZE
#define OW_BUFLEN 64
#else /* CONFIG_APPS_OWFSD_BUFFER_SIZE */
#define OW_BUFLEN CONFIG_APPS_OWFSD_BUFFER_SIZE
#endif /* CONFIG_APPS_OWFSD_BUFFER_SIZE */

#define LOCK_TIMER_INTERVAL (3 * CLOCK_SECOND)

struct owfsd_state;

struct owfs_command {
	uint8_t cmd;
	int (* fn)(struct owfsd_state *s);
	struct {
		uint8_t bus_op : 1; // Bus operation; requires lock
		uint8_t lock_auto : 1; // Causes bus reset; auto-acquires lock
	} flags;
};

struct owfs_packet {
	uint8_t len;
	uint8_t cmd;
	union {
		uint8_t bytes[OW_BUFLEN];
		struct {
			ow_addr_t addr;
			uint8_t flags;
		} search;
		struct {
			uint8_t delay;
			uint8_t byte;
		} spu;
		uint8_t error;
	} buf;
};

struct owfsd_state {
	struct psock sock;
	struct pt pt;
	uint8_t buf_in[OW_BUFLEN];
	uint8_t status;
	struct owfs_packet pkt;
	struct owfs_command cmd;
	struct timer lock_timer;
	struct {
		uint8_t locked : 1;
	} flags;
};

static int cmd_reset(struct owfsd_state *s);
static int cmd_byte(struct owfsd_state *s);
static int cmd_bit(struct owfsd_state *s);
static int cmd_search(struct owfsd_state *s);
static int cmd_byte_spu(struct owfsd_state *s);

static const struct owfs_command commands[] PROGMEM = {
	{ CMD_RESET,	cmd_reset,		{ .bus_op = 1, .lock_auto = 1, } },
	{ CMD_BYTES,	cmd_byte,		{ .bus_op = 1, } },
	{ CMD_BITS,		cmd_bit,		{ .bus_op = 1, } },
	{ CMD_SEARCH,	cmd_search,		{ .bus_op = 1, .lock_auto = 1, } },
	{ CMD_BYTE_SPU,	cmd_byte_spu,	{ .bus_op = 1, } },
	{} // end-of-table marker
};

static uint8_t conns_free = MAX_CONNS;

PROCESS(owfsd_process, "owfsd");
INIT_PROCESS(owfsd_process);

static PT_THREAD(read_bytes(struct owfsd_state *s, uint8_t len)) {
	PSOCK_BEGIN(&s->sock);

	// Check buffer length
	if (len > OW_BUFLEN) {
		s->status = ERR_BUFSZ;
		PSOCK_EXIT(&s->sock);
	}

	// Read exactly len bytes
	s->sock.bufsize = len;
	PSOCK_READBUF(&s->sock);

	// Check length
	if (PSOCK_DATALEN(&s->sock) != len) {
		s->status = ERR_BUFSZ;
		PSOCK_EXIT(&s->sock);
	}

	// Read successful
	s->status = ERR_OK;

	PSOCK_END(&s->sock);
}

static PT_THREAD(send_response(struct owfsd_state *s)) {
	PSOCK_BEGIN(&s->sock);

	// Send the response
	s->sock.bufsize = OW_BUFLEN;
	PSOCK_SEND(&s->sock, (uint8_t *)&s->pkt, s->pkt.len + 2);

	PSOCK_END(&s->sock);
}

static PT_THREAD(send_error(struct owfsd_state *s)) {
	PSOCK_BEGIN(&s->sock);

	// Clobber response size & length
	s->pkt.len = sizeof(s->pkt.buf.error);
	s->pkt.cmd = CMD_RET_ERROR;

	// Copy over error code
	s->pkt.buf.error = s->status;

	// Send the error response
	s->sock.bufsize = OW_BUFLEN;
	PSOCK_SEND(&s->sock, (uint8_t *)&s->pkt, s->pkt.len + 2);

	PSOCK_END(&s->sock);
}

static int cmd_reset(struct owfsd_state *s) {
	if (s->pkt.len != 0) {
		return ERR_INVALID;
	}

	// Reset the bus
	int ret = ow_reset();
	if (ret == -2) {
#if CONFIG_APPS_SYSLOG
		// Log something
		syslog_P(LOG_DAEMON | LOG_ERR,
			PSTR("1-Wire bus short circuit detected"));
#endif

		return ERR_OWSD;
	}
	else if (ret < 0) {
#if CONFIG_APPS_SYSLOG
		// Log something
		syslog_P(LOG_DAEMON | LOG_ERR,
			PSTR("1-Wire bus reset failure"));
#endif

		return ERR_OWERR;
	}

	return ERR_OK;
}

static int cmd_byte(struct owfsd_state *s) {
	if (s->pkt.len == 0) {
		return ERR_INVALID;
	}

	// Read/write bytes
	int ret = ow_block(s->pkt.buf.bytes, s->pkt.len);
	if (ret < 0) {
		return ERR_OWERR;
	}

	return ERR_OK;
}

static int cmd_bit(struct owfsd_state *s) {
	if (s->pkt.len == 0) {
		return ERR_INVALID;
	}

	// Loop through the data buffer touching bits
	for (int i = 0; i < s->pkt.len; i++) {
		int ret = ow_touch_bit(s->pkt.buf.bytes[i]);
		if (ret < 0) {
			return ERR_OWERR;
		}

		s->pkt.buf.bytes[i] = ret ? 0xff : 0x00;
	}

	return ERR_OK;
}

static int cmd_search(struct owfsd_state *s) {
	if (s->pkt.len != sizeof(s->pkt.buf.search)) {
		return ERR_INVALID;
	}

	ow_search_t src;

	// Set up the fields
	memcpy(&src.rom_no, &s->pkt.buf.search.addr, sizeof(src.rom_no));
	src.last_discrepancy = s->pkt.buf.search.flags & 0x7f;
	src.last_family_discrepancy = 0;
	src.last_device_flag = 0;
	src.alarm = s->pkt.buf.search.flags & 0x80 ? 1 : 0;

	int ret = ow_search_next(&src);
	if (ret < 0) {
#if CONFIG_APPS_SYSLOG
		// Log something
		syslog_P(LOG_DAEMON | LOG_ERR,
			PSTR("1-Wire bus search failed"));
#endif

		return ERR_OWERR;
	}
	else if (ret == 0) {
		memset(&s->pkt.buf.search.addr, 0, sizeof(ow_addr_t));
		s->pkt.buf.search.flags = 0xff; // no devices on bus

		return ERR_OK;
	}

	// Copy back the found 1-Wire address
	memcpy(&s->pkt.buf.search.addr, &src.rom_no, sizeof(src.rom_no));

	// Found last device?
	if (src.last_device_flag) {
		s->pkt.buf.search.flags = 0xfe;
	}
	else {
		s->pkt.buf.search.flags = src.last_discrepancy;
	}

	return ERR_OK;
}

static int cmd_byte_spu(struct owfsd_state *s) {
	if (s->pkt.len != sizeof(s->pkt.buf.spu)) {
		return ERR_INVALID;
	}

	// Send power byte command
	int ret = ow_write_byte_power(s->pkt.buf.spu.byte);
	if (ret) {
		return ERR_OWERR;
	}

	// Wait for delay * 10ms
	while (s->pkt.buf.spu.delay--) {
		// FIXME: find a better way of doing this!
		_delay_ms(10);
	}

	ret = ow_level_std();
	if (ret) {
		return ERR_OWERR;
	}

	return ERR_OK;
}

static PT_THREAD(handle_connection(struct owfsd_state *s)) {
	PT_BEGIN(&s->pt);

	while (1) {
		// Read length byte and command
		PT_WAIT_THREAD(&s->pt, read_bytes(s, 2));
		if (s->status) {
			PT_WAIT_THREAD(&s->pt, send_error(s));
			continue;
		}

		// Read in the length and command bytes
		s->pkt.len = s->buf_in[0];
		s->pkt.cmd = s->buf_in[1];

		// Make sure the length isn't too long
		if (s->pkt.len > OW_BUFLEN) {
			// Weed to consume the sent bytes anyway
			while (s->pkt.len) {
				PT_WAIT_THREAD(&s->pt, read_bytes(s,
					s->pkt.len < OW_BUFLEN ? s->pkt.len : OW_BUFLEN));
				s->pkt.len -= s->pkt.len < OW_BUFLEN ? s->pkt.len : OW_BUFLEN;
			}

			// Send error
			s->status = ERR_BUFSZ;
			PT_WAIT_THREAD(&s->pt, send_error(s));
			continue;
		}
		else if (s->pkt.len) {
			// Read in the data packet
			PT_WAIT_THREAD(&s->pt, read_bytes(s, s->pkt.len));
			if (s->status) {
				PT_WAIT_THREAD(&s->pt, send_error(s));
				continue;
			}

			// Copy the packet contents
			memcpy(s->pkt.buf.bytes, s->buf_in, s->pkt.len);
		}

		// Get the command info
		const struct owfs_command *cmd = commands;
		do {
			uint8_t c = pgm_read_byte(&cmd->cmd);
			if (!c) {
				memset(&s->cmd, 0, sizeof(s->cmd));
				break;
			}
			else if (c == s->pkt.cmd) {
				memcpy_P(&s->cmd, cmd, sizeof(s->cmd));
				break;
			}
			else {
				cmd++;
			}
		} while (1);

		// Sanity check command
		if (!s->cmd.cmd) {
			s->status = ERR_INVALID;
			PT_WAIT_THREAD(&s->pt, send_error(s));
			continue;
		}

		// Check if a lock is required
		if (s->cmd.flags.bus_op) {
			if (s->flags.locked) {
				// Refresh lock timer
				timer_restart(&s->lock_timer);
			}
			else if (s->cmd.flags.lock_auto) {
				// Acquire a lock
				while (!ow_lock()) {
					uip_poll_conn(uip_conn);
					PT_YIELD(&s->pt);
				}

				s->flags.locked = 1;
				timer_set(&s->lock_timer, LOCK_TIMER_INTERVAL);
			}
			else {
				s->status = ERR_NOLOCK;
				PT_WAIT_THREAD(&s->pt, send_error(s));
				continue;
			}
		}

		// Run command
		s->status = s->cmd.fn(s);

		// Check command result
		if (s->status) {
			PT_WAIT_THREAD(&s->pt, send_error(s));
		}
		else {
			// Send response
			PT_WAIT_THREAD(&s->pt, send_response(s));
		}
	}

	PT_END(&s->pt);
}

static void owfsd_appcall(void *state) {
	struct owfsd_state *s = (struct owfsd_state *)state;

	if (uip_closed() || uip_aborted() || uip_timedout()) {
		if (s != NULL) {
			// Make sure we release the lock
			if (s->flags.locked) {
				ow_unlock();
			}

			// Free state data
			free(s);
			tcp_markconn(uip_conn, NULL);
			conns_free++;
		}
	}
	else if (uip_connected()) {
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
			// Reset the connection so the remote end knows something is up
			uip_abort();

#if CONFIG_APPS_SYSLOG
			// Log something
			syslog_P(LOG_DAEMON | LOG_WARNING,
				PSTR("%d.%d.%d.%d: too much going on, try later"),
				uip_ipaddr_to_quad(&uip_conn->ripaddr));
#endif

			return;
		}

		// Set up the connection
		tcp_markconn(uip_conn, s);
		PSOCK_INIT(&s->sock, s->buf_in, OW_BUFLEN);
		PT_INIT(&s->pt);

#if CONFIG_APPS_SYSLOG
		// Log something
		syslog_P(LOG_DAEMON | LOG_INFO,
			PSTR("%d.%d.%d.%d: Connected"),
			uip_ipaddr_to_quad(&uip_conn->ripaddr));
#endif

		// Handle the connection
		handle_connection(s);
	}
	else if (s != NULL) {
		handle_connection(s);

		// Check for expired locks
		if (s->flags.locked && timer_expired(&s->lock_timer)) {
			ow_unlock();
			s->flags.locked = 0;
		}
	}
	else {
		uip_abort();
	}
}

PROCESS_THREAD(owfsd_process, ev, data) {
	PROCESS_BEGIN();

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

