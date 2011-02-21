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

#include <contiki-net.h>
#include <net/tcpdump.h>
#include <sys/log.h>
#include <init.h>

#include "network.h"

#if CONFIG_DRIVERS_ENC28J60
#include "drivers/enc28j60.h"
#elif CONFIG_DRIVERS_ENC424J600
#include "drivers/enc424j600.h"
#else
#error No network interface defined!
#endif

#include <avr/pgmspace.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

#if CONFIG_APPS_DHCP
#include "apps/dhcp.h"
#endif
#if CONFIG_APPS_SYSLOG
#include "apps/syslog.h"
#endif

#define TCPDUMP 0
#define TCPDUMP_RAWPKT 0
#define BUF ((struct uip_eth_hdr *)&uip_buf[0])
#define IPBUF ((struct uip_tcpip_hdr *)&uip_buf[UIP_LLH_LEN])

process_event_t net_link_event;
network_flags_t net_flags;

#if CONFIG_DRIVERS_ENC28J60
static struct uip_eth_addr mac PROGMEM =
	{{ 0x52, 0x54, 0x00, 0x01, 0x02, 0x03 }};
#endif

#if !CONFIG_LIB_CONTIKI_IPV6
static struct timer arp_timer;
#endif

PROCESS(network_process, "Network");
INIT_PROCESS(network_process);
INIT_PROCESS(tcpip_process);

#if TCPDUMP
static void tcpdump(uint8_t *pkt, uint16_t len) {
	char prt[41];

	if (((struct uip_eth_hdr *)pkt)->type == uip_htons(UIP_ETHTYPE_IP)) {
		tcpdump_format(pkt + 14, len - 14, prt, sizeof(prt));
		printf_P(PSTR("%s\n"), prt);
	}
	else if (((struct uip_eth_hdr *)pkt)->type == uip_htons(UIP_ETHTYPE_ARP)) {
		printf_P(PSTR("ARP\n"));
	}
	else {
		printf_P(PSTR("Ethertype: %04x\n"),
			uip_ntohs(((struct uip_eth_hdr *)pkt)->type));
	}

#if TCPDUMP_RAWPKT
	for (uint8_t i = 0; i < 64; i++) {
		if (i >= len) {
			printf_P(PSTR("**"));
			break;
		}
		printf("%02x ", *(pkt + i));
		if ((i > 0) && !((i + 1) % 4)) {
			printf_P(PSTR(" "));
		}
	}
	printf("\n");
#endif
}
#endif

static uint16_t network_read(void) {
	uint16_t len;

#if CONFIG_DRIVERS_ENC28J60
	len = enc28j60PacketReceive(UIP_BUFSIZE, (uint8_t *)uip_buf);
#endif
#if CONFIG_DRIVERS_ENC424J600
	len = enc424j600PacketReceive(UIP_BUFSIZE, (uint8_t *)uip_buf);
#endif

#if TCPDUMP
	if (len > 0) {
		printf_P(PSTR("IN:  "));
		tcpdump(uip_buf, len);
	}
#endif

	return len;
}

static uint8_t network_send(void) {
#if TCPDUMP
	printf_P(PSTR("OUT: "));
	tcpdump(uip_buf, uip_len);
#endif

#if CONFIG_DRIVERS_ENC28J60
	if (uip_len <= UIP_LLH_LEN + 40) {
		enc28j60PacketSend(
			uip_len,
			(uint8_t *)uip_buf,
			0,
			0);
	}
	else {
		enc28j60PacketSend(
			54,
			(uint8_t *)uip_buf,
			uip_len - UIP_LLH_LEN - 40,
			(uint8_t*)uip_appdata);
	}
#endif
#if CONFIG_DRIVERS_ENC424J600
	enc424j600PacketSend(uip_len, (uint8_t *)uip_buf);
#endif

	uip_len = 0;

	return 0;
}

#if CONFIG_LIB_CONTIKI_IPV6
static uint8_t network_send_tcpip(uip_lladdr_t *lladdr) {
	if (lladdr == NULL) {
		(&BUF->dest)->addr[0] = 0x33;
		(&BUF->dest)->addr[1] = 0x33;
		(&BUF->dest)->addr[2] = IPBUF->destipaddr.u8[12];
		(&BUF->dest)->addr[3] = IPBUF->destipaddr.u8[13];
		(&BUF->dest)->addr[4] = IPBUF->destipaddr.u8[14];
		(&BUF->dest)->addr[5] = IPBUF->destipaddr.u8[15];
	}
	else {
		memcpy(&BUF->dest, lladdr, UIP_LLADDR_LEN);
	}
	memcpy(&BUF->src, &uip_lladdr, UIP_LLADDR_LEN);
	BUF->type = UIP_HTONS(UIP_ETHTYPE_IPV6);
	uip_len += sizeof(struct uip_eth_hdr);

	network_send();

	return 0;
}
#else
static uint8_t network_send_tcpip(void) {
	uip_arp_out();

	return network_send();
}
#endif

