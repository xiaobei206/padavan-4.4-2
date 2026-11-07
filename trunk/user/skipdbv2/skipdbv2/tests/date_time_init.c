#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif

#include "Date.h"

#include <math.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
static int test_parse_resolves_daylight_saving(void)
{
    const char *current_tz = getenv("TZ");
    char *saved_tz = NULL;
    int had_tz = current_tz != NULL;
    Date parsed = {0};
    struct tm expected = {0};
    time_t expected_time;
    int okay = 0;

    if (had_tz) {
        size_t length = strlen(current_tz) + 1;
        saved_tz = (char *)malloc(length);
        if (NULL == saved_tz) {
            fprintf(stderr, "Date DST test could not save the current TZ value\n");
            return 0;
        }
        memcpy(saved_tz, current_tz, length);
    }

    if (setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1) != 0) {
        fprintf(stderr, "Date DST test could not install its test timezone\n");
        free(saved_tz);
        return 0;
    }
    tzset();

    expected.tm_year = 2024 - 1900;
    expected.tm_mon = 6;
    expected.tm_mday = 1;
    expected.tm_hour = 12;
    expected.tm_min = 34;
    expected.tm_sec = 56;
    expected.tm_isdst = -1;
    expected_time = mktime(&expected);
    Date_fromString_format_(&parsed, "2024-07-01 12:34:56",
        "%Y-%m-%d %H:%M:%S");
    if (expected_time == (time_t)-1 || expected.tm_isdst <= 0 ||
        parsed.tv.tv_sec != expected_time || parsed.tv.tv_usec != 0) {
        fprintf(stderr, "Date string parsing ignored active daylight saving time\n");
        goto restore_timezone;
    }
    okay = 1;

restore_timezone:
    if (had_tz) {
        if (setenv("TZ", saved_tz, 1) != 0) {
            fprintf(stderr, "Date DST test could not restore the current TZ value\n");
            okay = 0;
        }
    } else if (unsetenv("TZ") != 0) {
        fprintf(stderr, "Date DST test could not clear its test timezone\n");
        okay = 0;
    }
    tzset();
    free(saved_tz);
    return okay;
}
#else
static int test_parse_resolves_daylight_saving(void)
{
    return 1;
}
#endif

static int test_timezone_conversion(void)
{
    Date date = {0};
    Date before;
    struct timezone utc = { 0, 0 };
    struct timezone east = { -60, 0 };

    date.tv.tv_sec = (time_t)1000000000;
    date.tv.tv_usec = 250000;
    date.tz = utc;
    Date_convertToTimeZone_(&date, east);
    if (date.tv.tv_sec != (time_t)1000003600 ||
        date.tv.tv_usec != 250000 ||
        date.tz.tz_minuteswest != east.tz_minuteswest ||
        date.tz.tz_dsttime != east.tz_dsttime) {
        fprintf(stderr, "Date timezone conversion changed the wrong instant or zone\\n");
        return 0;
    }

    {
        Date precise = {0};
        const long long exactSeconds = 9007199254740993LL;
        const long long expectedSeconds = exactSeconds + 3600LL;

        precise.tv.tv_sec = (time_t)exactSeconds;
        if ((long long)precise.tv.tv_sec == exactSeconds &&
            exactSeconds <= (long long)LONG_MAX - 3600LL) {
            precise.tv.tv_usec = 654321;
            precise.tz = utc;
            Date_convertToTimeZone_(&precise, east);
            if ((long long)precise.tv.tv_sec != expectedSeconds ||
                precise.tv.tv_usec != 654321 ||
                precise.tz.tz_minuteswest != east.tz_minuteswest) {
                fprintf(stderr, "Date timezone conversion lost exact seconds above double precision\n");
                return 0;
            }
        }
    }

    {
        Date malformed = {0};
        Date beforeMalformed;

        malformed.tv.tv_sec = (time_t)1000000000;
        malformed.tv.tv_usec = 1000000;
        malformed.tz = utc;
        beforeMalformed = malformed;
        Date_convertToTimeZone_(&malformed, east);
        if (malformed.tv.tv_sec != beforeMalformed.tv.tv_sec ||
            malformed.tv.tv_usec != beforeMalformed.tv.tv_usec ||
            malformed.tz.tz_minuteswest != beforeMalformed.tz.tz_minuteswest ||
            malformed.tz.tz_dsttime != beforeMalformed.tz.tz_dsttime) {
            fprintf(stderr, "Date timezone conversion accepted malformed microseconds\n");
            return 0;
        }
    }

    date.tv.tv_sec = (time_t)LONG_MAX;
    date.tv.tv_usec = 0;
    date.tz = utc;
    before = date;
    Date_convertToTimeZone_(&date, east);
    if (date.tv.tv_sec != before.tv.tv_sec ||
        date.tv.tv_usec != before.tv.tv_usec ||
        date.tz.tz_minuteswest != before.tz.tz_minuteswest ||
        date.tz.tz_dsttime != before.tz.tz_dsttime) {
        fprintf(stderr, "Date timezone conversion partially committed an out-of-range shift\\n");
        return 0;
    }

#if LONG_MAX > INT_MAX
    if (sizeof(time_t) >= sizeof(long)) {
        Date wide = {0};
        struct timezone extremeWest = { INT_MAX, 0 };
        struct timezone extremeEast = { INT_MIN, 0 };
        long long expected = 1000000000LL +
            (((long long)INT_MAX - (long long)INT_MIN) * 60LL);

        wide.tv.tv_sec = (time_t)1000000000;
        wide.tv.tv_usec = 125000;
        wide.tz = extremeWest;
        Date_convertToTimeZone_(&wide, extremeEast);
        if ((long long)wide.tv.tv_sec != expected ||
            wide.tv.tv_usec != 125000 ||
            wide.tz.tz_minuteswest != extremeEast.tz_minuteswest) {
            fprintf(stderr, "Date timezone offset arithmetic lost the full integer range\\n");
            return 0;
        }
    }
#endif

    return 1;
}


