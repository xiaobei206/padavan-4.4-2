#include "Date.h"

#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

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


int main(void)
{
    Date *date;
    double before;
    double after;

    if (!test_timezone_conversion()) return EXIT_FAILURE;

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
