/**********************************************************/
/* Serial Bootloader for Atmel megaAVR Controllers        */
/*                                                        */
/* tested with ATmega8, ATmega128 and ATmega168           */
/* should work with other mega's, see code for details    */
/*                                                        */
/* ATmegaBOOT.c                                           */
/*                                                        */
/* 20101214: rejigged to work on the ATmega644 on the     */
/*           AVRBC100 battery charger reference design.   */
/*           This flashes the LEDs nicely. Flashing       */
/*           routine was also rewritten to use avr/boot.h */
/*           calls.                                       */
/* 20090308: integrated Mega changes into main bootloader */
/*           source by D. Mellis                          */
/* 20080930: hacked for Arduino Mega (with the 1280       */
/*           processor, backwards compatible)             */
/*           by D. Cuartielles                            */
/* 20070626: hacked for Arduino Diecimila (which auto-    */
/*           resets when a USB connection is made to it)  */
/*           by D. Mellis                                 */
/* 20060802: hacked for Arduino by D. Cuartielles         */
/*           based on a previous hack by D. Mellis        */
/*           and D. Cuartielles                           */
/*                                                        */
/* Monitor and debug functions were added to the original */
/* code by Dr. Erik Lins, chip45.com. (See below)         */
/*                                                        */
/* Thanks to Karl Pitrich for fixing a bootloader pin     */
/* problem and more informative LED blinking!             */
/*                                                        */
/* For the latest version see:                            */
/* http://www.chip45.com/                                 */
/*                                                        */
/* ------------------------------------------------------ */
/*                                                        */
/* based on stk500boot.c                                  */
/* Copyright (c) 2003, Jason P. Kyle                      */
/* All rights reserved.                                   */
/* see avr1.org for original file and information         */
/*                                                        */
/* This program is free software; you can redistribute it */
/* and/or modify it under the terms of the GNU General    */
/* Public License as published by the Free Software       */
/* Foundation; either version 2 of the License, or        */
/* (at your option) any later version.                    */
/*                                                        */
/* This program is distributed in the hope that it will   */
/* be useful, but WITHOUT ANY WARRANTY; without even the  */
/* implied warranty of MERCHANTABILITY or FITNESS FOR A   */
/* PARTICULAR PURPOSE.  See the GNU General Public        */
/* License for more details.                              */
/*                                                        */
/* You should have received a copy of the GNU General     */
/* Public License along with this program; if not, write  */
/* to the Free Software Foundation, Inc.,                 */
/* 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */
/*                                                        */
/* Licence can be viewed at                               */
/* http://www.fsf.org/licenses/gpl.txt                    */
/*                                                        */
/* Target = Atmel AVR m128,m64,m32,m16,m8,m162,m163,m169, */
/* m8515,m8535. ATmega161 has a very small boot block so  */
/* isn't supported.                                       */
/*                                                        */
/* Tested with m168                                       */
/**********************************************************/

/* $Id$ */

/* some includes */
#include <inttypes.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/boot.h>
#include <util/delay.h>
#include <avr/eeprom.h>

#include "board.h"

#if CONFIG_BOOTLDR_WATCHDOG_MODS
#define WATCHDOG_MODS
#endif

#define BAUD CONFIG_UART0_BAUD

/* 20101214: hacked by Chris Boot */
/* 20060803: hacked by DojoCorp */
/* 20070626: hacked by David A. Mellis  */

/* set the waiting time for the bootloader */
#define MAX_TIME_COUNT (F_CPU >> CONFIG_BOOTLDR_MAX_TIME_SHIFT)

/* 20070707: hacked by David A. Mellis - after this many errors give up and launch application */
#define MAX_ERROR_COUNT CONFIG_BOOTLDR_MAX_ERROR_COUNT

/* SW_MAJOR and MINOR needs to be updated from time to time to avoid warning message from AVR Studio */
/* never allow AVR Studio to do an update !!!! */
#define HW_VER	 0x02
#define SW_MAJOR 0x01
#define SW_MINOR 0x10

/* function prototypes */
void putch(char);
char getch(void);
void getNch(uint8_t);
void byte_response(uint8_t);
void nothing_response(void);
char gethex(void);
void puthex(char);
void flash_led(uint8_t);

/* some variables */
union address_union {
	uint16_t word;
	uint8_t  byte[2];
} address;

union length_union {
	uint16_t word;
	uint8_t  byte[2];
} length;

struct flags_struct {
	unsigned eeprom : 1;
} flags;

uint8_t buff[SPM_PAGESIZE];
uint8_t error_count = 0;
uint8_t sig1, sig2, sig3;

typedef void (*bootloader_jump_type)(void) __ATTR_NORETURN__;
bootloader_jump_type app_start_real = (bootloader_jump_type)0x0000;

void app_start(void) {
#ifdef WATCHDOG_MODS
	// autoreset via watchdog (sneaky!)
	wdt_enable(WDTO_15MS);
	while (1); // 16 ms
#else
	// move interrupt vectors to app section
	uint8_t mcucr = MCUCR;
	MCUCR = mcucr | _BV(IVCE);
	MCUCR = mcucr & ~_BV(IVSEL);

	// start the app
	app_start_real();
#endif
}

