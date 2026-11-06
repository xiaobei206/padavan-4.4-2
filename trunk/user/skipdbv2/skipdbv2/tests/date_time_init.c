#include "Date.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    Date *date;
    double before;
    double after;

    Date_now(NULL);
    Date_setToLocalTimeZone(NULL);

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
