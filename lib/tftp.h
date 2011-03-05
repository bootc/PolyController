/*=============================================================================
  Cyan Technology Limited

  FILE - tpftp.h
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

#ifndef __TFTP_H__
#define __TFTP_H__

#include <sys/stimer.h>

enum tftp_state_num {
	TFTP_STATE_IDLE = 1,	/* idle */
	TFTP_STATE_CONN,		/* connect */
	TFTP_STATE_XFR,			/* transfer */
	TFTP_STATE_CLOSE,		/* close */
	TFTP_STATE_ERR,			/* error */
	TFTP_STATE_TIMEOUT		/* time out */
};

typedef struct tftp_state {
	uip_ipaddr_t addr;			/* tftp server's ip */
	struct uip_udp_conn *conn;
	/* UDP connection */
	uint16_t state;		/* tftp machine state */

	uint16_t opcode;		/* opcode */
	uint16_t block;		/* block # */
	uint16_t ack;			/* acked block */
	uint16_t error_code;	/* error code */
	struct stimer timer;		/* timeout timer */
	uint32_t size;			/* file size */
	char *filename;		/* pointer to filename string */
	char *mode;			/* pointer to mode of transfer */
	const char *errmsg;		/* pointer to error message */

	int (*iofunc)(struct tftp_state *s, uint32_t offset,
		uint16_t size, void *buf);
} tftp_state_t;

/* TFTP status */
#define TFTP_OK		0
#define TFTP_FAIL	1

/* TFTP header */
#define TFTP_IDLE	-1
#define TFTP_RRQ	1
#define TFTP_WRQ	2
#define TFTP_DATA	3
#define TFTP_ACK	4
#define TFTP_ERROR	5
#define TFTP_OACK	6

/* TFTP Error Codes */
#define TFTP_EUNDEF		0		/* Undefined error code */
#define TFTP_ENOTFOUND	1		/* File not found */
#define TFTP_EACCESS	2		/* Access denied */
#define TFTP_ENOSPACE	3		/* Disk full or allocation exceeded */
#define TFTP_EBADOP		4		/* Illegal TFTP operation */
#define TFTP_EBADID		5		/* Unknown transfer ID */
#define TFTP_EEXISTS	6		/* File already exists */
#define TFTP_ENOUSER	7		/* No such user */
#define TFTP_ETIMEOUT	1024	/* Timeout */

/* TFTP port number */
#define TFTP_PORT		69

/* TFTP client TID */
#define TFTP_TID		1997

/* TFTP timeout */
#define TIME_TIMEOUT	20

void tftp_init(tftp_state_t *s);
void tftp_appcall(tftp_state_t *s);

void tftp_get(tftp_state_t *s, char *filename);
void tftp_put(tftp_state_t *s, char *filename);
void tftp_size(tftp_state_t *s, char *size);
void tftp_set_ip(tftp_state_t *s, char *strr);

void send_tftp_rq(tftp_state_t *s);
void send_tftp_ack(tftp_state_t *s);
void send_tftp_data(tftp_state_t *s);
void send_tftp_error(tftp_state_t *s);

#endif /* __TFTP_H__ */
