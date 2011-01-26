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

  FILE - sntp.c

  DESCRIPTION
  Main functionality for the SNTP client on the uIP 1.0 TCP/IP Stack.

  MODIFICATION DETAILS
  =============================================================================*/

#include "contiki-net.h"
#include "sntp.h"

#include <string.h>

static struct sntp_state s;

struct sntp_state
{
	struct pt pt;
	struct uip_udp_conn *conn;
	struct etimer timer;
	u16_t retry;
};


/*---------------------------------------------------------------------------*/
/*
   configure with SNTP server for time synchronization

param: ipaddr, IP address of SNTP/NTP server
*/
void sntp_sync(uip_ipaddr_t ipaddr)
{
	// remove connection if it is setup already
	if (s.conn != NULL)
	{
		uip_udp_remove(s.conn);
	}

	PT_INIT(&s.pt);

	// setup new connection
	s.conn = udp_new(&ipaddr, UIP_HTONS(SNTP_PORT), NULL);

	// bind to SNTP port
	udp_bind(s.conn, UIP_HTONS(SNTP_PORT));

	// setup retry counter
	s.retry = UIP_SNTP_RETRIES;
}


/*---------------------------------------------------------------------------*/
/*
   send out SNTP message
   */
static void sntp_update(void)
{
	struct sntp_hdr *hdr = (struct sntp_hdr *)uip_appdata;

	memset(hdr, 0, sizeof(struct sntp_hdr));

	hdr->LI = 0;	// leap indicator: normal
	hdr->VN = 3;	// version number: NTP version 3
	hdr->Mode = 3;	// mode: client

	hdr->TxTimestamp[0] = uip_htonl(sntp_seconds());

	uip_udp_send(sizeof(struct sntp_hdr));
}


PT_THREAD(handle_sntp(process_event_t ev, void *data))
{
	PT_BEGIN(&s.pt);

	while (s.retry) {
		while (ev != tcpip_event) {
			tcpip_poll_udp(s.conn);
			PT_YIELD(&s.pt);
		}

		sntp_update();

		// setup timeout timer
		etimer_set(&s.timer, (UIP_SNTP_TIMEOUT * CLOCK_SECOND));	

		do {
			PT_YIELD(&s.pt);
			if (ev == tcpip_event && uip_newdata()) {
				goto out;
			}
		} while (!etimer_expired(&s.timer));

		s.retry--;
	}

out:
	if (etimer_expired(&s.timer)) {
		sntp_synced(NULL);		
	}
	else {
		sntp_synced((struct sntp_hdr *)uip_appdata);
	}

	uip_udp_remove(s.conn);

	while (1)
	{
		PT_YIELD(&s.pt);
	}

	PT_END(&s.pt);
}


/*---------------------------------------------------------------------------*/
/*
   main UDP function
   */
void sntp_appcall(process_event_t ev, void *data)
{
	if (ev == tcpip_event || ev == PROCESS_EVENT_TIMER) {
		handle_sntp(ev, data);
	}	
}



/*---------------------------------------------------------------------------*/