void early_init(void) __attribute__((naked)) __attribute__((section(".init3")));
void early_init(void) {
	// Set up IO ports
	board_init();

#ifdef WATCHDOG_MODS
	uint8_t ch = MCUSR;
	MCUSR = 0;

	wdt_disable();

	// Check if the WDT was used to reset, in which case we dont bootload
	// and skip straight to the code. woot.
	if (ch & _BV(WDRF)) {// if it's a watchdog reset...
		// move interrupt vectors to app section
		uint8_t mcucr = MCUCR;
		MCUCR = mcucr | _BV(IVCE);
		MCUCR = mcucr & ~_BV(IVSEL);

		// start the app
		app_start_real();
	}
#endif

	// Move interrupt vectors to bootloader section
	uint8_t mcucr = MCUCR;
	MCUCR = mcucr | _BV(IVCE);
	MCUCR = mcucr | _BV(IVSEL);
}

/* main program starts here */
int main(void) {
	uint8_t ch,ch2;
	uint16_t w;

	/* initialize UART(s) depending on CPU defined */
#include <util/setbaud.h>
	UBRR0H = UBRRH_VALUE;
	UBRR0L = UBRRL_VALUE;
#if USE_2X
	UCSR0A = (1 << U2X0);
#else
	UCSR0A = 0;
#endif
	UCSR0B = (1<<RXEN0) | (1<<TXEN0);
	UCSR0C = (1<<UCSZ00) | (1<<UCSZ01);

	/* Enable internal pull-up resistor on pin D0 (RX), in order
	   to supress line noise that prevents the bootloader from
	   timing out (DAM: 20070509) */
	CONFIG_UART0_DDR &= ~_BV(CONFIG_UART0_PIN_RX);
	CONFIG_UART0_PORT |= _BV(CONFIG_UART0_PIN_RX);

	sig1 = boot_signature_byte_get(0x0000);
	sig2 = boot_signature_byte_get(0x0002);
	sig3 = boot_signature_byte_get(0x0004);

	/* 20050803: by DojoCorp, this is one of the parts provoking the
	   system to stop listening, cancelled from the original */
	//putch('\0');

	/* forever loop */
	for (;;) {

		/* get character from UART */
		ch = getch();

		/* A bunch of if...else if... gives smaller code than switch...case ! */

		/* Hello is anyone home ? */
		if(ch=='0') {
			nothing_response();
		}

		/* Request programmer ID */
		/* Not using PROGMEM string due to boot block in m128 being beyond 64kB boundry  */
		/* Would need to selectively manipulate RAMPZ, and it's only 9 characters anyway so who cares.  */
		else if(ch=='1') {
			if (getch() == ' ') {
				putch(0x14);
				putch('A');
				putch('V');
				putch('R');
				putch(' ');
				putch('I');
				putch('S');
				putch('P');
				putch(0x10);
			} else {
				if (++error_count == MAX_ERROR_COUNT)
					app_start();
			}
		}

		/* AVR ISP/STK500 board commands  DON'T CARE so default nothing_response */
		else if(ch=='@') {
			ch2 = getch();
			if (ch2>0x85) getch();
			nothing_response();
		}

		/* AVR ISP/STK500 board requests */
		else if(ch=='A') {
			ch2 = getch();
			if(ch2==0x80) byte_response(HW_VER);		// Hardware version
			else if(ch2==0x81) byte_response(SW_MAJOR);	// Software major version
			else if(ch2==0x82) byte_response(SW_MINOR);	// Software minor version
			else if(ch2==0x98) byte_response(0x03);		// Unknown but seems to be required by avr studio 3.56
			else byte_response(0x00);				// Covers various unnecessary responses we don't care about
		}

		/* Device Parameters  DON'T CARE, DEVICE IS FIXED  */
		else if(ch=='B') {
			getNch(20);
			nothing_response();
		}

		/* Parallel programming stuff  DON'T CARE  */
		else if(ch=='E') {
			getNch(5);
			nothing_response();
		}

		/* P: Enter programming mode  */
		/* R: Erase device, don't care as we will erase one page at a time anyway.  */
		else if(ch=='P' || ch=='R') {
			nothing_response();
		}

		/* Leave programming mode  */
		else if(ch=='Q') {
			nothing_response();
#ifdef WATCHDOG_MODS
			app_start();
#endif
		}

		/* Set address, little endian. EEPROM in bytes, FLASH in words  */
		/* Perhaps extra address bytes may be added in future to support > 128kB FLASH.  */
		/* This might explain why little endian was used here, big endian used everywhere else.  */
		else if(ch=='U') {
			address.byte[0] = getch();
			address.byte[1] = getch();
			nothing_response();
		}

		/* Universal SPI programming command, disabled.  Would be used for fuses and lock bits.  */
		else if(ch=='V') {
			if (getch() == 0x30) {
				getch();
				ch = getch();
				getch();
				if (ch == 0) {
					byte_response(sig1);
				} else if (ch == 1) {
					byte_response(sig2);
				} else {
					byte_response(sig3);
				}
			} else {
				getNch(3);
				byte_response(0x00);
			}
		}

		/* Write memory, length is big endian and is in bytes  */
		else if(ch=='d') {
			length.byte[1] = getch();
			length.byte[0] = getch();

			if (getch() == 'E') {
				flags.eeprom = 1;
			}
			else {
				flags.eeprom = 0;
			}

			for (w = 0; w < length.word; w++) {
				// Store data in buffer, can't keep up with serial data stream whilst programming pages
				buff[w] = getch();
			}

			if (getch() == ' ') {
				// Write to EEPROM one byte at a time
				if (flags.eeprom) {
					address.word <<= 1;
					for (w=0; w < length.word; w++) {
						eeprom_write_byte((void *)address.word, buff[w]);
						address.word++;
					}
				}
				// Write to FLASH one page at a time
				else {
					uint32_t addr = address.word << 1;

					// Even up an odd number of bytes
					if ((length.byte[0] & 0x01)) {
						length.word++;
					}

					// Disable interrupts
					uint8_t sreg = SREG;
					cli();

					// Erase the memory page
					boot_page_erase_safe(addr);
					boot_spm_busy_wait();

					// Fill the page buffer
					for (w = 0; w < length.word; w += 2) {
						uint16_t word;

						word  = buff[w];
						word += buff[w + 1] << 8;

						boot_page_fill_safe(addr + w, word);
					}

					// Write the page
					boot_page_write_safe(addr);
					boot_spm_busy_wait();

					// Re-enable the RWW section
					boot_rww_enable();

					// Re-enable interrupts
					SREG = sreg;

					address.word = addr >> 1;
				}

				putch(0x14);
				putch(0x10);
			}
			else {
				if (++error_count == MAX_ERROR_COUNT)
					app_start();
			}
		}

		/* Read memory block mode, length is big endian.  */
		else if(ch=='t') {
			length.byte[1] = getch();
			length.byte[0] = getch();

			if (getch() == 'E') flags.eeprom = 1;
			else flags.eeprom = 0;

			uint32_t addr = address.word << 1;

			if (getch() == ' ') {		                // Command terminator
				putch(0x14);
				for (w=0;w < length.word;w++) {		        // Can handle odd and even lengths okay
					if (flags.eeprom) {	                        // Byte access EEPROM read
						putch(eeprom_read_byte((void *)(uint16_t)addr));
						addr++;
					}
					else {
#if __AVR_HAVE_ELPM__
						putch(pgm_read_byte_far(addr));
#else
						putch(pgm_read_byte_near(addr));
#endif
						addr++;
					}
				}
				putch(0x10);
			}

			address.word = addr >> 1;
		}

		/* Get device signature bytes  */
		else if(ch=='u') {
			if (getch() == ' ') {
				putch(0x14);
				putch(sig1);
				putch(sig2);
				putch(sig3);
				putch(0x10);
			} else {
				if (++error_count == MAX_ERROR_COUNT)
					app_start();
			}
		}

		/* Read oscillator calibration byte */
		else if(ch=='v') {
			byte_response(boot_signature_byte_get(0x0001));
		}

		else if (++error_count == MAX_ERROR_COUNT) {
			app_start();
		}
	} /* end of forever loop */

}

