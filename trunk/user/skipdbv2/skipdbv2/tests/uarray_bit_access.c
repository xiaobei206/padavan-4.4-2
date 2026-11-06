#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return EXIT_FAILURE;
}

int main(void)
{
    const unsigned char initial[] = { 0x00, 0x00, 0x00 };
    const unsigned char expected[] = { 0x80, 0x00, 0x01 };
    unsigned char snapshot[sizeof(expected)];
    unsigned char *originalData;
    size_t originalSize, originalItemSize, malformedBitCount;
    static const char expectedBitString[] = "000000010000000010000000";
    UArray *bits = NULL;
    UArray *invalidBits = NULL;
    UArray *array = UArray_newWithData_type_size_copy_(
        (void *)initial, CTYPE_uint8_t, sizeof(initial), 1);
    int invalidBit, malformedSummariesRejected;

    if (NULL == array)
        return fail("UArray bit access allocation failed");

    UArray_setBit_at_(array, 1, 0);
    UArray_setBit_at_(array, 1, 7);
    UArray_setBit_at_(array, 1, 8);
    UArray_setBit_at_(array, 1, 8);
    UArray_setBit_at_(array, 1, 15);
    UArray_setBit_at_(array, 1, 16);
    if (array->data[0] != 0x81 || array->data[1] != 0x81 ||
        array->data[2] != 0x01) {
        UArray_free(array);
        return fail("UArray bit setter used the wrong bit after a byte boundary");
    }

    UArray_setBit_at_(array, 0, 8);
    UArray_setBit_at_(array, 0, 8);
    UArray_setBit_at_(array, 0, 15);
    UArray_setBit_at_(array, 0, 0);
    if (memcmp(array->data, expected, sizeof(expected)) != 0) {
        UArray_free(array);
        return fail("UArray bit false assignment toggled instead of clearing");
    }

    memcpy(snapshot, array->data, sizeof(snapshot));
    if (UArray_bitAt_(NULL, 0) != 0 ||
        UArray_bitAt_(array, 0) != 0 ||
        UArray_bitAt_(array, 7) != 1 ||
        UArray_bitAt_(array, 8) != 0 ||
        UArray_bitAt_(array, 15) != 0 ||
        UArray_bitAt_(array, 16) != 1 ||
        UArray_bitAt_(array, 23) != 0) {
        UArray_free(array);
        return fail("UArray bit reader returned an incorrect byte-boundary result");
    }
    if (memcmp(array->data, snapshot, sizeof(snapshot)) != 0) {
        UArray_free(array);
        return fail("UArray bit read mutated the source bytes");
    }

    if (UArray_bitCount(array) != 2) {
        UArray_free(array);
        return fail("UArray bit count did not count set bits");
    }
    bits = UArray_asBits(array);
    if (NULL == bits || bits->size != sizeof(expectedBitString) - 1 ||
        NULL == bits->data ||
        memcmp(bits->data, expectedBitString, sizeof(expectedBitString) - 1) != 0 ||
        bits->data[bits->size] != 0) {
        UArray_free(bits);
        UArray_free(array);
        return fail("UArray bit string reader returned incorrect bits");
    }
    UArray_free(bits);
    bits = NULL;
    if (UArray_bitCount(NULL) != 0 || UArray_asBits(NULL) != NULL) {
        UArray_free(array);
        return fail("UArray bit summaries accepted NULL storage");
    }

    UArray_setBit_at_(array, 1, 24);
    UArray_setBit_at_(array, 1, (size_t)-1);
    if (UArray_bitAt_(array, 24) != 0 ||
        UArray_bitAt_(array, (size_t)-1) != 0 ||
        memcmp(array->data, snapshot, sizeof(snapshot)) != 0) {
        UArray_free(array);
        return fail("UArray invalid bit index was not rejected");
    }

    array->hash = (uintptr_t)0x1234;
    UArray_setBit_at_(array, 1, 16);
    if (array->hash != 0) {
        UArray_free(array);
        return fail("UArray bit mutation did not invalidate the cached hash");
    }

    originalData = array->data;
    originalSize = array->size;
    array->data = NULL;
    array->size = 1;
    invalidBit = UArray_bitAt_(array, 0);
    UArray_setBit_at_(array, 1, 0);
    malformedBitCount = UArray_bitCount(array);
    invalidBits = UArray_asBits(array);
    malformedSummariesRejected = malformedBitCount == 0 && NULL == invalidBits;
    array->data = originalData;
    array->size = originalSize;
    UArray_free(invalidBits);
    invalidBits = NULL;
    if (invalidBit != 0 || !malformedSummariesRejected ||
        memcmp(array->data, snapshot, sizeof(snapshot)) != 0) {
        UArray_free(array);
        return fail("UArray malformed backing storage was not rejected");
    }

    originalItemSize = array->itemSize;
    array->itemSize = 0;
    invalidBit = UArray_bitAt_(array, 0);
    UArray_setBit_at_(array, 1, 0);
    array->itemSize = originalItemSize;
    if (invalidBit != 0 ||
        memcmp(array->data, snapshot, sizeof(snapshot)) != 0) {
        UArray_free(array);
        return fail("UArray zero-width bit storage was not rejected");
    }

    originalSize = array->size;
    array->itemSize = 2;
    array->size = (size_t)-1 / 2 + 1;
    invalidBit = UArray_bitAt_(array, 0);
    UArray_setBit_at_(array, 1, 0);
    array->size = originalSize;
    array->itemSize = originalItemSize;
    if (invalidBit != 0 ||
        memcmp(array->data, snapshot, sizeof(snapshot)) != 0) {
        UArray_free(array);
        return fail("UArray overflowing bit storage geometry was not rejected");
    }

    UArray_free(array);
    puts("UArray bit access test passed");
    return EXIT_SUCCESS;
}
