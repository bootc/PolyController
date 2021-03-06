/*
 * Copyright (c) 2002, Adam Dunkels.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution. 
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.  
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 *
 * This file is part of the Contiki OS.
 *
 * $Id: webserver-nogui.c,v 1.7 2009-08-12 18:23:37 dak664 Exp $
 *
 */

#include <string.h>
#include <stdio.h>
#include <init.h>

#include "contiki.h"
#include "apps/syslog.h"

#include "http-strings.h"
#include "webserver.h"
#include "httpd.h"

PROCESS(webserver_process, "Webserver");
INIT_PROCESS(webserver_process);

PROCESS_THREAD(webserver_process, ev, data) {
	PROCESS_BEGIN();

	process_start(&tcpip_process, NULL);
	httpd_init();

	while(1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);
		httpd_appcall(data);
	}

	PROCESS_END();
}

void webserver_log_file(uip_ipaddr_t *requester, char *file) {
#if LOG_CONF_ENABLED
	/* Print out IP address of requesting host. */

#if UIP_CONF_IPV6
	char buf[48];
	httpd_sprint_ip6(*requester, buf);
#else
	char buf[20];
	sprintf_P(buf, PSTR("%d.%d.%d.%d"), uip_ipaddr_to_quad(requester));
#endif /* UIP_CONF_IPV6 */

	syslog_P(LOG_LOCAL0 | LOG_INFO, PSTR("%s: %s"), buf, file);
#endif /* LOG_CONF_ENABLED */
}

void webserver_log(char *msg) {
	syslog_P(LOG_LOCAL0 | LOG_INFO, PSTR("%s"), msg);
}