char gethexnib(void) {
	char a;
	a = getch(); putch(a);
	if(a >= 'a') {
		return (a - 'a' + 0x0a);
	} else if(a >= '0') {
		return(a - '0');
	}
	return a;
}

char gethex(void) {
	return (gethexnib() << 4) + gethexnib();
}

void puthex(char ch) {
	char ah;

	ah = ch >> 4;
	if(ah >= 0x0a) {
		ah = ah - 0x0a + 'a';
	} else {
		ah += '0';
	}

	ch &= 0x0f;
	if(ch >= 0x0a) {
		ch = ch - 0x0a + 'a';
	} else {
		ch += '0';
	}

	putch(ah);
	putch(ch);
}

void putch(char ch)
{
	while (!(UCSR0A & _BV(UDRE0)));
	UDR0 = ch;
}

char getch(void)
{
	uint32_t count = 0;
	while(!(UCSR0A & _BV(RXC0))){
		/* 20060803 DojoCorp:: Addon coming from the previous Bootloader*/
		/* HACKME:: here is a good place to count times*/
		count++;
		if (count > MAX_TIME_COUNT)
			app_start();
	}
	return UDR0;
}

void getNch(uint8_t count)
{
	while(count--) {
		/* m8,16,32,169,8515,8535,163 */
		/* 20060803 DojoCorp:: Addon coming from the previous Bootloader*/

		getch(); // need to handle time out
	}
}

void byte_response(uint8_t val)
{
	if (getch() == ' ') {
		putch(0x14);
		putch(val);
		putch(0x10);
	} else {
		if (++error_count == MAX_ERROR_COUNT)
			app_start();
	}
}

void nothing_response(void)
{
	if (getch() == ' ') {
		putch(0x14);
		putch(0x10);
	} else {
		if (++error_count == MAX_ERROR_COUNT)
			app_start();
	}
}

/* end of file ATmegaBOOT.c */
