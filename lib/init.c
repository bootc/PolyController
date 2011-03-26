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

#if CONFIG_LIB_CONTIKI
#include <contiki.h>
#endif

#include <avr/pgmspace.h>
#include "init.h"

#if !CONFIG_IMAGE_BOOTLOADER
#include <stdio.h>
#endif

// Linker symbols
extern struct init_entry *__init_drivers_start;
extern struct init_entry *__init_drivers_end;
extern struct init_entry *__init_libraries_start;
extern struct init_entry *__init_libraries_end;
#if CONFIG_LIB_CONTIKI
extern struct process *__init_processes_start;
extern struct process *__init_processes_end;
#endif
extern struct init_entry *__init_components_start;
extern struct init_entry *__init_components_end;

#if __AVR_LIBC_VERSION__ < 10700UL
static inline void *memcpy_PF(void *dest, uint_farptr_t src, size_t len) {
	uint8_t *ptr = dest;
	while (len--) {
		*ptr++ = pgm_read_byte_far(src++);
	}
	return dest;
}

#define pgm_get_far_address(var)                          \
({                                                    \
	uint_farptr_t tmp;                                \
                                                      \
	__asm__ __volatile__(                             \
                                                      \
			"ldi	%A0, lo8(%1)"           "\n\t"    \
			"ldi	%B0, hi8(%1)"           "\n\t"    \
			"ldi	%C0, hh8(%1)"           "\n\t"    \
			"clr	%D0"                    "\n\t"    \
		:                                             \
			"=d" (tmp)                                \
		:                                             \
			"p"  (&(var))                             \
	);                                                \
	tmp;                                              \
})
#endif

static void init_call_funcs(uint_farptr_t start, uint_farptr_t end) {
	while (start < end) {
		struct init_entry ent;
		memcpy_PF(&ent, start, sizeof(ent));

#if CONFIG_IMAGE_BOOTLOADER
		ent.fn();
#else
		printf_P(PSTR("\rInitialising %S: "), ent.name);

		int err = ent.fn();

		if (err) {
			printf_P(PSTR("FAIL (%d)\n"), err);
		}
		else {
			printf_P(PSTR("OK\n"));
		}
#endif

		start += sizeof(ent);
	}
}

void init_doinit(void) {
	// Initialise all drivers
	init_call_funcs(
		pgm_get_far_address(__init_drivers_start),
		pgm_get_far_address(__init_drivers_end));

	// Initialise all libraries
	init_call_funcs(
		pgm_get_far_address(__init_libraries_start),
		pgm_get_far_address(__init_libraries_end));

#if CONFIG_LIB_CONTIKI
	// Start all processes
	uint_farptr_t pp = pgm_get_far_address(__init_processes_start);
	uint_farptr_t ppe = pgm_get_far_address(__init_processes_end);
	while (pp < ppe) {
		struct process *p = (void *)pgm_read_word(pp);
		printf_P(PSTR("Starting process %S\n"), PROCESS_NAME_STRING(p));
		process_start(p, NULL);
		pp += sizeof(struct process *);
	}
#endif

	// Initialise all components
	init_call_funcs(
		pgm_get_far_address(__init_components_start),
		pgm_get_far_address(__init_components_end));
}

