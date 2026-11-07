#include "UArray.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
	uint8_t alpha_bytes[] = {'A', 'b', '9', '!'};
	uint8_t lower_bytes[] = {'A', 'b', 'Z', 0xff};
	uint8_t signed_high_byte[] = {0xff};
	uint16_t wide_words[] = {'A', 'B'};
	UArray alpha = UArray_stackAllocedWithData_type_size_(
		alpha_bytes, CTYPE_uint8_t, sizeof(alpha_bytes));
	UArray lower = UArray_stackAllocedWithData_type_size_(
		lower_bytes, CTYPE_uint8_t, sizeof(lower_bytes));
	UArray signed_array = UArray_stackAllocedWithData_type_size_(
		signed_high_byte, CTYPE_int8_t, sizeof(signed_high_byte));
	UArray wide = UArray_stackAllocedWithData_type_size_(
		wide_words, CTYPE_uint16_t, sizeof(wide_words) / sizeof(wide_words[0]));
	UArray invalid, impossible;
	uint8_t alpha_snapshot[sizeof(alpha_bytes)];
	const uintptr_t sentinelHash = (uintptr_t)0x5a17;
	int result = EXIT_FAILURE;

	UArray_isalpha(NULL);
	UArray_tolower(NULL);
	UArray_toupper(NULL);
	if (UArray_isLowercase(NULL) != 0 || UArray_isUppercase(NULL) != 0) {
		fprintf(stderr, "NULL case query did not return false\n");
		goto done;
	}

	UArray_isalpha(&alpha);
	if (alpha_bytes[0] != 1 || alpha_bytes[1] != 1 ||
		alpha_bytes[2] != 0 || alpha_bytes[3] != 0) {
		fprintf(stderr, "ctype predicate results were not normalized to Boolean bytes\n");
		goto done;
	}

	UArray_tolower(&lower);
	if (lower_bytes[0] != 'a' || lower_bytes[1] != 'b' ||
		lower_bytes[2] != 'z' || lower_bytes[3] != 0xff) {
		fprintf(stderr, "tolower did not preserve valid byte semantics\n");
		goto done;
	}
	if (UArray_isLowercase(&lower) == 0) {
		fprintf(stderr, "lowercase byte query rejected a lowercase byte sequence\n");
		goto done;
	}
	lower_bytes[0] = 'A';
	if (UArray_isLowercase(&lower) != 0) {
		fprintf(stderr, "lowercase byte query accepted uppercase ASCII\n");
		goto done;
	}

	UArray_tolower(&signed_array);
	if (signed_high_byte[0] != 0xff) {
		fprintf(stderr, "signed high-byte input changed unexpectedly\n");
		goto done;
	}

	UArray_tolower(&wide);
	UArray_isalpha(&wide);
	if (wide_words[0] != 'A' || wide_words[1] != 'B' ||
		UArray_isLowercase(&wide) != 0) {
		fprintf(stderr, "wide numeric array was modified or treated as a byte string\n");
		goto done;
	}

	invalid = alpha;
	invalid.data = NULL;
	UArray_isalpha(&invalid);
	UArray_toupper(&invalid);
	if (UArray_isLowercase(&invalid) != 0) {
		fprintf(stderr, "invalid NULL-backed array was accepted\n");
		goto done;
	}


	/* SIZE_MAX bytes cannot leave room for UArray's trailing NUL byte. */
	impossible = alpha;
	memcpy(alpha_snapshot, alpha_bytes, sizeof(alpha_snapshot));
	impossible.size = (size_t)-1;
	impossible.hash = sentinelHash;
	UArray_isalpha(&impossible);
	UArray_tolower(&impossible);
	UArray_toupper(&impossible);
	if (UArray_isLowercase(&impossible) != 0 ||
		UArray_isUppercase(&impossible) != 0 ||
		impossible.size != (size_t)-1 ||
		impossible.data != alpha_bytes ||
		impossible.hash != sentinelHash ||
		memcmp(alpha_bytes, alpha_snapshot, sizeof(alpha_snapshot)) != 0) {
		fprintf(stderr, "character ctype operations accepted an unrepresentable SIZE_MAX byte extent\\n");
		goto done;
	}

	result = EXIT_SUCCESS;
	puts("uarray character ctype test passed");

done:
	return result;
}
