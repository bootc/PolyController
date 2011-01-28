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
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "wallclock.h"

/**
 * This driver expects a 32.768KHz input on TOSC1, which is used to tick
 * Timer/Counter 2 in asynchronous mode. Since this is an 8-bit counter we set
 * the prescaler to 8, leaving a resolution of 1/4.096ms. This means our
 * overflow vector will be called at 16 Hz (4096/2^^8).
 *
 * To accurately count seconds, we use a 4-bit counter that gets incremented by
 * 1 each time the interrupt vector is called. When this counter rolls over to
 * 0, we increment the seconds counter.
 *
 * The values in the code below are carefully chosen to keep the clock as
 * accurate as possible without having too great a performance impact. Please be
 * careful when changing any of them.
 */

#define F_RTC 32768
#define PRESCALER 8
#define F_TIMER (F_RTC / PRESCALER)
#define F_VECTOR (F_TIMER / 256)

struct wallclock_status {
	uint32_t sec;
	uint8_t frac : 4;
};

static volatile struct wallclock_status status;

ISR(TIMER2_OVF_vect) {
	if (++status.frac == 0) {
		status.sec++;
	}
}

void wallclock_init(void) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// Set the external clock bit
		ASSR = _BV(EXCLK);

		// Now switch on asynchronous mode
		ASSR = _BV(EXCLK) | _BV(AS2);

		// Normal mode
		TCCR2A = 0x00;

		// Set prescaler
#if PRESCALER == 8
		TCCR2B = _BV(CS21);
#else
#error "PRESCALER settings not available"
#endif

		// Reset compare registers
		OCR2A = 0x00;
		OCR2B = 0x00;

		// Reset the counter
		TCNT2 = 0x00;

		// Set up interrupts
		TIFR2 = _BV(TOV2);
		TIMSK2 = _BV(TOIE2);

		// Zero the status struct
		status.sec = 0;
		status.frac = 0;
	}
}

void wallclock_set(wallclock_time_t time) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// Update the status structs
		status.sec = time.sec;
		status.frac = time.frac >> 8;

		// Update the timer register
		TCNT2 = time.frac & 0xff;

		// Reset the prescaler (is this really necessary?)
		GTCCR = _BV(PSRASY);
	}
}

wallclock_time_t wallclock_get() {
	wallclock_time_t time;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// Get the time from the status struct
		time.sec = status.sec;
		time.frac = status.frac << 8;

		// Fill-in the fractional part
		time.frac |= TCNT2;
	}

	return time;
}

uint32_t wallclock_seconds() {
	uint32_t sec;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		sec = status.sec;
	}

	return sec;
}

