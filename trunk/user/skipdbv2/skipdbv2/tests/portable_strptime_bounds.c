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
    char malformed_after_year[] = "2024 x-11-06";
    char malformed_after_month[] = "2024-11 x";
    char separated[] = "2024 \t11  06";
    char separated_format[] = "%Y %m %d";
    char empty[] = "";
    char alpha[] = "abc";
    char high_byte[] = { (char)0xff, '1', '2', '\0' };
    char high_byte_format[] = { (char)0xff, '\0' };
    char hour[] = "12";
    char format[] = "%Y-%m-%d";
    char month_format[] = "%M";
    char hour_format[] = "%H";
    char yday_format[] = "%j";
    char yday_empty[] = "";
    char yday_space[] = " ";
    char yday_zero[] = "000";
    char yday_001[] = "001";
    char yday_060[] = "060";
    char yday_366[] = "366";
    char yday_367[] = "367";
    char year_format[] = "%y";
    char year_00[] = "00";
    char year_68[] = "68";
    char year_69[] = "69";
    char year_99[] = "99";
    char space_day[] = " 6";
    char space_hour24[] = " 7";
    char space_hour12[] = " 7";
    char composite_space_date[] = "Friday, November,  6, 2026";
    char space_day_format[] = "%e";
    char space_hour24_format[] = "%k";
    char space_hour12_format[] = "%l";
    char composite_date_format[] = "%C";

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
    if (io_strptime(malformed_after_year, format, &invalid) != NULL ||
        io_strptime(malformed_after_month, format, &invalid) != NULL) {
        fprintf(stderr, "unexpected whitespace skipped required format directives\n");
        return EXIT_FAILURE;
    }
    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(separated, separated_format, &parsed) != separated + strlen(separated) ||
        parsed.tm_year != 124 || parsed.tm_mon != 10 || parsed.tm_mday != 6) {
        fprintf(stderr, "format whitespace did not consume separated date fields\n");
        return EXIT_FAILURE;
    }

    memset(&invalid, 0, sizeof(invalid));
    if (io_strptime(truncated, format, &invalid) != NULL ||
        io_strptime(empty, hour_format, &invalid) != NULL ||
        io_strptime(alpha, month_format, &invalid) != NULL ||
        io_strptime(high_byte, hour_format, &invalid) != NULL ||
        io_strptime(hour, high_byte_format, &invalid) != NULL) {
        fprintf(stderr, "truncated or non-numeric input was accepted\n");
        return EXIT_FAILURE;
    }

    memset(&parsed, 0, sizeof(parsed));
    if (NULL == io_strptime(hour, hour_format, &parsed) || parsed.tm_hour != 12) {
        fprintf(stderr, "valid hour did not parse correctly\n");
        return EXIT_FAILURE;
    }

    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(yday_001, yday_format, &parsed) != yday_001 + 3 ||
        parsed.tm_yday != 0) {
        fprintf(stderr, "day 001 was not converted to zero-based day 0\n");
        return EXIT_FAILURE;
    }
    if (io_strptime(yday_060, yday_format, &parsed) != yday_060 + 3 ||
        parsed.tm_yday != 59) {
        fprintf(stderr, "day 060 was not converted to zero-based day 59\n");
        return EXIT_FAILURE;
    }
    if (io_strptime(yday_366, yday_format, &parsed) != yday_366 + 3 ||
        parsed.tm_yday != 365) {
        fprintf(stderr, "day 366 was not converted to zero-based day 365\n");
        return EXIT_FAILURE;
    }
    if (io_strptime(yday_empty, yday_format, &parsed) != NULL ||
        io_strptime(yday_space, yday_format, &parsed) != NULL ||
        io_strptime(yday_zero, yday_format, &parsed) != NULL ||
        io_strptime(yday_367, yday_format, &parsed) != NULL) {
        fprintf(stderr, "invalid or missing day-of-year input was accepted\n");
        return EXIT_FAILURE;
    }

    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(year_00, year_format, &parsed) != year_00 + 2 ||
        parsed.tm_year != 100) {
        fprintf(stderr, "two-digit year 00 did not map to 2000\\n");
        return EXIT_FAILURE;
    }
    if (io_strptime(year_68, year_format, &parsed) != year_68 + 2 ||
        parsed.tm_year != 168) {
        fprintf(stderr, "two-digit year 68 did not map to 2068\\n");
        return EXIT_FAILURE;
    }
    if (io_strptime(year_69, year_format, &parsed) != year_69 + 2 ||
        parsed.tm_year != 69) {
        fprintf(stderr, "two-digit year 69 did not map to 1969\\n");
        return EXIT_FAILURE;
    }
    if (io_strptime(year_99, year_format, &parsed) != year_99 + 2 ||
        parsed.tm_year != 99) {
        fprintf(stderr, "two-digit year 99 did not map to 1999\\n");
        return EXIT_FAILURE;
    }

    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(space_day, space_day_format, &parsed) != space_day + 2 ||
        parsed.tm_mday != 6) {
        fprintf(stderr, "space-padded day-of-month directive did not parse correctly\n");
        return EXIT_FAILURE;
    }
    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(space_hour24, space_hour24_format, &parsed) != space_hour24 + 2 ||
        parsed.tm_hour != 7) {
        fprintf(stderr, "space-padded 24-hour directive did not parse correctly\n");
        return EXIT_FAILURE;
    }
    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(space_hour12, space_hour12_format, &parsed) != space_hour12 + 2 ||
        parsed.tm_hour != 7) {
        fprintf(stderr, "space-padded 12-hour directive did not parse correctly\n");
        return EXIT_FAILURE;
    }
    memset(&parsed, 0, sizeof(parsed));
    if (io_strptime(composite_space_date, composite_date_format, &parsed) !=
            composite_space_date + strlen(composite_space_date) ||
        parsed.tm_wday != 5 || parsed.tm_mon != 10 || parsed.tm_mday != 6 ||
        parsed.tm_year != 126) {
        fprintf(stderr, "composite %C date format did not parse its padded day field\n");
        return EXIT_FAILURE;
    }

    puts("PortableStrptime boundary test passed");
    return EXIT_SUCCESS;
}
