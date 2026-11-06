#include "UArray.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return EXIT_FAILURE;
}

static int partEquals(const UArray *part, const char *expected)
{
    size_t length = strlen(expected);
    return part && part->itemSize == 1 && part->size == length &&
        (length == 0 || (part->data && memcmp(part->data, expected, length) == 0));
}

static void freeParts(PtrUArray *parts)
{
    size_t i;
    if (NULL == parts)
        return;
    for (i = 0; i < parts->size; ++i)
        UArray_free((UArray *)UArray_pointerAt_(parts, i));
    UArray_free(parts);
}

int main(void)
{
    UArray input = UArray_stackAllocedWithCString_("alpha,beta;gamma");
    UArray comma = UArray_stackAllocedWithCString_(",");
    UArray semicolon = UArray_stackAllocedWithCString_(";");
    UArray malformedInput;
    UArray *delims = UArray_new();
    PtrUArray *parts;
    unsigned char *savedData;
    size_t savedSize, savedItemSize;
    CTYPE savedType;

    if (NULL == delims)
        return fail("UArray split delimiter allocation failed");
    UArray_setItemType_(delims, CTYPE_uintptr_t);
    UArray_appendPointer_(delims, &comma);
    UArray_appendPointer_(delims, &semicolon);
    if (delims->itemType != CTYPE_uintptr_t ||
        delims->itemSize != sizeof(void *) || delims->size != 2) {
        UArray_free(delims);
        return fail("UArray split delimiter fixture setup failed");
    }

    parts = UArray_split_(&input, delims);
    if (NULL == parts || parts->size != 3 ||
        !partEquals((UArray *)UArray_pointerAt_(parts, 0), "alpha") ||
        !partEquals((UArray *)UArray_pointerAt_(parts, 1), "beta") ||
        !partEquals((UArray *)UArray_pointerAt_(parts, 2), "gamma")) {
        freeParts(parts);
        UArray_free(delims);
        return fail("UArray split valid delimiter round trip failed");
    }
    freeParts(parts);
    if (UArray_splitCount_(&input, delims) != 3) {
        UArray_free(delims);
        return fail("UArray split count changed for valid delimiters");
    }

    savedItemSize = delims->itemSize;
    delims->itemSize = 1;
    parts = UArray_split_(&input, delims);
    delims->itemSize = savedItemSize;
    if (NULL != parts) {
        freeParts(parts);
        UArray_free(delims);
        return fail("UArray split accepted malformed delimiter element width");
    }

    savedType = delims->itemType;
    delims->itemType = CTYPE_uint8_t;
    parts = UArray_split_(&input, delims);
    delims->itemType = savedType;
    if (NULL != parts) {
        freeParts(parts);
        UArray_free(delims);
        return fail("UArray split accepted a non-pointer delimiter container");
    }

    savedData = delims->data;
    delims->data = NULL;
    parts = UArray_split_(&input, delims);
    delims->data = savedData;
    if (NULL != parts) {
        freeParts(parts);
        UArray_free(delims);
        return fail("UArray split accepted missing delimiter backing storage");
    }

    savedSize = delims->size;
    delims->size = (size_t)-1 / delims->itemSize + 1;
    parts = UArray_split_(&input, delims);
    delims->size = savedSize;
    if (NULL != parts) {
        freeParts(parts);
        UArray_free(delims);
        return fail("UArray split accepted overflowing delimiter geometry");
    }

    malformedInput = input;
    malformedInput.itemSize = 0;
    if (NULL != UArray_split_(&malformedInput, delims)) {
        UArray_free(delims);
        return fail("UArray split accepted zero-width source storage");
    }
    malformedInput = input;
    malformedInput.data = NULL;
    if (NULL != UArray_split_(&malformedInput, delims)) {
        UArray_free(delims);
        return fail("UArray split accepted missing source backing storage");
    }
    malformedInput = input;
    malformedInput.size = (size_t)LONG_MAX + 1u;
    if (NULL != UArray_split_(&malformedInput, delims)) {
        UArray_free(delims);
        return fail("UArray split accepted an unrepresentable source length");
    }

    UArray_free(delims);
    puts("UArray split delimiter validation test passed");
    return EXIT_SUCCESS;
}
