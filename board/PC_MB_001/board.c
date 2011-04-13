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
#include <avr/eeprom.h>
#include "board.h"

/*
 *	Port A:
 *	 0-7: (O,L)	Diagnostic outputs
 *
 *	Port B:
 *	 0: (I,PU)	<NC>
 *	 1: (O,L)	TP10/AVR_CLKO
 *	 2: (O,H)	!MEM_CS
 *	 3: (I)		!ETH_INT
 *	 4: (O,H)	!ETH_CS
 *	 5:	(O,L)	MOSI
 *	 6: (O,L)	MISO
 *	 7: (O,L)	SCK
 *
 *	Port C:
 *	 0:	(I)		I2C_SCL
 *	 1: (I)		I2C_SDA
 *	 2: (O,L)	OUT_OE
 *	 3: (O,H)	OUT_LATCH
 *	 4: (O,L)	OUT_DATA
 *	 5: (O,L)	OUT_CLOCK
 *	 6: (I,PU)	RTC_CLK
 *	 7: (I,PU)	<NC>
 *
 *	Port D:
 *	 0: (I,PU)	CON_RX
 *	 1: (O,L)	CON_TX
 *	 2: (I,PU)	SER_RX
 *	 3: (O,L)	SER_TX
 *	 4:	(I,PU)	GPIO_1
 *	 5:	(I,PU)	GPIO_2
 *	 6:	(I,PU)	GPIO_3
 *	 7:	(I,PU)	GPIO_4
 */

#if !__AVR_ATmega1284P__
#error Wrong MCU type selected!
#endif

static struct version_info version_info
	__attribute__((used))
	__attribute__((section("_version_info")))
= {
	.major = CONFIG_VERSION_MAJOR,
	.minor = CONFIG_VERSION_MINOR,
	.patch = CONFIG_VERSION_PATCH,
	.str = CONFIG_VERSION,
};

// This gets called to set up the board
void board_init(void) {
	/*
	 * Disable all interrupt sources
	 */
	// External pin interrupts
	EIMSK = 0;
	EIFR = _BV(INTF2) | _BV(INTF1) | _BV(INTF0);
	
	// External pin change interrupts
	PCICR = 0;
	PCIFR = _BV(PCIF3) | _BV(PCIF2) | _BV(PCIF1) | _BV(PCIF0);
	PCMSK3 = 0;
	PCMSK2 = 0;
	PCMSK1 = 0;
	PCMSK0 = 0;

	// Timer/Counter 0
	TIMSK0 = 0;
	TIFR0 = _BV(OCF0B) | _BV(OCF0A) | _BV(TOV0);

	// Timer/Counter 1
	TIMSK1 = 0;
	TIFR1 = _BV(ICF1) | _BV(OCF1B) | _BV(OCF1A) | _BV(TOV1);

	// Timer/Counter 2
	TIMSK2 = 0;
	TIFR2 = _BV(OCF2B) | _BV(OCF2A) | _BV(TOV2);

	// SPI
	SPCR &= ~(_BV(SPIE) | _BV(SPE));
	if (SPSR & _BV(SPIF)) (void)SPDR;

	// USART
	UCSR0A |= _BV(TXC0);
	UCSR0B = 0;
	UCSR1A |= _BV(TXC1);
	UCSR1B = 0;

	// I2C
	TWCR |= _BV(TWINT);
	TWCR &= ~(_BV(TWEN) | _BV(TWIE));

	// Analog Comparator
	ACSR &= ~_BV(ACIE);
	ACSR |= _BV(ACD) | _BV(ACI);

	// ADC
	ADCSRA &= ~(_BV(ADEN) | _BV(ADIE));
	ADCSRA |= _BV(ADIF);

	// SPM
	SPMCSR &= ~_BV(SPMIE);

	/*
	 * Set up IO ports (default states)
	 */
	/* Port A */
	DDRA = 0xff; // all outputs
	PORTA = 0x00; // all pulled low

	/* Port B */
	DDRB =
		_BV(PINB1) |
		_BV(PINB2) |
		_BV(PINB4) |
		_BV(PINB5) |
		_BV(PINB6) |
		_BV(PINB7);
	PORTB =
		_BV(PINB0) |
		_BV(PINB2) |
		_BV(PINB4);

	/* Port C */
	DDRC =
		_BV(PINC2) |
		_BV(PINC3) |
		_BV(PINC4) |
		_BV(PINC5);
	PORTC =
		_BV(PINC3) |
		_BV(PINC6) |
		_BV(PINC7);

	/* Port D */
	DDRD = 0x00;
	PORTD =
		_BV(PIND4) |
		_BV(PIND5) |
		_BV(PIND6) |
		_BV(PIND7);
}

// Read the board info block out of EEPROM
void board_info_read(struct board_info *info) {
	const void *eeptr = (const void *)BOARD_INFO_ADDR;
	eeprom_read_block(info, eeptr, sizeof(*info));
}

// _crc_ccitt_update from util/crc16.h is Kermit CRC, not what we want
uint16_t crc_ccitt_update(uint16_t crc, uint8_t x) {
	uint16_t crc_new = (uint8_t)(crc >> 8) | (crc << 8);
	crc_new ^= x;
	crc_new ^= (unsigned char)(crc_new & 0xff) >> 4;
	crc_new ^= crc_new << 12;
	crc_new ^= (crc_new & 0xff) << 5;
	return crc_new;
}

// Validate the board info block
int board_info_validate(const struct board_info *info) {
	const uint8_t *u8 = (const uint8_t *)info;
	uint16_t crc = 0xffff;

	// Calculate CRC of entire block minus the CRC at the start
	for (int i = sizeof(crc); i < sizeof(*info); i++) {
		crc = crc_ccitt_update(crc, u8[i]);
	}

	return (crc == info->crc) ? 0 : -1;
}

