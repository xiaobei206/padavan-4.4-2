#define _GNU_SOURCE

#include "UArray.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
	size_t offset;
} ErrorCookie;

static ssize_t errorCookieRead(void *opaque, char *buffer, size_t size)
{
	static const uint8_t payload[] = {'a', 'b', 'c'};
	ErrorCookie *cookie = (ErrorCookie *)opaque;

	if (cookie->offset < sizeof(payload) && size > 0) {
		buffer[0] = (char)payload[cookie->offset++];
		return 1;
	}
	errno = EIO;
	return -1;
}

static FILE *openErrorStream(ErrorCookie *cookie)
{
	cookie_io_functions_t functions;
	FILE *stream;

	memset(&functions, 0, sizeof(functions));
	functions.read = errorCookieRead;
	stream = fopencookie(cookie, "r", functions);
	if (stream && setvbuf(stream, NULL, _IONBF, 0) != 0) {
		fclose(stream);
		return NULL;
	}
	return stream;
}

static UArray *newByteArray(void)
{
	UArray *array = UArray_new();
	if (NULL == array)
		return NULL;
	UArray_setItemType_(array, CTYPE_uint8_t);
	if (array->itemType != CTYPE_uint8_t || array->itemSize != 1) {
		UArray_free(array);
		return NULL;
	}
	return array;
}

static int lineEquals(UArray *line, FILE *stream,
	const uint8_t *expected, size_t expectedSize)
{
	int result;
	UArray_setSize_(line, 0);
	result = UArray_readLineFromCStream_(line, stream);
	return result == 1 && line->size == expectedSize &&
		(expectedSize == 0 || memcmp(line->data, expected, expectedSize) == 0);
}

static int testLineBoundaries(void)
{
	static const uint8_t input[] =
		"\nfirst\rsecond\r\nthird\nlast";
	static const uint8_t first[] = "first";
	static const uint8_t second[] = "second";
	static const uint8_t third[] = "third";
	static const uint8_t last[] = "last";
	static const uint8_t binary[] = {'A', 0, 'B', '\n'};
	uint8_t longLine[4096];
	FILE *stream = tmpfile();
	UArray *line = newByteArray();
	int ok = 0;

	if (NULL == stream || NULL == line)
		goto done;
	if (fwrite(input, 1, sizeof(input) - 1, stream) != sizeof(input) - 1)
		goto done;
	rewind(stream);
	if (!lineEquals(line, stream, (const uint8_t *)"", 0) ||
		!lineEquals(line, stream, first, sizeof(first) - 1) ||
		!lineEquals(line, stream, second, sizeof(second) - 1) ||
		!lineEquals(line, stream, third, sizeof(third) - 1) ||
		!lineEquals(line, stream, last, sizeof(last) - 1))
		goto done;
	UArray_setSize_(line, 0);
	if (UArray_readLineFromCStream_(line, stream) != 0)
		goto done;
	fclose(stream);
	stream = tmpfile();
	if (NULL == stream)
		goto done;
	memset(longLine, 'x', sizeof(longLine));
	if (fwrite(longLine, 1, sizeof(longLine), stream) != sizeof(longLine) ||
		fwrite("\r\nZ\n", 1, 4, stream) != 4)
		goto done;
	rewind(stream);
	if (!lineEquals(line, stream, longLine, sizeof(longLine)) ||
		!lineEquals(line, stream, (const uint8_t *)"Z", 1))
		goto done;
	fclose(stream);
	stream = tmpfile();
	if (NULL == stream)
		goto done;
	if (fwrite(binary, 1, sizeof(binary), stream) != sizeof(binary))
		goto done;
	rewind(stream);
	if (!lineEquals(line, stream, binary, sizeof(binary) - 1))
		goto done;
	ok = 1;

done:
	if (stream)
		fclose(stream);
	UArray_free(line);
	return ok;
}

