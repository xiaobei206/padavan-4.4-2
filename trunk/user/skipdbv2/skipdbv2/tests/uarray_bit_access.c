#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return EXIT_FAILURE;
}

static int test_bitwise_mutations(void)
{
    const unsigned char initial[] = {0x0f, 0x55, 0xaa};
    const unsigned char rhs[] = {0xf0, 0x0f, 0x0f};
    const unsigned char expectedOr[] = {0xff, 0x5f, 0xaf};
    const unsigned char expectedAnd[] = {0x00, 0x05, 0x0a};
    const unsigned char expectedXor[] = {0xff, 0x5a, 0xa5};
    const unsigned char expectedNot[] = {0xf0, 0xaa, 0x55};
    const unsigned char expectedAll[] = {0xff, 0xff, 0xff};
    const unsigned char expectedNone[] = {0x00, 0x00, 0x00};
    UArray *left = UArray_newWithData_type_size_copy_((void *)initial, CTYPE_uint8_t, 3, 1);
    UArray *right = UArray_newWithData_type_size_copy_((void *)rhs, CTYPE_uint8_t, 3, 1);
    uint8_t *originalData;
    size_t originalSize, originalItemSize;

    if (NULL == left || NULL == right) {
        UArray_free(left);
        UArray_free(right);
        return fail("UArray bitwise mutation allocation failed");
    }

    left->hash = (uintptr_t)0x1111;
    UArray_bitwiseOr_(left, right);
    if (memcmp(left->data, expectedOr, sizeof(expectedOr)) != 0 || left->hash != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise OR failed or retained the cached hash");
    }

    memcpy(left->data, initial, sizeof(initial));
    left->hash = (uintptr_t)0x2222;
    UArray_bitwiseAnd_(left, right);
    if (memcmp(left->data, expectedAnd, sizeof(expectedAnd)) != 0 || left->hash != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise AND failed or retained the cached hash");
    }

    memcpy(left->data, initial, sizeof(initial));
    left->hash = (uintptr_t)0x3333;
    UArray_bitwiseXor_(left, right);
    if (memcmp(left->data, expectedXor, sizeof(expectedXor)) != 0 || left->hash != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise XOR failed or retained the cached hash");
    }

    memcpy(left->data, initial, sizeof(initial));
    left->hash = (uintptr_t)0x4444;
    UArray_bitwiseNot(left);
    if (memcmp(left->data, expectedNot, sizeof(expectedNot)) != 0 || left->hash != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise NOT failed or retained the cached hash");
    }

    left->hash = (uintptr_t)0x5555;
    UArray_setAllBitsTo_(left, 1);
    if (memcmp(left->data, expectedAll, sizeof(expectedAll)) != 0 || left->hash != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray set-all-bits true failed or retained the cached hash");
    }

    left->hash = (uintptr_t)0x6666;
    UArray_setAllBitsTo_(left, 0);
    if (memcmp(left->data, expectedNone, sizeof(expectedNone)) != 0 || left->hash != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray set-all-bits false failed or retained the cached hash");
    }

    UArray_bitwiseOr_(NULL, right);
    UArray_bitwiseAnd_(left, NULL);
    UArray_bitwiseXor_(NULL, NULL);
    UArray_bitwiseNot(NULL);
    UArray_setAllBitsTo_(NULL, 1);

    originalData = left->data;
    originalSize = left->size;
    left->hash = (uintptr_t)0x7777;
    left->data = NULL;
    left->size = 1;
    UArray_bitwiseNot(left);
    UArray_setAllBitsTo_(left, 1);
    UArray_bitwiseOr_(left, right);
    left->data = originalData;
    left->size = originalSize;
    if (left->hash != (uintptr_t)0x7777 ||
        memcmp(left->data, expectedNone, sizeof(expectedNone)) != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise mutations accepted missing receiver storage");
    }

    originalData = right->data;
    right->data = NULL;
    left->hash = (uintptr_t)0x8888;
    UArray_bitwiseAnd_(left, right);
    right->data = originalData;
    if (left->hash != (uintptr_t)0x8888 ||
        memcmp(left->data, expectedNone, sizeof(expectedNone)) != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray binary bitwise mutation accepted missing operand storage");
    }

    originalItemSize = left->itemSize;
    left->hash = (uintptr_t)0x9999;
    left->itemSize = 0;
    UArray_bitwiseNot(left);
    UArray_setAllBitsTo_(left, 1);
    UArray_bitwiseXor_(left, right);
    left->itemSize = originalItemSize;
    if (left->hash != (uintptr_t)0x9999 ||
        memcmp(left->data, expectedNone, sizeof(expectedNone)) != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise mutations accepted zero-width storage");
    }

    originalSize = left->size;
    left->hash = (uintptr_t)0xaaaa;
    left->size = (size_t)-1 / left->itemSize + 1;
    UArray_bitwiseNot(left);
    UArray_bitwiseOr_(left, right);
    left->size = originalSize;
    if (left->hash != (uintptr_t)0xaaaa ||
        memcmp(left->data, expectedNone, sizeof(expectedNone)) != 0) {
        UArray_free(left); UArray_free(right);
        return fail("UArray bitwise mutations accepted overflowing storage geometry");
    }

    UArray_free(left);
    UArray_free(right);
    return EXIT_SUCCESS;
}

int main(void)
{
    if (test_bitwise_mutations() != EXIT_SUCCESS)
        return EXIT_FAILURE;

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

    {
        UArray overflowBits;
        uint8_t sentinel = 0xff;

        memset(&overflowBits, 0, sizeof(overflowBits));
        overflowBits.data = &sentinel;
        overflowBits.size = ((size_t)-1 / 8) + 1;
        overflowBits.itemSize = 1;
        if (UArray_bitCount(&overflowBits) != 0) {
            UArray_free(array);
            return fail("UArray bit count accepted an overflowing result size");
        }
    }

    UArray_free(array);
    puts("UArray bit access test passed");
    return EXIT_SUCCESS;
}
