#include "Duration.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    Duration *duration;
    Duration *clone;
    Duration *copy;
    Duration *extreme;
    Duration *not_a_number;
    Duration *long_span = NULL;
    UArray *formatted;

    if (Duration_clone(NULL) != NULL ||
        Duration_asSeconds(NULL) != 0.0 ||
        Duration_compare(NULL, NULL) != 0 ||
        Duration_compare(NULL, (const Duration *)1) != -1) {
        fprintf(stderr, "NULL Duration queries did not return safe results\n");
        return EXIT_FAILURE;
    }
    Duration_copy_(NULL, NULL);
    Duration_free(NULL);
    Duration_fromSeconds_(NULL, 1.0);
    Duration_setYears_(NULL, 1);
    Duration_setDays_(NULL, 1);
    Duration_setHours_(NULL, 1);
    Duration_setMinutes_(NULL, 1);
    Duration_setSeconds_(NULL, 1);
    Duration_add_(NULL, NULL);
    Duration_subtract_(NULL, NULL);
    if (Duration_asUArrayWithFormat_(NULL, NULL) != NULL) {
        fprintf(stderr, "formatting a NULL Duration returned an object\n");
        return EXIT_FAILURE;
    }
    Duration_print(NULL);

    duration = Duration_newWithSeconds_(3661.25);
    if (NULL == duration) {
        fprintf(stderr, "Duration construction failed\n");
        return EXIT_FAILURE;
    }
    clone = Duration_clone(duration);
    copy = Duration_new();
    if (NULL == clone || NULL == copy) {
        fprintf(stderr, "Duration clone/copy allocation failed\n");
        Duration_free(duration);
        Duration_free(clone);
        Duration_free(copy);
        return EXIT_FAILURE;
    }
    Duration_copy_(copy, clone);
    if (Duration_compare(duration, clone) != 0 ||
        Duration_asSeconds(copy) != 3661.25 ||
        Duration_hours(duration) != 1 ||
        Duration_minutes(duration) != 1 ||
        fabs(Duration_seconds(duration) - 1.25) > 0.000001) {
        fprintf(stderr, "Duration clone, copy, or component conversion failed\n");
        goto failure;
    }

    formatted = Duration_asUArrayWithFormat_(duration, "%H:%M");
    if (NULL == formatted || strcmp(UArray_asCString(formatted), "01:01") != 0) {
        fprintf(stderr, "Duration formatting or allocation handling failed\n");
        UArray_free(formatted);
        goto failure;
    }
    UArray_free(formatted);

    Duration_add_(copy, clone);
    if (fabs(Duration_asSeconds(copy) - 7322.5) > 0.000001) {
        fprintf(stderr, "Duration addition failed\n");
        goto failure;
    }
    Duration_subtract_(copy, clone);
    if (fabs(Duration_asSeconds(copy) - Duration_asSeconds(duration)) > 0.000001) {
        fprintf(stderr, "Duration subtraction failed\n");
        goto failure;
    }

    {
        const double year_seconds = 365.0 * 24.0 * 60.0 * 60.0;
        const double span_seconds = 100.0 * year_seconds + 2.0 * 86400.0 +
            3.0 * 3600.0 + 4.0 * 60.0 + 5.25;
        long_span = Duration_newWithSeconds_(span_seconds);
        if (NULL == long_span ||
            Duration_years(long_span) != 100 ||
            Duration_days(long_span) != 2 ||
            Duration_hours(long_span) != 3 ||
            Duration_minutes(long_span) != 4 ||
            fabs(Duration_seconds(long_span) - 5.25) > 0.000001) {
            fprintf(stderr, "positive multi-decade Duration components were corrupted\n");
            goto failure;
        }
        Duration_fromSeconds_(long_span, -span_seconds);
        if (Duration_years(long_span) != -100 ||
            Duration_days(long_span) != -2 ||
            Duration_hours(long_span) != -3 ||
            Duration_minutes(long_span) != -4 ||
            fabs(Duration_seconds(long_span) + 5.25) > 0.000001) {
            fprintf(stderr, "negative multi-decade Duration components were corrupted\n");
            goto failure;
        }
    }

    extreme = Duration_newWithSeconds_(HUGE_VAL);
    not_a_number = Duration_newWithSeconds_(NAN);
    if (NULL == extreme || NULL == not_a_number ||
        Duration_years(extreme) != INT_MAX ||
        Duration_days(extreme) != 0 ||
        Duration_years(not_a_number) != 0 ||
        Duration_seconds(not_a_number) != 0.0) {
        fprintf(stderr, "non-finite Duration component conversion was not bounded\n");
        Duration_free(extreme);
        Duration_free(not_a_number);
        goto failure;
    }

    Duration_free(long_span);
    Duration_free(extreme);
    Duration_free(not_a_number);
    Duration_free(duration);
    Duration_free(clone);
    Duration_free(copy);
    puts("Duration lifecycle test passed");
    return EXIT_SUCCESS;

failure:
    Duration_free(long_span);
    Duration_free(duration);
    Duration_free(clone);
    Duration_free(copy);
    return EXIT_FAILURE;
}
