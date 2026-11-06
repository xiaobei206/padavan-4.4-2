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

static int testBStreamWriteValidation(void)
{
	const unsigned char payload[] = { 0x31, 0x32, 0x33 };
	BStream *stream;
	unsigned char *originalData;
	size_t originalSize;
	const char *error;

	stream = BStream_new();
	if (NULL == stream)
		return fail("BStream write validation allocation failed");
	originalData = stream->ba->data;
	originalSize = stream->ba->size;
	stream->ba->size = 1;
	stream->ba->data = NULL;
	BStream_writeData_length_(stream, payload, sizeof(payload));
	error = BStream_errorString(stream);
	if (NULL == error || error[0] == '\0' ||
		stream->ba->size != 1 || stream->ba->data != NULL) {
		stream->ba->size = originalSize;
		stream->ba->data = originalData;
		BStream_free(stream);
		return fail("BStream write accepted malformed existing backing storage");
	}
	stream->ba->size = originalSize;
	stream->ba->data = originalData;
	BStream_free(stream);

	stream = BStream_new();
	if (NULL == stream)
		return fail("BStream tagged write validation allocation failed");
	BStream_writeTaggedData_length_(stream, NULL, 1);
	error = BStream_errorString(stream);
	if (NULL == error || error[0] == '\0' ||
		stream->ba->size != 0 || stream->index != 0) {
		BStream_free(stream);
		return fail("BStream tagged write silently accepted a NULL payload");
	}
	BStream_free(stream);

	stream = BStream_new();
	if (NULL == stream)
		return fail("BStream tagged storage validation allocation failed");
	originalData = stream->ba->data;
	originalSize = stream->ba->size;
	stream->ba->size = 1;
	stream->ba->data = NULL;
	BStream_writeTaggedData_length_(stream, payload, sizeof(payload));
	error = BStream_errorString(stream);
	if (NULL == error || error[0] == '\0' ||
		stream->ba->size != 1 || stream->ba->data != NULL) {
		stream->ba->size = originalSize;
		stream->ba->data = originalData;
		BStream_free(stream);
		return fail("BStream tagged write accepted malformed existing backing storage");
	}
	stream->ba->size = originalSize;
	stream->ba->data = originalData;
	BStream_free(stream);
	return EXIT_SUCCESS;
}

static int expectBStreamAliasedBackingRejected(BStream *stream,
    int tagged, int interior, const char *message)
{
    UArray alias;
    unsigned char *originalData;
    size_t originalSize;
    const char *error;

    if (NULL == stream || NULL == stream->ba || NULL == stream->ba->data)
        return fail("BStream backing alias fixture is invalid");
    originalData = stream->ba->data;
    originalSize = stream->ba->size;
    alias = *stream->ba;
    if (interior) {
        if (originalSize < 2)
            return fail("BStream backing alias fixture is too short");
        alias.data = stream->ba->data + 1;
        alias.size = originalSize - 1;
    }

    stream->index = 0;
    if (tagged)
        BStream_readTaggedUArray_(stream, &alias);
    else
        BStream_readUArray_(stream, &alias);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' || stream->index != 0 ||
        stream->ba->data != originalData || stream->ba->size != originalSize)
        return fail(message);
    return EXIT_SUCCESS;
}


static int testBStreamReadValidation(void)
{
    const unsigned char payload[] = { 0x51, 0x52, 0x53, 0x54 };
    UArray source = UArray_stackAllocedWithData_type_size_(
        (void *)payload, CTYPE_uint8_t, sizeof(payload));
    BStream *stream = BStream_new();
    UArray *destination = UArray_new();
    unsigned char *originalData;
    size_t originalSize;
    const char *error;

    if (NULL == stream || NULL == destination) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream read validation allocation failed");
    }

    BStream_writeUArray_(stream, &source);
    if (NULL != BStream_errorString(stream) &&
        BStream_errorString(stream)[0] != '\0') {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream plain array read fixture write failed");
    }
    stream->index = 0;
    BStream_readUArray_(stream, NULL);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' || stream->index != 0) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream plain array read silently accepted a NULL destination");
    }

    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    stream->index = 0;
    BStream_readUArray_(stream, stream->ba);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' || stream->index != 0) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream plain array read accepted an aliased destination");
    }

    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    if (expectBStreamAliasedBackingRejected(stream, 0, 0,
            "BStream plain array read accepted distinct shared backing storage") != EXIT_SUCCESS) {
        BStream_free(stream);
        UArray_free(destination);
        return EXIT_FAILURE;
    }

    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    if (expectBStreamAliasedBackingRejected(stream, 0, 1,
            "BStream plain array read accepted partially overlapping backing storage") != EXIT_SUCCESS) {
        BStream_free(stream);
        UArray_free(destination);
        return EXIT_FAILURE;
    }

    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    stream->index = stream->ba->size + 1;
    BStream_readUArray_(stream, destination);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' ||
        stream->index != stream->ba->size + 1) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream plain array read accepted an out-of-range cursor");
    }

    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    stream->index = 0;
    originalData = destination->data;
    originalSize = destination->size;
    destination->size = 1;
    destination->data = NULL;
    BStream_readUArray_(stream, destination);
    error = BStream_errorString(stream);
    destination->size = originalSize;
    destination->data = originalData;
    if (NULL == error || error[0] == '\0' || stream->index != 0) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream plain array read accepted malformed destination storage");
    }

    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    stream->index = 0;
    BStream_readUArray_(stream, destination);
    if ((NULL != BStream_errorString(stream) &&
         BStream_errorString(stream)[0] != '\0') ||
        destination->size != sizeof(payload) || NULL == destination->data ||
        memcmp(destination->data, payload, sizeof(payload)) != 0) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream plain array read round trip failed");
    }

    BStream_empty(stream);
    BStream_writeTaggedUArray_(stream, &source);
    stream->index = 0;
    BStream_readTaggedUArray_(stream, stream->ba);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' || stream->index != 0) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream tagged array read accepted an aliased destination");
    }

    BStream_empty(stream);
    BStream_writeTaggedUArray_(stream, &source);
    if (expectBStreamAliasedBackingRejected(stream, 1, 0,
            "BStream tagged array read accepted distinct shared backing storage") != EXIT_SUCCESS) {
        BStream_free(stream);
        UArray_free(destination);
        return EXIT_FAILURE;
    }

    BStream_empty(stream);
    BStream_writeTaggedUArray_(stream, &source);
    if (expectBStreamAliasedBackingRejected(stream, 1, 1,
            "BStream tagged array read accepted partially overlapping backing storage") != EXIT_SUCCESS) {
        BStream_free(stream);
        UArray_free(destination);
        return EXIT_FAILURE;
    }

    BStream_empty(stream);
    BStream_writeTaggedUArray_(stream, &source);
    stream->index = 0;
    BStream_readTaggedUArray_(stream, destination);
    if ((NULL != BStream_errorString(stream) &&
         BStream_errorString(stream)[0] != '\0') ||
        destination->size != sizeof(payload) || NULL == destination->data ||
        memcmp(destination->data, payload, sizeof(payload)) != 0) {
        BStream_free(stream);
        UArray_free(destination);
        return fail("BStream tagged array read round trip failed");
    }

    BStream_free(stream);
    UArray_free(destination);
    return EXIT_SUCCESS;
}

