
#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include "settings.h"
#include <avr/eeprom.h>
#include <avr/wdt.h>

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
	settings_key_t key;
	uint16_t size;
	uint8_t check;
} item_header_t;

static uint8_t header_checkbyte(item_header_t *hdr) {
	uint8_t *hdr2 = (uint8_t *)hdr;
	uint8_t bytes = sizeof(*hdr) - 1;
	uint8_t check = 0xb2; // random constant

	while (bytes--) {
		check ^= *hdr2++;
	}

	return check;
}

static inline void item_read_header(void *item, item_header_t *hdr) {
	eeprom_read_block(
		hdr,
		(uint8_t *)item - sizeof(*hdr),
		sizeof(*hdr));
}

static bool settings_is_item_valid_(void *item_addr) {
	item_header_t header;

	if (item_addr == NULL) {
		return false;
	}

	//	if((SETTINGS_TOP_ADDR-item_addr)>=SETTINGS_MAX_SIZE-3)
	//		return false;

	item_read_header(item_addr, &header);

	if (header.check != header_checkbyte(&header)) {
		return false;
	}

	// TODO: Check length as well

	return true;
}

static settings_key_t settings_get_key_(void *item_addr) {
	item_header_t header;

	item_read_header(item_addr, &header);

	if (header.check != header_checkbyte(&header)) {
		return SETTINGS_INVALID_KEY;
	}

	return header.key;
}

static size_t settings_get_value_length_(void *item_addr) {
	item_header_t header;

	item_read_header(item_addr, &header);

	if (header.check != header_checkbyte(&header)) {
		return 0;
	}

	return header.size;
}

static void *settings_get_value_addr_(void *item_addr) {
	size_t len = settings_get_value_length_(item_addr);

	if (!len) {
		return NULL;
	}

	return (uint8_t *)item_addr - (sizeof(item_header_t) + len);
}

static inline void *settings_next_item_(void *item_addr) {
	void *value_addr = settings_get_value_addr_(item_addr);

	if (!value_addr) {
		return NULL;
	}

	return (uint8_t *)value_addr - 1;
}

bool settings_check(settings_key_t key, uint8_t index) {
	void *current_item;

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

	return false;
}

settings_status_t settings_get(settings_key_t key, uint8_t index,
	void *value, size_t *value_size)
{
	void *current_item;

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
	void *current_item;
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
	header.size = value_size;
	header.check = header_checkbyte(&header);

	// Write the header first
	eeprom_write_block(
		&header,
		(char *)current_item - sizeof(header),
		sizeof(header));

	// Sanity check, remove once confident
	size_t checksize = settings_get_value_length_(current_item);
	if (checksize != value_size) {
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
	void *current_item;

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

