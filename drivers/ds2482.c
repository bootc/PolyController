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

#include <string.h>
#include <util/crc16.h>

#include "ds2482.h"
#include "i2c.h"

typedef struct {
	uint8_t		addr;
	int			cfg_1ws : 1;
	int			cfg_spu : 1;
	int			cfg_apu : 1;
} ds2482_status_t;

static ds2482_status_t s;

#define POLL_LIMIT 200

#define CMD_DRST 0xf0
#define CMD_WCFG 0xd2
#define CMD_CHSL 0xc3
#define CMD_SRP  0xe1
#define CMD_1WRS 0xb4
#define CMD_1WWB 0xa5
#define CMD_1WRB 0x96
#define CMD_1WSB 0x87
#define CMD_1WT  0x78

#define STATUS_DIR 0x80
#define STATUS_TSB 0x40
#define STATUS_SBR 0x20
#define STATUS_RST 0x10
#define STATUS_LL  0x08
#define STATUS_SD  0x04
#define STATUS_PPD 0x02
#define STATUS_1WB 0x01

#define CONFIG_1WS 0x08
#define CONFIG_SPU 0x04
#define CONFIG_APU 0x01

#define MODE_STANDARD  0x00
#define MODE_OVERDRIVE 0x01
#define MODE_STRONG    0x02

static int ds2482_write_config(void);
static int ow_search(ow_search_t *s);
static uint8_t ds2482_search_triplet(int search_direction);

/*
 * DS2428 Detect routine that sets the I2C address and then performs a
 * device reset followed by writing the configuration byte to default values:
 *   1-Wire speed (c1WS) = standard (0)
 *   Strong pullup (cSPU) = off (0)
 *   Presence pulse masking (cPPM) = off (0)
 *   Active pullup (cAPU) = on (CONFIG_APU = 0x01)
 *
 * Return:
 *  0 if device was detected and written
 *  -1 on failure
 */
int ds2482_detect(uint8_t addr) {
	int err;

	// set global address
	s.addr = addr;

	// reset the DS2482 on selected address
	err = ds2482_reset();
	if (err) {
		return err;
	}

	// default configuration
	s.cfg_1ws = 0;
	s.cfg_spu = 0;
	s.cfg_apu = CONFIG_DRIVERS_DS2482_APU ? 1 : 0;

	// write the default configuration setup
	err = ds2482_write_config();
	if (err) {
		return err;
	}

	return 0;
}

/*
 * Perform a device reset on the DS2482.
 *
 * Return:
 *  0 if device was reset
 *  -1 on failure
 */
int ds2482_reset() {
	uint8_t status;

	/*
	 * Device Reset
	 *   S AD,0 [A] DRST [A] Sr AD,1 [A] [SS] A\ P
	 *  [] indicates from slave
	 *  SS status byte to read to verify state
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_DRST)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}
	status = i2c_read(0);
	i2c_stop();

	// check for failure due to incorrect read back of status
	if ((status & 0xF7) != 0x10) {
		return -1;
	}

	return 0;
}

/*
 * Write the configuration register in the DS2482. The configuration
 * options are provided in the lower nibble of the provided config byte.
 * The uppper nibble in bitwise inverted when written to the DS2482.
 *
 * Return:
 *  0 if config written and verified
 *  -1 on failure
 */
static int ds2482_write_config() {
	uint8_t config = 0;
	uint8_t read_config;

	/*
	 * Write configuration (Case A)
	 *   S AD,0 [A] WCFG [A] CF [A] Sr AD,1 [A] [CF] A\ P
	 *  [] indicates from slave
	 *  CF configuration byte to write
	 */

	if (s.cfg_1ws) {
		config |= CONFIG_1WS;
	}
	if (s.cfg_spu) {
		config |= CONFIG_SPU;
	}
	if (s.cfg_apu) {
		config |= CONFIG_APU;
	}

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_WCFG)) {
		return -1;
	}
	if (i2c_write(config | (~config << 4))) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}
	read_config = i2c_read(0);
	i2c_stop();

	// check for failure due to incorrect read back
	if (config != read_config) {
		ds2482_reset();
		return -1;
	}

	return 0;
}

