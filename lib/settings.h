#ifndef __AVR_SETTINGS_H__
#define __AVR_SETTINGS_H__

#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>

typedef uint16_t settings_key_t;

typedef enum {
	SETTINGS_STATUS_OK=0,
	SETTINGS_STATUS_INVALID_ARGUMENT,
	SETTINGS_STATUS_NOT_FOUND,
	SETTINGS_STATUS_OUT_OF_SPACE,
	SETTINGS_STATUS_UNIMPLEMENTED,
	SETTINGS_STATUS_FAILURE,
} settings_status_t;

#define SETTINGS_BASE_NETWORKING	(0x01 << 8)
#define SETTINGS_BASE_NTP			(0x02 << 8)
#define SETTINGS_BASE_SYSLOG		(0x03 << 8)

#define SETTINGS_LAST_INDEX		(0xFF)
#define SETTINGS_INVALID_KEY	(0x00)
#define SETTINGS_MAX_VALUE_SIZE	(0x3FFF)	// 16383 bytes

settings_status_t settings_get(settings_key_t key, uint8_t index,
	void *value, size_t *value_size);

settings_status_t settings_add(settings_key_t key,
	const void *value, size_t value_size);

bool settings_check(settings_key_t key, uint8_t index);

settings_status_t settings_set(settings_key_t key,
	const void *value, size_t value_size);

settings_status_t settings_delete(settings_key_t key, uint8_t index);

void settings_wipe(void);

/*
 * Helper functions
 */

static inline uint8_t settings_get_uint8(settings_key_t key, uint8_t index) {
	uint8_t value = 0;
	size_t size = sizeof(value);
	settings_get(key, index, &value, &size);
	return value;
}

static inline settings_status_t settings_add_uint8(settings_key_t key,
	uint8_t value)
{
	return settings_add(key, &value, sizeof(value));
}

static inline settings_status_t settings_set_uint8(settings_key_t key, 
	uint8_t value)
{
	return settings_set(key, &value, sizeof(value));
}

static inline uint16_t settings_get_uint16(settings_key_t key, uint8_t index) {
	uint16_t value = 0;
	size_t size = sizeof(value);
	settings_get(key, index, &value, &size);
	return value;
}

static inline settings_status_t settings_add_uint16(settings_key_t key,
	uint16_t value)
{
	return settings_add(key, &value, sizeof(value));
}

static inline settings_status_t settings_set_uint16(settings_key_t key,
	uint16_t value)
{
	return settings_set(key, &value, sizeof(value));
}

static inline uint32_t settings_get_uint32(settings_key_t key, uint8_t index) {
	uint32_t value = 0;
	size_t size = sizeof(value);
	settings_get(key, index, &value, &size);
	return value;
}

static inline settings_status_t settings_add_uint32(settings_key_t key,
	uint32_t value)
{
	return settings_add(key, &value, sizeof(value));
}

static inline settings_status_t settings_set_uint32(settings_key_t key,
	uint32_t value)
{
	return settings_set(key, &value, sizeof(value));
}

static inline uint64_t settings_get_uint64(settings_key_t key, uint8_t index) {
	uint64_t value = 0;
	size_t size = sizeof(value);
	settings_get(key, index, &value, &size);
	return value;
}

static inline settings_status_t settings_add_uint64(settings_key_t key,
	uint64_t value)
{
	return settings_add(key, &value, sizeof(value));
}

static inline settings_status_t settings_set_uint64(settings_key_t key,
	uint64_t value)
{
	return settings_set(key, &value, sizeof(value));
}

#endif
