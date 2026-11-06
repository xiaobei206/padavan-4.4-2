/*
	copyright: Steve Dekorte, 2006. All rights reserved.
	license: See _BSDLicense.txt.
*/

#include "UArray.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>

/* read ------------------------------------------------------ */

size_t UArray_fread_(UArray *self, FILE *fp)
{
	size_t itemsRead;
	if (NULL == self || NULL == fp || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && NULL == self->data))
		return 0;
	if (self->size == 0)
		return 0;

	itemsRead = fread(self->data, self->itemSize, self->size, fp);
	UArray_setSize_(self, itemsRead);
	if (self->size != itemsRead) {
		/* Shrinking realloc may fail; the old allocation remains large enough. */
		self->size = itemsRead;
		if (self->data)
			self->data[itemsRead * self->itemSize] = 0;
		UArray_changed(self);
	}
	return itemsRead;
}

long UArray_readFromCStream_(UArray *self, FILE *fp)
{
	long totalItemsRead = 0;
	size_t itemsPerBuffer;
	UArray *buffer;
	if (NULL == self || NULL == fp || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && NULL == self->data))
		return -1;

	itemsPerBuffer = 4096 / self->itemSize;
	if (itemsPerBuffer == 0)
		itemsPerBuffer = 1;
	buffer = UArray_new();
	if (NULL == buffer)
		return -1;
	UArray_setItemType_(buffer, self->itemType);
	if (buffer->itemType != self->itemType) {
		UArray_free(buffer);
		return -1;
	}
	UArray_setSize_(buffer, itemsPerBuffer);
	if (buffer->size != itemsPerBuffer ||
		(itemsPerBuffer > 0 && NULL == buffer->data)) {
		UArray_free(buffer);
		return -1;
	}

	while (!feof(fp) && !ferror(fp)) {
		size_t itemsRead;
		size_t oldSize;
		UArray_setSize_(buffer, itemsPerBuffer);
		if (buffer->size != itemsPerBuffer) {
			UArray_free(buffer);
			return -1;
		}
		itemsRead = UArray_fread_(buffer, fp);
		if (itemsRead > (size_t)(LONG_MAX - totalItemsRead) ||
			itemsRead > (size_t)-1 - self->size ||
			itemsRead > (size_t)-1 / self->itemSize - self->size) {
			UArray_free(buffer);
			return -1;
		}
		oldSize = self->size;
		UArray_append_(self, buffer);
		if (self->size != oldSize + itemsRead) {
			UArray_free(buffer);
			return -1;
		}
		totalItemsRead += (long)itemsRead;
		if (itemsRead != itemsPerBuffer)
			break;
	}

	if (ferror(fp)) {
		perror("UArray_readFromCStream_");
		UArray_free(buffer);
		return -1;
	}
	UArray_free(buffer);
	return totalItemsRead;
}

long UArray_readNumberOfItems_fromCStream_(UArray *self, size_t size, FILE *stream)
{
	size_t itemsRead, oldSize;
	UArray *buffer;
	if (NULL == self || NULL == stream || self->itemSize == 0 ||
		(self->size > 0 && NULL == self->data) ||
		size > (size_t)LONG_MAX ||
		self->size > (size_t)-1 / self->itemSize ||
		size > (size_t)-1 - self->size)
		return -1;

	buffer = UArray_new();
	if (NULL == buffer)
		return -1;
	UArray_setItemType_(buffer, self->itemType);
	if (buffer->itemType != self->itemType) {
		UArray_free(buffer);
		return -1;
	}
	UArray_setSize_(buffer, size);
	if (buffer->size != size || (size > 0 && NULL == buffer->data)) {
		UArray_free(buffer);
		return -1;
	}

	itemsRead = UArray_fread_(buffer, stream);
	oldSize = self->size;
	if (itemsRead > (size_t)-1 / self->itemSize - oldSize) {
		UArray_free(buffer);
		return -1;
	}
	UArray_append_(self, buffer);
	UArray_free(buffer);
	if (self->size != oldSize + itemsRead || ferror(stream))
		return -1;
	return (long)itemsRead;
}

