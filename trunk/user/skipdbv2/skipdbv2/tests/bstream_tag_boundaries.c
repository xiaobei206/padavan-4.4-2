#include "BStreamTag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return EXIT_FAILURE;
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
	puts("BStreamTag boundaries test passed");
	return EXIT_SUCCESS;
}
