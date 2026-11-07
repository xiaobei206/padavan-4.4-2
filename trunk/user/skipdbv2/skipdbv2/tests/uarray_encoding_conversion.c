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

static int preserves_embedded_nul_as_utf8(void *bytes, CTYPE type,
    CENCODING encoding, size_t count)
{
    static const unsigned char expected[] = {'A', '\0', 'B'};
    UArray *source = UArray_newWithData_type_encoding_size_copy_(
        bytes, type, encoding, count, 1);
    UArray *converted;
    int result;

    if (NULL == source)
        return 0;
    converted = UArray_asUTF8(source);
    result = converted != NULL &&
        converted->encoding == CENCODING_UTF8 &&
        converted->itemType == CTYPE_uint8_t &&
        converted->itemSize == sizeof(uint8_t) &&
        converted->size == sizeof(expected) &&
        converted->data != NULL &&
        memcmp(converted->data, expected, sizeof(expected)) == 0;
    if (converted != NULL)
        UArray_free(converted);
    UArray_free(source);
    return result;
}


static int preserves_embedded_nul_as_utf16_utf32(void *bytes, CTYPE type,
    CENCODING encoding, size_t count)
{
    static const uint16_t expected16[] = {'A', 0, 'B'};
    static const uint32_t expected32[] = {'A', 0, 'B'};
    UArray *source = UArray_newWithData_type_encoding_size_copy_(
        bytes, type, encoding, count, 1);
    UArray *converted16 = NULL;
    UArray *converted32 = NULL;
    int result = 0;

    if (NULL == source)
        return 0;
    converted16 = UArray_asUTF16(source);
    converted32 = UArray_asUTF32(source);
    result = converted16 != NULL &&
        converted16->encoding == CENCODING_UTF16 &&
        converted16->itemType == CTYPE_uint16_t &&
        converted16->itemSize == sizeof(uint16_t) &&
        converted16->size == sizeof(expected16) / sizeof(expected16[0]) &&
        converted16->data != NULL &&
        memcmp(converted16->data, expected16, sizeof(expected16)) == 0 &&
        converted32 != NULL &&
        converted32->encoding == CENCODING_UTF32 &&
        converted32->itemType == CTYPE_uint32_t &&
        converted32->itemSize == sizeof(uint32_t) &&
        converted32->size == sizeof(expected32) / sizeof(expected32[0]) &&
        converted32->data != NULL &&
        memcmp(converted32->data, expected32, sizeof(expected32)) == 0;

    UArray_free(converted32);
    UArray_free(converted16);
    UArray_free(source);
    return result;
}


typedef struct
{
    char leadingByte;
    uint16_t value;
} TestUTF16AlignmentProbe;

typedef struct
{
    char leadingByte;
    uint32_t value;
} TestUTF32AlignmentProbe;

