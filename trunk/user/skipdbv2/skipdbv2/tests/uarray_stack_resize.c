#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int testDuplicateIndexes(void)
{
    uint16_t source[] = {0x1234, 0x5678, 0x9abc};
    const uint16_t expected[] = {
        0x1234, 0x1234, 0x5678, 0x5678, 0x9abc, 0x9abc
    };
    uint8_t malformedSource = 0x5a;
    UArray duplicate = UArray_stackAllocedWithData_type_size_(
        source, CTYPE_uint16_t, sizeof(source) / sizeof(source[0]));
    UArray malformed = UArray_stackAllocedWithData_type_size_(
        &malformedSource, CTYPE_uint8_t, 1);
    size_t invalidSize;
    int okay = 0;

    UArray_duplicateIndexes(NULL);
    if (NULL == duplicate.data || NULL == malformed.data)
        goto done;
    UArray_duplicateIndexes(&duplicate);
    if (duplicate.size != sizeof(expected) / sizeof(expected[0]) ||
        duplicate.itemSize != sizeof(uint16_t) ||
        duplicate.stackAllocated || duplicate.data == (uint8_t *)source ||
        memcmp(duplicate.data, expected, sizeof(expected)) != 0 ||
        duplicate.data[sizeof(expected)] != 0 ||
        source[0] != 0x1234 || source[1] != 0x5678 || source[2] != 0x9abc) {
        fprintf(stderr, "UArray_duplicateIndexes changed the duplicate order or source storage\n");
        goto done;
    }

    invalidSize = (size_t)-1 / 2 + 1;
    malformed.size = invalidSize;
    UArray_duplicateIndexes(&malformed);
    if (malformed.size != invalidSize || malformed.data != &malformedSource) {
        fprintf(stderr, "UArray item-count overflow modified duplicate storage\n");
        goto done;
    }

    malformed.itemSize = 2;
    invalidSize = (size_t)-1 / 2;
    malformed.size = invalidSize;
    UArray_duplicateIndexes(&malformed);
    if (malformed.size != invalidSize || malformed.data != &malformedSource ||
        malformed.itemSize != 2) {
        fprintf(stderr, "UArray item-byte overflow modified duplicate storage\n");
        goto done;
    }

    okay = 1;

done:
    malformed.size = 1;
    malformed.itemSize = sizeof(malformedSource);
    UArray_stackFree(&malformed);
    UArray_stackFree(&duplicate);
    return okay;
}

int main(void)
{
    unsigned char grow_source[] = { 0x11, 0x22, 0x33 };
    unsigned char shrink_source[] = { 0xa1, 0xb2, 0xc3, 0xd4 };
    static const unsigned char grown_expected[] = {
        0x11, 0x22, 0x33, 0x00, 0x00
    };
    UArray grown = UArray_stackAllocedWithData_type_size_(
        grow_source, CTYPE_uint8_t, sizeof(grow_source));
    UArray shrunk = UArray_stackAllocedWithData_type_size_(
        shrink_source, CTYPE_uint8_t, sizeof(shrink_source));
    int result = EXIT_FAILURE;

    UArray_setSize_(&grown, 5);
    if (grown.stackAllocated || grown.data == grow_source ||
        grown.size != 5 || NULL == grown.data ||
        memcmp(grown.data, grown_expected, sizeof(grown_expected)) != 0 ||
        grown.data[5] != 0 ||
        memcmp(grow_source, grown_expected, sizeof(grow_source)) != 0) {
        fprintf(stderr, "growing a borrowed UArray did not detach and preserve its bytes\n");
        goto done;
    }

    UArray_setSize_(&shrunk, 2);
    if (shrunk.stackAllocated || shrunk.data == shrink_source ||
        shrunk.size != 2 || NULL == shrunk.data ||
        shrunk.data[0] != 0xa1 || shrunk.data[1] != 0xb2 ||
        shrunk.data[2] != 0 ||
        shrink_source[0] != 0xa1 || shrink_source[1] != 0xb2 ||
        shrink_source[2] != 0xc3 || shrink_source[3] != 0xd4) {
        fprintf(stderr, "shrinking a borrowed UArray did not detach without changing its source\n");
        goto done;
    }

    if (!testDuplicateIndexes()) {
        fprintf(stderr, "UArray_duplicateIndexes failed safety or duplication checks\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("UArray stack resize test passed");

done:
    UArray_stackFree(&shrunk);
    UArray_stackFree(&grown);
    return result;
}
