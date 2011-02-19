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
#include "board.h"

#if F_CPU != 8000000
#error "Funny old clock speed..."
#endif

// This gets called to set up the IO pins
void board_init(void) {
	// Twiddle ports for LEDs
	DDRD = _BV(PIND6) | _BV(PIND7) | _BV(PIND3);
	PORTD = _BV(PIND6) | _BV(PIND7);

	DDRB = _BV(PINB6);
	PORTB = _BV(PINB6);
}

