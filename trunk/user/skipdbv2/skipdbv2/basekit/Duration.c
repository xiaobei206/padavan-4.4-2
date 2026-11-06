//metadoc Duration copyright Steve Dekorte 2002
//metadoc Duration license BSD revised

#define DURATION_C
#include "Duration.h"
#undef DURATION_C
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

typedef struct
{
	double years;
	double days;
	double hours;
	double minutes;
	double seconds;
} DurationComponents;

Duration *Duration_new(void)
{
	Duration *self = (Duration *)io_calloc(1, sizeof(Duration));
	return self;
}

Duration *Duration_newWithSeconds_(double s)
{
	Duration *self = Duration_new();
	if (NULL == self)
		return NULL;
	self->seconds = s;
	return self;
}

Duration *Duration_clone(const Duration *self)
{
	Duration *copy;
	if (NULL == self)
		return NULL;
	copy = Duration_new();
	if (NULL == copy)
		return NULL;
	Duration_copy_(copy, self);
	return copy;
}

void Duration_copy_(Duration *self, const Duration *other)
{
	if (NULL == self || NULL == other || self == other)
		return;
	memcpy(self, other, sizeof(Duration));
}

void Duration_free(Duration *self)
{
	if (self)
		io_free(self);
}

int Duration_compare(const Duration *self, const Duration *other)
{
	if (NULL == self || NULL == other)
		return self == other ? 0 : (self ? 1 : -1);
	if (isnan(self->seconds))
		return isnan(other->seconds) ? 0 : 1;
	if (isnan(other->seconds))
		return -1;
	if (self->seconds == other->seconds)
		return 0;
	return self->seconds > other->seconds ? 1 : -1;
}

// components --------------------------------------------------------

#define SECONDS_IN_YEAR   (60 * 60 * 24 * 365)
#define SECONDS_IN_DAY    (60 * 60 * 24)
#define SECONDS_IN_HOUR   (60 * 60)
#define SECONDS_IN_MINUTE (60)

DurationComponents Duration_asComponents(const Duration *self)
{
	DurationComponents c;
	double t;
	memset(&c, 0, sizeof(c));
	if (NULL == self)
		return c;
	t = self->seconds;
	if (isnan(t))
		return c;
	/* Keep every later integer component conversion within int range. */
	if (t > (double)INT_MAX * SECONDS_IN_YEAR) {
		c.years = INT_MAX;
		return c;
	}
	if (t < (double)INT_MIN * SECONDS_IN_YEAR) {
		c.years = INT_MIN;
		return c;
	}
	c.years   = (int)(t / SECONDS_IN_YEAR);
	t -= c.years * SECONDS_IN_YEAR;
	c.days    = (int)(t / SECONDS_IN_DAY);
	t -= c.days * SECONDS_IN_DAY;
	c.hours   = (int)(t / SECONDS_IN_HOUR);
	t -= c.hours * SECONDS_IN_HOUR;
	c.minutes = (int)(t / SECONDS_IN_MINUTE);
	t -= c.minutes * SECONDS_IN_MINUTE;
	c.seconds = (t);
	return c;
}

void Duration_fromComponents_(Duration *self, DurationComponents c)
{
	double t;
	if (NULL == self)
		return;
	t = c.years * SECONDS_IN_YEAR;
	t += c.days * SECONDS_IN_DAY;
	t += c.hours * SECONDS_IN_HOUR;
	t += c.minutes * SECONDS_IN_MINUTE;
	t += c.seconds;
	self->seconds = t;
}

// years --------------------------------------------------------

int Duration_years(const Duration *self)
{
	return (int)Duration_asComponents(self).years;
}

void Duration_setYears_(Duration *self, double y)
{
	DurationComponents c = Duration_asComponents(self);
	c.years = y;
	Duration_fromComponents_(self, c);
}

// days --------------------------------------------------------

int Duration_days(const Duration *self)
{
	return (int)Duration_asComponents(self).days;
}

void Duration_setDays_(Duration *self, double d)
{
	DurationComponents c = Duration_asComponents(self);
	c.days = d;
	Duration_fromComponents_(self, c);
}

// hours --------------------------------------------------------

int Duration_hours(const Duration *self)
{
	return (int)Duration_asComponents(self).hours;
}

void Duration_setHours_(Duration *self, double m)
{
	DurationComponents c = Duration_asComponents(self);
	c.hours = m;
	Duration_fromComponents_(self, c);
}

// minutes --------------------------------------------------------

int Duration_minutes(const Duration *self)
{
	return (int)Duration_asComponents(self).minutes;
}

void Duration_setMinutes_(Duration *self, double m)
{
	DurationComponents c = Duration_asComponents(self);
	c.minutes = m;
	Duration_fromComponents_(self, c);
}

// seconds --------------------------------------------------------

double Duration_seconds(const Duration *self)
{
	return Duration_asComponents(self).seconds;
}

void Duration_setSeconds_(Duration *self, double s)
{
	DurationComponents c = Duration_asComponents(self);
	c.seconds = s;
	Duration_fromComponents_(self, c);
}

// total seconds --------------------------------------------------------

double Duration_asSeconds(const Duration *self)
{
	return self ? self->seconds : 0.0;
}

void Duration_fromSeconds_(Duration *self, double s)
{
	if (self)
		self->seconds = s;
}

// strings --------------------------------------------------------

UArray *Duration_asUArrayWithFormat_(const Duration *self, const char *format)
{
	DurationComponents c;
	char s[128];
	UArray *ba;
	if (NULL == self)
		return NULL;
	c = Duration_asComponents(self);
	ba = UArray_newWithCString_(format ? format : "%Y years %d days %H:%M:%S");
	if (NULL == ba)
		return NULL;

	snprintf(s, 128, "%i", (int)c.years);
	UArray_replaceCString_withCString_(ba, "%Y", s);

	snprintf(s, 128, "%04i", (int)c.years);
	UArray_replaceCString_withCString_(ba, "%y", s);

	snprintf(s, 128, "%02i", (int)c.days);
	UArray_replaceCString_withCString_(ba, "%d", s);

	snprintf(s, 128, "%02i", (int)c.hours);
	UArray_replaceCString_withCString_(ba, "%H", s);

	snprintf(s, 128, "%02i", (int)c.minutes);
	UArray_replaceCString_withCString_(ba, "%M", s);

	snprintf(s, 128, "%02f", c.seconds);
	UArray_replaceCString_withCString_(ba, "%S", s);

	return ba;
}

void Duration_print(const Duration *self)
{
	UArray *ba = Duration_asUArrayWithFormat_(self, NULL);
	if (NULL == ba)
		return;
	UArray_print(ba);
	UArray_free(ba);
}

// math --------------------------------------------------------

void Duration_add_(Duration *self, const Duration *other)
{
	if (NULL == self || NULL == other)
		return;
	self->seconds += other->seconds;
}

void Duration_subtract_(Duration *self, const Duration *other)
{
	if (NULL == self || NULL == other)
		return;
	self->seconds -= other->seconds;
}

