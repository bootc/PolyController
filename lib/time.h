#ifndef TIME_H_
#define TIME_H_

typedef int32_t time_t;

struct tm {
	uint8_t tm_sec;           /* Seconds. [0-60] (1 leap second) */
	uint8_t tm_min;           /* Minutes. [0-59] */
	uint8_t tm_hour;          /* Hours.   [0-23] */
	uint8_t tm_mday;          /* Day.     [1-31] */
	uint8_t tm_mon;           /* Month.   [0-11] */
	uint8_t tm_year;          /* Year - 1900.  */
	uint8_t tm_wday;          /* Day of week. [0-6] */
	uint16_t tm_yday : 9;          /* Days in year.[0-365] */
};

size_t strftime(
	char * const s,
	const size_t maxsize,
	const char * const format,
	const struct tm * const t);

struct tm *gmtime(time_t time, struct tm *tm);
time_t mktime(const struct tm * const tmp);
int tm_valid(const struct tm *tm);

#endif