static int test_calendar_year_bounds(void)
{
    Date value = {0};
    Date before;

    Date_fromSeconds_(&value, 1000000000.25);
    Date_setYear_(&value, 2024);
    if (Date_year(&value) != 2024 || value.tv.tv_usec != 250000) {
        fprintf(stderr, "Date_setYear failed to preserve a valid calendar year or fractional second\\n");
        return 0;
    }

    before = value;
    Date_setYear_(&value, LONG_MIN);
    if (value.tv.tv_sec != before.tv.tv_sec ||
        value.tv.tv_usec != before.tv.tv_usec) {
        fprintf(stderr, "Date_setYear accepted a year whose tm_year subtraction underflows\\n");
        return 0;
    }

#if LONG_MAX > INT_MAX + 1900LL
    Date_setYear_(&value, LONG_MAX);
    if (value.tv.tv_sec != before.tv.tv_sec ||
        value.tv.tv_usec != before.tv.tv_usec) {
        fprintf(stderr, "Date_setYear narrowed an out-of-range tm_year\\n");
        return 0;
    }

    if (sizeof(time_t) >= sizeof(long)) {
        Date wideLeap = {0};
        long wideLeapYear = (long)(((long long)INT_MAX + 1900LL) / 400LL * 400LL);
        Date_fromSeconds_(&wideLeap, 0.0);
        Date_setYear_(&wideLeap, wideLeapYear);
        if (Date_year(&wideLeap) != wideLeapYear ||
            !Date_isLeapYear(&wideLeap)) {
            fprintf(stderr, "Date_isLeapYear narrowed a representable future year\\n");
            return 0;
        }
    }
#endif

    return 1;
}

static int test_calendar_mktime_failure_is_atomic(void)
{
#if defined(EOVERFLOW)
    struct tm probe = {0};
    struct tm impossible = {0};
    struct tm beforeImpossible;
    Date value = {0};
    Date before;
    time_t converted;

    probe.tm_year = INT_MAX - 1900;
    probe.tm_mon = 0;
    probe.tm_mday = 1;
    probe.tm_isdst = -1;
    errno = 0;
    converted = mktime(&probe);
    if (converted != (time_t)-1 || errno == 0)
        return 1; /* This platform's time_t can represent the probe year. */

    Date_fromSeconds_(&value, 1700000000.25);
    before = value;
    impossible.tm_year = INT_MAX - 1900;
    impossible.tm_mon = 0;
    impossible.tm_mday = 1;
    impossible.tm_isdst = -1;
    beforeImpossible = impossible;
    Date_fromLocalTime_(&value, &impossible);
    if (value.tv.tv_sec != before.tv.tv_sec ||
        value.tv.tv_usec != before.tv.tv_usec ||
        memcmp(&impossible, &beforeImpossible, sizeof(impossible)) != 0) {
        fprintf(stderr, "Date_fromLocalTime committed an unrepresentable calendar value\n");
        return 0;
    }

    Date_setYear_(&value, (long)INT_MAX);
    if (value.tv.tv_sec != before.tv.tv_sec ||
        value.tv.tv_usec != before.tv.tv_usec) {
        fprintf(stderr, "Date_setYear committed mktime's overflow sentinel\n");
        return 0;
    }
#endif
    return 1;
}

