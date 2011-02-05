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

#include <avr/io.h>
#include "stack.h"

#define MAX(a, b) ((a) > (b)? (a): (b))

/* Found somewhere on the web, credit where credit is due */
/* http://www.avrfreaks.net/index.php?name=PNphpBB2&file=viewtopic&t=52249 */

void StackPaint(void)
	__attribute__ ((naked)) __attribute__ ((section (".init1")));

void StackPaint(void) {
#if 0
	uint8_t *p = &_end;

	while(p <= &__stack)
	{
		*p = STACK_CANARY;
		p++;
	}
#else
	__asm volatile (
			"    ldi r30,lo8(_end)\n"
			"    ldi r31,hi8(_end)\n"
			"    ldi r24,lo8(0xc5)\n" /* STACK_CANARY = 0xc5 */
			"    ldi r25,hi8(__stack)\n"
			"    rjmp .cmp\n"
			".loop:\n"
			"    st Z+,r24\n"
			".cmp:\n"
			"    cpi r30,lo8(__stack)\n"
			"    cpc r31,r25\n"
			"    brlo .loop\n"
			"    breq .loop"::);
#endif
} 

#define STACK_CANARY 0xc5
uint16_t StackCount(void) {
	const uint8_t *p = MAX(&_end, (uint8_t *)__brkval);
	uint16_t c = 0;

	while(*p == STACK_CANARY && p <= &__stack) {
		p++;
		c++;
	}

	return c;
}

