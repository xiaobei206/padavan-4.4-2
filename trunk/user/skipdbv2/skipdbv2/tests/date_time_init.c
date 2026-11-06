#include "Date.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    Date *date;
    double before;
    double after;

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
