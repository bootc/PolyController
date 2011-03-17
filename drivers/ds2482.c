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
#include <util/delay.h>

#include "ds2482.h"
#include "i2c.h"

typedef struct {
	uint8_t		addr;
	int			cfg_1ws : 1;
	int			cfg_spu : 1;
	int			cfg_apu : 1;
} ds2482_status_t;

static ds2482_status_t s;

#define POLL_LIMIT	200

#define CMD_DRST	0xf0
#define CMD_WCFG	0xd2
#define CMD_CHSL	0xc3
#define CMD_SRP		0xe1
#define CMD_1WRS	0xb4
#define CMD_1WWB	0xa5
#define CMD_1WRB	0x96
#define CMD_1WSB	0x87
#define CMD_1WT		0x78

#define STATUS_DIR	0x80
#define STATUS_TSB	0x40
#define STATUS_SBR	0x20
#define STATUS_RST	0x10
#define STATUS_LL	0x08
#define STATUS_SD	0x04
#define STATUS_PPD	0x02
#define STATUS_1WB	0x01

#define CONFIG_1WS	0x08
#define CONFIG_SPU	0x04
#define CONFIG_APU	0x01

/*
 * Perform a device reset on the DS2482.
 *
 * Return:
 *   0 - device was reset
 *  -1 - failure
 */
static int ds2482_reset(void);

/*
 * Write the configuration register in the DS2482. The configuration
 * options are provided in the lower nibble of the provided config byte.
 * The uppper nibble in bitwise inverted when written to the DS2482.
 *
 * Return:
 *   0 - config written and verified
 *  -1 - failure
 */
static int ds2482_write_config(void);

/*
 * Use the DS2482 help command '1-Wire triplet' to perform one bit of a
 * 1-Wire search.
 *
 * This command does two read bits and one write bit. The write bit
 * is either the default direction (all device have same bit) or in case of
 * a discrepancy, the 'search_direction' parameter is used.
 *
 * Return:
 *  0x00-0xff - command status byte
 *  -1        - failure
 */
static int ds2482_search_triplet(int search_direction);

/*
 * Send 1 bit of communication to the 1-Wire Net and return the
 * result 1 bit read from the 1-Wire Net.
 *
 * Return:
 *   0 - bit value 0 read
 *   1 - bit value 1 read
 *  -1 - failure
 */
static int ow_touch_bit(uint8_t bit);

/*
 * Send 8 bits of communication to the 1-Wire Net and return the
 * result 8 bits read from the 1-Wire Net.
 *
 * Return:
 *  0x00:0xff - byte value read
 *  -1        - failure
 */
static int ow_touch_byte(uint8_t byte);

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
 * Return:
 *   1 - device found
 *   0 - device not found
 *  -1 - failure
 *  -2 - short detected
 */
static int ow_search(ow_search_t *s);

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

