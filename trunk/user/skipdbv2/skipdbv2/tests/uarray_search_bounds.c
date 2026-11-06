#include "UArray.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int check_valid_searches(void)
{
	unsigned char bytes[] = { 'a', 'B', 'c', 'A', 'b' };
	unsigned char pattern_bytes[] = { 'A', 'b' };
	unsigned char case_pattern_bytes[] = { 'b', 'C' };
	unsigned char value_bytes[] = { 'B', 'x' };
	UArray source = UArray_stackAllocedWithData_type_size_(
		bytes, CTYPE_uint8_t, sizeof(bytes));
	UArray pattern = UArray_stackAllocedWithData_type_size_(
		pattern_bytes, CTYPE_uint8_t, sizeof(pattern_bytes));
	UArray case_pattern = UArray_stackAllocedWithData_type_size_(
		case_pattern_bytes, CTYPE_uint8_t, sizeof(case_pattern_bytes));
	UArray values = UArray_stackAllocedWithData_type_size_(
		value_bytes, CTYPE_uint8_t, sizeof(value_bytes));
	UArray empty = UArray_stackAllocedEmptyUArray();
	int result = 0;

	if (UArray_find_(&source, &pattern) != 3 ||
		!UArray_contains_(&source, &pattern) ||
		UArray_find_from_(&source, &pattern, 3) != 3 ||
		UArray_find_from_(&source, &pattern, 4) != -1 ||
		UArray_findAnyCase_(&source, &case_pattern) != 1 ||
		!UArray_containsAnyCase_(&source, &case_pattern) ||
		UArray_rFind_(&source, &pattern) != 3 ||
		UArray_rFind_from_(&source, &pattern, 4) != -1 ||
		UArray_rFind_from_(&source, &pattern, 5) != 3 ||
		UArray_rFindAnyCase_(&source, &case_pattern) != 1 ||
		UArray_rFindAnyValue_(&source, &values) != 1 ||
		UArray_findLongValue_(&source, 'c') != 2 ||
		!UArray_containsLong_(&source, 'c') ||
		UArray_findDoubleValue_(&source, (double)'A') != 3 ||
		!UArray_containsDouble_(&source, (double)'A') ||
		UArray_find_from_(&empty, &pattern, 0) != -1 ||
		UArray_rFind_from_(&empty, &pattern, 0) != -1) {
		fprintf(stderr, "valid forward, reverse, case-insensitive, scalar, or empty search failed\n");
		goto done;
	}

	result = 1;

done:
	UArray_stackFree(&empty);
	UArray_stackFree(&values);
	UArray_stackFree(&case_pattern);
	UArray_stackFree(&pattern);
	UArray_stackFree(&source);
	return result;
}

static int check_invalid_source(const UArray *invalid, const UArray *valid)
{
	if (UArray_find_(invalid, valid) != -1 ||
		UArray_contains_(invalid, valid) != 0 ||
		UArray_find_from_(invalid, valid, 0) != -1 ||
		UArray_findAnyCase_(invalid, valid) != -1 ||
		UArray_containsAnyCase_(invalid, valid) != 0 ||
		UArray_findLongValue_(invalid, 'x') != -1 ||
		UArray_containsLong_(invalid, 'x') != 0 ||
		UArray_findDoubleValue_(invalid, (double)'x') != -1 ||
		UArray_containsDouble_(invalid, (double)'x') != 0 ||
		UArray_rFind_(invalid, valid) != -1 ||
		UArray_rFind_from_(invalid, valid, 0) != -1 ||
		UArray_rFindAnyCase_(invalid, valid) != -1 ||
		UArray_rFindAnyValue_(invalid, valid) != -1) {
		fprintf(stderr, "malformed or NULL UArray search source was not rejected\n");
		return 0;
	}
	return 1;
}

static int check_invalid_pattern(const UArray *valid, const UArray *invalid)
{
	if (UArray_find_(valid, invalid) != -1 ||
		UArray_find_from_(valid, invalid, 0) != -1 ||
		UArray_findAnyCase_(valid, invalid) != -1 ||
		UArray_rFind_(valid, invalid) != -1 ||
		UArray_rFind_from_(valid, invalid, 0) != -1 ||
		UArray_rFindAnyCase_(valid, invalid) != -1 ||
		UArray_rFindAnyValue_(valid, invalid) != -1) {
		fprintf(stderr, "malformed or NULL UArray search pattern was not rejected\n");
		return 0;
	}
	return 1;
}

int main(void)
{
	unsigned char bytes[] = { 'a', 'B', 'c', 'A', 'b' };
	unsigned char pattern_bytes[] = { 'A', 'b' };
	UArray source = UArray_stackAllocedWithData_type_size_(
		bytes, CTYPE_uint8_t, sizeof(bytes));
	UArray pattern = UArray_stackAllocedWithData_type_size_(
		pattern_bytes, CTYPE_uint8_t, sizeof(pattern_bytes));
	UArray invalid_data = source;
	UArray invalid_layout = source;
	UArray invalid_type = source;
	UArray oversized = source;
	int result = EXIT_FAILURE;

	invalid_data.data = NULL;
	invalid_data.size = 1;
	invalid_layout.itemSize = 0;
	invalid_type.itemType = (CTYPE)999;
	oversized.size = (size_t)LONG_MAX + 1;

	if (!check_valid_searches()) {
		goto done;
	}
	if (!check_invalid_source(NULL, &pattern) ||
		!check_invalid_pattern(&source, NULL) ||
		!check_invalid_source(&invalid_data, &pattern) ||
		!check_invalid_pattern(&source, &invalid_data) ||
		!check_invalid_source(&invalid_layout, &pattern) ||
		!check_invalid_pattern(&source, &invalid_layout) ||
		!check_invalid_source(&invalid_type, &pattern) ||
		!check_invalid_pattern(&source, &invalid_type) ||
		!check_invalid_source(&oversized, &pattern)) {
		goto done;
	}

	result = EXIT_SUCCESS;
	puts("UArray search bounds test passed");

done:
	UArray_stackFree(&pattern);
	UArray_stackFree(&source);
	return result;
}
