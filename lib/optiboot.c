/*
 * NOTE: This version of optiboot has been heavily modified for use as the
 * rescue loader on the PolyController.
 */

/**********************************************************/
/* Optiboot bootloader for Arduino                        */
/*                                                        */
/* Heavily optimised bootloader that is faster and        */
/* smaller than the Arduino standard bootloader           */
/*                                                        */
/* Enhancements:                                          */
/*   Fits in 512 bytes, saving 1.5K of code space         */
/*   Background page erasing speeds up programming        */
/*   Higher baud rate speeds up programming               */
/*   Written almost entirely in C                         */
/*   Customisable timeout with accurate timeconstant      */
/*                                                        */
/* What you lose:                                         */
/*   Implements a skeleton STK500 protocol which is       */
/*     missing several features including EEPROM          */
/*     programming and non-page-aligned writes            */
/*   High baud rate breaks compatibility with standard    */
/*     Arduino flash settings                             */
/*                                                        */
/* Currently supports:                                    */
/*   ATmega168 based devices (Diecimila etc)              */
/*   ATmega328P based devices (Duemilanove etc)           */
/*                                                        */
/* Does not support:                                      */
/*   ATmega1280 based devices (eg. Mega)                  */
/*                                                        */
/* Assumptions:                                           */
/*   The code makes several assumptions that reduce the   */
/*   code size. They are all true after a hardware reset, */
/*   but may not be true if the bootloader is called by   */
/*   other means or on other hardware.                    */
/*     No interrupts can occur                            */
/*     UART and Timer 1 are set to their reset state      */
/*     SP points to RAMEND                                */
/*                                                        */
/* Code builds on code, libraries and optimisations from: */
/*   stk500boot.c          by Jason P. Kyle               */
/*   Arduino bootloader    http://arduino.cc              */
/*   Spiff's 1K bootloader http://spiffie.org/know/arduino_1k_bootloader/bootloader.shtml */
/*   avr-libc project      http://nongnu.org/avr-libc     */
/*   Adaboot               http://www.ladyada.net/library/arduino/bootloader.html */
/*   AVR305                Atmel Application Note         */
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
/**********************************************************/

#include "optiboot.h"

#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <util/atomic.h>

#include "drivers/uart.h"

/* STK500 constants list, from AVRDUDE */
#define STK_OK              0x10
#define STK_FAILED          0x11  // Not used
#define STK_UNKNOWN         0x12  // Not used
#define STK_NODEVICE        0x13  // Not used
#define STK_INSYNC          0x14  // ' '
#define STK_NOSYNC          0x15  // Not used
#define ADC_CHANNEL_ERROR   0x16  // Not used
#define ADC_MEASURE_OK      0x17  // Not used
#define PWM_CHANNEL_ERROR   0x18  // Not used
#define PWM_ADJUST_OK       0x19  // Not used
#define CRC_EOP             0x20  // 'SPACE'
#define STK_GET_SYNC        0x30  // '0'
#define STK_GET_SIGN_ON     0x31  // '1'
#define STK_SET_PARAMETER   0x40  // '@'
#define STK_GET_PARAMETER   0x41  // 'A'
#define STK_SET_DEVICE      0x42  // 'B'
#define STK_SET_DEVICE_EXT  0x45  // 'E'
#define STK_ENTER_PROGMODE  0x50  // 'P'
#define STK_LEAVE_PROGMODE  0x51  // 'Q'
#define STK_CHIP_ERASE      0x52  // 'R'
#define STK_CHECK_AUTOINC   0x53  // 'S'
#define STK_LOAD_ADDRESS    0x55  // 'U'
#define STK_UNIVERSAL       0x56  // 'V'
#define STK_PROG_FLASH      0x60  // '`'
#define STK_PROG_DATA       0x61  // 'a'
#define STK_PROG_FUSE       0x62  // 'b'
#define STK_PROG_LOCK       0x63  // 'c'
#define STK_PROG_PAGE       0x64  // 'd'
#define STK_PROG_FUSE_EXT   0x65  // 'e'
#define STK_READ_FLASH      0x70  // 'p'
#define STK_READ_DATA       0x71  // 'q'
#define STK_READ_FUSE       0x72  // 'r'
#define STK_READ_LOCK       0x73  // 's'
#define STK_READ_PAGE       0x74  // 't'
#define STK_READ_SIGN       0x75  // 'u'
#define STK_READ_OSCCAL     0x76  // 'v'
#define STK_READ_FUSE_EXT   0x77  // 'w'
#define STK_READ_OSCCAL_EXT 0x78  // 'x'

