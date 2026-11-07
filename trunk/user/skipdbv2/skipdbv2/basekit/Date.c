
//metadoc Date copyright Steve Dekorte 2002
//metadoc Date license BSD revised

#define DATE_C
#include "Date.h"
#undef DATE_C
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include "PortableStrptime.h"
#include "UArray.h"

static struct tm *Date_localTime_(const Date *self, time_t *timestamp)
{
	if (NULL == self || NULL == timestamp)
		return NULL;
	*timestamp = self->tv.tv_sec;
	return localtime(timestamp);
}


/* Normalize a calendar value off to the side and publish it only on success.
 * A -1 timestamp is valid when mktime leaves errno clear. */
static int Date_calendarToTime_(struct tm *calendar, time_t *timestamp)
{
	struct tm normalized;
	time_t converted;
	int savedErrno;

	if (NULL == calendar || NULL == timestamp)
		return 0;
	normalized = *calendar;
	savedErrno = errno;
	errno = 0;
	converted = mktime(&normalized);
	if (converted == (time_t)-1 && errno != 0)
		return 0;
	errno = savedErrno;
	*calendar = normalized;
	*timestamp = converted;
	return 1;
}

/* Split seconds into a whole-second part and normalized positive microseconds.
 * Reject values before any floating-to-integer conversion can leave its range. */
static int Date_splitSeconds_(double seconds, long *wholeSeconds,
	long *microseconds)
{
	double integral, fractional;
	long whole, micros;

	if (NULL == wholeSeconds || NULL == microseconds || !isfinite(seconds) ||
		seconds < (double)LONG_MIN || seconds >= (double)LONG_MAX)
		return 0;

	fractional = modf(seconds, &integral);
	if (fractional < 0.0) {
		integral -= 1.0;
		fractional += 1.0;
	}
	if (integral < (double)LONG_MIN || integral >= (double)LONG_MAX)
		return 0;

	whole = (long)integral;
	micros = (long)(fractional * 1000000.0);
	if (micros >= 1000000) {
		if (whole == LONG_MAX)
			return 0;
		++whole;
		micros -= 1000000;
	}
	if (micros < 0 || micros >= 1000000)
		return 0;

	*wholeSeconds = whole;
	*microseconds = micros;
	return 1;
}


Date *Date_new(void)
{
	Date *self = (Date *)io_calloc(1, sizeof(Date));
	if (NULL == self)
		return NULL;
	Date_now(self);
	return self;
}

void Date_copy_(Date *self, const Date *other)
{
	if (NULL == self || NULL == other || self == other)
		return;
	memcpy(self, other, sizeof(Date));
}

void Date_free(Date *self)
{
	io_free(self);
}

int Date_compare(const Date *self, const Date *other)
{
	if (NULL == self || NULL == other)
		return 0;
	if (self->tv.tv_sec != other->tv.tv_sec)
		return self->tv.tv_sec > other->tv.tv_sec ? 1 : -1;
	if (self->tv.tv_usec != other->tv.tv_usec)
		return self->tv.tv_usec > other->tv.tv_usec ? 1 : -1;
	return 0;
}

double Date_SecondsFrom1970ToNow(void)
{
	struct timeval timeval;
	struct timezone timezone;

	if (gettimeofday(&timeval, &timezone) != 0)
		return 0.0;
	return (double)timeval.tv_sec + ((double)timeval.tv_usec / 1000000.0);
}

void Date_now(Date *self)
{
	struct timeval timeval;
	struct timezone timezone;

	if (NULL == self || gettimeofday(&timeval, &timezone) != 0)
		return;
	self->tv = timeval;
	self->tz = timezone;
}

double Date_Clock(void)
{
	return ((double)clock())/((double)CLOCKS_PER_SEC);
}

// zone --------------------------------------------------------

void Date_setToLocalTimeZone(Date *self)
{
	struct timeval timeval;
	if (NULL == self)
		return;
	gettimeofday(&timeval, &(self->tz));
}

struct timezone Date_timeZone(const Date *self)
{
	struct timezone empty = { 0, 0 };
	return self ? self->tz : empty;
}

