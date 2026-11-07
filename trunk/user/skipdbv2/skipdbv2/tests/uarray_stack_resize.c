#include "UArray.h"

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



static int testRejectsMalformedInsertItemLayouts(void)
{
    uint8_t destinationBytes[] = {'a', 'b'};
    uint8_t sourceBytes[] = {'x'};
    UArray malformedDestination = UArray_stackAllocedWithData_type_size_(
        destinationBytes, CTYPE_uint8_t, sizeof(destinationBytes));
    UArray validSource = UArray_stackAllocedWithData_type_size_(
        sourceBytes, CTYPE_uint8_t, sizeof(sourceBytes));
    UArray validDestination = UArray_stackAllocedWithData_type_size_(
        destinationBytes, CTYPE_uint8_t, sizeof(destinationBytes));
    UArray malformedSource = UArray_stackAllocedWithData_type_size_(
        sourceBytes, CTYPE_uint8_t, sizeof(sourceBytes));
    int okay = 0;

    malformedDestination.itemType = CTYPE_uint64_t;
    UArray_at_putAll_(&malformedDestination, 1, &validSource);
    if (malformedDestination.size != sizeof(destinationBytes) ||
        malformedDestination.data != destinationBytes ||
        memcmp(destinationBytes, "ab", sizeof(destinationBytes)) != 0) {
        fprintf(stderr, "UArray insertion accepted malformed destination item metadata\n");
        goto done;
    }

    malformedSource.itemType = CTYPE_uint64_t;
    UArray_at_putAll_(&validDestination, 1, &malformedSource);
    if (validDestination.size != sizeof(destinationBytes) ||
        validDestination.data != destinationBytes ||
        memcmp(destinationBytes, "ab", sizeof(destinationBytes)) != 0) {
        fprintf(stderr, "UArray insertion accepted malformed source item metadata\n");
        goto done;
    }

    okay = 1;
done:
    UArray_stackFree(&malformedDestination);
    UArray_stackFree(&validSource);
    UArray_stackFree(&validDestination);
    UArray_stackFree(&malformedSource);
    return okay;
}


static int testInsertSnapshotsTrailingTerminator(void)
{
    UArray *owner = UArray_newWithCString_("A");
    UArray terminatorView;
    int okay;

    if (NULL == owner)
        return 0;
    terminatorView = UArray_stackAllocedWithData_type_size_(
        owner->data + owner->size, CTYPE_uint8_t, 1);
    UArray_at_putAll_(owner, 0, &terminatorView);
    okay = owner->size == 2 && owner->data != NULL &&
        owner->data[0] == 0 && owner->data[1] == 'A';
    if (!okay)
        fprintf(stderr, "UArray insertion did not snapshot a borrowed trailing-terminator view\n");
    UArray_free(owner);
    return okay;
}

static int testStackRangeRejectsMismatchedItemLayout(void)
{
    uint8_t bytes[] = {0x11, 0x22};
    UArray malformed = UArray_stackAllocedWithData_type_size_(
        bytes, CTYPE_uint32_t, sizeof(bytes));
    UArray valid = UArray_stackAllocedWithData_type_size_(
        bytes, CTYPE_uint8_t, sizeof(bytes));
    UArray view = UArray_stackAllocedEmptyUArray();
    int okay = 0;

    malformed.itemSize = sizeof(uint8_t);
    view = UArray_stackRange(&malformed, 0, 2);
    if (view.data != NULL || view.size != 0 ||
        view.itemType != CTYPE_uint8_t || view.itemSize != sizeof(uint8_t) ||
        !view.stackAllocated) {
        fprintf(stderr, "UArray stack range accepted mismatched item layout\n");
        goto done;
    }

    view = UArray_stackRange(&valid, 1, 1);
    if (view.data != bytes + 1 || view.size != 1 ||
        view.itemType != CTYPE_uint8_t || view.itemSize != sizeof(uint8_t) ||
        !view.stackAllocated) {
        fprintf(stderr, "UArray stack range changed valid slice semantics\n");
        goto done;
    }

    okay = 1;
done:
    UArray_stackFree(&view);
    UArray_stackFree(&valid);
    UArray_stackFree(&malformed);
    return okay;
}


