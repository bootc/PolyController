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

#ifndef SPI_H
#define SPI_H

#include <stdint.h>

void spi_init(void);
void spi_release(void);
inline uint8_t spi_rw(uint8_t out);

inline uint8_t spi_rw(uint8_t out) {
	uint8_t in;

	SPDR = out;
	while( !(SPSR & (1<<SPIF)) ) { ; }
	in = SPDR;

	return in;
}

#endif /* SPI_H */

