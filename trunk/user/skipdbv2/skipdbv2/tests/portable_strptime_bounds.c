#include "PortableStrptime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct tm parsed;
    struct tm invalid;
    char valid[] = "2024-11-06";
    char truncated[] = "2024-11";
    char empty[] = "";
    char alpha[] = "abc";
    char high_byte[] = { (char)0xff, '1', '2', '\0' };
    char hour[] = "12";
    char format[] = "%Y-%m-%d";
    char month_format[] = "%M";
    char hour_format[] = "%H";

    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(NULL, format, &parsed) != NULL ||
        io_strptime(valid, NULL, &parsed) != NULL ||
        io_strptime(valid, format, NULL) != NULL) {
        fprintf(stderr, "NULL parser arguments were accepted\n");
        return EXIT_FAILURE;
    }

    if (NULL == io_strptime(valid, format, &parsed) ||
        parsed.tm_year != 124 || parsed.tm_mon != 10 || parsed.tm_mday != 6) {
        fprintf(stderr, "valid date did not parse correctly\n");
        return EXIT_FAILURE;
    }

    memset(&invalid, 0, sizeof(invalid));
    if (io_strptime(truncated, format, &invalid) != NULL ||
        io_strptime(empty, hour_format, &invalid) != NULL ||
        io_strptime(alpha, month_format, &invalid) != NULL ||
        io_strptime(high_byte, hour_format, &invalid) != NULL) {
        fprintf(stderr, "truncated or non-numeric input was accepted\n");
        return EXIT_FAILURE;
    }

    memset(&parsed, 0, sizeof(parsed));
    if (NULL == io_strptime(hour, hour_format, &parsed) || parsed.tm_hour != 12) {
        fprintf(stderr, "valid hour did not parse correctly\n");
        return EXIT_FAILURE;
    }

    puts("PortableStrptime boundary test passed");
    return EXIT_SUCCESS;
}
