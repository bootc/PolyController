/*
 ** Copyright (c) 1989 The Regents of the University of California.
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms are permitted
 ** provided that the above copyright notice and this paragraph are
 ** duplicated in all such forms and that any documentation,
 ** advertising materials, and other materials related to such
 ** distribution and use acknowledge that the software was developed
 ** by the University of California, Berkeley. The name of the
 ** University may not be used to endorse or promote products derived
 ** from this software without specific prior written permission.
 ** THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 ** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include "time.h"

#define SECSPERMIN	60
#define MINSPERHOUR	60
#define HOURSPERDAY	24
#define DAYSPERWEEK	7
#define DAYSPERNYEAR	365
#define DAYSPERLYEAR	366
#define MONSPERYEAR	12
#define TM_YEAR_BASE	1900

/*
 ** Since everything in isleap is modulo 400 (or a factor of 400), we know that
 **	isleap(y) == isleap(y % 400)
 ** and so
 **	isleap(a + b) == isleap((a + b) % 400)
 ** or
 **	isleap(a + b) == isleap(a % 400 + b % 400)
 ** This is true even if % means modulo rather than Fortran remainder
 ** (which is allowed by C89 but not C99).
 ** We use this to avoid addition overflow problems.
 */
#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
#define isleap_sum(a, b)	isleap((a) % 400 + (b) % 400)

#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#define TYPE_SIGNED(type) (((type) -1) < 0)

/*
 ** 302 / 1000 is log10(2.0) rounded up.
 ** Subtract one for the sign bit if the type is signed;
 ** add one for integer division truncation;
 ** add one more for a minus sign if the type is signed.
 */
