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

#ifndef INIT_H
#define INIT_H

#include <avr/pgmspace.h>

typedef int (*init_fn)(void);

struct init_entry {
	init_fn fn;
	PGM_P name;
};

#define _INIT_FN(_type, _name) \
	static int _init_##_type##_##_name##_fn(void); \
	static const char _init_##_type##_##_name##_str[] PROGMEM = #_name; \
	static const struct init_entry _init_##_type##_##_name \
		__attribute__((used)) __attribute__((section("_init_" #_type))) \
	= { \
		.fn = _init_##_type##_##_name##_fn, \
		.name = _init_##_type##_##_name##_str, \
	}; \
	static int _init_##_type##_##_name##_fn(void)

#define _INIT_FP(_type, _name, _initfunc) \
	static const char _init_##_type##_##_name##_str[] PROGMEM = #_name; \
	static const struct init_entry _init_##_type##_##_name \
		__attribute__((used)) __attribute__((section("_init_" #_type))) \
	= { \
		.fn = _initfunc, \
		.name = _init_##_type##_##_name##_str, \
	}

#define INIT_DRIVER(_name, _initfunc) \
	_INIT_FP(drivers, _name, _initfunc)

#define INIT_LIBRARY(_name, _initfunc) \
	_INIT_FP(libraries, _name, _initfunc)

#define INIT_PROCESS(_process) \
	static const struct process *_init_process_##_process \
		__attribute__((used)) \
		__attribute__((section("_init_processes"))) \
		= &_process;

#define INIT_COMPONENT(_name) \
	_INIT_FN(components, _name)

void init_doinit(void);

#endif // INIT_H
