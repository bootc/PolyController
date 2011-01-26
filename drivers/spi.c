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

#include <stdint.h>
#include <avr/io.h>

#include "spi.h"

void spi_init(void) {
	// Set up initial output values / pull-ups
	CONFIG_DRIVERS_SPI_PORT |=
		_BV(CONFIG_DRIVERS_SPI_MISO) |
		_BV(CONFIG_DRIVERS_SPI_SS);
	CONFIG_DRIVERS_SPI_PORT &= ~(
		_BV(CONFIG_DRIVERS_SPI_SCK) |
		_BV(CONFIG_DRIVERS_SPI_MOSI));

	// Set up pin direction
	CONFIG_DRIVERS_SPI_DDR |=
		_BV(CONFIG_DRIVERS_SPI_MOSI) |
		_BV(CONFIG_DRIVERS_SPI_SCK) |
		_BV(CONFIG_DRIVERS_SPI_SS);
	CONFIG_DRIVERS_SPI_DDR &= ~(
		_BV(CONFIG_DRIVERS_SPI_MISO));

	// Initialize the SPI system
	SPCR = _BV(SPE) | _BV(MSTR);
	SPSR = _BV(SPI2X);
}

void spi_release(void) {
	// Disable the SPI system
	SPCR = 0;

	// Release pins
	CONFIG_DRIVERS_SPI_DDR  &= ~(
		_BV(CONFIG_DRIVERS_SPI_MOSI) |
		_BV(CONFIG_DRIVERS_SPI_SCK) |
		_BV(CONFIG_DRIVERS_SPI_SS));
	CONFIG_DRIVERS_SPI_PORT &= ~(
		_BV(CONFIG_DRIVERS_SPI_MISO) |
		_BV(CONFIG_DRIVERS_SPI_SS));
}