void Date_setTimeZone_(Date *self, struct timezone tz)
{
	if (self)
		self->tz = tz;
}

void Date_convertToTimeZone_(Date *self, struct timezone tz)
{
	long long currentSeconds, offsetSeconds, newSeconds;
	time_t timestamp;

	if (NULL == self || self->tv.tv_usec < 0 || self->tv.tv_usec >= 1000000)
		return;

	/* Keep the whole-second shift exact; a double loses low bits above 2^53. */
	currentSeconds = (long long)self->tv.tv_sec;
	if ((time_t)currentSeconds != self->tv.tv_sec ||
		currentSeconds < (long long)LONG_MIN ||
		currentSeconds > (long long)LONG_MAX)
		return;

	offsetSeconds = ((long long)self->tz.tz_minuteswest -
		(long long)tz.tz_minuteswest) * 60LL;
	if ((offsetSeconds > 0 &&
		currentSeconds > (long long)LONG_MAX - offsetSeconds) ||
		(offsetSeconds < 0 &&
		currentSeconds < (long long)LONG_MIN - offsetSeconds))
		return;

	newSeconds = currentSeconds + offsetSeconds;
	timestamp = (time_t)newSeconds;
	if ((long long)timestamp != newSeconds)
		return;

	self->tv.tv_sec = timestamp;
	self->tz = tz;
}
// time --------------------------------------------------------

