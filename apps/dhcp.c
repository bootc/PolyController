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

#include "contiki-net.h"
#include "network.h"
#include "dhcp.h"

#define DHCPC_CLIENT_PORT  68

PROCESS(dhcp_process, "DHCP");

process_event_t dhcp_event;
dhcp_status_t dhcp_status;

PROCESS_THREAD(dhcp_process, ev, data) {
	PROCESS_BEGIN();

	dhcp_event = process_alloc_event();
	dhcp_status.state = NULL;
	dhcp_status.running = 0;
	dhcp_status.configured = 0;

	while (1) {
		PROCESS_WAIT_EVENT();

		if (ev == tcpip_event || ev == PROCESS_EVENT_TIMER) {
			// If DHCP is running, pass events to it
			if (dhcp_status.running) {
				dhcpc_appcall(ev, data);
			}
		}
		else if (ev == net_link_event) {
			if (net_flags.link) {
				// Start the dhcp client
				dhcpc_init(uip_ethaddr.addr, sizeof(uip_ethaddr.addr));

				// Update our internal status
				dhcp_status.state = NULL;
				dhcp_status.running = 1;
				dhcp_status.configured = 0;

				// Post an event
				process_post(PROCESS_BROADCAST, dhcp_event, &dhcp_status);
			}
			else {
				// De-configure if we need to
				if (dhcp_status.configured) {
					dhcpc_unconfigured(dhcp_status.state);
				}
	
				// bit of a hack to clean up the connection table
				for (uint8_t i = 0; i < UIP_UDP_CONNS; i++) {
					if (uip_udp_conns[i].lport ==
						UIP_HTONS(DHCPC_CLIENT_PORT))
					{
						uip_udp_remove(&uip_udp_conns[i]);
						break;
					}
				}

				// Update our internal status
				dhcp_status.state = NULL;
				dhcp_status.running = 0;
				dhcp_status.configured = 0;

				// Post an event
				process_post(PROCESS_BROADCAST, dhcp_event, &dhcp_status);
			}
		}
		else if (ev == PROCESS_EVENT_EXIT) {
			process_exit(&dhcp_process);
			LOADER_UNLOAD();
		}
	}

	PROCESS_END();
}

void dhcpc_configured(const struct dhcpc_state *s) {
	// Configure uIP
	uip_sethostaddr(&s->ipaddr);
	uip_setnetmask(&s->netmask);
	uip_setdraddr(&s->default_router);
	//resolv_conf(&s->dnsaddr);

	// Update our internal status
	dhcp_status.state = s;
	dhcp_status.configured = 1;

	// Post an event
	process_post(PROCESS_BROADCAST, dhcp_event, &dhcp_status);
}

void dhcpc_unconfigured(const struct dhcpc_state *s) {
	// Update our internal status
	dhcp_status.state = s;
	dhcp_status.configured = 0;

	// Post an event
	process_post(PROCESS_BROADCAST, dhcp_event, &dhcp_status);
}

