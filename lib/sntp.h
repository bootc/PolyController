/***************************************************************

Date: November 6th, 2007.
Copyright (c) 2007 Cyan Technology Limited. All Rights Reserved.

Cyan Technology Limited, hereby grants a non-exclusive license
under Cyan Technology Limited's copyright to copy, modify and
distribute this software for any purpose and without fee,
provided that the above copyright notice and the following
paragraphs appear on all copies.

Cyan Technology Limited makes no representation that this
source code is correct or is an accurate representation of any
standard.

In no event shall Cyan Technology Limited have any liability
for any direct, indirect, or speculative damages, (including,
without limiting the forgoing, consequential, incidental and
special damages) including, but not limited to infringement,
loss of use, business interruptions, and loss of profits,
respective of whether Cyan Technology Limited has advance
notice of the possibility of any such damages.

Cyan Technology Limited specifically disclaims any warranties 
including, but not limited to, the implied warranties of
merchantability, fitness for a particular purpose, and non-
infringement. The software provided hereunder is on an "as is"
basis and Cyan Technology Limited has no obligation to provide
maintenance, support, updates, enhancements, or modifications.

****************************************************************/

/*=============================================================================
Cyan Technology Limited

FILE - sntp.h

DESCRIPTION
    Include file for the SNTP client on the uIP 1.0 TCP/IP Stack.

MODIFICATION DETAILS
=============================================================================*/

#ifndef __SNTP_H__
#define __SNTP_H__

#define SNTP_PORT	123

// timeout time
#define UIP_SNTP_TIMEOUT 10

// number of retries
#define UIP_SNTP_RETRIES 10

/*
   SNTP message structure
   refer RFC-1305 for detail
   */
struct sntp_hdr
{
	u8_t Mode:3;
	u8_t VN:3;
	u8_t LI:2;
	u8_t Stratum;
	u8_t Poll;
	u8_t Precision;
	u32_t RootDelay;
	u32_t RootDispersion;
	u32_t RefID;
	u32_t RefTimestamp[2];
	u32_t OrgTimestamp[2];
	u32_t RxTimestamp[2];
	u32_t TxTimestamp[2];
#if 0
	u32_t KeyID;			// optional
	u16_t MsgDigest[8];		// optional
#endif
};


void sntp_sync(uip_ipaddr_t ipaddr);
void sntp_appcall(process_event_t ev, void *data);
void sntp_synced(const struct sntp_hdr *message);

extern uint32_t sntp_seconds(void);

#endif