static void network_init(void) {
	net_link_event = process_alloc_event();

#if CONFIG_DRIVERS_ENC28J60
	// Set our MAC address
	struct uip_eth_addr local_mac;
	memcpy_P(&local_mac, &mac, sizeof(mac));
	uip_setethaddr(local_mac);

	// Set up ethernet
	enc28j60Init(&local_mac);
	enc28j60Write(ECOCON, 0 & 0x7); // Disable clock output
	_delay_ms(10);

	/* Magjack leds configuration, see enc28j60 datasheet, page 11 */
	// LEDA=green LEDB=yellow
	//
	// 0x476 is PHLCON LEDA=links status, LEDB=receive/transmit
	enc28j60PhyWrite(PHLCON, 0x476);
	_delay_ms(100);
#endif
#if CONFIG_DRIVERS_ENC424J600
	// Initialise the hardware
	enc424j600Init();

	// Disable clock output and set up LED stretch
	uint16_t econ2 = enc424j600ReadReg(ECON2);
	econ2 |= ECON2_STRCH; // stretch LED duration
	econ2 &= ~(ECON2_COCON3 | ECON2_COCON2 | ECON2_COCON1 | ECON2_COCON0);
	enc424j600WriteReg(ECON2, econ2);

	// Set up LEDs
	uint16_t eidled = enc424j600ReadReg(EIDLED);
	eidled &= 0x00ff; // and-out the high byte (LED config)
	eidled |= EIDLED_LACFG1 | EIDLED_LBCFG2 | EIDLED_LBCFG1;
	enc424j600WriteReg(EIDLED, eidled);
#endif

#if !CONFIG_LIB_CONTIKI_IPV6
	// Set up timers
	timer_set(&arp_timer, CLOCK_SECOND * 10);
#endif
}

static void update_status(void) {
	network_flags_t new = net_flags;

#if CONFIG_DRIVERS_ENC28J60
	uint16_t phstat1 = enc28j60PhyRead(PHSTAT1);
	uint16_t phstat2 = enc28j60PhyRead(PHSTAT2);

	new.link = (phstat1 & PHSTAT1_LLSTAT) ? 1 : 0;
	new.speed_100m = 0; // this chip only does 10M
	new.full_duplex = (phstat2 & PHSTAT2_DPXSTAT) ? 1 : 0;

#endif
#if CONFIG_DRIVERS_ENC424J600
	uint16_t phstat1 = enc424j600ReadPHYReg(PHSTAT1);
	uint16_t phstat3 = enc424j600ReadPHYReg(PHSTAT3);

	new.link = (phstat1 & PHSTAT1_LLSTAT) ? 1 : 0;
	new.speed_100m = (phstat3 & PHSTAT3_SPDDPX1) ? 1 : 0;
	new.full_duplex = (phstat3 & PHSTAT3_SPDDPX2) ? 1 : 0;
#endif

	if (!new.link) {
		new.configured = 0;
	}

	// Check if the flags have changed
	if (memcmp(&new, &net_flags, sizeof(net_flags)) != 0) {
		net_flags = new;

		// Send link change event
		process_post(PROCESS_BROADCAST, net_link_event, &net_flags);
	}
}

static void pollhandler(void) {
	process_poll(&network_process);

	update_status();
	uip_len = network_read();

	if (uip_len > 0) {
#if CONFIG_LIB_CONTIKI_IPV6
		// Handle IP packets
		if (BUF->type == UIP_HTONS(UIP_ETHTYPE_IPV6)) {
			tcpip_input();
		}
#else
		// Handle IP packets
		if (BUF->type == UIP_HTONS(UIP_ETHTYPE_IP)) {
			tcpip_input();
		}
		// Handle ARP packets
		else if (BUF->type == UIP_HTONS(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			if (uip_len > 0) {
				network_send();
			}
		}
#endif
		else {
			uip_len = 0;
		}
	}
#if !CONFIG_LIB_CONTIKI_IPV6
	else if (timer_expired(&arp_timer)) {
		timer_reset(&arp_timer);
		uip_arp_timer();
	}
#endif
}

PROCESS_THREAD(network_process, ev, data) {
	PROCESS_POLLHANDLER(pollhandler());
	PROCESS_BEGIN();

	network_init();
	tcpip_set_outputfunc(network_send_tcpip);
	process_poll(&network_process);

	while (1) {
		PROCESS_WAIT_EVENT();

#if CONFIG_APPS_SYSLOG
		if (ev == net_link_event) {
			if (net_flags.link) {
				syslog_P(
					LOG_KERN | LOG_NOTICE,
					PSTR("Link UP, %S-%S, %Sconfigured"),
					net_flags.speed_100m ? PSTR("100M") : PSTR("10M"),
					net_flags.full_duplex ? PSTR("FDX") : PSTR("HDX"),
					net_flags.configured ? PSTR("") : PSTR("not "));
			}
			else {
				syslog_P(
					LOG_KERN | LOG_NOTICE,
					PSTR("NET: Link DOWN"));
			}
		}
		else
#endif
#if CONFIG_APPS_DHCP
		if (ev == dhcp_event) {
			if ((dhcp_status.configured && !net_flags.configured) ||
				(!dhcp_status.configured && net_flags.configured))
			{
				net_flags.configured = dhcp_status.configured;
				process_post(PROCESS_BROADCAST, net_link_event, &net_flags);
			}
		}
		else
#endif
		if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&network_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

void uip_log(char *msg) {
#if CONFIG_APPS_SYSLOG
	syslog_P(
		LOG_KERN | LOG_NOTICE,
		PSTR("%s"), msg);
#else
	printf_P(PSTR("%s\n"), msg);
#endif
}