void Date_fromLocalTime_(Date *self, struct tm *t)
{
	time_t timestamp;

	if (NULL == self || NULL == t ||
		!Date_calendarToTime_(t, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
	self->tv.tv_usec = 0;
}

void Date_fromTime_(Date *self, time_t t)
{
	if (NULL == self)
		return;
	self->tv.tv_sec = t;
	self->tv.tv_usec = 0;
}

time_t Date_asTime(const Date *self)
{
	return self ? (time_t)self->tv.tv_sec : (time_t)0;
}

// sconds --------------------------------------------------------

double Date_asSeconds(const Date *self)
{
	if (NULL == self)
		return 0.0;
	return ((double)self->tv.tv_sec) + (((double)self->tv.tv_usec) / 1000000.0);
}

void Date_fromSeconds_(Date *self, double s)
{
	long secs, usecs;
	time_t timestamp;

	if (NULL == self || !Date_splitSeconds_(s, &secs, &usecs))
		return;
	timestamp = (time_t)secs;
	if ((long double)timestamp != (long double)secs)
		return;
	self->tv.tv_sec = timestamp;
	self->tv.tv_usec = usecs;
}

void Date_addSeconds_(Date *self, double s)
{
	long deltaSeconds, deltaMicroseconds;
	long currentSeconds, currentMicroseconds;
	long newSeconds, newMicroseconds;
	time_t timestamp;

	if (NULL == self ||
		!Date_splitSeconds_(s, &deltaSeconds, &deltaMicroseconds) ||
		(long double)self->tv.tv_sec < (long double)LONG_MIN ||
		(long double)self->tv.tv_sec >= (long double)LONG_MAX)
		return;
	currentSeconds = (long)self->tv.tv_sec;
	if ((long double)(time_t)currentSeconds != (long double)currentSeconds)
		return;
	currentMicroseconds = (long)self->tv.tv_usec;
	if (currentMicroseconds < 0 || currentMicroseconds >= 1000000)
		return;

	if ((deltaSeconds > 0 && currentSeconds > LONG_MAX - deltaSeconds) ||
		(deltaSeconds < 0 && currentSeconds < LONG_MIN - deltaSeconds))
		return;
	newSeconds = currentSeconds + deltaSeconds;
	newMicroseconds = currentMicroseconds + deltaMicroseconds;
	if (newMicroseconds >= 1000000) {
		if (newSeconds == LONG_MAX)
			return;
		++newSeconds;
		newMicroseconds -= 1000000;
	}

	timestamp = (time_t)newSeconds;
	if ((long double)timestamp != (long double)newSeconds)
		return;
	self->tv.tv_sec = timestamp;
	self->tv.tv_usec = newMicroseconds;
}

double Date_secondsSince_(const Date *self, const Date *other)
{
	uintmax_t wholeSeconds;
	double whole, fractional;

	if (NULL == self || NULL == other)
		return 0.0;

	/* Subtract whole seconds before converting to double, preserving small
	 * deltas between timestamps beyond double's exact-integer range. */
	fractional = ((double)self->tv.tv_usec -
		(double)other->tv.tv_usec) / 1000000.0;
	if (self->tv.tv_sec >= other->tv.tv_sec) {
		wholeSeconds = (uintmax_t)self->tv.tv_sec -
			(uintmax_t)other->tv.tv_sec;
		whole = (double)(wholeSeconds >> 32) * 4294967296.0 +
			(double)(wholeSeconds & UINT32_MAX);
		return whole + fractional;
	}

	wholeSeconds = (uintmax_t)other->tv.tv_sec -
		(uintmax_t)self->tv.tv_sec;
	whole = (double)(wholeSeconds >> 32) * 4294967296.0 +
		(double)(wholeSeconds & UINT32_MAX);
	return fractional - whole;
}

// components --------------------------------------------------------

/* Convert a calendar year to struct tm's narrower tm_year without overflow. */
static int Date_tmYearFromCalendarYear_(long calendarYear, int *tmYear)
{
	long long value;
	if (NULL == tmYear)
		return 0;
	value = (long long)calendarYear - 1900LL;
	if (value < INT_MIN || value > INT_MAX)
		return 0;
	*tmYear = (int)value;
	return 1;
}

long Date_year(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	return tm ? (long)tm->tm_year + 1900L : 0L;
}

void Date_setYear_(Date *self, long v)
{
	time_t t, timestamp;
	int tmYear;
	struct tm *tm;
	struct tm calendar;

	if (NULL == self || !Date_tmYearFromCalendarYear_(v, &tmYear))
		return;
	tm = Date_localTime_(self, &t);
	if (NULL == tm)
		return;
	calendar = *tm;
	calendar.tm_year = tmYear;
	if (!Date_calendarToTime_(&calendar, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
}

int Date_month(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	return tm ? tm->tm_mon : 0;
}

void Date_setMonth_(Date *self, int v)
{
	time_t t, timestamp;
	struct tm *tm = Date_localTime_(self, &t);
	struct tm calendar;

	if (NULL == tm)
		return;
	calendar = *tm;
	calendar.tm_mon = v;
	if (!Date_calendarToTime_(&calendar, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
}

int Date_day(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	return tm ? tm->tm_mday : 0;
}

void Date_setDay_(Date *self, int v)
{
	time_t t, timestamp;
	struct tm *tm = Date_localTime_(self, &t);
	struct tm calendar;

	if (NULL == tm)
		return;
	calendar = *tm;
	calendar.tm_mday = v;
	if (!Date_calendarToTime_(&calendar, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
}

int Date_hour(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	return tm ? tm->tm_hour : 0;
}

void Date_setHour_(Date *self, int v)
{
	time_t t, timestamp;
	struct tm *tm = Date_localTime_(self, &t);
	struct tm calendar;

	if (NULL == tm)
		return;
	calendar = *tm;
	calendar.tm_hour = v;
	if (!Date_calendarToTime_(&calendar, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
}

int Date_minute(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	return tm ? tm->tm_min : 0;
}

void Date_setMinute_(Date *self, int v)
{
	time_t t, timestamp;
	struct tm *tm = Date_localTime_(self, &t);
	struct tm calendar;

	if (NULL == tm)
		return;
	calendar = *tm;
	calendar.tm_min = v;
	if (!Date_calendarToTime_(&calendar, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
}

double Date_second(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	if (NULL == tm)
		return 0.0;
	return ((double)tm->tm_sec) + ((double)self->tv.tv_usec)/1000000.0;
}

void Date_setSecond_(Date *self, double v)
{
	long wholeSeconds, microseconds;
	time_t t, timestamp;
	struct tm *tm;
	struct tm calendar;

	if (NULL == self || !Date_splitSeconds_(v, &wholeSeconds, &microseconds) ||
		wholeSeconds < INT_MIN || wholeSeconds > INT_MAX)
		return;
	tm = Date_localTime_(self, &t);
	if (NULL == tm)
		return;
	calendar = *tm;
	calendar.tm_sec = (int)wholeSeconds;
	if (!Date_calendarToTime_(&calendar, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
	self->tv.tv_usec = microseconds;
}

unsigned char Date_isDaylightSavingsTime(const Date *self)
{
	time_t t;
	struct tm *tm = Date_localTime_(self, &t);
	return tm ? (unsigned char)tm->tm_isdst : 0;
}

int Date_isLeapYear(const Date *self)
{
	time_t timestamp;
	struct tm *tm = Date_localTime_(self, &timestamp);
	long long year;

	if (NULL == tm)
		return 0;
	/* tm_year is an int; retain its full calendar-year range before modulo. */
	year = (long long)tm->tm_year + 1900LL;
	return ((year % 4LL == 0LL) && (year % 100LL != 0LL)) ||
		(year % 400LL == 0LL);
}

// format --------------------------------------------------------

static struct tm EmptyTM(void)
{
	time_t tmp = 0;
	struct tm *tt = localtime(&tmp);
	struct tm t;

	memset(&t, 0, sizeof(t));
	if (tt)
		memcpy(&t, tt, sizeof(struct tm));
	t.tm_sec  = 0;
	t.tm_min  = 0;
	t.tm_hour = 0;
	t.tm_mday = 0;
	t.tm_mon  = 0;
	t.tm_year = 0;
	t.tm_wday = 0;
	t.tm_yday = 0;
	/* Let mktime resolve DST for the parsed date, not the epoch template. */
	t.tm_isdst = -1;
	return t;
}

void Date_fromString_format_(Date *self, const char *s, const char *format)
{
	struct tm tm;
	time_t timestamp;

	if (NULL == self || NULL == s || NULL == format)
		return;
	tm = EmptyTM();
	if (NULL == io_strptime((char *)s, (char *)format, &tm) ||
		!Date_calendarToTime_(&tm, &timestamp))
		return;
	self->tv.tv_sec = timestamp;
	self->tv.tv_usec = 0;
}

// durations --------------------------------------------------------

Duration *Date_newDurationBySubtractingDate_(const Date *self, const Date *other)
{
	double d;
	if (NULL == self || NULL == other)
		return NULL;
	d = Date_secondsSince_(self, other);
	return Duration_newWithSeconds_(d);
}

void Date_addDuration_(Date *self, const Duration *d)
{
	if (NULL == self || NULL == d)
		return;
	Date_addSeconds_(self, Duration_asSeconds(d));
}

void Date_subtractDuration_(Date *self, const Duration *d)
{
	if (NULL == self || NULL == d)
		return;
	Date_addSeconds_(self, -Duration_asSeconds(d));
}

// -----------------------------------------------------------

double Date_secondsSinceNow(const Date *self)
{
	double n, s;
	if (NULL == self)
		return 0.0;
	n = Date_SecondsFrom1970ToNow();
	s = Date_asSeconds(self);
	return n - s;
}

// format --------------------------------------------------------

UArray *Date_asString(const Date *self, const char *format)
{
	UArray *u;
	time_t t;
	struct tm *tm;
	size_t formatLength, capacity, written;
	char *buffer;

	if (NULL == self || NULL == format)
		return NULL;

	t = self->tv.tv_sec;
	tm = localtime(&t);
	if (NULL == tm)
		return NULL;

	formatLength = strlen(format);
	if (formatLength > (size_t)-1 - 1024)
		return NULL;
	capacity = 1024 + formatLength;

	u = UArray_new();
	if (NULL == u)
		return NULL;
	UArray_setSize_(u, capacity);
	buffer = (char *)UArray_bytes(u);
	if (NULL == buffer || UArray_size(u) < capacity) {
		UArray_free(u);
		return NULL;
	}

	/* Keep a valid terminator if strftime cannot represent the result. */
	buffer[0] = '\0';
	written = strftime(buffer, capacity, format, tm);
	if (written == 0) {
		buffer[0] = '\0';
		written = 0;
	}
	UArray_setSize_(u, written);
	if (UArray_size(u) != written) {
		UArray_free(u);
		return NULL;
	}

	return u;
}