/* Function Prototypes */
static inline void putch(char);
static inline uint8_t getch(void);
static inline void getNch(uint8_t);
static void verifySpace(void);
static inline uint8_t getLen(void);

#if (FLASHEND > USHRT_MAX)
uint32_t address;
#else
uint16_t address;
#endif

uint8_t buff[SPM_PAGESIZE];
uint8_t length;

/* main program starts here */
void optiboot(void) {
	uint8_t ch;

	/* Forever loop */
	for (;;) {
		/* get character from UART */
		ch = getch();

		if(ch == STK_GET_PARAMETER) {
			// GET PARAMETER returns a generic 0x03 reply - enough to keep Avrdude happy
			getNch(1);
			putch(0x03);
		}
		else if(ch == STK_SET_DEVICE) {
			// SET DEVICE is ignored
			getNch(20);
		}
		else if(ch == STK_SET_DEVICE_EXT) {
			// SET DEVICE EXT is ignored
			getNch(5);
		}
		else if(ch == STK_LOAD_ADDRESS) {
			// LOAD ADDRESS
			address = getch();
			address = (address & 0xff) | (getch() << 8);
			address += address; // Convert from word address to byte address
			verifySpace();
		}
		else if(ch == STK_UNIVERSAL) {
			// UNIVERSAL command is ignored
			getNch(4);
			putch(0x00);
		}
		/* Write memory, length is big endian and is in bytes  */
		else if(ch == STK_PROG_PAGE) {
			// PROGRAM PAGE - we support flash programming only, not EEPROM
			uint8_t *bufPtr;
#if (FLASHEND > USHRT_MAX)
			uint32_t addrPtr;
#else
			uint16_t addrPtr;
#endif

			getLen();

			// Immediately start page erase - this will 4.5ms
			boot_page_erase(address);

			// While that is going on, read in page contents
			bufPtr = buff;
			do *bufPtr++ = getch();
			while (--length);

			// Read command terminator, start reply
			verifySpace();

			// Disable interrupts
			ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
				// If only a partial page is to be programmed, the erase might not be complete.
				// So check that here
				boot_spm_busy_wait();

				// Copy buffer into programming buffer
				bufPtr = buff;
				addrPtr = address;
				ch = SPM_PAGESIZE / 2;
				do {
					uint16_t a;
					a = *bufPtr++;
					a |= (*bufPtr++) << 8;
					boot_page_fill(addrPtr, a);
					addrPtr += 2;
				} while (--ch);

				// Write from programming buffer
				boot_page_write(address);
				boot_spm_busy_wait();

#if defined(RWWSRE)
				// Reenable read access to flash
				boot_rww_enable();
#endif
			}
		}
		/* Read memory block mode, length is big endian.  */
		else if(ch == STK_READ_PAGE) {
			// READ PAGE - we only read flash
			getLen();
			verifySpace();
#if (FLASHEND > USHRT_MAX)
			do putch(pgm_read_byte_far(address++));
#else
			do putch(pgm_read_byte_near(address++));
#endif
			while (--length);
		}

		/* Get device signature bytes  */
		else if(ch == STK_READ_SIGN) {
			// READ SIGN - return what Avrdude wants to hear
			verifySpace();
			putch(SIGNATURE_0);
			putch(SIGNATURE_1);
			putch(SIGNATURE_2);
		}
		else if (ch == 'Q') {
			verifySpace();
			putch(STK_OK);
			return;
		}
		else {
			// This covers the response to commands like STK_ENTER_PROGMODE
			verifySpace();
		}
		putch(STK_OK);
	}
}

void putch(char ch) {
	uart_putc(ch);
}

uint8_t getch(void) {
	uint16_t ch;

	do {
		ch = uart_getc();
	} while (ch & UART_NO_DATA);

	return (uint8_t)ch;
}

void getNch(uint8_t count) {
	do getch(); while (--count);
	verifySpace();
}

void verifySpace() {
	if (getch() == CRC_EOP) {
		putch(STK_INSYNC);
	}
}

uint8_t getLen() {
	getch();
	length = getch();
	return getch();
}