static int testCopyRejectsMismatchedItemLayouts(void)
{
    uint8_t shortSourceBytes[] = {0x11, 0x22};
    uint8_t shortTargetBytes[] = {0xa1, 0xb2};
    uint64_t targetValues[] = {
        UINT64_C(0x1122334455667788), UINT64_C(0x8877665544332211)
    };
    uint64_t sourceValues[] = {
        UINT64_C(0x0102030405060708), UINT64_C(0x1020304050607080)
    };
    const uint64_t expectedTarget[] = {
        UINT64_C(0x1122334455667788), UINT64_C(0x8877665544332211)
    };
    const uint8_t expectedShortTarget[] = {0xa1, 0xb2};
    UArray malformedSource = UArray_stackAllocedWithData_type_size_(
        shortSourceBytes, CTYPE_uint32_t, 2);
    UArray target = UArray_stackAllocedWithData_type_size_(
        targetValues, CTYPE_uint64_t, 2);
    UArray malformedTarget = UArray_stackAllocedWithData_type_size_(
        shortTargetBytes, CTYPE_uint32_t, 2);
    UArray validSource = UArray_stackAllocedWithData_type_size_(
        sourceValues, CTYPE_uint64_t, 2);
    uint8_t copyTargetBytes[] = {0x31, 0x42};
    UArray *copyTarget = UArray_newWithData_type_size_copy_(
        copyTargetBytes, CTYPE_uint8_t, 2, 1);
    int okay = 0;

    if (NULL == copyTarget) {
        fprintf(stderr, "UArray malformed-layout copy target allocation failed\n");
        goto done;
    }

    malformedSource.itemSize = sizeof(uint8_t);
    UArray_copyItems_(&target, &malformedSource);
    if (memcmp(targetValues, expectedTarget, sizeof(expectedTarget)) != 0) {
        fprintf(stderr, "UArray typed copy accepted malformed source item width\n");
        goto done;
    }

    malformedTarget.itemSize = sizeof(uint8_t);
    UArray_copyItems_(&malformedTarget, &validSource);
    if (memcmp(shortTargetBytes, expectedShortTarget, sizeof(expectedShortTarget)) != 0) {
        fprintf(stderr, "UArray typed copy accepted malformed destination item width\n");
        goto done;
    }

    UArray_copy_(copyTarget, &malformedSource);
    if (copyTarget->itemType != CTYPE_uint8_t ||
        copyTarget->itemSize != sizeof(uint8_t) || copyTarget->size != 2 ||
        memcmp(copyTarget->data, copyTargetBytes, sizeof(copyTargetBytes)) != 0) {
        fprintf(stderr, "UArray copy changed destination state for malformed source layout\n");
        goto done;
    }

    okay = 1;
done:
    UArray_free(copyTarget);
    UArray_stackFree(&validSource);
    UArray_stackFree(&malformedTarget);
    UArray_stackFree(&target);
    UArray_stackFree(&malformedSource);
    return okay;
}



typedef struct
{
    char leadingByte;
    uint64_t value;
} TestUArrayUInt64AlignmentProbe;