/*
 * Select the 1-Wire channel on a DS2482-800.
 *
 * Return:
 *  0 on channel selected
 *  -1 on failure
 */
int ds2482_channel_select(int channel) {
	uint8_t ch, ch_read, check;

	/*
	 * Channel Select (Case A)
	 *   S AD,0 [A] CHSL [A] CC [A] Sr AD,1 [A] [RR] A\ P
	 *  [] indicates from slave
	 *  CC channel value
	 *  RR channel read back
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_CHSL)) {
		return -1;
	}

	switch (channel) {
		default: case 0: ch = 0xF0; ch_read = 0xB8; break;
		case 1: ch = 0xE1; ch_read = 0xB1; break;
		case 2: ch = 0xD2; ch_read = 0xAA; break;
		case 3: ch = 0xC3; ch_read = 0xA3; break;
		case 4: ch = 0xB4; ch_read = 0x9C; break;
		case 5: ch = 0xA5; ch_read = 0x95; break;
		case 6: ch = 0x96; ch_read = 0x8E; break;
		case 7: ch = 0x87; ch_read = 0x87; break;
	}

	if (i2c_write(ch)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}
	check = i2c_read(0);
	i2c_stop();

	// check for failure due to incorrect read back of channel
	if (check != ch_read) {
		return -1;
	}

	return 0;
}

/*
 * Reset all of the devices on the 1-Wire Net and return the result.
 *
 * Return:
 *   1 - presence pulse(s) detected and device(s) reset
 *   0 - no presence pulses detected
 *  -1 - failure
 *  -2 - short detected
 */
int ow_reset(void) {
	uint8_t status;
	int poll_count = 0;

	/*
	 * 1-Wire reset (Case B)
	 *   S AD,0 [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
	 *                                   \--------/
	 *                       Repeat until 1WB bit has changed to 0
	 *  [] indicates from slave
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_1WRS)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = i2c_read(1);
	do {
		status = i2c_read(status & STATUS_1WB);
	}
	while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

	i2c_stop();

	// check for failure due to poll limit reached
	if (poll_count >= POLL_LIMIT) {
		// handle error
		ds2482_reset();
		return -1;
	}

	// check for short condition
	if (status & STATUS_SD) {
		return -2;
	}
	// check for presence detect
	else if (status & STATUS_PPD) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * Send 1 bit of communication to the 1-Wire Net.
 * The parameter 'sendbit' least significant bit is used.
 */
void ow_write_bit(uint8_t sendbit) {
	ow_touch_bit(sendbit);
}

/*
 * Reads 1 bit of communication from the 1-Wire Net and returns the
 * result.
 *
 *
 */
int8_t ow_read_bit(void) {
	return ow_touch_bit(0x01);
}

/*
 * Send 1 bit of communication to the 1-Wire Net and return the
 * result 1 bit read from the 1-Wire Net. The parameter 'sendbit'
 * least significant bit is used and the least significant bit
 * of the result is the return bit.
 */
int8_t ow_touch_bit(uint8_t sendbit) {
	uint8_t status;
	int poll_count = 0;

	/*
	 * 1-Wire bit (Case B)
	 *   S AD,0 [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status] A\ P
	 *                                          \--------/
	 *                           Repeat until 1WB bit has changed to 0
	 *  [] indicates from slave
	 *  BB indicates byte containing bit value in msbit
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_1WSB)) {
		return -1;
	}
	if (i2c_write(sendbit ? 0x80 : 0x00)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = i2c_read(1);
	do {
		status = i2c_read(status & STATUS_1WB);
	}
	while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

	i2c_stop();

	// check for failure due to poll limit reached
	if (poll_count >= POLL_LIMIT) {
		ds2482_reset();
		return -1;
	}

	// return bit state
	if (status & STATUS_SBR) {
		return 1;
	}
	else {
		return 0;
	}
}

/*
 * Send 8 bits of communication to the 1-Wire Net and verify that the
 * 8 bits read from the 1-Wire Net are the same (write operation).
 * The parameter 'sendbyte' least significant 8 bits are used.
 */
int ow_write_byte(uint8_t sendbyte) {
	uint8_t status;
	int poll_count = 0;

	/*
	 * 1-Wire Write Byte (Case B)
	 *   S AD,0 [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status] A\ P
	 *                                          \--------/
	 *                             Repeat until 1WB bit has changed to 0
	 *  [] indicates from slave
	 *  DD data to write
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_1WWB)) {
		return -1;
	}
	if (i2c_write(sendbyte)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = i2c_read(1);
	do {
		status = i2c_read(status & STATUS_1WB);
	}
	while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

	i2c_stop();

	// check for failure due to poll limit reached
	if (poll_count >= POLL_LIMIT) {
		ds2482_reset();
		return -1;
	}

	return 0;
}

/*
 * Send 8 bits of read communication to the 1-Wire Net and return the
 * result 8 bits read from the 1-Wire Net.
 */
int16_t ow_read_byte(void) {
	uint8_t data, status;
	int poll_count = 0;

	/*
	 * 1-Wire Read Bytes (Case C)
	 *   S AD,0 [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] A\
	 *                                   \--------/
	 *                     Repeat until 1WB bit has changed to 0
	 *   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P
	 *
	 *  [] indicates from slave
	 *  DD data read
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_1WRB)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = i2c_read(1);
	do {
		status = i2c_read(status & STATUS_1WB);
	}
	while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

	// check for failure due to poll limit reached
	if (poll_count >= POLL_LIMIT) {
		ds2482_reset();
		return -1;
	}

	if (i2c_rep_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_SRP)) {
		return -1;
	}
	if (i2c_write(0xE1)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}

	data =  i2c_read(0);
	i2c_stop();

	return data;
}

/*
 * The 'ow_block' transfers a block of data to and from the
 * 1-Wire Net. The result is returned in the same buffer.
 */
void ow_block(uint8_t *tran_buf, int tran_len) {
	for (int i = 0; i < tran_len; i++) {
		tran_buf[i] = ow_touch_byte(tran_buf[i]);
	}
}

/*
 * Send 8 bits of communication to the 1-Wire Net and return the
 * result 8 bits read from the 1-Wire Net.
 */
uint8_t ow_touch_byte(uint8_t sendbyte) {
	if (sendbyte == 0xFF) {
		return ow_read_byte();
	}
	else {
		ow_write_byte(sendbyte);
		return sendbyte;
	}
}

/*
 * Find the 'first' devices on the 1-Wire network
 * Return 1  : device found, ROM number in ROM_NO buffer
 *        0 : no device present
 */
int ow_first(ow_search_t *s) {
	// reset the search state
	s->last_discrepancy = 0;
	s->last_device_flag = 0;
	s->last_family_discrepancy = 0;

	return ow_search(s);
}

/*
 * Find the 'next' devices on the 1-Wire network
 * Return 1  : device found, ROM number in ROM_NO buffer
 *        0 : device not found, end of search
 */
int ow_next(ow_search_t *s) {
	// leave the search state alone
	return ow_search(s);
}

/*
 * Verify the device with the ROM number in ROM_NO buffer is present.
 * Return 1  : device verified present
 *        0 : device not present
 */
int ow_verify(ow_addr_t addr) {
	int res;
	ow_search_t s;

	s.last_discrepancy = 64;
	s.last_device_flag = 0;
	memcpy(s.rom_no, addr, sizeof(addr));

	res = ow_search(&s);
	if (res == 1) {
		// check if same device found
		if (memcmp(s.rom_no, addr, sizeof(addr)) != 0) {
			res = 0;
		}
	}

	// return the result of the verify
	return res;
}

/*
 * Setup the search to find the device type 'family' on the next call
 * to OWNext() if it is present.
 */
void ow_target_setup(ow_search_t *s, uint8_t family) {
	memset(s->rom_no, 0, sizeof(s->rom_no));
	s->last_discrepancy = 64;
	s->last_family_discrepancy = 0;
	s->last_device_flag = 0;

	// set the search state to find SearchFamily type devices
	s->rom_no[0] = family;
}

/*
 * Setup the search to skip the current device type on the next call
 * to OWNext().
 */
void ow_family_skip_setup(ow_search_t *s) {
	// set the Last discrepancy to last family discrepancy
	s->last_discrepancy = s->last_family_discrepancy;

	// clear the last family discrpepancy
	s->last_family_discrepancy = 0;

	// check for end of list
	if (s->last_discrepancy == 0) {
		s->last_device_flag = 1;
	}
}

/*
 * The 'ow_search' function does a general search. This function
 * continues from the previous search state. The search state
 * can be reset by using the 'ow_first' function.
 * This function contains one parameter 'alarm_only'.
 * When 'alarm_only' is 1 (1) the find alarm command
 * 0xEC is sent instead of the normal search command 0xF0.
 * Using the find alarm command 0xEC will limit the search to only
 * 1-Wire devices that are in an 'alarm' state.
 *
 * Returns:   1 (1) : when a 1-Wire device was found and its
 *                       Serial Number placed in the global ROM
 *            0 (0): when no new device was found.  Either the
 *                       last search was the last device or there
 *                       are no devices on the 1-Wire Net.
 */
int ow_search(ow_search_t *s) {
//	int ret;
	int id_bit_number;
	int last_zero, rom_byte_number, search_result;
	int id_bit, cmp_id_bit;
	uint8_t rom_byte_mask, search_direction, status;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;
	s->crc = 0;

	// if the last call was not the last one
	if (!s->last_device_flag) {
		// 1-Wire reset
		if (!ow_reset()) {
			// reset the search
			s->last_discrepancy = 0;
			s->last_device_flag = 0;
			s->last_family_discrepancy = 0;
			return 0;
		}

		// issue the search command
		ow_write_byte(0xF0);

		// loop to do the search
		do {
			// if this discrepancy if before the Last Discrepancy
			// on a previous next then pick the same as last time
			if (id_bit_number < s->last_discrepancy) {
				if ((s->rom_no[rom_byte_number] & rom_byte_mask) > 0)
					search_direction = 1;
				else
					search_direction = 0;
			}
			else {
				// if equal to last pick 1, if not then pick 0
				if (id_bit_number == s->last_discrepancy)
					search_direction = 1;
				else
					search_direction = 0;
			}

			// Perform a triple operation on the DS2482 which will perform
			// 2 read bits and 1 write bit
			status = ds2482_search_triplet(search_direction);

			// check bit results in status byte
			id_bit = ((status & STATUS_SBR) == STATUS_SBR);
			cmp_id_bit = ((status & STATUS_TSB) == STATUS_TSB);
			search_direction =
				((status & STATUS_DIR) == STATUS_DIR) ? (uint8_t)1 : (uint8_t)0;

			// check for no devices on 1-Wire
			if ((id_bit) && (cmp_id_bit)) {
				break;
			}
			else {
				if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0)) {
					last_zero = id_bit_number;

					// check for Last discrepancy in family
					if (last_zero < 9) {
						s->last_family_discrepancy = last_zero;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1) {
					s->rom_no[rom_byte_number] |= rom_byte_mask;
				}
				else {
					s->rom_no[rom_byte_number] &= (uint8_t)~rom_byte_mask;
				}

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number
				// and reset mask
				if (rom_byte_mask == 0) {
					// accumulate the CRC
					s->crc = _crc_ibutton_update(s->crc,
						s->rom_no[rom_byte_number]);
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}
		while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!((id_bit_number < 65) || (s->crc != 0))) {
			// search successful so set s->last_discrepancy,s->last_device_flag
			// search_result
			s->last_discrepancy = last_zero;

			// check for last device
			if (s->last_discrepancy == 0)
				s->last_device_flag = 1;

			search_result = 1;
		}
	}

	// if no device found then reset counters so next
	// 'search' will be like a first

	if (!search_result || (s->rom_no[0] == 0)) {
		s->last_discrepancy = 0;
		s->last_device_flag = 0;
		s->last_family_discrepancy = 0;
		search_result = 0;
	}

	return search_result;
}

//--------------------------------------------------------------------------
// Use the DS2482 help command '1-Wire triplet' to perform one bit of a
//1-Wire search.
//This command does two read bits and one write bit. The write bit
// is either the default direction (all device have same bit) or in case of
// a discrepancy, the 'search_direction' parameter is used.
//
// Returns – The DS2482 status byte result from the triplet command
//
uint8_t ds2482_search_triplet(int search_direction) {
	uint8_t status;
	int poll_count = 0;

	// 1-Wire Triplet (Case B)
	//   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
	//                                         \--------/
	//                           Repeat until 1WB bit has changed to 0
	//  [] indicates from slave
	//  SS indicates byte containing search direction bit value in msbit

	i2c_start(s.addr | I2C_WRITE);
	i2c_write(CMD_1WT);
	i2c_write(search_direction ? 0x80 : 0x00);
	i2c_rep_start(s.addr | I2C_READ);

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = i2c_read(1);
	do {
		status = i2c_read(status & STATUS_1WB);
	}
	while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

	i2c_stop();

	// check for failure due to poll limit reached
	if (poll_count >= POLL_LIMIT) {
		// handle error
		// ...
		ds2482_reset();
		return 0;
	}

	// return status byte
	return status;
}

//--------------------------------------------------------------------------
// Set the 1-Wire Net communication speed.
//
// 'new_speed' - new speed defined as
//                MODE_STANDARD   0x00
//                MODE_OVERDRIVE  0x01
//
// Returns:  current 1-Wire Net speed
//
int ow_speed(int new_speed) {
	// set the speed
	if (new_speed == MODE_OVERDRIVE) {
		s.cfg_1ws = 1;
	}
	else {
		s.cfg_1ws = 0;
	}

	// write the new config
	ds2482_write_config();

	return new_speed;
}

//--------------------------------------------------------------------------
// Set the 1-Wire Net line level pullup to normal. The DS2482 only
// allows enabling strong pullup on a bit or byte event. Consequently this
// function only allows the MODE_STANDARD argument. To enable strong pullup
// use ow_write_byte_power or ow_read_bit_power.
//
// 'new_level' - new level defined as
//                MODE_STANDARD     0x00
//
// Returns:  current 1-Wire Net level
//
int ow_level(int new_level) {
	// function only will turn back to non-strong pullup
	if (new_level != MODE_STANDARD) {
		return MODE_STRONG;
	}

	// clear the strong pullup bit in the global config state
	s.cfg_spu = 0;

	// write the new config
	ds2482_write_config();

	return MODE_STANDARD;
}

//--------------------------------------------------------------------------
// Send 1 bit of communication to the 1-Wire Net and verify that the
// response matches the 'applyPowerResponse' bit and apply power delivery
// to the 1-Wire net.  Note that some implementations may apply the power
// first and then turn it off if the response is incorrect.
//
// 'applyPowerResponse' - 1 bit response to check, if correct then start
//                        power delivery
//
// Returns:  1: bit written and response correct, strong pullup now on
//           0: response incorrect
//
int ow_read_bit_power(int applyPowerResponse) {
	uint8_t rdbit;

	// set strong pullup enable
	s.cfg_spu = 1;

	// write the new config
	if (!ds2482_write_config())
		return 0;

	// perform read bit
	rdbit = ow_read_bit();

	// check if response was correct, if not then turn off strong pullup
	if (rdbit != applyPowerResponse) {
		ow_level(MODE_STANDARD);
		return 0;
	}

	return 1;
}

//--------------------------------------------------------------------------
// Send 8 bits of communication to the 1-Wire Net and verify that the
// 8 bits read from the 1-Wire Net are the same (write operation).
// The parameter 'sendbyte' least significant 8 bits are used. After the
// 8 bits are sent change the level of the 1-Wire net.
//
// 'sendbyte' - 8 bits to send (least significant bit)
//
// Returns:  1: bytes written and echo was the same, strong pullup now on
//           0: echo was not the same
//
int ow_write_byte_power(int sendbyte) {
	// set strong pullup enable
	s.cfg_spu = 1;

	// write the new config
	if (!ds2482_write_config())
		return 0;

	// perform write byte
	ow_write_byte(sendbyte);

	return 1;
}