#define INT_STRLEN_MAXIMUM(type) \
	((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302 / 1000 + \
	 1 + TYPE_SIGNED(type))

struct lc_time_T {
	const char *mon[MONSPERYEAR];
	const char *month[MONSPERYEAR];
	const char *wday[DAYSPERWEEK];
	const char *weekday[DAYSPERWEEK];
	const char *X_fmt;
	const char *x_fmt;
	const char *c_fmt;
	const char *am;
	const char *pm;
	const char *date_fmt;
};

#define Locale	(&C_time_locale)

static const struct lc_time_T C_time_locale = {
	{
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	}, {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	}, {
		"Sun", "Mon", "Tue", "Wed",
		"Thu", "Fri", "Sat"
	}, {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	},

	/* X_fmt */
	"%H:%M:%S",

	/*
	 ** x_fmt
	 ** C99 requires this format.
	 ** Using just numbers (as here) makes Quakers happier;
	 ** it's also compatible with SVR4.
	 */
	"%m/%d/%y",

	/*
	 ** c_fmt
	 ** C99 requires this format.
	 ** Previously this code used "%D %X", but we now conform to C99.
	 ** Note that
	 ** "%a %b %d %H:%M:%S %Y"
	 ** is used by Solaris 2.3.
	 */
	"%a %b %e %T %Y",

	/* am */
	"AM",

	/* pm */
	"PM",

	/* date_fmt */
	"%a %b %e %H:%M:%S %Z %Y"
};

static char *_add(const char *, char *, const char *);
static char *_conv(int, PGM_P fmt, char *, const char *);
static char *_fmt(const char *, const struct tm *, char *, const char *);
static char *_yconv(int, int, int, int, char *, const char *);

size_t strftime(char *const s, const size_t maxsize,
	const char *const format, const struct tm *const t)
{
	char *p;

	p = _fmt(((format == NULL) ? "%c" : format), t, s, s + maxsize);

	if (p == s + maxsize)
		return 0;

	*p = '\0';
	return (size_t) (p - s);
}

static char *_fmt(const char *format, const struct tm *const t,
	char *pt, const char *const ptlim)
{
	for (; *format; ++format) {
		if (*format == '%') {
label:;
			char f = *++format;

			if (f == '\0') {
				--format;
			}
			else if (f == 'A') {
				pt = _add((t->tm_wday < 0 ||
							t->tm_wday >= DAYSPERWEEK) ?
						"?" : Locale->weekday[t->tm_wday], pt, ptlim);
				continue;
			}
			else if (f == 'a') {
				pt = _add((t->tm_wday < 0 ||
							t->tm_wday >= DAYSPERWEEK) ?
						"?" : Locale->wday[t->tm_wday], pt, ptlim);
				continue;
			}
			else if (f == 'B') {
				pt = _add((t->tm_mon < 0 ||
							t->tm_mon >= MONSPERYEAR) ?
						"?" : Locale->month[t->tm_mon], pt, ptlim);
				continue;
			}
			else if (f == 'b' || f == 'h') {
				pt = _add((t->tm_mon < 0 ||
							t->tm_mon >= MONSPERYEAR) ?
						"?" : Locale->mon[t->tm_mon], pt, ptlim);
				continue;
			}
			else if (f == 'C') {
				/*
				 ** %C used to do a...
				 ** _fmt("%a %b %e %X %Y", t);
				 ** ...whereas now POSIX 1003.2 calls for
				 ** something completely different.
				 ** (ado, 1993-05-24)
				 */
				pt = _yconv(t->tm_year, TM_YEAR_BASE, 1, 0, pt, ptlim);
				continue;
			}
			else if (f =='c') {
				pt = _fmt(Locale->c_fmt, t, pt, ptlim);
				continue;
			}
			else if (f == 'D') {
				pt = _fmt("%m/%d/%y", t, pt, ptlim);
				continue;
			}
			else if (f == 'd') {
				pt = _conv(t->tm_mday, PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'E' || f == 'O') {
				/*
				 ** C99 locale modifiers.
				 ** The sequences
				 ** %Ec %EC %Ex %EX %Ey %EY
				 ** %Od %oe %OH %OI %Om %OM
				 ** %OS %Ou %OU %OV %Ow %OW %Oy
				 ** are supposed to provide alternate
				 ** representations.
				 */
				goto label;
			}
			else if (f == 'e') {
				pt = _conv(t->tm_mday, PSTR("%2d"), pt, ptlim);
				continue;
			}
			else if (f == 'F') {
				pt = _fmt("%Y-%m-%d", t, pt, ptlim);
				continue;
			}
			else if (f == 'H') {
				pt = _conv(t->tm_hour, PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'I') {
				pt = _conv((t->tm_hour % 12) ?
						(t->tm_hour % 12) : 12, PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'j') {
				pt = _conv(t->tm_yday + 1, PSTR("%03d"), pt, ptlim);
				continue;
			}
			else if (f == 'k') {
				/*
				 ** This used to be...
				 ** _conv(t->tm_hour % 12 ?
				 **     t->tm_hour % 12 : 12, 2, ' ');
				 ** ...and has been changed to the below to
				 ** match SunOS 4.1.1 and Arnold Robbins'
				 ** strftime version 3.0. That is, "%k" and
				 ** "%l" have been swapped.
				 ** (ado, 1993-05-24)
				 */
				pt = _conv(t->tm_hour, PSTR("%2d"), pt, ptlim);
				continue;
			}
			else if (f == 'l') {
				/*
				 ** This used to be...
				 ** _conv(t->tm_hour, 2, ' ');
				 ** ...and has been changed to the below to
				 ** match SunOS 4.1.1 and Arnold Robbin's
				 ** strftime version 3.0. That is, "%k" and
				 ** "%l" have been swapped.
				 ** (ado, 1993-05-24)
				 */
				pt = _conv((t->tm_hour % 12) ?
						(t->tm_hour % 12) : 12, PSTR("%2d"), pt, ptlim);
				continue;
			}
			else if (f == 'M') {
				pt = _conv(t->tm_min, PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'm') {
				pt = _conv(t->tm_mon + 1, PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'n') {
				pt = _add("\n", pt, ptlim);
				continue;
			}
			else if (f == 'p') {
				pt = _add((t->tm_hour >= (HOURSPERDAY / 2)) ?
						Locale->pm : Locale->am, pt, ptlim);
				continue;
			}
			else if (f == 'R') {
				pt = _fmt("%H:%M", t, pt, ptlim);
				continue;
			}
			else if (f == 'r') {
				pt = _fmt("%I:%M:%S %p", t, pt, ptlim);
				continue;
			}
			else if (f == 'S') {
				pt = _conv(t->tm_sec, PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 's') {
				time_t mkt = mktime(t);
				int max = ptlim - pt;
				int len;

				if (TYPE_SIGNED(time_t)) {
					len = snprintf_P(pt, max, PSTR("%ld"), (long)mkt);
				}
				else {
					len = snprintf_P(pt, max, PSTR("%ld"), (unsigned long)mkt);
				}

				if (len > max) {
					pt += max;
				}
				else {
					pt += len;
				}

				continue;
			}
			else if (f == 'T') {
				pt = _fmt("%H:%M:%S", t, pt, ptlim);
				continue;
			}
			else if (f == 't') {
				pt = _add("\t", pt, ptlim);
				continue;
			}
			else if (f == 'U') {
				pt = _conv((t->tm_yday + DAYSPERWEEK - t->tm_wday) /
						DAYSPERWEEK,
						PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'u') {
				/*
				 ** From Arnold Robbins' strftime version 3.0:
				 ** "ISO 8601: Weekday as a decimal number
				 ** [1 (Monday) - 7]"
				 ** (ado, 1993-05-24)
				 */
				pt = _conv((t->tm_wday == 0) ?
						DAYSPERWEEK : t->tm_wday, PSTR("%d"), pt, ptlim);
				continue;
			}
			else if (f == 'V' || f == 'G' || f == 'g') {
/*
** From Arnold Robbins' strftime version 3.0: "the week number of the
** year (the first Monday as the first day of week 1) as a decimal number
** (01-53)."
** (ado, 1993-05-24)
**
** From "http://www.ft.uni-erlangen.de/~mskuhn/iso-time.html" by Markus Kuhn:
** "Week 01 of a year is per definition the first week which has the
** Thursday in this year, which is equivalent to the week which contains
** the fourth day of January. In other words, the first week of a new year
** is the week which has the majority of its days in the new year. Week 01
** might also contain days from the previous year and the week before week
** 01 of a year is the last week (52 or 53) of the previous year even if
** it contains days from the new year. A week starts with Monday (day 1)
** and ends with Sunday (day 7). For example, the first week of the year
** 1997 lasts from 1996-12-30 to 1997-01-05..."
** (ado, 1996-01-02)
*/
				int year;
				int base;
				int yday;
				int wday;
				int w;

				year = t->tm_year;
				base = TM_YEAR_BASE;
				yday = t->tm_yday;
				wday = t->tm_wday;

				for (;;) {
					int len;
					int bot;
					int top;

					len = isleap_sum(year, base) ?
						DAYSPERLYEAR : DAYSPERNYEAR;
					/*
					 ** What yday (-3 ... 3) does
					 ** the ISO year begin on?
					 */
					bot = ((yday + 11 - wday) % DAYSPERWEEK) - 3;

					/*
					 ** What yday does the NEXT
					 ** ISO year begin on?
					 */
					top = bot - (len % DAYSPERWEEK);
					if (top < -3)
						top += DAYSPERWEEK;
					top += len;
					if (yday >= top) {
						++base;
						w = 1;
						break;
					}
					if (yday >= bot) {
						w = 1 + ((yday - bot) / DAYSPERWEEK);
						break;
					}
					--base;
					yday += isleap_sum(year, base) ?
						DAYSPERLYEAR : DAYSPERNYEAR;
				}

				if (*format == 'V') {
					pt = _conv(w, PSTR("%02d"), pt, ptlim);
				}
				else if (*format == 'g') {
					pt = _yconv(year, base, 0, 1, pt, ptlim);
				}
				else {
					pt = _yconv(year, base, 1, 1, pt, ptlim);
				}

				continue;
			}
			else if (f == 'v') {
				/*
				 ** From Arnold Robbins' strftime version 3.0:
				 ** "date as dd-bbb-YYYY"
				 ** (ado, 1993-05-24)
				 */
				pt = _fmt("%e-%b-%Y", t, pt, ptlim);
				continue;
			}
			else if (f == 'W') {
				pt = _conv((t->tm_yday + DAYSPERWEEK -
							(t->tm_wday ? (t->tm_wday - 1) : (DAYSPERWEEK - 1)))
						/ DAYSPERWEEK,
						PSTR("%02d"), pt, ptlim);
				continue;
			}
			else if (f == 'w') {
				pt = _conv(t->tm_wday, PSTR("%d"), pt, ptlim);
				continue;
			}
			else if (f == 'X') {
				pt = _fmt(Locale->X_fmt, t, pt, ptlim);
				continue;
			}
			else if (f == 'x') {
				pt = _fmt(Locale->x_fmt, t, pt, ptlim);
				continue;
			}
			else if (f == 'y') {
				pt = _yconv(t->tm_year, TM_YEAR_BASE, 0, 1, pt, ptlim);
				continue;
			}
			else if (f == 'Y') {
				pt = _yconv(t->tm_year, TM_YEAR_BASE, 1, 1, pt, ptlim);
				continue;
			}
			else if (f == 'Z') {
				/*
				 ** C99 says that %Z must be replaced by the
				 ** empty string if the time zone is not
				 ** determinable.
				 */
				continue;
			}
			else if (f == 'z') {
				/*
				 ** C99 says that the UTC offset must
				 ** be computed by looking only at
				 ** tm_isdst. This requirement is
				 ** incorrect, since it means the code
				 ** must rely on magic (in this case
				 ** altzone and timezone), and the
				 ** magic might not have the correct
				 ** offset. Doing things correctly is
				 ** tricky and requires disobeying C99;
				 ** see GNU C strftime for details.
				 ** For now, punt and conform to the
				 ** standard, even though it's incorrect.
				 **
				 ** C99 says that %z must be replaced by the
				 ** empty string if the time zone is not
				 ** determinable, so output nothing if the
				 ** appropriate variables are not available.
				 */
				continue;
			}
			else if (f == '+') {
				pt = _fmt(Locale->date_fmt, t, pt, ptlim);
				continue;
			}
			else if (f == '%') {
				/*
				 ** X311J/88-090 (4.12.3.5): if conversion char is
				 ** undefined, behavior is undefined. Print out the
				 ** character itself as printf(3) also does.
				 */
			}
		}

		if (pt == ptlim)
			break;

		*pt++ = *format;
	}

	return pt;
}

static char *_conv(int n, PGM_P format, char * const pt,
	const char * const ptlim)
{
	int max = ptlim - pt;
	int len = snprintf_P(pt, max, format, n);

	if (len > max) {
		return pt + max;
	}
	else {
		return pt + len;
	}
}

static char *_add(str, pt, ptlim)
	const char *str;
	char *pt;
	const char *const ptlim;
{
	while (pt < ptlim && (*pt = *str++) != '\0')
		++pt;
	return pt;
}

/*
 ** POSIX and the C Standard are unclear or inconsistent about
 ** what %C and %y do if the year is negative or exceeds 9999.
 ** Use the convention that %C concatenated with %y yields the
 ** same output as %Y, and that %Y contains at least 4 bytes,
 ** with more only if necessary.
 */

static char *_yconv(a, b, convert_top, convert_yy, pt, ptlim)
	const int a;
	const int b;
	const int convert_top;
	const int convert_yy;
	char *pt;
	const char *const ptlim;
{
	int lead;
	int trail;

#define DIVISOR	100
	trail = a % DIVISOR + b % DIVISOR;
	lead = a / DIVISOR + b / DIVISOR + trail / DIVISOR;
	trail %= DIVISOR;
	if (trail < 0 && lead > 0) {
		trail += DIVISOR;
		--lead;
	}
	else if (lead < 0 && trail > 0) {
		trail -= DIVISOR;
		++lead;
	}
	if (convert_top) {
		if (lead == 0 && trail < 0)
			pt = _add("-0", pt, ptlim);
		else
			pt = _conv(lead, PSTR("%02d"), pt, ptlim);
	}
	if (convert_yy)
		pt = _conv(((trail < 0) ? -trail : trail), PSTR("%02d"), pt, ptlim);
	return pt;
}

