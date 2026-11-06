#include "UArray.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_bytes(const UArray *array, const uint8_t *expected, size_t length)
{
	return array != NULL && UArray_sizeInBytes(array) == length &&
		(length == 0 || memcmp(UArray_bytes(array), expected, length) == 0);
}

int main(void)
{
	uint8_t bytes16[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
	const uint8_t expected16[] = { 0x20, 0x10, 0x40, 0x30, 0x60, 0x50 };
	uint8_t bytes32[] = { 0x11, 0x22, 0x33, 0x44 };
	const uint8_t expected32[] = { 0x44, 0x33, 0x22, 0x11 };
	UArray words16 = UArray_stackAllocedWithData_type_size_(
		bytes16, CTYPE_uint16_t, 3);
	UArray word32 = UArray_stackAllocedWithData_type_size_(
		bytes32, CTYPE_uint32_t, 1);
	UArray empty = UArray_stackAllocedEmptyUArray();
	UArray invalid = words16;

	UArray_reverseItemByteOrders(NULL);
	UArray_reverseItemByteOrders(&empty);
	invalid.data = NULL;
	UArray_reverseItemByteOrders(&invalid);

	if (words16.itemSize != sizeof(uint16_t) ||
		word32.itemSize != sizeof(uint32_t)) {
		fprintf(stderr, "UArray test fixtures have unexpected item widths\n");
		return EXIT_FAILURE;
	}

	UArray_reverseItemByteOrders(&words16);
	if (!check_bytes(&words16, expected16, sizeof(expected16))) {
		fprintf(stderr, "multi-item byte reversal did not reverse each item independently\n");
		return EXIT_FAILURE;
	}

	UArray_reverseItemByteOrders(&word32);
	if (!check_bytes(&word32, expected32, sizeof(expected32))) {
		fprintf(stderr, "single-item byte reversal accessed bytes outside the item\n");
		return EXIT_FAILURE;
	}

	UArray_stackFree(&words16);
	UArray_stackFree(&word32);
	UArray_stackFree(&empty);
	puts("UArray reverse item byte order test passed");
	return EXIT_SUCCESS;
}