static int testReadCountsAndErrors(void)
{
	static const uint8_t expected[] = {'a', 'b', 'c'};
	ErrorCookie cookie = {0};
	FILE *stream = tmpfile();
	FILE *errorStream = NULL;
	UArray *array = newByteArray();
	int ok = 0;

	if (NULL == stream || NULL == array)
		goto done;
	if (fwrite(expected, 1, sizeof(expected), stream) != sizeof(expected))
		goto done;
	rewind(stream);
	if (UArray_readNumberOfItems_fromCStream_(array, 5, stream) != 3 ||
		array->size != sizeof(expected) ||
		memcmp(array->data, expected, sizeof(expected)) != 0)
		goto done;
	fclose(stream);
	stream = NULL;

	UArray_setSize_(array, 0);
	errorStream = openErrorStream(&cookie);
	if (NULL == errorStream)
		goto done;
	if (UArray_readNumberOfItems_fromCStream_(array, 5, errorStream) != -1 ||
		array->size != sizeof(expected) ||
		memcmp(array->data, expected, sizeof(expected)) != 0)
		goto done;
	fclose(errorStream);
	errorStream = NULL;

	UArray_setSize_(array, 0);
	cookie.offset = 0;
	errorStream = openErrorStream(&cookie);
	if (NULL == errorStream)
		goto done;
	if (UArray_readLineFromCStream_(array, errorStream) != -1 ||
		array->size != sizeof(expected) ||
		memcmp(array->data, expected, sizeof(expected)) != 0)
		goto done;
	ok = 1;

done:
	if (errorStream)
		fclose(errorStream);
	if (stream)
		fclose(stream);
	UArray_free(array);
	return ok;
}


static int testMalformedDestinationRejected(void)
{
	FILE *stream = tmpfile();
	UArray *array = newByteArray();
	UArray *wide = UArray_new();
	uint8_t payload[] = {'x', 'y'};
	uint8_t *originalData = NULL;
	uint8_t *originalWideData = NULL;
	size_t originalSize = 0;
	size_t originalWideSize = 0;
	size_t invalidWideSize = 0;
	size_t maxWideItems = 0;
	int okay = 0;

	if (NULL == stream || NULL == array || NULL == wide)
		goto done;
	if (fwrite(payload, 1, sizeof(payload), stream) != sizeof(payload) ||
		fseek(stream, 0, SEEK_SET) != 0)
		goto done;
	UArray_setSize_(array, 1);
	if (array->size != 1 || NULL == array->data)
		goto done;

	/* Public structs can be malformed by a caller; reject before appending. */
	originalData = array->data;
	originalSize = array->size;
	array->data = NULL;
	okay = UArray_readNumberOfItems_fromCStream_(array, 1, stream) == -1 &&
		array->size == 1 && NULL == array->data && ftell(stream) == 0;
	array->data = originalData;
	originalData = NULL;
	if (!okay)
		goto done;
	okay = 0;

	UArray_setItemType_(wide, CTYPE_uint16_t);
	UArray_setSize_(wide, 1);
	if (wide->itemSize != sizeof(uint16_t) || wide->size != 1 ||
		NULL == wide->data)
		goto done;
	originalWideData = wide->data;
	originalWideSize = wide->size;
	maxWideItems = (size_t)-1 / wide->itemSize;
	invalidWideSize = maxWideItems + 1;

	/* Reject an impossible current byte extent without consuming input. */
	wide->size = invalidWideSize;
	if (UArray_readFromCStream_(wide, stream) != -1 ||
		wide->size != invalidWideSize || wide->data != originalWideData ||
		ftell(stream) != 0 ||
		UArray_readNumberOfItems_fromCStream_(wide, 1, stream) != -1 ||
		wide->size != invalidWideSize || wide->data != originalWideData ||
		ftell(stream) != 0) {
		wide->size = originalWideSize;
		fprintf(stderr, "UArray stream read accepted a malformed byte extent\n");
		goto done;
	}

	/* A valid maximum extent must not be grown past the byte-size limit. */
	wide->size = maxWideItems;
	if (UArray_readFromCStream_(wide, stream) != -1 ||
		wide->size != maxWideItems || wide->data != originalWideData ||
		ftell(stream) != (long)sizeof(payload)) {
		wide->size = originalWideSize;
		fprintf(stderr, "UArray stream read allowed item-count byte overflow\n");
		goto done;
	}
	wide->size = originalWideSize;
	if (fseek(stream, 0, SEEK_SET) != 0)
		goto done;
	wide->size = maxWideItems;
	if (UArray_readNumberOfItems_fromCStream_(wide, 1, stream) != -1 ||
		wide->size != maxWideItems || wide->data != originalWideData ||
		ftell(stream) != (long)sizeof(uint16_t)) {
		wide->size = originalWideSize;
		fprintf(stderr, "UArray bounded read allowed item-count byte overflow\n");
		goto done;
	}
	wide->size = originalWideSize;
	okay = 1;

done:
	if (wide && originalWideData) {
		wide->data = originalWideData;
		wide->size = originalWideSize;
	}
	if (array && originalData) {
		array->data = originalData;
		array->size = originalSize;
	}
	if (stream)
		fclose(stream);
	UArray_free(wide);
	UArray_free(array);
	return okay;
}