static int testBStreamAliasedWrites(void)
{
    BStream *stream;
    UArray source;
    size_t oldSize;
    const unsigned char seed = 0x6a;

    /* A direct append may read the current trailing terminator. */
    stream = BStream_new();
    if (NULL == stream || NULL == stream->ba->data) {
        BStream_free(stream);
        return fail("BStream append alias fixture allocation failed");
    }
    oldSize = stream->ba->size;
    BStream_writeData_length_(stream, stream->ba->data + oldSize, 1);
    if (stream->ba->size != oldSize + 1 ||
        stream->ba->data[oldSize] != 0 ||
        UArray_size(stream->errorBa) != 0 ||
        stream->index != oldSize + 1) {
        BStream_free(stream);
        return fail("BStream plain byte append lost an aliased tail terminator");
    }
    BStream_free(stream);

    /* The same source is aliasing even when the logical stream is empty. */
    stream = BStream_new();
    if (NULL == stream || NULL == stream->ba->data) {
        BStream_free(stream);
        return fail("BStream empty append alias fixture allocation failed");
    }
    BStream_writeData_length_(stream, stream->ba->data, 1);
    if (stream->ba->size != 1 || stream->ba->data[0] != 0 ||
        UArray_size(stream->errorBa) != 0 || stream->index != 1) {
        BStream_free(stream);
        return fail("BStream empty-stream append lost its terminator source");
    }
    BStream_free(stream);

    /* The four-byte length prefix must not invalidate a tail-backed array. */
    stream = BStream_new();
    if (NULL == stream)
        return fail("BStream plain array alias fixture allocation failed");
    BStream_writeData_length_(stream, &seed, 1);
    oldSize = stream->ba->size;
    source = UArray_stackAllocedWithData_type_size_(
        stream->ba->data + oldSize, CTYPE_uint8_t, 1);
    BStream_writeUArray_(stream, &source);
    if (stream->ba->size != oldSize + 5 ||
        stream->ba->data[stream->ba->size - 1] != 0 ||
        UArray_size(stream->errorBa) != 0 ||
        stream->index != stream->ba->size) {
        BStream_free(stream);
        return fail("BStream plain array write lost an aliased tail terminator");
    }
    BStream_free(stream);

    /* Tagged framing likewise precedes the payload copy. */
    stream = BStream_new();
    if (NULL == stream)
        return fail("BStream tagged data alias fixture allocation failed");
    BStream_writeData_length_(stream, &seed, 1);
    oldSize = stream->ba->size;
    BStream_writeTaggedData_length_(stream, stream->ba->data + oldSize, 1);
    if (stream->ba->size != oldSize + 7 ||
        stream->ba->data[stream->ba->size - 1] != 0 ||
        UArray_size(stream->errorBa) != 0 ||
        stream->index != stream->ba->size) {
        BStream_free(stream);
        return fail("BStream tagged write lost an aliased tail terminator");
    }
    BStream_free(stream);

    /* Exercise the UArray wrapper, which delegates to the tagged byte writer. */
    stream = BStream_new();
    if (NULL == stream)
        return fail("BStream tagged array alias fixture allocation failed");
    BStream_writeData_length_(stream, &seed, 1);
    oldSize = stream->ba->size;
    source = UArray_stackAllocedWithData_type_size_(
        stream->ba->data + oldSize, CTYPE_uint8_t, 1);
    BStream_writeTaggedUArray_(stream, &source);
    if (stream->ba->size != oldSize + 7 ||
        stream->ba->data[stream->ba->size - 1] != 0 ||
        UArray_size(stream->errorBa) != 0 ||
        stream->index != stream->ba->size) {
        BStream_free(stream);
        return fail("BStream tagged array write lost an aliased tail terminator");
    }
    BStream_free(stream);
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
	if (testBStreamWriteValidation() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamReadValidation() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamAliasedWrites() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	puts("BStreamTag boundaries test passed");
	return EXIT_SUCCESS;
}
