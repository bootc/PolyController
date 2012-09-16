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

#ifndef __CONTIKI_CONF_H__
#define __CONTIKI_CONF_H__

#define CC_CONF_REGISTER_ARGS          1
#define CC_CONF_FUNCTION_POINTER_ARGS  1

#define CCIF
#define CLIF

#define HAVE_STDINT_H
#include "avrdef.h"

#define AUTOSTART_ENABLE 0
#define LOG_CONF_ENABLED 1
#define CLOCK_CONF_SECOND CONFIG_LIB_CONTIKI_SECOND

#define SERIAL_LINE_CONF_BUFSIZE 64

#define CFS_CONF_OFFSET_TYPE uint32_t

//#define PROCESS_CONF_NO_PROCESS_NAMES 1
#define PROCESS_CONF_STATS			1
#define PROCESS_CONF_NUMEVENTS		16

#define UIP_CONF_UDP				1
#define UIP_CONF_UDP_CHECKSUMS		1
#define UIP_CONF_UDP_CONNS			6

#define UIP_CONF_TCP				1
#define UIP_CONF_ACTIVE_OPEN		1
#define UIP_CONF_MAX_CONNECTIONS	15
#define UIP_CONF_MAX_LISTENPORTS	5
#define UIP_CONF_TCP_SPLIT			1

#define UIP_CONF_BUFFER_SIZE		1280
#define UIP_CONF_STATISTICS			1
#define UIP_CONF_LOGGING			0
#define UIP_CONF_BROADCAST			1

typedef uint16_t uip_stats_t;
typedef uint16_t clock_time_t;

void clock_delay(unsigned int us2);
void clock_set_seconds(uint32_t s);
uint32_t clock_seconds(void);

#endif /* __CONTIKI_CONF_H__ */
