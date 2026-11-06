#define _GNU_SOURCE

#include "UArray.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

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

int main(void)
{
	if (!testLineBoundaries()) {
		fprintf(stderr, "UArray line boundaries or line payload preservation failed\n");
		return EXIT_FAILURE;
	}
	if (!testReadCountsAndErrors()) {
		fprintf(stderr, "UArray stream short-read or error propagation failed\n");
		return EXIT_FAILURE;
	}
	puts("UArray stream I/O test passed");
	return EXIT_SUCCESS;
}
