/*=============================================================================
Cyan Technology Limited

FILE - tpftp.c
    Example eCOG1 application.

DESCRIPTION
    Demonstrates Trivial File Transfer protocol on the uIP stack.

MODIFICATION DETAILS
=============================================================================*/



/*
 Date: Nov 20th, 2006. Copyright (c) 2006 Cyan Technology Limited.
 All Rights Reserved.

 Cyan Technology Limited, hereby grants a non-exclusive license under
 Cyan Technology Limited's copyright to copy, modify and distribute
 this software for any purpose and without fee, provided that the above
 copyright notice and the following paragraphs appear on all copies.

 Cyan Technology Limited makes no representation that this source
 code is correct or is an accurate representation of any standard.

 In no event shall Cyan Technology Limited have any liability for
 any direct, indirect, or speculative damages, (including, without
 limiting the forgoing, consequential, incidental and special
 damages) including, but not limited to infringement, loss of use,
 business interruptions, and loss of profits, respective of whether
 Cyan Technology Limited has advance notice of the possibility of
 any such damages.

 Cyan Technology Limited specifically disclaims any warranties
 including, but not limited to, the implied warranties of
 merchantability, fitness for a particular purpose,
 and non-infringement. The software provided hereunder is on an
 "as is" basis and Cyan Technology Limited has no obligation to
 provide maintenance, support, updates, enhancements, or modifications.
 */

// Includes

#include <contiki-net.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tftp.h"

struct modes {
	const char *m_name;
	const char *m_mode;
};

static const struct modes modes[] = {
	{ "netascii", "netascii" },
	{ "ascii",    "netascii" },
	{ "octet",    "octet"    },
	{ "binary",   "octet"    },
	{ "image",    "octet"    },
	{ 0, 0 }
};

#define MODE_OCTET    (&modes[2])
#define MODE_NETASCII (&modes[0])
#define MODE_BINARY   (&modes[3])
#define MODE_DEFAULT  MODE_BINARY

#define UDPBUF ((struct uip_udpip_hdr *)&uip_buf[UIP_LLH_LEN])
#define TFTP_MSG_ETIMEOUT Errmsg_TIMEOUT

const char Errmsg_TIMEOUT[] = "Timeout";
const char Errmsg_IOERROR[] = "I/O Error";

static uint16_t parse_msg(tftp_state_t *s) {
	uint8_t *m = (uint8_t *)uip_appdata;
	uint16_t opcode = UIP_HTONS(*(uint16_t *)m);

	switch (opcode) {
		case TFTP_DATA:
			s->block = UIP_HTONS(*(uint16_t *)(m + 2));  /* block number */
			break;

		case TFTP_ACK:
			s->ack = UIP_HTONS(*(uint16_t *)(m + 2));    /* ack number */
			break;

		case TFTP_ERROR:
			s->error_code = UIP_HTONS(*(uint16_t *)(m + 2)); /* error code */
			break;

		case TFTP_OACK:
			break;
	}

	return opcode;
}

void tftp_init(tftp_state_t *s) {
	s->state = TFTP_STATE_IDLE;
	s->opcode = TFTP_IDLE;

	s->conn = udp_new(&s->addr, UIP_HTONS(TFTP_PORT), s);
}

void tftp_appcall(tftp_state_t *s) {
	// check if UDP connection established
	if (uip_udp_conn->lport == (s->conn)->lport) {
		if (s->state == TFTP_STATE_CONN) {
			send_tftp_rq(s);
#if TIME_TIMEOUT != 0
			stimer_set(&s->timer, TIME_TIMEOUT);
#endif
			s->state = TFTP_STATE_XFR;
		}
		else if (s->state == TFTP_STATE_XFR) {
			if (s->conn->rport == UIP_HTONS(TFTP_PORT)) {
				s->conn->rport = 0;
			}
		}

		if (uip_newdata()) {
			int ret = 0;

			s->opcode = parse_msg(s);

			switch (s->opcode) {
				case TFTP_OACK:
					/* receive OACK */
					/* remove port 69, setup new port with server's TID */
					s->conn->rport = UDPBUF->srcport;

					/* reply OACK */
					send_tftp_ack(s);
					break;

				case TFTP_DATA:
					/* receive DATA */
					/* remove port 69, setup new port with server's TID */
					s->conn->rport = UDPBUF->srcport;

					/*********************************/
					/* copy payload to here          */
					/* collect data from uip_appdata */
					/*********************************/

					/* check payload size, the last packet if size < 512 */
					/* 512 (data) + 8 (UDP header) + 4 (TFTP header) */
					if (UIP_HTONS(UDPBUF->udplen) < 524) {
						s->size = (512L * (uint32_t)s->block) -
							(uint32_t)(524L - UIP_HTONS(UDPBUF->udplen));
						s->state = TFTP_STATE_CLOSE;
					}

					if (s->iofunc) {
						uint32_t offset = 512L * (uint32_t)(s->block - 1);
						uint16_t size = UIP_HTONS(UDPBUF->udplen) - 12;
						ret = s->iofunc(s, offset, size,
							((uint8_t *)uip_appdata) + 4);
					}

					if (ret == 0) {
						/* send ACK */
						s->ack = s->block;
						send_tftp_ack(s);

#if TIME_TIMEOUT != 0
						stimer_set(&s->timer, TIME_TIMEOUT);
#endif
					}
					else {
						s->state = TFTP_STATE_ERR;
						s->error_code = TFTP_EUNDEF;
						s->errmsg = Errmsg_IOERROR;
						send_tftp_error(s);
					}
					break;

				case TFTP_ACK:
					/* receive ACK */
					/* remove port 69, setup new port with server's TID */
					s->conn->rport = UDPBUF->srcport;

					/* send DATA packet */
					if (s->state == TFTP_STATE_XFR) {
						s->block = s->ack + 1;
						send_tftp_data(s);
#if TIME_TIMEOUT != 0
						stimer_set(&s->timer, TIME_TIMEOUT);
#endif
					}
					break;

				case TFTP_ERROR:
					/* receive ERROR */
					s->state = TFTP_STATE_ERR;
					break;
			}
		}
#if TIME_TIMEOUT != 0
		if (s->state == TFTP_STATE_XFR) {
			if (stimer_expired(&s->timer)) {
				s->state = TFTP_STATE_TIMEOUT;
				s->error_code = TFTP_ETIMEOUT;
				s->errmsg = TFTP_MSG_ETIMEOUT;
				send_tftp_error(s);
			}
		}
#endif
	}
}

