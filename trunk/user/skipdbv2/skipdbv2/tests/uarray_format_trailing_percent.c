#include "UArray.h"

#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

static int equals_bytes(const UArray *value, const char *expected, size_t length)
{
    return value != NULL &&
        UArray_size(value) == length &&
        (length == 0 || memcmp(UArray_bytes(value), expected, length) == 0);
}

int main(void)
{
    UArray *trailing = UArray_newWithFormat_("prefix%");
    UArray *only_marker = UArray_newWithFormat_("%");
    UArray *normal = UArray_newWithFormat_("value %i", 42);
    UArray *aliased_format = NULL;
    UArray *large_float = NULL;
    char expected_large_float[512];
    int expected_large_float_length;
    int result = EXIT_FAILURE;

    if (!equals_bytes(trailing, "prefix%", 7)) {
        fprintf(stderr, "trailing format marker was not preserved safely\n");
        goto done;
    }
    if (!equals_bytes(only_marker, "%", 1)) {
        fprintf(stderr, "format consisting only of a percent marker was mishandled\n");
        goto done;
    }
    if (!equals_bytes(normal, "value 42", 8)) {
        fprintf(stderr, "ordinary UArray formatting changed unexpectedly\n");
        goto done;
    
    aliased_format = UArray_newWithCString_("value %i");
    if (NULL == aliased_format) {
        fprintf(stderr, "UArray aliased-format fixture allocation failed\n");
        goto done;
    }
    {
        const char *format = (const char *)UArray_bytes(aliased_format);
        UArray_fromFormat_(aliased_format, format, 7);
    }
    if (!equals_bytes(aliased_format, "value %ivalue 7", 15)) {
        fprintf(stderr, "aliased format string was invalidated during append\n");
        goto done;
    }
    UArray_free(aliased_format);
    aliased_format = NULL;}

    expected_large_float_length = snprintf(expected_large_float,
        sizeof(expected_large_float), "%f", DBL_MAX);
    large_float = UArray_newWithFormat_("%f", DBL_MAX);
    if (expected_large_float_length < 0 ||
        (size_t)expected_large_float_length >= sizeof(expected_large_float) ||
        expected_large_float_length <= 100 ||
        !equals_bytes(large_float, expected_large_float,
            (size_t)expected_large_float_length)) {
        fprintf(stderr, "UArray floating formatter truncated DBL_MAX output\n");
        goto done;
    }

    {
        const unsigned char raw_bytes[] = { 0x00, 0xab, 0xff };
        UArray *bytes = UArray_newWithData_type_size_copy_(
            (void *)raw_bytes, CTYPE_uint8_t, sizeof(raw_bytes), 1);
        UArray *hex = NULL;
        if (NULL == bytes) {
            fprintf(stderr, "UArray hex conversion fixture allocation failed\n");
            goto done;
        }
        hex = UArray_asNewHexStringUArray(bytes);
        if (!equals_bytes(hex, "00abff", 6)) {
            fprintf(stderr, "UArray hex conversion changed byte order or casing\n");
            UArray_free(hex);
            UArray_free(bytes);
            goto done;
        }
        UArray_free(hex);
        hex = NULL;

        bytes->itemType = CTYPE_int8_t;
        hex = UArray_asNewHexStringUArray(bytes);
        if (!equals_bytes(hex, "00abff", 6)) {
            fprintf(stderr, "UArray hex conversion rejected a valid signed byte array\n");
            UArray_free(hex);
            UArray_free(bytes);
            goto done;
        }
        UArray_free(hex);
        hex = NULL;

        /* A one-byte width paired with a wider type must never be read as a wide value. */
        bytes->itemType = CTYPE_uint64_t;
        hex = UArray_asNewHexStringUArray(bytes);
        bytes->itemType = CTYPE_uint8_t;
        if (NULL != hex) {
            fprintf(stderr, "UArray hex conversion accepted malformed byte type\n");
            UArray_free(hex);
            UArray_free(bytes);
            goto done;
        }
        UArray_free(bytes);
    }

    result = EXIT_SUCCESS;
    puts("UArray trailing format marker and hex-byte test passed");

done:
    UArray_free(aliased_format);
    UArray_free(large_float);
    UArray_free(normal);
    UArray_free(only_marker);
    UArray_free(trailing);
    return result;
}
