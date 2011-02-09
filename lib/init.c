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

#include "init.h"

// Linker symbols
extern init_fn __init_drivers_start;
extern init_fn __init_drivers_end;
extern init_fn __init_libraries_start;
extern init_fn __init_libraries_end;
extern struct process *__init_processes_start;
extern struct process *__init_processes_end;
extern init_fn __init_components_start;
extern init_fn __init_components_end;

static void init_call_funcs(init_fn *start, init_fn *end) {
	while (start < end) {
		init_fn fn = (init_fn)pgm_read_word(start++);
		fn();
	}
}

void init_doinit(void) {
	// Initialise all drivers
	init_call_funcs(&__init_drivers_start, &__init_drivers_end);

	// Initialise all libraries
	init_call_funcs(&__init_libraries_start, &__init_libraries_end);

	// Start all processes
	struct process **pp = &__init_processes_start;
	while (pp < &__init_processes_end) {
		struct process *p = (void *)pgm_read_word(pp++);
		process_start(p, NULL);
	}

	// Initialise all components
	init_call_funcs(&__init_components_start, &__init_components_end);
}

