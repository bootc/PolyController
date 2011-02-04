/*
 * Copyright (c) 2001-2005, Adam Dunkels.
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
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: httpd.h,v 1.6 2009-08-12 18:23:37 dak664 Exp $
 *
 */

#ifndef __HTTPD_H__
#define __HTTPD_H__


#include <contiki-net.h>
#include <cfs/cfs.h>

#ifndef CONFIG_APPS_WEBSERVER_PATHLEN
#define HTTPD_PATHLEN 80
#else /* CONFIG_APPS_WEBSERVER_PATHLEN */
#define HTTPD_PATHLEN CONFIG_APPS_WEBSERVER_PATHLEN
#endif /* CONFIG_APPS_WEBSERVER_PATHLEN */

struct httpd_state {
	struct timer timer;
	struct psock sin;
	struct psock sout;
	struct pt outputpt;
	struct pt scriptpt;
	uint8_t inputbuf[HTTPD_PATHLEN + 30];
	char filename[HTTPD_PATHLEN];
	char script[HTTPD_PATHLEN];
	char state;
	int fd;
	int len;
	cfs_offset_t fpos;
	int savefd;
	cfs_offset_t savefpos;
	struct {
		uint8_t err : 1;
		uint8_t eof : 1;
	} flags;
};


void httpd_init(void);
void httpd_appcall(void *state);

#if UIP_CONF_IPV6
uint8_t httpd_sprint_ip6(uip_ip6addr_t addr, char * result);
#endif /* UIP_CONF_IPV6 */

#endif /* __HTTPD_H__ */

