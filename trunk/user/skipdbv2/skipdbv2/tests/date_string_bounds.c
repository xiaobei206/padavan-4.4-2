#include "Date.h"
#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LONG_FORMAT_LENGTH 1300
#define EXPANDED_FORMAT_REPEATS 1500
#define DATE_FORMAT_TOKEN "%Y-%m-%d"
#define DATE_FORMAT_TOKEN_LENGTH (sizeof(DATE_FORMAT_TOKEN) - 1)

int main(void)
{
	Date date = {0};
	UArray *year;
	UArray *longResult;
	UArray *dateToken;
	UArray *expandedResult;
	char format[LONG_FORMAT_LENGTH + 1];
	char expandedFormat[EXPANDED_FORMAT_REPEATS * DATE_FORMAT_TOKEN_LENGTH + 1];
	size_t i;

	date.tv.tv_sec = (time_t)1000000000;
	date.tv.tv_usec = 0;

	if (Date_asString(NULL, "%Y") != NULL ||
		Date_asString(&date, NULL) != NULL) {
		fprintf(stderr, "Date_asString accepted a NULL argument\n");
		return EXIT_FAILURE;
	}

	year = Date_asString(&date, "%Y");
	if (NULL == year || UArray_size(year) != 4 ||
		0 != memcmp(UArray_bytes(year), "2001", 4)) {
		fprintf(stderr, "Date_asString failed to format a valid year\n");
		UArray_free(year);
		return EXIT_FAILURE;
	}
	UArray_free(year);

	for (i = 0; i < LONG_FORMAT_LENGTH; ++i)
		format[i] = 'x';
	format[LONG_FORMAT_LENGTH] = '\0';
	longResult = Date_asString(&date, format);
	if (NULL == longResult || UArray_size(longResult) != LONG_FORMAT_LENGTH) {
		fprintf(stderr, "Date_asString truncated a long literal format\n");
		UArray_free(longResult);
		return EXIT_FAILURE;
	}
	for (i = 0; i < LONG_FORMAT_LENGTH; ++i) {
		if (((const char *)UArray_bytes(longResult))[i] != 'x') {
			fprintf(stderr, "Date_asString corrupted a long literal format\n");
			UArray_free(longResult);
			return EXIT_FAILURE;
		}
	}
	UArray_free(longResult);

	/* Each date directive expands to more bytes than its source format width. */
	dateToken = Date_asString(&date, DATE_FORMAT_TOKEN);
	if (NULL == dateToken || UArray_size(dateToken) == 0) {
		fprintf(stderr, "Date_asString date-token fixture failed\n");
		UArray_free(dateToken);
		return EXIT_FAILURE;
	}
	for (i = 0; i < EXPANDED_FORMAT_REPEATS; ++i)
		memcpy(expandedFormat + i * DATE_FORMAT_TOKEN_LENGTH,
			DATE_FORMAT_TOKEN, DATE_FORMAT_TOKEN_LENGTH);
	expandedFormat[EXPANDED_FORMAT_REPEATS * DATE_FORMAT_TOKEN_LENGTH] = '\0';
	expandedResult = Date_asString(&date, expandedFormat);
	if (NULL == expandedResult ||
		UArray_size(expandedResult) != UArray_size(dateToken) * EXPANDED_FORMAT_REPEATS) {
		fprintf(stderr, "Date_asString truncated an expanded directive format\n");
		UArray_free(expandedResult);
		UArray_free(dateToken);
		return EXIT_FAILURE;
	}
	for (i = 0; i < EXPANDED_FORMAT_REPEATS; ++i) {
		if (memcmp((const unsigned char *)UArray_bytes(expandedResult) +
			i * UArray_size(dateToken), UArray_bytes(dateToken),
			UArray_size(dateToken)) != 0) {
			fprintf(stderr, "Date_asString corrupted an expanded directive format\n");
			UArray_free(expandedResult);
			UArray_free(dateToken);
			return EXIT_FAILURE;
		}
	}
	UArray_free(expandedResult);
	UArray_free(dateToken);

	puts("Date string bounds test passed");
	return EXIT_SUCCESS;
}