static int test_date_compare_precision(void)
{
#if LONG_MAX > 9007199254740992LL
    const long long exactSeconds = 9007199254740992LL;
    Date earlier = {0};
    Date later = {0};

    earlier.tv.tv_sec = (time_t)exactSeconds;
    later.tv.tv_sec = (time_t)(exactSeconds + 1LL);
    if ((long long)earlier.tv.tv_sec == exactSeconds &&
        (long long)later.tv.tv_sec == exactSeconds + 1LL) {
        if (Date_compare(&earlier, &later) != -1 ||
            Date_compare(&later, &earlier) != 1) {
            fprintf(stderr, "Date_compare collapsed distinct whole seconds above double precision\\n");
            return 0;
        }
        if (Date_secondsSince_(&later, &earlier) != 1.0 ||
            Date_secondsSince_(&earlier, &later) != -1.0) {
            fprintf(stderr, "Date_secondsSince collapsed a one-second delta above double precision\\n");
            return 0;
        }
    }

    {
        Date fromTime = {0};
        time_t preciseTime = (time_t)(exactSeconds + 1LL);

        if ((long long)preciseTime == exactSeconds + 1LL) {
            Date_fromTime_(&fromTime, preciseTime);
            if (fromTime.tv.tv_sec != preciseTime || fromTime.tv.tv_usec != 0) {
                fprintf(stderr, "Date_fromTime lost an exact whole second above double precision\\n");
                return 0;
            }
        }
    }

    earlier.tv.tv_sec = (time_t)exactSeconds;
    later.tv.tv_sec = (time_t)exactSeconds;
    earlier.tv.tv_usec = 100;
    later.tv.tv_usec = 101;
    if ((long long)earlier.tv.tv_sec == exactSeconds &&
        (long long)later.tv.tv_sec == exactSeconds) {
        if (Date_compare(&earlier, &later) != -1 ||
            Date_compare(&later, &earlier) != 1) {
            fprintf(stderr, "Date_compare collapsed distinct microseconds at a large timestamp\\n");
            return 0;
        }
        if (fabs(Date_secondsSince_(&later, &earlier) - 0.000001) > 1e-12 ||
            fabs(Date_secondsSince_(&earlier, &later) + 0.000001) > 1e-12) {
            fprintf(stderr, "Date_secondsSince collapsed a microsecond delta at a large timestamp\\n");
            return 0;
        }
    }
#endif
    return 1;
}

