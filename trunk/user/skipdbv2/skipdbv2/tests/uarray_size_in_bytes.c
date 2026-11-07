#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>

/* Internal size conversion helper retained in UArray.c for legacy callers. */
extern size_t UArray_sizeRequiredToContain_(const UArray *self, const UArray *other);

int main(void)
{
    unsigned char bytes[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
    UArray valid = UArray_stackAllocedWithData_type_size_(
        bytes, CTYPE_uint16_t, 3);
    UArray empty = UArray_stackAllocedEmptyUArray();
    UArray invalid = valid;
    UArray overflow = valid;
    UArray no_data = valid;
    UArray no_width = valid;
    UArray huge_bytes = { 0 };

    invalid.itemSize = 0;
    overflow.itemSize = 2;
    overflow.size = (size_t)-1 / overflow.itemSize + 1;
    no_data.data = NULL;
    no_width.itemSize = 0;
    huge_bytes.itemSize = 1;
    huge_bytes.size = (size_t)-1;

    if (UArray_sizeInBytes(NULL) != 0) {
        fprintf(stderr, "NULL UArray size query did not return zero\n");
        return EXIT_FAILURE;
    }
    if (UArray_byteAt_(NULL, 0) != 0 || UArray_byteAt_(&empty, 0) != 0) {
        fprintf(stderr, "invalid or empty UArray byte access did not return zero\n");
        return EXIT_FAILURE;
    }
    if (UArray_sizeInBytes(&valid) != sizeof(bytes)) {
        fprintf(stderr, "valid UArray byte size was calculated incorrectly\n");
        return EXIT_FAILURE;
    }
    if (UArray_byteAt_(&valid, 0) != bytes[0] ||
        UArray_byteAt_(&valid, 3) != bytes[3] ||
        UArray_byteAt_(&valid, 5) != bytes[5] ||
        UArray_byteAt_(&valid, sizeof(bytes)) != 0) {
        fprintf(stderr, "UArray byte accessor did not honor the full byte extent\n");
        return EXIT_FAILURE;
    }
    if (UArray_sizeInBytes(&empty) != 0) {
        fprintf(stderr, "empty UArray did not report zero bytes\n");
        return EXIT_FAILURE;
    }
    if (UArray_sizeInBytes(&invalid) != 0 || UArray_byteAt_(&invalid, 0) != 0) {
        fprintf(stderr, "zero-width UArray storage was accepted for byte access\n");
        return EXIT_FAILURE;
    }
    if (UArray_byteAt_(&no_data, 0) != 0) {
        fprintf(stderr, "UArray byte accessor accepted missing backing storage\n");
        return EXIT_FAILURE;
    }
    if (UArray_sizeInBytes(&overflow) != 0) {
        fprintf(stderr, "overflowing UArray byte size did not return zero\n");
        return EXIT_FAILURE;
    }
    if (UArray_byteAt_(&overflow, 0) != 0) {
        fprintf(stderr, "overflowing UArray byte geometry was accepted\n");
        return EXIT_FAILURE;
    }

    if (UArray_sizeRequiredToContain_(NULL, &valid) != 0 ||
        UArray_sizeRequiredToContain_(&valid, NULL) != 0 ||
        UArray_sizeRequiredToContain_(&no_width, &valid) != 0) {
        fprintf(stderr, "UArray item-count conversion accepted invalid arguments\\n");
        return EXIT_FAILURE;
    }
    if (UArray_sizeRequiredToContain_(&valid, &huge_bytes) !=
        (size_t)-1 / valid.itemSize + 1) {
        fprintf(stderr, "UArray item-count ceiling division overflowed near SIZE_MAX\\n");
        return EXIT_FAILURE;
    }

    UArray_stackFree(&valid);
    UArray_stackFree(&empty);
    puts("UArray byte-size test passed");
    return EXIT_SUCCESS;
}
