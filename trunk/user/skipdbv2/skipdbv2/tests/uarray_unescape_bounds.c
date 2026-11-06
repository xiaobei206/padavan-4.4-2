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
    UArray *single_backslash = UArray_newWithCString_("\\");
    UArray *trailing_backslash = UArray_newWithCString_("tail\\");
    UArray *escaped_newline = UArray_newWithCString_("a\\nb");
    UArray *escaped_backslash = UArray_newWithCString_("\\\\");
    const char expected_newline[] = { 'a', '\n', 'b' };
    int result = EXIT_FAILURE;

    UArray_unescape(NULL);

    if (NULL == single_backslash || NULL == trailing_backslash ||
        NULL == escaped_newline || NULL == escaped_backslash) {
        fprintf(stderr, "UArray unescape fixture allocation failed\n");
        goto done;
    }

    UArray_unescape(single_backslash);
    if (!equals_bytes(single_backslash, "\\", 1)) {
        fprintf(stderr, "a lone trailing backslash was not preserved\n");
        goto done;
    }

    UArray_unescape(trailing_backslash);
    if (!equals_bytes(trailing_backslash, "tail\\", 5)) {
        fprintf(stderr, "a trailing backslash after ordinary text was not preserved\n");
        goto done;
    }

    UArray_unescape(escaped_newline);
    if (!equals_bytes(escaped_newline, expected_newline, sizeof(expected_newline))) {
        fprintf(stderr, "ordinary escape decoding changed unexpectedly\n");
        goto done;
    }

    UArray_unescape(escaped_backslash);
    if (!equals_bytes(escaped_backslash, "\\", 1)) {
        fprintf(stderr, "escaped backslash decoding changed unexpectedly\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("UArray unescape bounds test passed");

done:
    UArray_free(single_backslash);
    UArray_free(trailing_backslash);
    UArray_free(escaped_newline);
    UArray_free(escaped_backslash);
    return result;
}