int main(void)
{
    Date *date;
    double before;
    double after;

    if (!test_timezone_conversion() || !test_calendar_year_bounds() ||
        !test_calendar_mktime_failure_is_atomic() ||
        !test_date_compare_precision() || !test_parse_resolves_daylight_saving())
        return EXIT_FAILURE;

    {
        Date sample = {0};
        Date copy = {0};
        struct timezone emptyZone = Date_timeZone(NULL);

        Date_now(NULL);
        Date_setToLocalTimeZone(NULL);
        Date_copy_(NULL, &sample);
        Date_copy_(&sample, NULL);
        Date_setTimeZone_(NULL, emptyZone);
        Date_convertToTimeZone_(NULL, emptyZone);
        Date_fromLocalTime_(NULL, NULL);
        Date_fromTime_(NULL, (time_t)0);
        Date_fromSeconds_(NULL, 1.0);
        Date_addSeconds_(NULL, 1.0);
        Date_setYear_(NULL, 2026);
        Date_setMonth_(NULL, 10);
        Date_setDay_(NULL, 5);
        Date_setHour_(NULL, 12);
        Date_setMinute_(NULL, 30);
        Date_setSecond_(NULL, 45.0);
        Date_fromString_format_(NULL, NULL, NULL);
        Date_addDuration_(NULL, NULL);
        Date_subtractDuration_(NULL, NULL);

        if (Date_compare(NULL, &sample) != 0 ||
            Date_compare(&sample, NULL) != 0 ||
            Date_asTime(NULL) != (time_t)0 ||
            Date_asSeconds(NULL) != 0.0 ||
            Date_secondsSince_(NULL, &sample) != 0.0 ||
            Date_secondsSince_(&sample, NULL) != 0.0 ||
            Date_year(NULL) != 0 || Date_month(NULL) != 0 ||
            Date_day(NULL) != 0 || Date_hour(NULL) != 0 ||
            Date_minute(NULL) != 0 || Date_second(NULL) != 0.0 ||
            Date_isDaylightSavingsTime(NULL) != 0 ||
            Date_isLeapYear(NULL) != 0 ||
            Date_timeZone(NULL).tz_minuteswest != 0 ||
            Date_timeZone(NULL).tz_dsttime != 0 ||
            Date_newDurationBySubtractingDate_(NULL, &sample) != NULL ||
            Date_newDurationBySubtractingDate_(&sample, NULL) != NULL ||
            Date_secondsSinceNow(NULL) != 0.0) {
            fprintf(stderr, "Date NULL receiver/argument contract failed\n");
            return EXIT_FAILURE;
        }

        Date_fromSeconds_(&sample, 1000000000.5);
        Date_copy_(&copy, &sample);
        Date_addSeconds_(&copy, 1.25);
        if (Date_compare(&sample, &sample) != 0 ||
            fabs(Date_asSeconds(&copy) - 1000000001.75) > 0.000001) {
            fprintf(stderr, "Date valid-state behavior changed during NULL hardening\n");
            return EXIT_FAILURE;
        }
    }

    before = Date_SecondsFrom1970ToNow();
    if (!isfinite(before) || before <= 0.0) {
        fprintf(stderr, "wall-clock query returned an invalid timestamp\n");
        return EXIT_FAILURE;
    }

    {
        Date value = {0};
        Date beforeInvalid;
        char truncated_date[] = "2024-11";
        char full_date_format[] = "%Y-%m-%d";

        Date_fromSeconds_(&value, -0.5);
        if (value.tv.tv_sec != (time_t)-1 || value.tv.tv_usec != 500000 ||
            fabs(Date_asSeconds(&value) + 0.5) > 0.000001) {
            fprintf(stderr, "negative fractional Date timestamp was not normalized\\n");
            return EXIT_FAILURE;
        }

        Date_fromSeconds_(&value, 10.75);
        Date_addSeconds_(&value, 0.5);
        if (value.tv.tv_sec != (time_t)11 || value.tv.tv_usec != 250000 ||
            fabs(Date_asSeconds(&value) - 11.25) > 0.000001) {
            fprintf(stderr, "Date fractional addition did not carry microseconds\\n");
            return EXIT_FAILURE;
        }
        Date_addSeconds_(&value, -0.5);
        if (value.tv.tv_sec != (time_t)10 || value.tv.tv_usec != 750000 ||
            fabs(Date_asSeconds(&value) - 10.75) > 0.000001) {
            fprintf(stderr, "Date negative fractional addition did not borrow seconds\\n");
            return EXIT_FAILURE;
        }

        Date_fromSeconds_(&value, 1700000000.25);
        beforeInvalid = value;
        Date_fromString_format_(&value, truncated_date, full_date_format);
        if (value.tv.tv_sec != beforeInvalid.tv.tv_sec ||
            value.tv.tv_usec != beforeInvalid.tv.tv_usec) {
            fprintf(stderr, "Date parse failure changed the stored time\\n");
            return EXIT_FAILURE;
        }
        Date_setSecond_(&value, -0.5);
        if (value.tv.tv_usec != 500000 ||
            Date_second(&value) < 59.499999 ||
            Date_second(&value) > 59.500001) {
            fprintf(stderr, "Date_setSecond did not normalize a negative fraction\\n");
            return EXIT_FAILURE;
        }

        beforeInvalid = value;
        Date_fromSeconds_(&value, NAN);
        Date_addSeconds_(&value, INFINITY);
        Date_addSeconds_(&value, -INFINITY);
        Date_setSecond_(&value, NAN);
        Date_setSecond_(&value, INFINITY);
        if (value.tv.tv_sec != beforeInvalid.tv.tv_sec ||
            value.tv.tv_usec != beforeInvalid.tv.tv_usec) {
            fprintf(stderr, "non-finite Date seconds input changed the stored time\\n");
            return EXIT_FAILURE;
        }

        value.tv.tv_sec = (time_t)LONG_MAX;
        value.tv.tv_usec = 0;
        Date_addSeconds_(&value, 1.0);
        if (value.tv.tv_sec != (time_t)LONG_MAX || value.tv.tv_usec != 0) {
            fprintf(stderr, "Date seconds addition overflowed the whole-second field\\n");
            return EXIT_FAILURE;
        }
    }

    date = Date_new();
    if (NULL == date) {
        fprintf(stderr, "Date construction failed\n");
        return EXIT_FAILURE;
    }
    after = Date_asSeconds(date);
    if (!isfinite(after) || after <= 0.0 || fabs(after - before) > 5.0) {
        fprintf(stderr, "Date_new did not initialize the wall-clock state\n");
        Date_free(date);
        return EXIT_FAILURE;
    }

    Date_now(date);
    Date_setToLocalTimeZone(date);
    if (!isfinite(Date_asSeconds(date))) {
        fprintf(stderr, "Date_now produced an invalid timestamp\n");
        Date_free(date);
        return EXIT_FAILURE;
    }

    Date_free(date);
    puts("Date time-initialization test passed");
    return EXIT_SUCCESS;
}
