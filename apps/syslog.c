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

#include <alloca.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <contiki-net.h>

#include <drivers/wallclock.h>
#include <init.h>
#include <time.h>

#include "syslog.h"

#if UIP_CONF_BUFFER_SIZE < 128
#define SYSLOG_INIT_ALLOC UIP_CONF_BUFFER_SIZE
#else
#define SYSLOG_INIT_ALLOC 128
#endif

static const PGM_P time_fmt = "%b %e %H:%M:%S";
static const uip_ipaddr_t syslog_server = { .u8 = { 81,187,55,68 }};

PROCESS(syslog_process, "syslog");
INIT_PROCESS(syslog_process);
LIST(msgq);

static struct uip_udp_conn *conn;

// Log everything by default (!!! mask is inverted !!!)
static uint8_t log_mask = 0x00;

/*
 * Set the log mask level.
 *
 * mask is a bit string with one bit corresponding to each of the possible
 * message priorities. If the bit is on, syslog handles messages of that
 * priority normally. If it is off, syslog discards messages of that priority
 */
uint32_t setlogmask(uint32_t mask) {
	uint32_t temp = ~log_mask;
	log_mask = ~mask;
	return temp;
}

/* Generate a log message using FMT string and option arguments. */
void syslog(uint32_t pri, const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsyslog(pri, fmt, args);
	va_end(args);
}

/* Generate a log message using FMT string and option arguments. */
void syslog_P(uint32_t pri, PGM_P fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsyslog_P(pri, fmt, args);
	va_end(args);
}

static void append(char *msg, uint8_t *offset, PGM_P format, ...) {
	va_list args;
	int ret;

	// Check offset
	if (*offset >= SYSLOG_INIT_ALLOC) {
		return;
	}

	// Append formatted string
	va_start(args, format);
	ret = vsnprintf_P(
		msg + *offset,
		SYSLOG_INIT_ALLOC - *offset,
		format, args);
	va_end(args);

	// Check length
	if (*offset + ret > SYSLOG_INIT_ALLOC) {
		*offset = SYSLOG_INIT_ALLOC;
	}
	else {
		*offset += ret;
	}
}

static void append_time(char *msg, uint8_t *offset) {
	char *fmt;
	struct tm tm;
	int ret;

	// Check offset
	if (*offset >= SYSLOG_INIT_ALLOC) {
		return;
	}

	// Bring the format string into RAM
	fmt = alloca(sizeof(time_fmt));
	strcpy_P(fmt, time_fmt);

	// Get the current time
	gmtime(wallclock_seconds(), &tm);

	// Append formatted string
	ret = strftime(
		msg + *offset,
		SYSLOG_INIT_ALLOC - *offset,
		fmt, &tm);

	// Check length
	if (*offset + ret > SYSLOG_INIT_ALLOC) {
		*offset = SYSLOG_INIT_ALLOC;
	}
	else {
		*offset += ret;
	}
}

static char *init_msg(uint32_t pri, uint8_t *off) {
	char *msg;
	uip_ipaddr_t addr;

	// Check the priority against the log_mask
	if (log_mask & LOG_MASK(LOG_PRI(pri))) {
		return NULL;
	}

	// Allocate memory for the log entry
	msg = malloc(SYSLOG_INIT_ALLOC);
	if (msg == NULL) {
		return NULL;
	}

	*off = 0;

	// Insert syslog priority
	append(msg, off, PSTR("<%lu>"), pri);

	// Append time
	append_time(msg, off);

	// Append hostname (IP address)
	uip_gethostaddr(&addr);
	append(msg, off, PSTR(" %d.%d.%d.%d"), uip_ipaddr_to_quad(&addr));

	// Append the process name
	struct process *p = PROCESS_CURRENT();
	append(msg, off, PSTR(" %S: "), PROCESS_NAME_STRING(p));

	// Check length
	if (*off >= SYSLOG_INIT_ALLOC) {
		free(msg);
		return NULL;
	}

	return msg;
}

/* Generate a log message using FMT and using arguments pointed to by AP. */
void vsyslog(uint32_t pri, const char *fmt, va_list ap) {
	uint8_t off;
	char *msg;
	int ret;
   
	// Start off the message
	msg = init_msg(pri, &off);
	if (msg == NULL) {
		return;
	}

	// Append the formatted string
	ret = vsnprintf(
		msg + off,
		SYSLOG_INIT_ALLOC - off,
		fmt, ap);
	if (off + ret > SYSLOG_INIT_ALLOC) {
		off = SYSLOG_INIT_ALLOC;
	}
	else {
		off += ret;
	}

	// Reduce memory allocation
	msg = realloc(msg, off + 1);

	// Add to the end of the queue
	list_add(msgq, msg);
}

/* Generate a log message using FMT and using arguments pointed to by AP. */
void vsyslog_P(uint32_t pri, PGM_P fmt, va_list ap) {
	uint8_t off;
	char *msg;
	int ret;
   
	// Start off the message
	msg = init_msg(pri, &off);
	if (msg == NULL) {
		return;
	}

	// Append the formatted string
	ret = vsnprintf_P(
		msg + off,
		SYSLOG_INIT_ALLOC - off,
		fmt, ap);
	if (off + ret > SYSLOG_INIT_ALLOC) {
		off = SYSLOG_INIT_ALLOC;
	}
	else {
		off += ret;
	}

	// Reduce memory allocation
	msg = realloc(msg, off + 1);

	// Add to the end of the queue
	list_add(msgq, msg);
}

static void init(void) {
	list_init(msgq);

	conn = udp_new(&syslog_server, UIP_HTONS(SYSLOG_PORT), NULL);
	if (conn) {
		udp_bind(conn, UIP_HTONS(SYSLOG_PORT));
	}
}

static void send_message(char *msg) {
	int len = strlen(msg);

	// Copy the message into uip_appdata
	memcpy(uip_appdata, msg, len);

	// Send the message
	uip_udp_send(len);
}

PROCESS_THREAD(syslog_process, ev, data) {
	PROCESS_BEGIN();

	// Set things up
	init();

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event) {
			// Send a message
			char *msg = list_pop(msgq);
			if (msg) {
				send_message(msg);
				free(msg);
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&syslog_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

