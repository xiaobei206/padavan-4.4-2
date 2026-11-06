#include "PortableGettimeofday.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void)
{
    double now = secondsSince1970();
    time_t whole_seconds = time(NULL);

    if (!isfinite(now) || now <= 0.0 ||
        fabs(now - (double)whole_seconds) > 5.0) {
        fprintf(stderr, "portable wall-clock query returned an invalid timestamp\n");
        return EXIT_FAILURE;
    }

    puts("PortableGettimeofday test passed");
    return EXIT_SUCCESS;
}