static int testRejectsMisalignedTypedItemLayouts(void)
{
    union {
        uint64_t alignment;
        uint8_t bytes[sizeof(uint64_t) + 1];
    } sourceStorage, targetStorage;
    const uint64_t sourceValue = UINT64_C(0x123456789abcdef0);
    uint8_t sourceSnapshot[sizeof(sourceValue)];
    uint32_t targetValue = UINT32_C(0x55667788);
    const uint32_t expectedTarget = UINT32_C(0x55667788);
    uint32_t replacementValue = UINT32_C(0x10203040);
    UArray malformedSource, malformedTarget, alignedTarget, alignedSource;
    UArray range;
    uintptr_t misalignedSourceAddress, misalignedTargetAddress;
    size_t alignment = offsetof(TestUArrayUInt64AlignmentProbe, value);
    int okay = 0;

    if (alignment <= 1)
        return 1;

    memset(sourceStorage.bytes, 0, sizeof(sourceStorage.bytes));
    memset(targetStorage.bytes, 0, sizeof(targetStorage.bytes));
    memcpy(sourceSnapshot, &sourceValue, sizeof(sourceSnapshot));
    memcpy(sourceStorage.bytes + 1, sourceSnapshot, sizeof(sourceSnapshot));
    memcpy(targetStorage.bytes + 1, sourceSnapshot, sizeof(sourceSnapshot));
    misalignedSourceAddress = (uintptr_t)(sourceStorage.bytes + 1);
    misalignedTargetAddress = (uintptr_t)(targetStorage.bytes + 1);
    if (misalignedSourceAddress % alignment == 0 ||
        misalignedTargetAddress % alignment == 0)
        return 0;

    malformedSource = UArray_stackAllocedWithData_type_size_(
        sourceStorage.bytes + 1, CTYPE_uint64_t, 1);
    malformedTarget = UArray_stackAllocedWithData_type_size_(
        targetStorage.bytes + 1, CTYPE_uint64_t, 1);
    alignedTarget = UArray_stackAllocedWithData_type_size_(
        &targetValue, CTYPE_uint32_t, 1);
    alignedSource = UArray_stackAllocedWithData_type_size_(
        &replacementValue, CTYPE_uint32_t, 1);

    alignedTarget.hash = (uintptr_t)0x1234;
    UArray_copyItems_(&alignedTarget, &malformedSource);
    if (targetValue != expectedTarget || alignedTarget.hash != (uintptr_t)0x1234) {
        fprintf(stderr, "UArray typed copy read misaligned source storage\\n");
        goto done;
    }

    malformedTarget.hash = (uintptr_t)0x2345;
    UArray_copyItems_(&malformedTarget, &alignedSource);
    if (memcmp(targetStorage.bytes + 1, sourceSnapshot, sizeof(sourceSnapshot)) != 0 ||
        malformedTarget.hash != (uintptr_t)0x2345) {
        fprintf(stderr, "UArray typed copy wrote misaligned destination storage\\n");
        goto done;
    }

    range = UArray_stackRange(&malformedSource, 0, 1);
    if (range.data != NULL || range.size != 0 ||
        range.itemType != CTYPE_uint8_t || range.itemSize != sizeof(uint8_t)) {
        fprintf(stderr, "UArray stack range exposed misaligned typed storage\\n");
        UArray_stackFree(&range);
        goto done;
    }
    UArray_stackFree(&range);

    malformedTarget.hash = (uintptr_t)0x3456;
    UArray_at_putLong_(&malformedTarget, 0, 0);
    UArray_at_putDouble_(&malformedTarget, 0, 1.0);
    if (memcmp(targetStorage.bytes + 1, sourceSnapshot, sizeof(sourceSnapshot)) != 0 ||
        malformedTarget.hash != (uintptr_t)0x3456) {
        fprintf(stderr, "UArray scalar write accepted misaligned typed storage\\n");
        goto done;
    }

    okay = 1;
done:
    UArray_stackFree(&malformedSource);
    UArray_stackFree(&malformedTarget);
    UArray_stackFree(&alignedTarget);
    UArray_stackFree(&alignedSource);
    return okay;
}


