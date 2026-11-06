#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return EXIT_FAILURE;
}

static int bytesEqual(const UArray *array, const unsigned char *expected, size_t size)
{
    return array && array->itemSize == 1 && array->size == size &&
        (size == 0 || (array->data && memcmp(array->data, expected, size) == 0));
}

int main(void)
{
    const unsigned char inputBytes[] = { 'a', 'b', 'c', 'a' };
    const unsigned char expected[] = { 'x', 'y', 'c', 'x' };
    const unsigned char unchanged[] = { 'a', 'b', 'c', 'a' };
    const unsigned char fromBytes[] = { 'a', 'b' };
    const unsigned char toBytes[] = { 'x', 'y' };
    const unsigned char shortToBytes[] = { 'x' };
    UArray *input = UArray_newWithData_type_size_copy_(
        (void *)inputBytes, CTYPE_uint8_t, sizeof(inputBytes), 1);
    UArray *from = UArray_newWithData_type_size_copy_(
        (void *)fromBytes, CTYPE_uint8_t, sizeof(fromBytes), 1);
    UArray *to = UArray_newWithData_type_size_copy_(
        (void *)toBytes, CTYPE_uint8_t, sizeof(toBytes), 1);
    UArray *shortTo = UArray_newWithData_type_size_copy_(
        (void *)shortToBytes, CTYPE_uint8_t, sizeof(shortToBytes), 1);
    UArray malformed;

    if (!input || !from || !to || !shortTo) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translate test allocation failed");
    }

    input->hash = (uintptr_t)0x1234;
    UArray_translate(input, from, to);
    if (!bytesEqual(input, expected, sizeof(expected)) || input->hash != 0) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray valid byte translation or hash invalidation failed");
    }

    memcpy(input->data, unchanged, sizeof(unchanged));
    UArray_translate(input, from, shortTo);
    if (!bytesEqual(input, unchanged, sizeof(unchanged))) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translation mutated input when mapping lengths differed");
    }

    UArray_translate(input, NULL, to);
    UArray_translate(NULL, from, to);
    if (!bytesEqual(input, unchanged, sizeof(unchanged))) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translation did not safely reject NULL arguments");
    }

    malformed = *input;
    malformed.itemSize = 0;
    UArray_translate(&malformed, from, to);
    malformed = *input;
    malformed.data = NULL;
    UArray_translate(&malformed, from, to);
    malformed = *from;
    malformed.data = NULL;
    UArray_translate(input, &malformed, to);
    malformed = *to;
    malformed.itemSize = 2;
    UArray_translate(input, from, &malformed);
    if (!bytesEqual(input, unchanged, sizeof(unchanged))) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translation mutated data after malformed byte-array input");
    }

    /* A forged one-byte width must not make a non-byte CTYPE valid. */
    malformed = *input;
    malformed.itemType = CTYPE_uint16_t;
    malformed.hash = (uintptr_t)0x2345;
    UArray_translate(&malformed, from, to);
    if (!bytesEqual(input, unchanged, sizeof(unchanged)) ||
        malformed.hash != (uintptr_t)0x2345) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translation accepted a non-byte receiver CTYPE");
    }

    malformed = *from;
    malformed.itemType = CTYPE_int16_t;
    input->hash = (uintptr_t)0x3456;
    UArray_translate(input, &malformed, to);
    if (!bytesEqual(input, unchanged, sizeof(unchanged)) ||
        input->hash != (uintptr_t)0x3456) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translation accepted a non-byte source CTYPE");
    }

    malformed = *to;
    malformed.itemType = CTYPE_uint32_t;
    input->hash = (uintptr_t)0x4567;
    UArray_translate(input, from, &malformed);
    if (!bytesEqual(input, unchanged, sizeof(unchanged)) ||
        input->hash != (uintptr_t)0x4567) {
        UArray_free(input); UArray_free(from);
        UArray_free(to); UArray_free(shortTo);
        return fail("UArray translation accepted a non-byte target CTYPE");
    }

    UArray_free(input); UArray_free(from);
    UArray_free(to); UArray_free(shortTo);
    puts("UArray translate validation test passed");
    return EXIT_SUCCESS;
}
