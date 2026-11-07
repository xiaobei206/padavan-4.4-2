#include "BStream.h"
#include "BStreamTag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern intptr_t BStream_readTaggedPointer(BStream *self);

/* Internal writer is exercised here to test raw wire-field range checks. */
extern void BStream_writeTag(BStream *self, unsigned int t, unsigned int b, unsigned int a);

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

static int testBStreamSetUArrayOwnership(void)
{
	const unsigned char payload[] = { 0x61, 0x62, 0x63, 0x64 };
	unsigned char external_bytes[] = { 0x71, 0x72 };
	BStream *stream = BStream_new();
	UArray alias;
	UArray external = UArray_stackAllocedWithData_type_size_(
		external_bytes, CTYPE_uint8_t, sizeof(external_bytes));
	UArray *ownedArray;
	unsigned char *ownedData;
	size_t ownedSize;

	if (NULL == stream)
		return fail("BStream setUArray fixture allocation failed");
	BStream_writeData_length_(stream, payload, sizeof(payload));
	if (stream->ba->size != sizeof(payload) || NULL == stream->ba->data ||
		UArray_size(stream->errorBa) != 0) {
		BStream_free(stream);
		return fail("BStream setUArray owned-buffer fixture could not be initialized");
	}
	ownedArray = stream->ba;
	ownedData = stream->ba->data;
	ownedSize = stream->ba->size;
	stream->index = 2;

	/* A distinct UArray object can still borrow the exact owned allocation. */
	alias = *stream->ba;
	BStream_setUArray_(stream, &alias);
	if (stream->ba != ownedArray || stream->ownsUArray != 1 ||
		stream->index != 2 || stream->ba->data != ownedData ||
		stream->ba->size != ownedSize ||
		memcmp(stream->ba->data, payload, sizeof(payload)) != 0) {
		BStream_free(stream);
		return fail("BStream setUArray accepted an exact alias of owned storage");
	}

	/* Reject interior views before releasing the owner of their bytes. */
	alias = UArray_stackAllocedWithData_type_size_(
		ownedData + 1, CTYPE_uint8_t, 2);
	BStream_setUArray_(stream, &alias);
	if (stream->ba != ownedArray || stream->ownsUArray != 1 ||
		stream->index != 2 || stream->ba->data != ownedData ||
		stream->ba->size != ownedSize) {
		BStream_free(stream);
		return fail("BStream setUArray accepted an interior alias of owned storage");
	}

	/* The trailing NUL is part of the allocation and must keep its owner alive. */
	alias = UArray_stackAllocedWithData_type_size_(
		ownedData + ownedSize, CTYPE_uint8_t, 0);
	BStream_setUArray_(stream, &alias);
	if (stream->ba != ownedArray || stream->ownsUArray != 1 ||
		stream->index != 2 || stream->ba->data != ownedData) {
		BStream_free(stream);
		return fail("BStream setUArray accepted a tail alias of owned storage");
	}

	/* An unrelated external view remains a valid non-owning attachment. */
	BStream_setUArray_(stream, &external);
	if (stream->ba != &external || stream->ownsUArray != 0 ||
		stream->index != 0 || stream->ba->data != external_bytes ||
		stream->ba->size != sizeof(external_bytes) ||
		memcmp(stream->ba->data, external_bytes, sizeof(external_bytes)) != 0) {
		BStream_free(stream);
		return fail("BStream setUArray rejected an unrelated external array");
	}
	BStream_free(stream);
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

static int testBStreamArrayLayoutValidation(void)
{
    const unsigned char payload[] = {0x71, 0x72, 0x73, 0x74};
    const uint32_t originalValues[] = {0x11223344U, 0x55667788U};
    uint32_t destinationValues[] = {0x11223344U, 0x55667788U};
    UArray source = UArray_stackAllocedWithData_type_size_(
        (void *)payload, CTYPE_uint8_t, sizeof(payload));
    UArray malformedDestination = UArray_stackAllocedWithData_type_size_(
        destinationValues, CTYPE_uint32_t, 2);
    BStream *stream = BStream_new();
    CTYPE streamType;
    const char *error;
    int okay = 0;

    if (NULL == stream) {
        fprintf(stderr, "BStream array-layout fixture allocation failed\n");
        goto done;
    }

    BStream_writeUArray_(stream, &source);
    if (UArray_size(stream->errorBa) != 0) {
        fprintf(stderr, "BStream plain array-layout fixture write failed\n");
        goto done;
    }
    malformedDestination.itemSize = sizeof(uint8_t);
    stream->index = 0;
    BStream_readUArray_(stream, &malformedDestination);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' || stream->index != 0 ||
        malformedDestination.itemType != CTYPE_uint32_t ||
        malformedDestination.itemSize != sizeof(uint8_t) ||
        malformedDestination.size != 2 ||
        memcmp(destinationValues, originalValues, sizeof(originalValues)) != 0) {
        fprintf(stderr, "BStream plain array read accepted a mismatched destination item width\n");
        goto done;
    }

    malformedDestination.itemSize = sizeof(uint32_t);
    BStream_empty(stream);
    BStream_writeUArray_(stream, &source);
    if (UArray_size(stream->errorBa) != 0) {
        fprintf(stderr, "BStream malformed stream-layout fixture write failed\n");
        goto done;
    }
    streamType = stream->ba->itemType;
    stream->ba->itemType = CTYPE_uint32_t;
    stream->index = 0;
    BStream_readUArray_(stream, &malformedDestination);
    error = BStream_errorString(stream);
    stream->ba->itemType = streamType;
    if (NULL == error || error[0] == '\0' || stream->index != 0 ||
        malformedDestination.size != 2 ||
        memcmp(destinationValues, originalValues, sizeof(originalValues)) != 0) {
        fprintf(stderr, "BStream array read accepted a mismatched stream item width\n");
        goto done;
    }

    BStream_empty(stream);
    BStream_writeTaggedUArray_(stream, &source);
    if (UArray_size(stream->errorBa) != 0) {
        fprintf(stderr, "BStream tagged array-layout fixture write failed\n");
        goto done;
    }
    malformedDestination.itemSize = sizeof(uint8_t);
    stream->index = 0;
    BStream_readTaggedUArray_(stream, &malformedDestination);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' || stream->index != 0 ||
        malformedDestination.itemType != CTYPE_uint32_t ||
        malformedDestination.itemSize != sizeof(uint8_t) ||
        malformedDestination.size != 2 ||
        memcmp(destinationValues, originalValues, sizeof(originalValues)) != 0) {
        fprintf(stderr, "BStream tagged array read accepted a mismatched destination item width\n");
        goto done;
    }

    okay = 1;
