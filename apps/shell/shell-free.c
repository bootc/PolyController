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

#include <contiki.h>
#include <stdarg.h>
#include "shell-free.h"
#include "shell.h"

#include <stack.h>
#include <stdio.h>
#include <avr/pgmspace.h>

#if !defined(RAMSTART)
#define RAMSTART (0x100)
#endif
#if !defined(RAMSIZE)
#define RAMSIZE (RAMEND - RAMSTART)
#endif

PROCESS(shell_free_process, "free");
SHELL_COMMAND(shell_free_command,
	"free", "free: show memory usage",
	&shell_free_process);
INIT_SHELL_COMMAND(shell_free_command);

#if PROCESS_CONF_STATS
extern process_num_events_t process_maxevents;
#endif

void shell_free_init(void) {
	shell_register_command(&shell_free_command);
}

PROCESS_THREAD(shell_free_process, ev, data) {
	PROCESS_BEGIN();

	const uint16_t stack_size = ((uint16_t)&__stack - (uint16_t)&_end) + 1;
	uint16_t stack_free = StackCount();

	shell_output_P(&shell_free_command,
		PSTR("           total        used        free"));

	shell_output_P(&shell_free_command,
		PSTR("Stack:      %4d        %4d        %4d"),
		stack_size, stack_size - stack_free, stack_free);

	shell_output_P(&shell_free_command,
		PSTR("Heap:       %4d        %4d        %4d"),
		RAMSIZE - stack_size, RAMSIZE - stack_size, 0);

#if PROCESS_CONF_STATS
	shell_output_P(&shell_free_command, PSTR(""));
	shell_output_P(&shell_free_command, PSTR("Max Events: %d"),
		process_maxevents);
#endif

	PROCESS_END();
}

