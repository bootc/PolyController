// Martin Thomas 4/2008

#include <stdint.h>
#include <avr/io.h>
#include <init.h>
#include "port_ext.h"

#define PORT_EXT_PORT       PORTC
#define PORT_EXT_DDR        DDRC
#define PORT_EXT_PIN_RESET  PB2  /* = /RESET*/
#define PORT_EXT_PIN_LATCH  PB3  /* = ST_CP */
#define PORT_EXT_PIN_DIN    PB4  /* = DS    */
#define PORT_EXT_PIN_CLK    PB5  /* = SH_CP */

static uint8_t virtports[] = {
	0x00,
};

static inline void strobe_delay(void) {
	// 1 cycle ca 130ns at 7,3MHz
	asm volatile("nop"::);
}

static inline void pulse_reset(void) {
	PORT_EXT_PORT &= ~(1 << PORT_EXT_PIN_RESET);
	strobe_delay();
	PORT_EXT_PORT |= (1 << PORT_EXT_PIN_RESET);
	strobe_delay();
}

static inline void pulse_latch(void) {
	PORT_EXT_PORT |= (1 << PORT_EXT_PIN_LATCH);
	strobe_delay();
	PORT_EXT_PORT &= ~(1 << PORT_EXT_PIN_LATCH);
	strobe_delay();
}

static inline void pulse_clock(void) {
	PORT_EXT_PORT |= (1 << PORT_EXT_PIN_CLK);
	strobe_delay();
	PORT_EXT_PORT &= ~(1 << PORT_EXT_PIN_CLK);
	strobe_delay();
}

static void shift_out(void) {
	uint8_t i, bit;

	// first bit shifted out is bit7 in virtports[PORT_EXT_OUTPORTS-1]
	// - on BC100 this is for U205 Q7
	// last bit shifted out is bit 0 in virtports[0]
	// - on BC100 this is for U202 Q0
	for (i = sizeof(virtports); i > 0; i--) {
		for (bit = 8; bit > 0; bit--) {
			if (virtports[i - 1] & (1 << (bit - 1))) {
				PORT_EXT_PORT |= (1 << PORT_EXT_PIN_DIN);
			}
			else {
				PORT_EXT_PORT &= ~(1 << PORT_EXT_PIN_DIN);
			}
			pulse_clock();
		}
	}
	pulse_latch();
}

void port_ext_init(void)  {
	PORT_EXT_PORT &= ~(
		(1 << PORT_EXT_PIN_CLK) |
		(1 << PORT_EXT_PIN_DIN) |
		(1 << PORT_EXT_PIN_LATCH) |
		(1 << PORT_EXT_PIN_RESET));
	PORT_EXT_DDR  |= (
		(1 << PORT_EXT_PIN_RESET) |
		(1 << PORT_EXT_PIN_CLK) |
		(1 << PORT_EXT_PIN_DIN) |
		(1 << PORT_EXT_PIN_LATCH));

	// ensure a known state
	pulse_reset();

	// set inititial values
	shift_out();
}

void port_ext_update(void) {
	shift_out();
}

void port_ext_bit_clear(uint8_t port, uint8_t bit) {
	if ((port < sizeof(virtports)) && (bit < 8)) {
		virtports[port] &= ~(1 << bit);
	}
}

void port_ext_bit_set(uint8_t port, uint8_t bit) {
	if ((port < sizeof(virtports)) && (bit < 8)) {
		virtports[port] |= (1 << bit);
	}
}

void port_ext_set(uint8_t port, uint8_t val) {
	if (port < sizeof(virtports)) {
		virtports[port] = val;
	}
}

INIT_DRIVER(port_ext, port_ext_init);