done:
    malformedDestination.itemSize = sizeof(uint32_t);
    BStream_free(stream);
    UArray_stackFree(&malformedDestination);
    UArray_stackFree(&source);
    return okay ? EXIT_SUCCESS : EXIT_FAILURE;
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


static int expectBStreamInvalidTagRejected_(unsigned int type,
    unsigned int byteCount, unsigned int arrayFlag, const char *message)
{
    BStream *stream = BStream_new();
    const char *error;
    if (NULL == stream || NULL == stream->ba) {
        BStream_free(stream);
        return fail("BStream tag-boundary fixture allocation failed");
    }
    BStream_writeTag(stream, type, byteCount, arrayFlag);
    error = BStream_errorString(stream);
    if (NULL == error || error[0] == '\0' ||
        UArray_size(stream->ba) != 0 || stream->index != 0) {
        BStream_free(stream);
        return fail(message);
    }
    BStream_free(stream);
    return EXIT_SUCCESS;
}

static int testBStreamTagWriterFieldBounds(void)
{
    BStream *stream;
    if (expectBStreamInvalidTagRejected_(4, 1, 0,
            "BStream tag writer accepted an out-of-range type") != EXIT_SUCCESS ||
        expectBStreamInvalidTagRejected_(BSTREAM_UNSIGNED_INT, 32, 0,
            "BStream tag writer accepted an out-of-range byte count") != EXIT_SUCCESS ||
        expectBStreamInvalidTagRejected_(BSTREAM_UNSIGNED_INT, 1, 2,
            "BStream tag writer accepted an out-of-range array flag") != EXIT_SUCCESS)
        return EXIT_FAILURE;

    stream = BStream_new();
    if (NULL == stream)
        return fail("BStream valid tag-boundary fixture allocation failed");
    BStream_writeTag(stream, BSTREAM_POINTER, 31, 1);
    if (NULL == stream->ba->data || UArray_size(stream->ba) != 1 ||
        stream->index != 1 || stream->ba->data[0] != 0xff ||
        UArray_size(stream->errorBa) != 0) {
        BStream_free(stream);
        return fail("BStream tag writer rejected field-width maximum");
    }
    BStream_free(stream);
    return EXIT_SUCCESS;
}


#if !defined(__SYMBIAN32__)
static int testBStreamTaggedPointerWidth(void)
{
	BStream *stream = BStream_new();
	unsigned char wire[1 + sizeof(int64_t)];
	int64_t encoded;
	intptr_t decoded;
	const char *error;

	if (NULL == stream)
		return fail("BStream tagged pointer fixture allocation failed");

	encoded = INT64_C(1234567);
	wire[0] = (unsigned char)((BSTREAM_POINTER << 5) | sizeof(encoded));
	memcpy(wire + 1, &encoded, sizeof(encoded));
	BStream_setData_length_(stream, wire, sizeof(wire));
	decoded = BStream_readTaggedPointer(stream);
	error = BStream_errorString(stream);
	if (decoded != (intptr_t)encoded ||
		(NULL != error && error[0] != '\0')) {
		BStream_free(stream);
		return fail("in-range tagged pointer was decoded incorrectly");
	}

	if (sizeof(intptr_t) < sizeof(int64_t)) {
		encoded = (int64_t)INTPTR_MAX + INT64_C(1);
	} else {
		encoded = INT64_MAX;
	}
	memcpy(wire + 1, &encoded, sizeof(encoded));
	BStream_setData_length_(stream, wire, sizeof(wire));
	decoded = BStream_readTaggedPointer(stream);
	error = BStream_errorString(stream);
	if (sizeof(intptr_t) < sizeof(int64_t)) {
		if (decoded != 0 || NULL == error || error[0] == '\0' ||
			strcmp(error, "tagged pointer exceeds platform width") != 0) {
			BStream_free(stream);
			return fail("out-of-range tagged pointer was silently truncated");
		}
	} else if (decoded != (intptr_t)encoded ||
		(NULL != error && error[0] != '\0')) {
		BStream_free(stream);
		return fail("native-width tagged pointer was rejected");
	}

	BStream_free(stream);
	return EXIT_SUCCESS;
}
#endif

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
	if (testBStreamTagWriterFieldBounds() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamClone() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamSetUArrayOwnership() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamWriteValidation() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamReadValidation() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamArrayLayoutValidation() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (testBStreamAliasedWrites() != EXIT_SUCCESS)
		return EXIT_FAILURE;
#if !defined(__SYMBIAN32__)
	if (testBStreamTaggedPointerWidth() != EXIT_SUCCESS)
		return EXIT_FAILURE;
#endif
	puts("BStreamTag boundaries test passed");
	return EXIT_SUCCESS;
}