static int testLineArgumentValidation(void)
{
	FILE *stream = tmpfile();
	UArray *line = newByteArray();
	UArray *wide = UArray_new();
	uint8_t payload = 'Q';
	uint8_t *originalData = NULL;
	int okay = 0;

	if (NULL == stream || NULL == line || NULL == wide)
		goto done;
	if (fwrite(&payload, 1, 1, stream) != 1 ||
		fseek(stream, 0, SEEK_SET) != 0)
		goto done;

	if (UArray_readLineFromCStream_(NULL, stream) != -1 ||
		ftell(stream) != 0) {
		fprintf(stderr, "UArray line reader accepted a NULL destination or consumed input\n");
		goto done;
	}
	if (UArray_readLineFromCStream_(line, NULL) != -1) {
		fprintf(stderr, "UArray line reader accepted a NULL stream\n");
		goto done;
	}

	UArray_setItemType_(wide, CTYPE_uint16_t);
	if (wide->itemSize != sizeof(uint16_t) ||
		UArray_readLineFromCStream_(wide, stream) != -1 ||
		ftell(stream) != 0) {
		fprintf(stderr, "UArray line reader accepted a non-byte destination or consumed input\n");
		goto done;
	}

	UArray_setSize_(line, 1);
	if (line->size != 1 || NULL == line->data)
		goto done;
	originalData = line->data;
	line->data = NULL;
	if (UArray_readLineFromCStream_(line, stream) != -1 ||
		line->size != 1 || NULL != line->data || ftell(stream) != 0) {
		fprintf(stderr, "UArray line reader accepted malformed storage or consumed input\n");
		goto done;
	}
	line->data = originalData;
	originalData = NULL;
	okay = 1;

done:
	if (line && originalData)
		line->data = originalData;
	if (stream)
		fclose(stream);
	UArray_free(wide);
	UArray_free(line);
	return okay;
}


static int testInvalidFileWritePreservesDestination(void)
{
	char pathName[] = "/tmp/uarray-stream-invalid-write-XXXXXX";
	static const uint8_t original[] = {'k', 'e', 'e', 'p'};
	uint8_t actual[sizeof(original)];
	int fd = -1;
	FILE *initial = NULL;
	FILE *check = NULL;
	UArray *source = newByteArray();
	UArray *path = NULL;
	uint8_t *originalData = NULL;
	int okay = 0;

	fd = mkstemp(pathName);
	if (fd < 0 || NULL == source)
		goto done;
	initial = fdopen(fd, "wb");
	if (NULL == initial)
		goto done;
	fd = -1;
	if (fwrite(original, 1, sizeof(original), initial) != sizeof(original) ||
		fclose(initial) != 0) {
		initial = NULL;
		goto done;
	}
	initial = NULL;

	path = UArray_newWithData_type_encoding_size_copy_(
		pathName, CTYPE_uint8_t, CENCODING_UTF8, strlen(pathName), 1);
	if (NULL == path)
		goto done;
	UArray_setSize_(source, 1);
	if (source->size != 1 || NULL == source->data)
		goto done;

	/* A malformed source must be rejected before fopen("wb") truncates the path. */
	originalData = source->data;
	source->data = NULL;
	if (UArray_writeToFilePath_(source, path) != -1)
		goto done;
	source->data = originalData;
	originalData = NULL;

	check = fopen(pathName, "rb");
	if (NULL == check)
		goto done;
	if (fread(actual, 1, sizeof(actual), check) != sizeof(actual) ||
		fgetc(check) != EOF || ferror(check) ||
		memcmp(actual, original, sizeof(original)) != 0)
		goto done;
	okay = 1;

done:
	if (source && originalData)
		source->data = originalData;
	if (initial)
		fclose(initial);
	if (fd >= 0)
		close(fd);
	if (check)
		fclose(check);
	UArray_free(path);
	UArray_free(source);
	unlink(pathName);
	return okay;
}

int main(void)
{
	if (!testInvalidFileWritePreservesDestination()) {
		fprintf(stderr, "UArray file write truncated an existing destination for invalid source storage\n");
		return EXIT_FAILURE;
	}
	if (!testLineArgumentValidation()) {
		fprintf(stderr, "UArray line argument validation failed\n");
		return EXIT_FAILURE;
	}
	if (!testLineBoundaries()) {
		fprintf(stderr, "UArray line boundaries or line payload preservation failed\n");
		return EXIT_FAILURE;
	}
	if (!testReadCountsAndErrors()) {
		fprintf(stderr, "UArray stream short-read or error propagation failed\n");
		return EXIT_FAILURE;
	}
	if (!testMalformedDestinationRejected()) {
		fprintf(stderr, "UArray bounded read accepted malformed destination storage\n");
		return EXIT_FAILURE;
	}
	puts("UArray stream I/O test passed");
	return EXIT_SUCCESS;
}
