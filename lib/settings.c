
#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include "settings.h"
#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include "contiki.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

// Defaults to end of EEPROM, minus one page for avrdude erase count
#ifndef SETTINGS_TOP_ADDR
#define SETTINGS_TOP_ADDR	((void *)(E2END - E2PAGESIZE))
#endif

// Defaults to half of EEPROM area
#ifndef SETTINGS_MAX_SIZE
#define SETTINGS_MAX_SIZE	((E2END + 1) / 2)
#endif

typedef struct {
	uint8_t size_extra;
	uint8_t size_low;
	uint8_t size_check;
	settings_key_t key;
} item_header_t;

static uint8_t settings_is_item_valid_(void *item_addr) {
	item_header_t header;

	if (item_addr == NULL) {
		return false;
	}

	//	if((SETTINGS_TOP_ADDR-item_addr)>=SETTINGS_MAX_SIZE-3)
	//		return false;

	eeprom_read_block(
		&header,
		(char *)item_addr + 1 - sizeof(header),
		sizeof(header));

	if (header.size_check != ~header.size_low) {
		return false;
	}

	// TODO: Check length as well

	return true;
}

static settings_key_t settings_get_key_(void *item_addr) {
	item_header_t header;

	eeprom_read_block(
		&header,
		(char *)item_addr + 1 - sizeof(header),
		sizeof(header));

	if (header.size_check != ~header.size_low) {
		return SETTINGS_INVALID_KEY;
	}

	return header.key;
}

static size_t settings_get_value_length_(void *item_addr) {
	item_header_t header;
	size_t ret = 0;

	eeprom_read_block(
		&header,
		(char *)item_addr + 1 - sizeof(header),
		sizeof(header));

	if (header.size_check != ~header.size_low) {
		return ret;
	}

	ret = header.size_low;

	if (ret & (1 << 7)) {
		ret = ((ret & ~(1 << 7)) << 8) | header.size_extra;
	}

	return ret;
}

static void *settings_get_value_addr_(void *item_addr) {
	size_t len = settings_get_value_length_(item_addr);

	if (len > 128) {
		return (char *)item_addr + 1 - sizeof(item_header_t) - len;
	}
	else {
		return (char *)item_addr + 1 - sizeof(item_header_t) + 1 - len;
	}
}

static inline void *settings_next_item_(void *item_addr) {
	return (char *)settings_get_value_addr_(item_addr) - 1;
}

bool settings_check(settings_key_t key, uint8_t index) {
	bool ret = false;
	void *current_item = SETTINGS_TOP_ADDR;

	for (current_item = SETTINGS_TOP_ADDR;
		settings_is_item_valid_(current_item);
		current_item = settings_next_item_(current_item))
	{
		if (settings_get_key_(current_item) == key) {
			if (index-- == 0) {
				return true;
			}
		}
	}

	return ret;
}

settings_status_t settings_get(settings_key_t key, uint8_t index,
	void *value, size_t *value_size)
{
	void *current_item = SETTINGS_TOP_ADDR;

	for (current_item = SETTINGS_TOP_ADDR;
		settings_is_item_valid_(current_item);
		current_item = settings_next_item_(current_item))
	{
		if (settings_get_key_(current_item) == key) {
			if (index-- == 0) {
				// We found it!
				*value_size = MIN(*value_size,
					settings_get_value_length_(current_item));

				eeprom_read_block(
					value,
					settings_get_value_addr_(current_item),
					*value_size);

				return SETTINGS_STATUS_OK;
			}
		}
	}

	return SETTINGS_STATUS_NOT_FOUND;
}

settings_status_t settings_add(settings_key_t key,
	const void *value, size_t value_size)
{
	void *current_item = SETTINGS_TOP_ADDR;
	item_header_t header;

	// Find end of list
	for (current_item = SETTINGS_TOP_ADDR;
		settings_is_item_valid_(current_item);
		current_item = settings_next_item_(current_item));

	if (current_item == NULL) {
		return SETTINGS_STATUS_FAILURE;
	}

	// TODO: size check!

	header.key = key;

	if (value_size < 0x80) {
		// If the value size is less than 128, then
		// we can get away with only using one byte
		// as the size.
		header.size_low = value_size;
	}
	else if (value_size <= SETTINGS_MAX_VALUE_SIZE) {
		// If the value size of larger than 128,
		// then we need to use two bytes. Store
		// the most significant 7 bits in the first
		// size byte (with MSB set) and store the
		// least significant bits in the second
		// byte (with LSB clear)
		header.size_low = (value_size>>7) | 0x80;		
		header.size_extra = value_size & ~0x80;
	}
	else {
		// Value size too big!
		return SETTINGS_STATUS_FAILURE;
	}

	header.size_check = ~header.size_low;

	// Write the header first
	eeprom_update_block(
		&header,
		(char *)current_item + 1 - sizeof(header),
		sizeof(header));

	// Sanity check, remove once confident
	if (settings_get_value_length_(current_item) != value_size) {
		return SETTINGS_STATUS_FAILURE;
	}

	// Now write the data
	eeprom_update_block(
		value,
		settings_get_value_addr_(current_item),
		value_size);

	return SETTINGS_STATUS_OK;
}

settings_status_t settings_set(settings_key_t key,
	const void *value, size_t value_size)
{
	void *current_item = SETTINGS_TOP_ADDR;

	for (current_item = SETTINGS_TOP_ADDR;
		settings_is_item_valid_(current_item);
		current_item = settings_next_item_(current_item))
	{
		if (settings_get_key_(current_item) == key) {
			break;
		}
	}

	if ((current_item == NULL) || !settings_is_item_valid_(current_item)) {
		return settings_add(key, value, value_size);
	}

	if (value_size != settings_get_value_length_(current_item)) {
		// Requires the settings store to be shifted. Currently unimplemented.
		return SETTINGS_STATUS_FAILURE;
	}

	// Now write the data
	eeprom_update_block(
		value,
		settings_get_value_addr_(current_item),
		value_size);

	return SETTINGS_STATUS_OK;
}

settings_status_t settings_delete(settings_key_t key, uint8_t index) {
	// Requires the settings store to be shifted. Currently unimplemented.
	// TODO: Writeme!
	return SETTINGS_STATUS_UNIMPLEMENTED;
}

// FIXME: Should write whole pages at a time to avoid lots of erase cycles
void settings_wipe(void) {
	uint8_t *addr = (uint8_t *)SETTINGS_TOP_ADDR - SETTINGS_MAX_SIZE;

	for (; addr <= (uint8_t *)SETTINGS_TOP_ADDR; addr++) {
		eeprom_write_byte(addr, 0xFF);
		wdt_reset();
	}
}