static int testIndexCompaction(void)
{
    const uint16_t source[] = {0x1111, 0x2222, 0x3333, 0x4444, 0x5555};
    const uint16_t expectedOddRemoval[] = {0x1111, 0x3333, 0x5555};
    const uint16_t expectedEvenRemoval[] = {0x2222, 0x4444};
    UArray *oddRemoval = UArray_newWithData_type_size_copy_(
        (void *)source, CTYPE_uint16_t, 5, 1);
    UArray *evenRemoval = UArray_newWithData_type_size_copy_(
        (void *)source, CTYPE_uint16_t, 5, 1);
    UArray invalid;
    int result = EXIT_FAILURE;

    if (NULL == oddRemoval || NULL == evenRemoval) {
        fprintf(stderr, "UArray index compaction allocation failed\n");
        goto cleanup;
    }

    oddRemoval->hash = (uintptr_t)0x1234;
    UArray_removeOddIndexes(oddRemoval);
    if (oddRemoval->size != 3 ||
        memcmp(oddRemoval->data, expectedOddRemoval, sizeof(expectedOddRemoval)) != 0 ||
        oddRemoval->hash != 0) {
        fprintf(stderr, "UArray_removeOddIndexes changed retained items or hash state\n");
        goto cleanup;
    }

    evenRemoval->hash = (uintptr_t)0x5678;
    UArray_removeEvenIndexes(evenRemoval);
    if (evenRemoval->size != 2 ||
        memcmp(evenRemoval->data, expectedEvenRemoval, sizeof(expectedEvenRemoval)) != 0 ||
        evenRemoval->hash != 0) {
        fprintf(stderr, "UArray_removeEvenIndexes changed retained items or hash state\n");
        goto cleanup;
    }

    UArray_removeOddIndexes(NULL);
    UArray_removeEvenIndexes(NULL);

    invalid = *oddRemoval;
    invalid.data = NULL;
    invalid.hash = (uintptr_t)0x1111;
    UArray_removeOddIndexes(&invalid);
    UArray_removeEvenIndexes(&invalid);
    if (invalid.size != oddRemoval->size || invalid.data != NULL ||
        invalid.hash != (uintptr_t)0x1111) {
        fprintf(stderr, "UArray index compaction accepted missing backing storage\n");
        goto cleanup;
    }

    invalid = *oddRemoval;
    invalid.itemSize = 0;
    invalid.hash = (uintptr_t)0x2222;
    UArray_removeOddIndexes(&invalid);
    UArray_removeEvenIndexes(&invalid);
    if (invalid.size != oddRemoval->size ||
        invalid.hash != (uintptr_t)0x2222) {
        fprintf(stderr, "UArray index compaction accepted zero-width storage\n");
        goto cleanup;
    }

    invalid = *oddRemoval;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = (uintptr_t)0x3333;
    UArray_removeOddIndexes(&invalid);
    UArray_removeEvenIndexes(&invalid);
    if (invalid.size != (size_t)-1 / invalid.itemSize + 1 ||
        invalid.hash != (uintptr_t)0x3333) {
        fprintf(stderr, "UArray index compaction accepted overflowing storage geometry\n");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(oddRemoval);
    UArray_free(evenRemoval);
    return result;
}

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

    if (!testRejectsMalformedInsertItemLayouts()) {
        fprintf(stderr, "UArray insertion did not reject malformed item layouts atomically\n");
        goto done;
    }
    if (!testInsertSnapshotsTrailingTerminator()) {
        fprintf(stderr, "UArray insertion did not preserve an aliased trailing terminator\n");
        goto done;
    }
    if (!testDuplicateIndexes()) {
        fprintf(stderr, "UArray_duplicateIndexes failed safety or duplication checks\n");
        goto done;
    }
    if (!testStackRangeRejectsMismatchedItemLayout()) {
        fprintf(stderr, "UArray stack-range layout validation failed\n");
        goto done;
    }
    if (!testCopyRejectsMismatchedItemLayouts()) {
        fprintf(stderr, "UArray typed copy accepted inconsistent element layout metadata\n");
        goto done;
    }
    if (!testRejectsMisalignedTypedItemLayouts()) {
        fprintf(stderr, "UArray typed operations accepted misaligned backing storage\\n");
        goto done;
    }
    if (testIndexCompaction() != EXIT_SUCCESS) {
        fprintf(stderr, "UArray index compaction failed safety or retained-item checks\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("UArray stack resize test passed");

done:
    UArray_stackFree(&shrunk);
    UArray_stackFree(&grown);
    return result;
}
