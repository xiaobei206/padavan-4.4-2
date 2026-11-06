#include "UArray.h"

#include <stdio.h>
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
    UArray_free(normal);
    UArray_free(only_marker);
    UArray_free(trailing);
    return result;
}
