#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    result = EXIT_SUCCESS;
    puts("UArray stack resize test passed");

done:
    UArray_stackFree(&shrunk);
    UArray_stackFree(&grown);
    return result;
}
