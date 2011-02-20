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

#include "sys/clock.h"
#include "sys/etimer.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "drivers/wallclock.h"

static volatile clock_time_t count;
static volatile uint16_t scount;
static volatile uint32_t seconds;
static uint32_t wallclock_delta;

/*
 * CLOCK_SECOND is the number of ticks per second.
 * It is defined through CONF_CLOCK_SECOND in the contiki-conf.h for each
 * platform. The usual AVR default is ~125 ticks per second, counting a
 * prescaler the CPU clock using the 8 bit timer0.
 *
 * As clock_time_t is an unsigned 16 bit data type, intervals up to 524 seconds
 * can be measured with 8 millisecond precision. For longer intervals a 32 bit
 * global is incremented every second.
 */


#define PRESCALER CONFIG_CLOCK_PRESCALER
#define OCRMATCHVAL ((F_CPU * 10 / PRESCALER / CLOCK_SECOND + 5) / 10 - 1)

#if CLOCK_SECOND != 250 || F_CPU != 8000000 || PRESCALER != 256
#error "clock settings need updating"
/*
 * The clock settings are carefully tuned so that there is no
 * computation-induced error. If you need to re-calculate the clock values use
 * the following formula. F_CPU is the CPU frequency in Hz, PRESCALER is one
 * of the permitted values from the AVR datasheet, CLOCK_SECOND is the number
 * of ticks per second and OCRMATCHVAL is what you put into OCRxA.
 *
 *  OCRMATCHVAL = F_CPU / PRESCALER / CLOCK_SECOND - 1
 *
 * Note specifically that all of the numbers must be integers so that there is
 * no error.
 */
#endif
#if ( OCRMATCHVAL > 0xfe )
#error "PRESCALER too small or F_CPU too high"
#endif
#if ( OCRMATCHVAL < 2 )
#error "PRESCALER too large or F_CPU too low"
#endif


ISR(TIMER0_COMPA_vect) {
	// Increment the tick counter
	count++;

	// Increment seconds if we need to
	if (++scount == CLOCK_SECOND) {
		scount = 0;
		seconds++;
	}

	// Call the etimer code if we need to
	if (etimer_pending()) {
		etimer_request_poll();
	}
}

void clock_init(void) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		// CTC mode
		TCCR0A = _BV(WGM01);

		// Set prescaler
#if PRESCALER == 256
		TCCR0B = _BV(CS02);
#else
#error "PRESCALER settings not available"
#endif

		// Set up match, clear counter
		OCR0A = OCRMATCHVAL;
		TCNT0 = 0;

		// Set up interrupts
		TIFR0 = _BV(OCF0A);
		TIMSK0 = _BV(OCIE0A);

		// Zero the clock
		scount = count = 0;
	}
}

clock_time_t clock_time(void) {
	clock_time_t tmp;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		tmp = count;
	}

	return tmp;
}

uint32_t clock_seconds(void) {
	uint32_t tmp;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		tmp = seconds;
	}

	return tmp;
}

void wallclock_init(void) {}

void wallclock_set(const wallclock_time_t * const time) {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		wallclock_delta = time->sec - seconds;
	}
}

void wallclock_get(wallclock_time_t *time) {
	time->sec = wallclock_seconds();
	time->frac = 0;
}

uint32_t wallclock_seconds(void) {
	return clock_seconds() + wallclock_delta;
}

