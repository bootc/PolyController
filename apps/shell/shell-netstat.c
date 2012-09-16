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
 * $Id: shell-netstat.c,v 1.2 2010/10/19 18:29:03 adamdunkels Exp $
 */

#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "contiki.h"
#include "shell.h"
#include "contiki-net.h"

static const char closed[] PROGMEM =   /*  "CLOSED",*/
{0x43, 0x4c, 0x4f, 0x53, 0x45, 0x44, 0};
static const char syn_rcvd[] PROGMEM = /*  "SYN-RCVD",*/
{0x53, 0x59, 0x4e, 0x2d, 0x52, 0x43, 0x56,
	0x44,  0};
static const char syn_sent[] PROGMEM = /*  "SYN-SENT",*/
{0x53, 0x59, 0x4e, 0x2d, 0x53, 0x45, 0x4e,
	0x54,  0};
static const char established[] PROGMEM = /*  "ESTABLISHED",*/
{0x45, 0x53, 0x54, 0x41, 0x42, 0x4c, 0x49,
	0x53, 0x48, 0x45, 0x44, 0};
static const char fin_wait_1[] PROGMEM = /*  "FIN-WAIT-1",*/
{0x46, 0x49, 0x4e, 0x2d, 0x57, 0x41, 0x49,
	0x54, 0x2d, 0x31, 0};
static const char fin_wait_2[] PROGMEM = /*  "FIN-WAIT-2",*/
{0x46, 0x49, 0x4e, 0x2d, 0x57, 0x41, 0x49,
	0x54, 0x2d, 0x32, 0};
static const char closing[] PROGMEM = /*  "CLOSING",*/
{0x43, 0x4c, 0x4f, 0x53, 0x49,
	0x4e, 0x47, 0};
static const char time_wait[] PROGMEM = /*  "TIME-WAIT,"*/
{0x54, 0x49, 0x4d, 0x45, 0x2d, 0x57, 0x41,
	0x49, 0x54, 0};
static const char last_ack[] PROGMEM = /*  "LAST-ACK"*/
{0x4c, 0x41, 0x53, 0x54, 0x2d, 0x41, 0x43,
	0x4b, 0};
static const char none[] PROGMEM = /*  "NONE"*/
{0x4e, 0x4f, 0x4e, 0x45, 0};
static const char running[] PROGMEM = /*  "RUNNING"*/
{0x52, 0x55, 0x4e, 0x4e, 0x49, 0x4e, 0x47,
	0};
static const char called[] PROGMEM = /*  "CALLED"*/
{0x43, 0x41, 0x4c, 0x4c, 0x45, 0x44, 0};

static const char * const states[] PROGMEM = {
	closed,
	syn_rcvd,
	syn_sent,
	established,
	fin_wait_1,
	fin_wait_2,
	closing,
	time_wait,
	last_ack,
	none,
	running,
	called};

extern u16_t uip_listenports[UIP_LISTENPORTS];

PROCESS(shell_netstat_process, "netstat");
SHELL_COMMAND(netstat_command,
		"netstat",
		"netstat: show UDP and TCP connections",
		&shell_netstat_process);
INIT_SHELL_COMMAND(netstat_command);

PROCESS_THREAD(shell_netstat_process, ev, data) {
	int i;
	struct uip_conn *conn;
	PROCESS_BEGIN();

	for(i = 0; i < UIP_CONNS; ++i) {
		conn = &uip_conns[i];
		shell_output_P(&netstat_command,
			PSTR("TCP %u, %u.%u.%u.%u:%u, %S, %u, %u, %c %c\n"),
			uip_htons(conn->lport),
			conn->ripaddr.u8[0],
			conn->ripaddr.u8[1],
			conn->ripaddr.u8[2],
			conn->ripaddr.u8[3],
			uip_htons(conn->rport),
			(PGM_P)pgm_read_word(&states[conn->tcpstateflags & UIP_TS_MASK]),
			conn->nrtx,
			conn->timer,
			(uip_outstanding(conn))? '*':' ',
			(uip_stopped(conn))? '!':' ');
	}

	for (i = 0; i < UIP_UDP_CONNS; i++) {
		struct uip_udp_conn *udp = &uip_udp_conns[i];
		shell_output_P(&netstat_command,
			PSTR("UDP %u, %u.%u.%u.%u:%u\n"),
			uip_htons(udp->lport),
			udp->ripaddr.u8[0],
			udp->ripaddr.u8[1],
			udp->ripaddr.u8[2],
			udp->ripaddr.u8[3],
			uip_htons(udp->rport));
	}

	shell_output_P(&netstat_command, PSTR("Listen ports:\n"));
	for (i = 0; i < UIP_LISTENPORTS; i++) {
		shell_output_P(&netstat_command, PSTR("%d\n"),
			UIP_HTONS(uip_listenports[i]));
	}

	PROCESS_END();
}