long UArray_readFromFilePath_(UArray *self, const UArray *path)
{
	FILE *stream;
	UArray *sysPath;
	long itemsRead;
	if (NULL == self || NULL == path)
		return -1;
	sysPath = UArray_asUTF8(path);
	if (NULL == sysPath || NULL == UArray_asCString(sysPath)) {
		UArray_free(sysPath);
		return -1;
	}
	stream = fopen(UArray_asCString(sysPath), "rb");
	if (NULL == stream) {
		UArray_free(sysPath);
		return -1;
	}
	itemsRead = UArray_readFromCStream_(self, stream);
	if (fclose(stream) != 0 && itemsRead >= 0)
		itemsRead = -1;
	UArray_free(sysPath);
	return itemsRead;
}

#define CHUNK_SIZE 4096

static int UArray_streamAppendLineChunk_(UArray *self,
	const uint8_t *bytes, size_t count)
{
	UArray chunk;
	size_t oldSize;

	if (count == 0)
		return 1;
	if (count > (size_t)-1 - self->size ||
		self->size + count == (size_t)-1)
		return 0;

	oldSize = self->size;
	chunk = UArray_stackAllocedWithData_type_size_((void *)bytes,
		self->itemType, count);
	if (chunk.itemSize != self->itemSize)
		return 0;
	UArray_append_(self, &chunk);
	return self->size == oldSize + count;
}

int UArray_readLineFromCStream_(UArray *self, FILE *stream)
{
	int readSomething = 0;
	int pushbackFailed = 0;
	size_t used = 0;
	uint8_t buffer[CHUNK_SIZE];

	if (NULL == self || NULL == stream || self->itemSize != 1 ||
		(self->size > 0 && NULL == self->data))
		return -1;

	for (;;) {
		int ch = fgetc(stream);
		if (ch == EOF)
			break;
		readSomething = 1;

		if (ch == '\n')
			break;
		if (ch == '\r') {
			int next = fgetc(stream);
			if (next != '\n' && next != EOF &&
				ungetc(next, stream) == EOF)
				pushbackFailed = 1;
			break;
		}

		buffer[used++] = (uint8_t)ch;
		if (used == sizeof(buffer)) {
			if (!UArray_streamAppendLineChunk_(self, buffer, used))
				return -1;
			used = 0;
		}
	}

	if (!UArray_streamAppendLineChunk_(self, buffer, used))
		return -1;
	if (ferror(stream) || pushbackFailed)
		return -1;
	return readSomething;
}

static int UArray_streamCanWrite_(const UArray *self)
{
	if (NULL == self || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		self->size > (size_t)LONG_MAX ||
		(self->size > 0 && NULL == self->data))
		return 0;
	return 1;
}

/* write ------------------------------------------------------ */

size_t UArray_fwrite_(const UArray *self, size_t size, FILE *fp)
{
	if (NULL == self || NULL == fp || self->itemSize == 0 ||
		size > self->size || size > (size_t)-1 / self->itemSize ||
		(size > 0 && NULL == self->data))
		return 0;
	if (size == 0)
		return 0;
	return fwrite(self->data, self->itemSize, size, fp);
}

long UArray_writeToCStream_(const UArray *self, FILE *stream)
{
	size_t itemsWritten;
	if (NULL == stream || !UArray_streamCanWrite_(self))
		return -1;
	itemsWritten = UArray_fwrite_(self, self->size, stream);
	if (ferror(stream) || itemsWritten != self->size ||
		itemsWritten > (size_t)LONG_MAX) {
		perror("UArray_writeToCStream_");
		return -1;
	}
	return (long)itemsWritten;
}

long UArray_writeToFilePath_(const UArray *self, const UArray *path)
{
	UArray *sysPath;
	FILE *fp;
	long itemsWritten;
	if (NULL == path || !UArray_streamCanWrite_(self))
		return -1;
	sysPath = UArray_asUTF8(path);
	if (NULL == sysPath || NULL == UArray_asCString(sysPath)) {
		UArray_free(sysPath);
		return -1;
	}
	fp = fopen(UArray_asCString(sysPath), "wb");
	if (NULL == fp) {
		UArray_free(sysPath);
		return -1;
	}
	itemsWritten = UArray_writeToCStream_(self, fp);
	if (fclose(fp) != 0 && itemsWritten >= 0)
		itemsWritten = -1;
	UArray_free(sysPath);
	return itemsWritten;
}