static int rejects_misaligned_unicode_storage(void)
{
    union {
        uint16_t alignment;
        unsigned char bytes[sizeof(uint16_t) + 1];
    } storage16;
    union {
        uint32_t alignment;
        unsigned char bytes[sizeof(uint32_t) + 1];
    } storage32;
    UArray malformed16;
    UArray malformed32;
    UArray *converted;

    memset(storage16.bytes, 0, sizeof(storage16.bytes));
    memset(storage32.bytes, 0, sizeof(storage32.bytes));
    malformed16 = UArray_stackAllocedWithData_type_size_(
        storage16.bytes + 1, CTYPE_uint16_t, 1);
    malformed16.encoding = CENCODING_UTF16;
    malformed32 = UArray_stackAllocedWithData_type_size_(
        storage32.bytes + 1, CTYPE_uint32_t, 1);
    malformed32.encoding = CENCODING_UTF32;

    if (((uintptr_t)malformed16.data %
            offsetof(TestUTF16AlignmentProbe, value)) == 0 ||
        ((uintptr_t)malformed32.data %
            offsetof(TestUTF32AlignmentProbe, value)) == 0)
        return 0;

    converted = UArray_asUTF8(&malformed16);
    if (converted) {
        UArray_free(converted);
        return 0;
    }
    converted = UArray_asUTF16(&malformed16);
    if (converted) {
        UArray_free(converted);
        return 0;
    }
    converted = UArray_asUTF32(&malformed16);
    if (converted) {
        UArray_free(converted);
        return 0;
    }

    converted = UArray_asUTF8(&malformed32);
    if (converted) {
        UArray_free(converted);
        return 0;
    }
    converted = UArray_asUTF16(&malformed32);
    if (converted) {
        UArray_free(converted);
        return 0;
    }
    converted = UArray_asUTF32(&malformed32);
    if (converted) {
        UArray_free(converted);
        return 0;
    }
    return 1;
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
	{
		int conversion_result = UArray_convertToFixedSizeType(array);
		uint32_t actual = 0;
		if (array->data != NULL && array->size > 0 &&
			array->itemSize == sizeof(uint32_t))
			actual = ((const uint32_t *)array->data)[0];
		if (!conversion_result || !is_utf32_value(array, 0x20ac)) {
			fprintf(stderr,
				"three-byte UTF-8 conversion failed: result=%d encoding=%u itemType=%d itemSize=%zu size=%zu data=%p first=0x%08x\n",
				conversion_result, (unsigned int)array->encoding, (int)array->itemType,
				array->itemSize, array->size, (void *)array->data, (unsigned int)actual);
			goto done;
		}
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


	{
		const uint64_t numericValue = UINT64_C(0x123456789abcdef0);
		array = UArray_newWithData_type_encoding_size_copy_(
			(void *)&numericValue, CTYPE_uint64_t, CENCODING_NUMBER, 1, 1);
		if (NULL == array) {
			fprintf(stderr, "numeric-layout fixture allocation failed\\n");
			goto done;
		}
		{
			UArray *converted = NULL;
			size_t originalItemSize = array->itemSize;
			array->itemSize = sizeof(uint8_t);
			converted = UArray_asUTF8(array);
			array->itemSize = originalItemSize;
			if (NULL != converted || array->itemType != CTYPE_uint64_t ||
				array->itemSize != sizeof(uint64_t) || array->size != 1 ||
				NULL == array->data ||
				memcmp(array->data, &numericValue, sizeof(numericValue)) != 0) {
				UArray_free(converted);
				fprintf(stderr, "numeric conversion accepted mismatched item metadata\\n");
				goto done;
			}
		}
		UArray_free(array);
		array = NULL;
	}

	{
		unsigned char embedded_utf8[] = {'A', '\0', 'B'};
		uint16_t embedded_utf16[] = {'A', 0, 'B'};
		uint32_t embedded_utf32[] = {'A', 0, 'B'};

		if (!preserves_embedded_nul_as_utf8(embedded_utf8, CTYPE_uint8_t,
				CENCODING_UTF8, sizeof(embedded_utf8)) ||
			!preserves_embedded_nul_as_utf8(embedded_utf16, CTYPE_uint16_t,
				CENCODING_UTF16, sizeof(embedded_utf16) / sizeof(embedded_utf16[0])) ||
			!preserves_embedded_nul_as_utf8(embedded_utf32, CTYPE_uint32_t,
				CENCODING_UTF32, sizeof(embedded_utf32) / sizeof(embedded_utf32[0])) ||
			!preserves_embedded_nul_as_utf16_utf32(embedded_utf8, CTYPE_uint8_t,
				CENCODING_UTF8, sizeof(embedded_utf8)) ||
			!preserves_embedded_nul_as_utf16_utf32(embedded_utf16, CTYPE_uint16_t,
				CENCODING_UTF16, sizeof(embedded_utf16) / sizeof(embedded_utf16[0])) ||
			!preserves_embedded_nul_as_utf16_utf32(embedded_utf32, CTYPE_uint32_t,
				CENCODING_UTF32, sizeof(embedded_utf32) / sizeof(embedded_utf32[0]))) {
			fprintf(stderr, "UTF-16/UTF-32 conversion truncated data after an embedded NUL\n");
			goto done;
		}
	}

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

	{
		uint64_t wideStorage[] = {
			UINT64_C(0x81), UINT64_C(0x82)
		};
		UArray malformed = UArray_stackAllocedWithData_type_size_(
			wideStorage, CTYPE_uint64_t, 2);
		malformed.encoding = CENCODING_UTF8;
		malformed.itemSize = sizeof(uint8_t);

		if (UArray_isMultibyte(&malformed) != 0) {
			fprintf(stderr, "UArray multibyte detection accepted a wide item type over byte storage\n");
			goto done;
		}
	}
	if (!rejects_misaligned_unicode_storage()) {
		fprintf(stderr, "UArray accepted misaligned UTF-16/UTF-32 source storage\\n");
		goto done;
	}
	result = EXIT_SUCCESS;
	puts("UArray encoding conversion test passed");

done:
	if (array != NULL)
		UArray_free(array);
	return result;
}
