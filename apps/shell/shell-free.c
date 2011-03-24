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

#define MAX(a, b) ((a) > (b)? (a): (b))

PROCESS(shell_free_process, "free");
SHELL_COMMAND(shell_free_command,
	"free", "free: show memory usage",
	&shell_free_process);
INIT_SHELL_COMMAND(shell_free_command);

#if PROCESS_CONF_STATS
extern process_num_events_t process_maxevents;
#endif

/* from stdlib_private.h */
struct __freelist {
	size_t sz;
	struct __freelist *nx;
};

extern char __heap_start;
extern char __heap_end;
extern char *__brkval;		/* first location not yet allocated */
extern struct __freelist *__flp; /* freelist pointer (head of freelist) */

static uint16_t walk_freelist(void) {
	struct __freelist *fp1;
	size_t free = 0;

	for (fp1 = __flp; fp1; fp1 = fp1->nx) {
		free += fp1->sz;
	}

	return free;
}

PROCESS_THREAD(shell_free_process, ev, data) {
	PROCESS_BEGIN();

	// Static memory sections (.data + .bss + .noinit)
	static const uint16_t static_start = RAMSTART;
	static const uint16_t static_end = (uint16_t)&_end - 1;

	// Heap memory (malloc area)
	static const uint16_t heap_start = (uint16_t)&__heap_start;
	const uint16_t heap_end = (uint16_t)MAX(&__heap_start, (char *)__brkval) - 1;
	const uint16_t heap_free = walk_freelist();

	// Stack memory
	const uint16_t stack_start = heap_end + 1;
	static const uint16_t stack_end = (uint16_t)&__stack;
	const uint16_t stack_free = StackCount();

	// Print header
	shell_output_P(&shell_free_command,
		PSTR("           total        used        free\n"));

	// Static memory
	shell_output_P(&shell_free_command,
		PSTR("Static:    %5u           -           -\n"),
		static_end - static_start + 1);

	// Heap memory
	shell_output_P(&shell_free_command,
		PSTR("Heap:      %5u       %5u       %5u\n"),
		heap_end - heap_start + 1,
		heap_end - heap_start + 1 - heap_free,
		heap_free);

	// Stack memory
	shell_output_P(&shell_free_command,
		PSTR("Stack:     %5u       %5u       %5u\n"),
		stack_end - stack_start + 1,
		stack_end - stack_start + 1 - stack_free,
		stack_free);

#if PROCESS_CONF_STATS
	// Process event stats
	shell_output_P(&shell_free_command, PSTR("\n"));
	shell_output_P(&shell_free_command, PSTR("Max Events: %u\n"),
		process_maxevents);
#endif

	PROCESS_END();
}

