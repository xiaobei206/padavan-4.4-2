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

    result = EXIT_SUCCESS;
    puts("UArray trailing format marker test passed");

done:
    UArray_free(normal);
    UArray_free(only_marker);
    UArray_free(trailing);
    return result;
}