static int ds2482_reset() {
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

static int ds2482_search_triplet(int search_direction) {
	uint8_t status;
	int poll_count = 0;

	/*
	 * 1-Wire Triplet (Case B)
	 *   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
	 *                                         \--------/
	 *                           Repeat until 1WB bit has changed to 0
	 *  [] indicates from slave
	 *  SS indicates byte containing search direction bit value in msbit
	 */

	if (i2c_start(s.addr | I2C_WRITE)) {
		return -1;
	}
	if (i2c_write(CMD_1WT)) {
		return -1;
	}
	if (i2c_write(search_direction ? 0x80 : 0x00)) {
		return -1;
	}
	if (i2c_rep_start(s.addr | I2C_READ)) {
		return -1;
	}

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = i2c_read(1);
	while (status & STATUS_1WB) {
		if (poll_count++ >= POLL_LIMIT) {
			break;
		}

		_delay_us(20);
		status = i2c_read(status & STATUS_1WB);
	}

	// check for failure due to poll limit reached
	if (status & STATUS_1WB) {
		i2c_stop();

		// handle error
		ds2482_reset();
		return -1;
	}

	// return status byte
	return status;
}

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
	while (status & STATUS_1WB) {
		if (poll_count++ >= POLL_LIMIT) {
			break;
		}

		_delay_us(20);
		status = i2c_read(status & STATUS_1WB);
	}

	// check for failure due to poll limit reached
	if (status & STATUS_1WB) {
		i2c_stop();

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

int ow_write_bit(uint8_t sendbit) {
	int ret = ow_touch_bit(sendbit);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int ow_read_bit(void) {
	return ow_touch_bit(0x01);
}

static int ow_touch_bit(uint8_t sendbit) {
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
	while (status & STATUS_1WB) {
		if (poll_count++ >= POLL_LIMIT) {
			break;
		}

		_delay_us(20);
		status = i2c_read(status & STATUS_1WB);
	}

	// check for failure due to poll limit reached
	if (status & STATUS_1WB) {
		i2c_stop();

		// handle error
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
	while (status & STATUS_1WB) {
		if (poll_count++ >= POLL_LIMIT) {
			break;
		}

		_delay_us(20);
		status = i2c_read(status & STATUS_1WB);
	}

	// check for failure due to poll limit reached
	if (status & STATUS_1WB) {
		i2c_stop();

		// handle error
		ds2482_reset();
		return -1;
	}

	return 0;
}

int ow_read_byte(void) {
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
	while (status & STATUS_1WB) {
		if (poll_count++ >= POLL_LIMIT) {
			break;
		}

		_delay_us(20);
		status = i2c_read(status & STATUS_1WB);
	}

	// check for failure due to poll limit reached
	if (status & STATUS_1WB) {
		i2c_stop();

		// handle error
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

int ow_block(uint8_t *tran_buf, int tran_len) {
	for (int i = 0; i < tran_len; i++) {
		int ret = ow_touch_byte(tran_buf[i]);
		if (ret < 0) {
			return ret;
		}
		tran_buf[i] = ret;
	}

	return 0;
}

static int ow_touch_byte(uint8_t sendbyte) {
	if (sendbyte == 0xFF) {
		return ow_read_byte();
	}
	else {
		int ret = ow_write_byte(sendbyte);
		if (ret < 0) {
			return ret;
		}
		return sendbyte;
	}
}

int ow_presence(ow_addr_t addr) {
	int res;
	ow_search_t se;

	se.last_discrepancy = 64;
	se.last_device_flag = 0;
	memcpy(se.rom_no, addr, sizeof(addr));

	res = ow_search(&se);
	if (res == 1) {
		// check if same device found
		if (memcmp(se.rom_no, addr, sizeof(addr)) != 0) {
			res = 0;
		}
	}

	// return the result of the verify
	return res;
}

int ow_search_first(ow_search_t *se) {
	// reset the search state
	se->last_discrepancy = 0;
	se->last_device_flag = 0;
	se->last_family_discrepancy = 0;

	return ow_search(se);
}

int ow_search_next(ow_search_t *se) {
	// leave the search state alone
	return ow_search(se);
}

int ow_search_target(ow_search_t *se, uint8_t family) {
	memset(se->rom_no, 0, sizeof(se->rom_no));
	se->last_discrepancy = 64;
	se->last_family_discrepancy = 0;
	se->last_device_flag = 0;

	// set the search state to find SearchFamily type devices
	se->rom_no[0] = family;

	return ow_search(se);
}

int ow_search_skip_family(ow_search_t *se) {
	// set the Last discrepancy to last family discrepancy
	se->last_discrepancy = se->last_family_discrepancy;

	// clear the last family discrpepancy
	se->last_family_discrepancy = 0;

	// check for end of list
	if (se->last_discrepancy == 0) {
		se->last_device_flag = 1;
	}

	return ow_search(se);
}

static int ow_search(ow_search_t *se) {
	int ret;
	int id_bit_number;
	int last_zero, rom_byte_number, search_result;
	int id_bit, cmp_id_bit;
	uint8_t rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;
	se->crc = 0;

	// if the last call was not the last one
	if (!se->last_device_flag) {
		// 1-Wire reset
		ret = ow_reset();
		if (ret < 0) {
			return ret;
		}
		else if (ret == 0) {
			// reset the search
			se->last_discrepancy = 0;
			se->last_device_flag = 0;
			se->last_family_discrepancy = 0;
			return 0;
		}

		// issue the search command
		ret = ow_write_byte(0xF0);
		if (ret < 0) {
			return ret;
		}

		// loop to do the search
		do {
			// if this discrepancy if before the Last Discrepancy
			// on a previous next then pick the same as last time
			if (id_bit_number < se->last_discrepancy) {
				if ((se->rom_no[rom_byte_number] & rom_byte_mask) > 0)
					search_direction = 1;
				else
					search_direction = 0;
			}
			else {
				// if equal to last pick 1, if not then pick 0
				if (id_bit_number == se->last_discrepancy)
					search_direction = 1;
				else
					search_direction = 0;
			}

			// Perform a triple operation on the DS2482 which will perform
			// 2 read bits and 1 write bit
			ret = ds2482_search_triplet(search_direction);
			if (ret < 0) {
				return ret;
			}

			// check bit results in status byte
			id_bit = ((ret & STATUS_SBR) == STATUS_SBR);
			cmp_id_bit = ((ret & STATUS_TSB) == STATUS_TSB);
			search_direction = ((ret & STATUS_DIR) == STATUS_DIR) ? 1 : 0;

			// check for no devices on 1-Wire
			if (id_bit && cmp_id_bit) {
				break;
			}
			else {
				if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0)) {
					last_zero = id_bit_number;

					// check for Last discrepancy in family
					if (last_zero < 9) {
						se->last_family_discrepancy = last_zero;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1) {
					se->rom_no[rom_byte_number] |= rom_byte_mask;
				}
				else {
					se->rom_no[rom_byte_number] &= (uint8_t)~rom_byte_mask;
				}

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte
				// rom_byte_number and reset mask
				if (rom_byte_mask == 0) {
					// accumulate the CRC
					se->crc = _crc_ibutton_update(se->crc,
						se->rom_no[rom_byte_number]);
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		}
		while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!((id_bit_number < 65) || (se->crc != 0))) {
			// search successful so set s->last_discrepancy,s->last_device_flag
			// search_result
			se->last_discrepancy = last_zero;

			// check for last device
			if (se->last_discrepancy == 0) {
				se->last_device_flag = 1;
			}

			search_result = 1;
		}
	}

	// if no device found then reset counters so next
	// 'search' will be like a first
	if (!search_result || (se->rom_no[0] == 0)) {
		se->last_discrepancy = 0;
		se->last_device_flag = 0;
		se->last_family_discrepancy = 0;
		search_result = 0;
	}

	return search_result;
}

int ow_speed(int speed) {
	int ret;

	// set the speed
	if (speed == MODE_OVERDRIVE) {
		s.cfg_1ws = 1;
	}
	else {
		s.cfg_1ws = 0;
	}

	// write the new config
	ret = ds2482_write_config();
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int ow_level_std() {
	int ret;

	// clear the strong pullup bit in the global config state
	s.cfg_spu = 0;

	// write the new config
	ret = ds2482_write_config();
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int ow_read_bit_power(uint8_t check_response) {
	int ret;
	uint8_t rdbit;

	// set strong pullup enable
	s.cfg_spu = 1;

	// write the new config
	ret = ds2482_write_config();
	if (ret < 0) {
		return ret;
	}

	// perform read bit
	rdbit = ow_read_bit();

	// check if response was correct, if not then turn off strong pullup
	if (rdbit != check_response) {
		ret = ow_level_std();
		if (ret < 0) {
			return ret;
		}
		return 0;
	}

	return 1;
}

int ow_write_byte_power(uint8_t sendbyte) {
	int ret;

	// set strong pullup enable
	s.cfg_spu = 1;

	// write the new config
	ret = ds2482_write_config();
	if (ret < 0) {
		return ret;
	}

	// perform write byte
	ret = ow_write_byte(sendbyte);

	return ret;
}

