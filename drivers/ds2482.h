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

#ifndef DS2482_H
#define DS2482_H

/**
 * Driver for DS2482 1-wire master.
 */

#define DS2482_ADDR_00	0x30
#define DS2482_ADDR_01	0x32
#define DS2482_ADDR_10	0x34
#define DS2482_ADDR_11	0x36

#define DS2482_MODE_STANDARD	0x00
#define DS2482_MODE_OVERDRIVE	0x01
#define DS2482_MODE_STRONG		0x02

typedef struct {
	union {
		uint8_t u[8];
		struct {
			uint8_t family;
			uint8_t id[6];
			uint8_t crc8;
		};
	};
} ow_addr_t;

typedef struct {
	ow_addr_t	rom_no;
	uint8_t		last_discrepancy;
	uint8_t		last_family_discrepancy;
	uint8_t		last_device_flag;
	uint8_t		crc;
} ow_search_t;

// DS2482 specific functions

/*
 * DS2428 Detect routine that sets the I2C address and then performs a
 * device reset followed by writing the configuration byte to default values:
 *   1-Wire speed (c1WS) = standard (0)
 *   Strong pullup (cSPU) = off (0)
 *   Presence pulse masking (cPPM) = off (0)
 *   Active pullup (cAPU) = on (CONFIG_APU = 0x01)
 *
 * Return:
 *   0 - device was detected and written
 *  -1 - failure
 */
int ds2482_detect(uint8_t addr);

/*
 * Select the 1-Wire channel on a DS2482-800.
 *
 * Return:
 *   0 - channel selected
 *  -1 - failure
 */
int ds2482_channel_select(int channel);

// General 1-wire bus functions

/*
 * Reset all of the devices on the 1-Wire Net and return the result.
 *
 * Return:
 *   1 - presence pulse(s) detected and device(s) reset
 *   0 - no presence pulses detected
 *  -1 - failure
 *  -2 - short detected
 */
int ow_reset(void);

/*
 * Send 1 bit of communication to the 1-Wire Net.
 *
 * Return:
 *   0 - bit written successfully
 *  -1 - failure
 */
int ow_write_bit(uint8_t bit);

/*
 * Reads 1 bit of communication from the 1-Wire Net and returns the
 * result.
 *
 * Return:
 *   0 - bit value 0 read
 *   1 - bit value 1 read
 *  -1 - failure
 */
int ow_read_bit(void);

/*
 * Send 8 bits of communication to the 1-Wire Net and verify that the
 * 8 bits read from the 1-Wire Net are the same (write operation).
 *
 * Return:
 *   0 - byte written
 *  -1 - failure
 */
int ow_write_byte(uint8_t byte);

/*
 * Send 8 bits of read communication to the 1-Wire Net and return the
 * result 8 bits read from the 1-Wire Net.
 *
 * Return:
 *  0x00:0xff - byte value read
 *  -1        - failure
 */
int ow_read_byte(void);

/*
 * The 'ow_block' transfers a block of data to and from the
 * 1-Wire Net. The result is returned in the same buffer.
 *
 * Return:
 *   0 - block write successful
 *  -1 - failure
 */
int ow_block(uint8_t *buf, int len);

/*
 * Send 1 bit of communication to the 1-Wire Net and return the
 * result 1 bit read from the 1-Wire Net.
 *
 * Return:
 *   0 - bit value 0 read
 *   1 - bit value 1 read
 *  -1 - failure
 */
int ow_touch_bit(uint8_t bit);

/*
 * Send 8 bits of communication to the 1-Wire Net and return the
 * result 8 bits read from the 1-Wire Net.
 *
 * Return:
 *  0x00:0xff - byte value read
 *  -1        - failure
 */
int ow_touch_byte(uint8_t byte);

// 1-wire search functions

/*
 * Verify the presence of a device on the bus.
 *
 * Return:
 *   1 - device found
 *   0 - device not found
 *  -1 - failure
 *  -2 - short detected
 */
int ow_presence(const ow_addr_t *s);

/*
 * Find the 'first' device on the 1-Wire network.
 *
 * Return:
 *   1 - device found
 *   0 - no devices found
 *  -1 - failure
 *  -2 - short detected
 */
int ow_search_first(ow_search_t *s);

/*
 * Find the 'next' devices on the 1-Wire network
 *
 * Return:
 *   1 - device found
 *   0 - no devices found
 *  -1 - failure
 *  -2 - short detected
 */
int ow_search_next(ow_search_t *s);

/*
 * Find the first device in family 'family'.
 *
 * Return:
 *   1 - device found
 *   0 - no devices found
 *  -1 - failure
 *  -2 - short detected
 */
int ow_search_target(ow_search_t *s, uint8_t family);

/*
 * Find the next device skipping the last found family entirely.
 *
 * Return:
 *   1 - device found
 *   0 - no devices found
 *  -1 - failure
 *  -2 - short detected
 */
int ow_search_skip_family(ow_search_t *s);

// Extended 1-wire functions

/*
 * Set the 1-Wire Net communication speed. Use this to enter overdrive mode or
 * to return to standard mode.
 *
 * speed - MODE_STANDARD or MODE_OVERDRIVE
 *
 * Return:
 *  0  - bus speed changed
 *  -1 - failure
 */
int ow_speed(int speed);

/*
 * Set the 1-Wire Net line pullup to normal.
 * 
 * Return:
 *   0 - level checked back to standard pullup
 *  -1 - failure
 */
int ow_level_std(void);

/*
 * Send 1 bit of communication to the 1-Wire Net and verify that the
 * response matches the 'check_response' bit and apply power delivery
 * to the 1-Wire net.  Note that some implementations may apply the power
 * first and then turn it off if the response is incorrect.
 *
 * Return:
 *   1 - response correct; strong pullup enabled
 *   0 - response incorrect; no strong pullup
 *  -1 - failure
 */
int ow_read_bit_power(uint8_t check_response);

/*
 * Send 8 bits of communication to the 1-Wire Net and verify that the
 * 8 bits read from the 1-Wire Net are the same (write operation). After the
 * 8 bits are sent change the level of the 1-Wire net.
 *
 * Return:
 *   0 - write success; strong pullup enabled
 *  -1 - failure
 */
int ow_write_byte_power(uint8_t sendbyte);

#endif /* DS2482_H */
