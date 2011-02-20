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
#include <strftime.h>

#include "apps/network.h"

#include "syslog.h"

#define SYSLOG_MAX_QUEUE_SIZE 4

#if UIP_CONF_BUFFER_SIZE < 64
#define SYSLOG_MSG_MAX_LEN UIP_CONF_BUFFER_SIZE
#else
#define SYSLOG_MSG_MAX_LEN 64
#endif

#define UIP_UDP_MAXLEN (UIP_BUFSIZE - UIP_LLH_LEN - UIP_IPUDPH_LEN)

static const uip_ipaddr_t syslog_server = { .u8 = { 81,187,55,68 }};

PROCESS(syslog_process, "syslog");
INIT_PROCESS(syslog_process);
LIST(msgq);

static struct uip_udp_conn *conn;

// Log everything by default (!!! mask is inverted !!!)
static uint8_t log_mask = 0x00;

struct msg_hdr {
	struct msg_hdr *next;
	uint32_t pri;
	time_t time;
	struct process *process;
	char msg[SYSLOG_MSG_MAX_LEN];
};

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

static void append(char *msg, uint16_t *offset, PGM_P format, ...) {
	va_list args;
	int ret;

	// Check offset
	if (*offset >= UIP_UDP_MAXLEN) {
		return;
	}

	// Append formatted string
	va_start(args, format);
	ret = vsnprintf_P(
		msg + *offset,
		UIP_UDP_MAXLEN - *offset,
		format, args);
	va_end(args);

	// Check length
	if (*offset + ret > UIP_UDP_MAXLEN) {
		*offset = UIP_UDP_MAXLEN;
	}
	else {
		*offset += ret;
	}
}

static void append_time(char *msg, uint16_t *offset, time_t time) {
	struct tm tm;
	int ret;

	// Check offset
	if (*offset >= UIP_UDP_MAXLEN) {
		return;
	}

	// Get the current time
	gmtime(time, &tm);

	// Append formatted string
	ret = strftime_P(
		msg + *offset,
		UIP_UDP_MAXLEN - *offset,
		PSTR("%b %e %H:%M:%S"), &tm);

	// Check length
	if (*offset + ret > UIP_UDP_MAXLEN) {
		*offset = UIP_UDP_MAXLEN;
	}
	else {
		*offset += ret;
	}
}

static struct msg_hdr *init_msg(uint32_t pri) {
	struct msg_hdr *msg;

	// Check the priority against the log_mask
	if (log_mask & LOG_MASK(LOG_PRI(pri))) {
		return NULL;
	}

	// Allocate memory for the log entry
	msg = malloc(sizeof(struct msg_hdr));
	if (msg == NULL) {
		return NULL;
	}

	msg->pri = pri;
	msg->time = wallclock_seconds();
	msg->process = PROCESS_CURRENT();

	return msg;
}

static void msg_finish(struct msg_hdr *msg) {
	int len = strlen(msg->msg);

	// Reduce memory allocation
	msg = realloc(msg, sizeof(*msg) - sizeof(msg->msg) + len + 3); // off by two?!

	// Add to the end of the queue
	list_add(msgq, msg);
/*
	// Trim queue if necessary
	while (list_length(msgq) > SYSLOG_MAX_QUEUE_SIZE) {
		list_chop(msgq);
	}*/

	// We have a message to send
	tcpip_poll_udp(conn);
}

/* Generate a log message using FMT and using arguments pointed to by AP. */
void vsyslog(uint32_t pri, const char *fmt, va_list ap) {
	struct msg_hdr *msg;

	// Start off the message
	msg = init_msg(pri);
	if (msg == NULL) {
		return;
	}

	// Append the formatted string
	vsnprintf(
		msg->msg,
		sizeof(msg->msg),
		fmt, ap);

	// Add to queue
	msg_finish(msg);
}

/* Generate a log message using FMT and using arguments pointed to by AP. */
void vsyslog_P(uint32_t pri, PGM_P fmt, va_list ap) {
	struct msg_hdr *msg;

	// Start off the message
	msg = init_msg(pri);
	if (msg == NULL) {
		return;
	}

	// Append the formatted string
	vsnprintf_P(
		msg->msg,
		sizeof(msg->msg),
		fmt, ap);

	// Add to queue
	msg_finish(msg);
}

static void init(void) {
	list_init(msgq);

	conn = udp_new(&syslog_server, UIP_HTONS(SYSLOG_PORT), NULL);
	if (conn) {
		udp_bind(conn, UIP_HTONS(SYSLOG_PORT));
	}
}

static void send_message(struct msg_hdr *msg) {
	uint16_t off = 0;
	uip_ipaddr_t addr;

	// Insert syslog priority
	append(uip_appdata, &off, PSTR("<%lu>"), msg->pri);

	// Append time
	append_time(uip_appdata, &off, msg->time);

	// Append hostname (IP address)
	uip_gethostaddr(&addr);
	append(uip_appdata, &off, PSTR(" %d.%d.%d.%d"), uip_ipaddr_to_quad(&addr));

	// Append the process name
	append(uip_appdata, &off, PSTR(" %S: "), PROCESS_NAME_STRING(msg->process));

	// Finally, add the message
	append(uip_appdata, &off, PSTR("%s"), msg->msg);

	// Send the message
	uip_udp_send(off);
}

PROCESS_THREAD(syslog_process, ev, data) {
	PROCESS_BEGIN();

	// Set things up
	init();

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event) {
			if (net_flags.configured) {
				// Send a message
				struct msg_hdr *msg = list_pop(msgq);
				if (msg) {
					send_message(msg);
					free(msg);
				}

				if (list_head(msgq)) {
					// We have more messages to send
					tcpip_poll_udp(conn);
				}
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&syslog_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

