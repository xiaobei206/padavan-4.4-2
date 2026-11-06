#include "UArray.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static UArray *new_utf8(const unsigned char *bytes, size_t size)
{
	return UArray_newWithData_type_encoding_size_copy_(
		(void *)bytes, CTYPE_uint8_t, CENCODING_UTF8, size, 1);
}

static int is_utf16_value(const UArray *array, uint16_t expected)
{
	return array != NULL && array->encoding == CENCODING_UTF16 &&
		array->itemType == CTYPE_uint16_t && array->itemSize == sizeof(uint16_t) &&
		array->size == 1 && array->data != NULL &&
		((const uint16_t *)array->data)[0] == expected;
}

static int is_utf32_value(const UArray *array, uint32_t expected)
{
	return array != NULL && array->encoding == CENCODING_UTF32 &&
		array->itemType == CTYPE_uint32_t && array->itemSize == sizeof(uint32_t) &&
		array->size == 1 && array->data != NULL &&
		((const uint32_t *)array->data)[0] == expected;
}

int main(void)
{
	static const unsigned char ascii_bytes[] = {0x41, 0x42};
	static const unsigned char cent_bytes[] = {0xc2, 0xa2};
	static const unsigned char euro_bytes[] = {0xe2, 0x82, 0xac};
	static const unsigned char emoji_bytes[] = {0xf0, 0x9f, 0x92, 0xa9};
	static const unsigned char invalid_bytes[] = {0xc0, 0x80};
	UArray *array = NULL;
	int result = EXIT_FAILURE;

	UArray_convertToUTF8(NULL);
	UArray_convertToUTF16(NULL);
	UArray_convertToUTF32(NULL);
	if (UArray_convertToFixedSizeType(NULL) != 0) {
		fprintf(stderr, "NULL fixed-size conversion reported success\n");
		goto done;
	}
	array = new_utf8(ascii_bytes, sizeof(ascii_bytes));
	if (NULL == array) {
		fprintf(stderr, "ASCII fixture allocation failed\n");
		goto done;
	}
	if (!UArray_convertToFixedSizeType(array) || array->encoding != CENCODING_ASCII ||
		array->itemSize != 1 || array->size != sizeof(ascii_bytes) ||
		NULL == array->data || memcmp(array->data, ascii_bytes, sizeof(ascii_bytes)) != 0) {
		fprintf(stderr, "ASCII fixed-size conversion changed bytes or metadata incorrectly\n");
		goto done;
	}
	UArray_free(array);
	array = NULL;

	array = new_utf8(cent_bytes, sizeof(cent_bytes));
	if (NULL == array) {
		fprintf(stderr, "two-byte UTF-8 fixture allocation failed\n");
		goto done;
	}
	if (!UArray_convertToFixedSizeType(array) || !is_utf16_value(array, 0x00a2)) {
		fprintf(stderr, "two-byte UTF-8 conversion did not produce one UTF-16 code unit\n");
		goto done;
	}
	UArray_free(array);
	array = NULL;

	array = new_utf8(euro_bytes, sizeof(euro_bytes));
	if (NULL == array) {
		fprintf(stderr, "three-byte UTF-8 fixture allocation failed\n");
		goto done;
	}
	if (!UArray_convertToFixedSizeType(array) || !is_utf32_value(array, 0x20ac)) {
		fprintf(stderr, "three-byte UTF-8 conversion did not produce one UTF-32 code point\n");
		goto done;
	}
	UArray_free(array);
	array = NULL;

	array = new_utf8(emoji_bytes, sizeof(emoji_bytes));
	if (NULL == array) {
		fprintf(stderr, "four-byte UTF-8 fixture allocation failed\n");
		goto done;
	}
	if (!UArray_convertToFixedSizeType(array) || !is_utf32_value(array, 0x1f4a9)) {
		fprintf(stderr, "four-byte UTF-8 conversion did not preserve a supplementary code point\n");
		goto done;
	}
	UArray_free(array);
	array = NULL;

	array = new_utf8(invalid_bytes, sizeof(invalid_bytes));
	if (NULL == array) {
		fprintf(stderr, "invalid UTF-8 fixture allocation failed\n");
		goto done;
	}
	if (UArray_convertToFixedSizeType(array) != 0 ||
		array->encoding != CENCODING_UTF8 || array->itemType != CTYPE_uint8_t ||
		array->itemSize != 1 || array->size != sizeof(invalid_bytes) ||
		NULL == array->data || memcmp(array->data, invalid_bytes, sizeof(invalid_bytes)) != 0) {
		fprintf(stderr, "invalid UTF-8 conversion reported success or changed the source\n");
		goto done;
	}

	result = EXIT_SUCCESS;
	puts("UArray encoding conversion test passed");

done:
	if (array != NULL)
		UArray_free(array);
	return result;
}
