#include "UArray.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		fprintf(stderr, "UArray character test: %s\n", message); \
		result = EXIT_FAILURE; \
		goto done; \
	} \
} while (0)

int main(void)
{
	int8_t signedBytes[] = { INT8_MIN, -1, -2, 'A', 'z' };
	int16_t wideValues[] = { -1, 256, 'A', 'Z' };
	int16_t lowerValues[] = { 'a', 'b', 'z' };
	int16_t upperValues[] = { 'A', 'B', 'Z' };
	double floatValues[] = { NAN, INFINITY, -INFINITY, -1.0, 256.0, 65.0 };
	UArray *classified = NULL;
	UArray *wide = NULL;
	UArray *lower = NULL;
	UArray *upper = NULL;
	UArray *floats = NULL;
	UArray *malformed = NULL;
	int result = EXIT_FAILURE;

	UArray_isalnum(NULL);
	UArray_tolower(NULL);
	CHECK(0 == UArray_isLowercase(NULL), "NULL lowercase query did not fail safely");
	CHECK(0 == UArray_isUppercase(NULL), "NULL uppercase query did not fail safely");

	classified = UArray_newWithData_type_size_copy_(
		signedBytes, CTYPE_int8_t,
		sizeof(signedBytes) / sizeof(signedBytes[0]), 1);
	CHECK(NULL != classified, "signed-byte array allocation failed");
	classified->hash = (uintptr_t)0x4321u;
	UArray_isalpha(classified);
	{
		int8_t *values = (int8_t *)classified->data;
		CHECK(values[0] == 0 && values[1] == 0 && values[2] == 0 &&
			values[3] != 0 && values[4] != 0,
			"signed high bytes were not handled as non-ASCII characters");
	}
	CHECK(classified->hash == 0,
		"character classification did not invalidate the cached hash");

	wide = UArray_newWithData_type_size_copy_(
		wideValues, CTYPE_int16_t,
		sizeof(wideValues) / sizeof(wideValues[0]), 1);
	CHECK(NULL != wide, "wide integer array allocation failed");
	wide->hash = (uintptr_t)0x1234u;
	UArray_tolower(wide);
	{
		int16_t *values = (int16_t *)wide->data;
		CHECK(values[0] == -1 && values[1] == 256 &&
			values[2] == 'a' && values[3] == 'z',
			"case conversion changed values outside the unsigned-byte domain");
	}
	CHECK(wide->hash == 0,
		"case conversion did not invalidate the cached hash");
	UArray_toupper(wide);
	{
		int16_t *values = (int16_t *)wide->data;
		CHECK(values[0] == -1 && values[1] == 256 &&
			values[2] == 'A' && values[3] == 'Z',
			"uppercase conversion changed values outside the byte domain");
	}

	lower = UArray_newWithData_type_size_copy_(
		lowerValues, CTYPE_int16_t,
		sizeof(lowerValues) / sizeof(lowerValues[0]), 1);
	upper = UArray_newWithData_type_size_copy_(
		upperValues, CTYPE_int16_t,
		sizeof(upperValues) / sizeof(upperValues[0]), 1);
	CHECK(NULL != lower && NULL != upper, "case-query array allocation failed");
	CHECK(UArray_isLowercase(lower), "valid lowercase ASCII array was rejected");
	CHECK(UArray_isUppercase(upper), "valid uppercase ASCII array was rejected");
	CHECK(!UArray_isLowercase(wide) && !UArray_isUppercase(wide),
		"wide and negative numeric values were accepted as byte characters");

	floats = UArray_newWithData_type_size_copy_(
		floatValues, CTYPE_float64_t,
		sizeof(floatValues) / sizeof(floatValues[0]), 1);
	CHECK(NULL != floats, "floating-point array allocation failed");
	UArray_tolower(floats);
	{
		double *values = (double *)floats->data;
		CHECK(isnan(values[0]) && isinf(values[1]) && isinf(values[2]) &&
			values[3] == -1.0 && values[4] == 256.0 && values[5] == 'a',
			"floating-point values were narrowed before byte-range validation");
	}

	malformed = UArray_new();
	CHECK(NULL != malformed, "malformed-array fixture allocation failed");
	malformed->size = 1;
	malformed->data = NULL;
	malformed->itemType = CTYPE_int8_t;
	UArray_isalnum(malformed);
	UArray_toupper(malformed);
	CHECK(!UArray_isLowercase(malformed) && !UArray_isUppercase(malformed),
		"non-empty array with NULL storage was not rejected");

	result = EXIT_SUCCESS;
	puts("UArray character-domain test passed");

done:
	UArray_free(malformed);
	UArray_free(floats);
	UArray_free(upper);
	UArray_free(lower);
	UArray_free(wide);
	UArray_free(classified);
	return result;
}