// send WRQ/RRQ packet
void send_tftp_rq(tftp_state_t *s) {
	uint8_t *m = (uint8_t *)uip_appdata;
	uint16_t len;

	*(uint16_t *)m = UIP_HTONS(s->opcode);
	m += 2;

	len = strlen((char *)s->filename);
	strncpy((char *)m, (char *)s->filename, len);
	m += len;
	*m++ = 0x00;

	len = strlen(MODE_DEFAULT->m_name);
	strncpy((char *)m, (char *)MODE_DEFAULT->m_name, len);
	m += len;
	*m++ = 0x00;

	uip_send(uip_appdata, m - (uint8_t *)uip_appdata);
}

// send ACK
void send_tftp_ack(tftp_state_t *s) {
	uint8_t *m = (uint8_t *)uip_appdata;

	*(uint16_t *)m = UIP_HTONS(TFTP_ACK);
	m += 2;

	*(uint16_t *)m = UIP_HTONS(s->ack);
	m += 2;

	uip_send(uip_appdata, 4);
}

// send DATA packet
void send_tftp_data(tftp_state_t *s) {
	uint8_t *m = (uint8_t *)uip_appdata;

	*(uint16_t *)m = UIP_HTONS(TFTP_DATA);
	m += 2;

	*(uint16_t *)m = UIP_HTONS(s->block);
	m += 2;

	/****************************/
	/* prepare payload to here  */
	/* copy data to uip_appdata */
	/****************************/
	if (((uint32_t) s->block * 512) < s->size) {
		m += 512;
	}
	else {
		m += s->size & 0x1ff;
		s->state = TFTP_STATE_CLOSE;
	}

	uip_send(uip_appdata, m - (uint8_t *)uip_appdata);
}

// send ERROR packet
void send_tftp_error(tftp_state_t *s) {
	uint8_t *m = (uint8_t *)uip_appdata;
	uint16_t len;

	*(uint16_t *)m = UIP_HTONS(TFTP_ERROR);
	m += 2;

	*(uint16_t *)m = UIP_HTONS(s->error_code);
	m += 2;

	len = strlen((char *)s->errmsg);
	strncpy((char *)m, (char *)s->errmsg, len);
	m += len;
	*m++ = 0x00;

	uip_send(uip_appdata, m - (uint8_t *)uip_appdata);
}

// GET command
void tftp_get(tftp_state_t *s, char *filename) {
	s->state = TFTP_STATE_CONN;  /* switch to TFTP_STATE_CONN state */
	s->filename = filename;    /* specify filename */
	s->opcode = TFTP_RRQ;
	s->size = 0;
	s->block = 0;
	s->ack = 0;
	s->conn->rport = UIP_HTONS(TFTP_PORT); /* connect UDP port */
}

// PUT command
void tftp_put(tftp_state_t *s, char *filename) {
	s->state = TFTP_STATE_CONN;  /* switch to TFTP_STATE_CONN state */
	s->filename = filename;    /* specify filename */
	//  s.size = 1024;
	s->opcode = TFTP_WRQ;
	s->block = 0;
	s->ack = 0;
	s->conn->rport = UIP_HTONS(TFTP_PORT); /* connect UDP port */
}

// specify the filesize to be uploaded
void tftp_size(tftp_state_t *s, char *size) {
	s->size = atol(size);
}

// set TFTP server IP
void tftp_set_ip(tftp_state_t *s, char *strr) {
	int ip[4];

	sscanf(strr, "%i.%i.%i.%i", &ip[0], &ip[1], &ip[2], &ip[3]);

	uip_ipaddr(&s->addr, ip[0], ip[1], ip[2], ip[3]);
	s->conn->ripaddr = s->addr;
}

