#include "UArray.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return EXIT_FAILURE;
}


typedef struct
{
    char leadingByte;
    void *value;
} TestPointerAlignmentProbe;

static int misaligned_pointer_storage_is_rejected(void)
{
    union {
        void *alignment;
        unsigned char bytes[sizeof(void *) + 1];
    } storage;
    int first = 101, second = 202;
    void *original = &first;
    void *replacement = &second;
    unsigned char snapshot[sizeof(original)];
    size_t alignment = offsetof(TestPointerAlignmentProbe, value);
    UArray malformed;

    if (alignment <= 1)
        return 1;
    memset(storage.bytes, 0, sizeof(storage.bytes));
    memcpy(snapshot, &original, sizeof(snapshot));
    memcpy(storage.bytes + 1, snapshot, sizeof(snapshot));
    if (((uintptr_t)(storage.bytes + 1) % alignment) == 0)
        return 0;

    malformed = UArray_stackAllocedWithData_type_size_(
        storage.bytes + 1, CTYPE_uintptr_t, 1);
    malformed.hash = (uintptr_t)0x7654;

    if (UArray_pointerAt_(&malformed, 0) != NULL ||
        UArray_rawPointerAt_(&malformed, 0) != NULL) {
        fprintf(stderr, "UArray pointer getter read misaligned pointer storage\\n");
        return 0;
    }

    UArray_at_putPointer_(&malformed, 0, replacement);
    UArray_appendPointer_(&malformed, replacement);
    if (malformed.size != 1 || malformed.hash != (uintptr_t)0x7654 ||
        memcmp(storage.bytes + 1, snapshot, sizeof(snapshot)) != 0) {
        fprintf(stderr, "UArray pointer setter mutated misaligned pointer storage\\n");
        return 0;
    }
    return 1;
}


int main(void)
{
    int first = 1, second = 2;
    UArray *array = UArray_new();
    UArray *wrongType = UArray_new();
    unsigned char *savedData;
    size_t savedSize, savedItemSize;
    CTYPE savedType;

    if (NULL == array || NULL == wrongType) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer access allocation failed");
    }
    UArray_setItemType_(array, CTYPE_uintptr_t);
    if (array->itemType != CTYPE_uintptr_t ||
        array->itemSize != sizeof(void *)) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer array has an unexpected element width");
    }

    UArray_appendPointer_(array, &first);
    UArray_appendPointer_(array, &second);
    if (array->size != 2 ||
        UArray_pointerAt_(array, 0) != &first ||
        UArray_pointerAt_(array, 1) != &second ||
        UArray_rawPointerAt_(array, 1) != &second) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray valid pointer array round trip failed");
    }
    if (UArray_pointerAt_(array, 2) != NULL ||
        UArray_pointerAt_(NULL, 0) != NULL ||
        UArray_rawPointerAt_(NULL, 0) != NULL) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer getter accepted NULL or out-of-range access");
    }

    array->hash = (uintptr_t)0x1234;
    UArray_at_putPointer_(array, 1, &first);
    if (array->hash != 0 || UArray_pointerAt_(array, 1) != &first) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer mutation failed to update data and invalidate hash");
    }

    savedData = array->data;
    savedSize = array->size;
    array->data = NULL;
    if (UArray_pointerAt_(array, 0) != NULL ||
        UArray_rawPointerAt_(array, 0) != NULL) {
        array->data = savedData;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer getter accepted non-empty NULL storage");
    }
    UArray_at_putPointer_(array, 0, &second);
    if (array->size != savedSize || array->data != NULL) {
        array->data = savedData;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer setter mutated malformed backing storage");
    }
    array->data = savedData;

    savedItemSize = array->itemSize;
    array->itemSize = 1;
    if (UArray_pointerAt_(array, 0) != NULL ||
        UArray_rawPointerAt_(array, 0) != NULL) {
        array->itemSize = savedItemSize;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer getter accepted malformed element width");
    }
    UArray_at_putPointer_(array, 0, &second);
    if (array->size != savedSize || array->itemSize != 1) {
        array->itemSize = savedItemSize;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer setter resized malformed element storage");
    }
    array->itemSize = savedItemSize;

    savedType = array->itemType;
    array->itemType = CTYPE_uint8_t;
    UArray_at_putPointer_(array, 2, &second);
    if (array->size != savedSize || UArray_pointerAt_(array, 0) != NULL) {
        array->itemType = savedType;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer setter changed an unsupported array type");
    }
    array->itemType = savedType;

    savedSize = array->size;
    array->size = (size_t)-1 / array->itemSize + 1;
    if (UArray_pointerAt_(array, 0) != NULL) {
        array->size = savedSize;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer getter accepted overflowing geometry");
    }
    UArray_at_putPointer_(array, 0, &second);
    if (array->size != (size_t)-1 / array->itemSize + 1) {
        array->size = savedSize;
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer setter modified overflowing geometry");
    }
    array->size = savedSize;

    UArray_at_putPointer_(wrongType, 0, &first);
    if (wrongType->size != 0) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer setter resized an unsupported empty array");
    }

    if (!misaligned_pointer_storage_is_rejected()) {
        UArray_free(array);
        UArray_free(wrongType);
        return fail("UArray pointer access accepted misaligned pointer storage");
    }

    UArray_free(array);
    UArray_free(wrongType);
    puts("UArray pointer access test passed");
    return EXIT_SUCCESS;
}
