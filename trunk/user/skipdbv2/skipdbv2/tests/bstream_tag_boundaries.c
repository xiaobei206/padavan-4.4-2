#include "BStream.h"
#include "BStreamTag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return EXIT_FAILURE;
}

static int testBStreamClone(void)
{
	const unsigned char payload[] = { 0x11, 0x22, 0x33, 0x44 };
	BStream *source = BStream_new();
	BStream *copy = NULL;
	BStream *malformed = NULL;
	if (NULL == source)
		return fail("BStream allocation failed");
	BStream_writeData_length_(source, payload, sizeof(payload));
	if (source->ba->size != sizeof(payload) ||
		NULL == source->ba->data ||
		memcmp(source->ba->data, payload, sizeof(payload)) != 0) {
		BStream_free(source);
		return fail("BStream clone fixture could not write payload");
	}
	source->index = 1;
	source->flipEndian = 1;
	copy = BStream_clone(source);
	if (NULL == copy || copy->index != source->index ||
		copy->flipEndian != source->flipEndian ||
		copy->ba->size != sizeof(payload) || NULL == copy->ba->data ||
		memcmp(copy->ba->data, payload, sizeof(payload)) != 0) {
		BStream_free(copy);
		BStream_free(source);
		return fail("BStream clone did not preserve bytes, cursor, and endianness");
	}
	BStream_free(copy);
	copy = NULL;

	source->index = source->ba->size + 1;
	copy = BStream_clone(source);
	source->index = 1;
	if (NULL != copy) {
		BStream_free(copy);
		BStream_free(source);
		return fail("BStream clone accepted an out-of-range cursor");
	}

	malformed = BStream_new();
	if (NULL == malformed) {
		BStream_free(source);
		return fail("BStream malformed-state fixture allocation failed");
	}
	free(malformed->ba->data);
	malformed->ba->data = NULL;
	malformed->ba->size = 1;
	copy = BStream_clone(malformed);
	BStream_free(malformed);
	BStream_free(source);
	if (NULL != copy) {
		BStream_free(copy);
		return fail("BStream clone accepted malformed backing storage");
	}
	return EXIT_SUCCESS;
}

int main(void)
{
	unsigned int arrayFlag, type, byteCount;
	BStreamTag sample = BStreamTag_TagArray_type_byteCount_(1, BSTREAM_POINTER, 8);

	if (BStreamTag_asUnsignedChar(NULL) != 0 ||
		BStreamTag_isEqual_(NULL, NULL) != 0 ||
		BStreamTag_isEqual_(&sample, NULL) != 0 ||
		BStreamTag_isEqual_(NULL, &sample) != 0 ||
		strcmp(BStreamTag_typeName(NULL), "UNKNOWN TYPE") != 0)
		return fail("BStreamTag null arguments were not handled safely");
	BStreamTag_print(NULL);

	for (arrayFlag = 0; arrayFlag < 2; ++arrayFlag) {
		for (type = 0; type < 4; ++type) {
			for (byteCount = 0; byteCount < 32; ++byteCount) {
				BStreamTag tag = BStreamTag_TagArray_type_byteCount_(
					arrayFlag, type, byteCount);
				unsigned char encoded = BStreamTag_asUnsignedChar(&tag);
				BStreamTag decoded = BStreamTag_FromUnsignedChar(encoded);
				unsigned char expected = (unsigned char)(
					(arrayFlag << 7) | (type << 5) | byteCount);

				if (encoded != expected ||
					decoded.isArray != arrayFlag ||
					decoded.type != type ||
					decoded.byteCount != byteCount ||
					!BStreamTag_isEqual_(&tag, &decoded))
					return fail("BStreamTag wire-format round trip failed");
			}
		}
	}

	if (strcmp(BStreamTag_typeName(&sample), "pointer") != 0)
		return fail("BStreamTag valid type name changed");
	if (testBStreamClone() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	puts("BStreamTag boundaries test passed");
	return EXIT_SUCCESS;
}
