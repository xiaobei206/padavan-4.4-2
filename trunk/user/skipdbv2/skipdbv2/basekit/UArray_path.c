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

static size_t UArray_pathItemSize_(CTYPE itemType)
{
	switch (itemType) {
		case CTYPE_uint8_t: return sizeof(uint8_t);
		case CTYPE_uint16_t: return sizeof(uint16_t);
		case CTYPE_uint32_t: return sizeof(uint32_t);
		case CTYPE_uint64_t: return sizeof(uint64_t);
		case CTYPE_int8_t: return sizeof(int8_t);
		case CTYPE_int16_t: return sizeof(int16_t);
		case CTYPE_int32_t: return sizeof(int32_t);
		case CTYPE_int64_t: return sizeof(int64_t);
		case CTYPE_float32_t: return sizeof(float32_t);
		case CTYPE_float64_t: return sizeof(float64_t);
		case CTYPE_uintptr_t: return sizeof(uintptr_t);
		default: return 0;
	}
}


static int UArray_hasValidPathStorage_(const UArray *self)
{
	size_t expectedItemSize;
	if (NULL == self)
		return 0;
	expectedItemSize = UArray_pathItemSize_(self->itemType);
	if (expectedItemSize == 0 || self->itemSize != expectedItemSize ||
		self->size > (size_t)-1 / expectedItemSize ||
		self->size > (size_t)LONG_MAX ||
		(self->size > 0 && NULL == self->data))
		return 0;
	return 1;
}


void UArray_appendPath_(UArray *self, const UArray *path)
{
	const UArray sep = UArray_stackAllocedWithCString_(IO_PATH_SEPARATOR);
	int selfEndsWithSep, pathStartsWithSep;

	if (!UArray_hasValidPathStorage_(self) ||
		!UArray_hasValidPathStorage_(path))
		return;
	selfEndsWithSep = IS_PATH_SEPERATOR(UArray_lastLong(self));
	pathStartsWithSep = IS_PATH_SEPERATOR(UArray_firstLong(path));

	if (!selfEndsWithSep && !pathStartsWithSep)
	{
		if(self->size != 0) UArray_append_(self, &sep);
		UArray_append_(self, path);
	}
	else if (selfEndsWithSep && pathStartsWithSep)
	{
		const UArray pathPart = UArray_stackRange(path, 1, path->size - 1);
		UArray_append_(self, &pathPart);
	}
	else
	{
		UArray_append_(self, path);
	}
}

void UArray_removeLastPathComponent(UArray *self)
{
	long pos;
	if (!UArray_hasValidPathStorage_(self))
		return;
	pos = UArray_findLastPathComponent(self);
	if (pos < 0)
		return;
	if (pos > 0)
		pos--;
	UArray_setSize_(self, (size_t)pos);
}

void UArray_clipBeforeLastPathComponent(UArray *self)
{
	long pos;
	if (!UArray_hasValidPathStorage_(self))
		return;
	pos = UArray_findLastPathComponent(self);
	if (pos != -1)
	{
		UArray_removeRange(self, 0, pos);
	}
}

long UArray_findLastPathComponent(const UArray *self)
{
	if (!UArray_hasValidPathStorage_(self))
		return -1;
	if (self->size)
	{
		UArray seps = UArray_stackAllocedWithCString_(IO_PATH_SEPARATORS);
		UArray s = UArray_stackRange(self, 0, self->size);
		long pos = 0;

		while (s.size && (pos = UArray_rFindAnyValue_(&s, &seps)) == s.size - 1)
		{
			s.size = pos;
		}

		if (pos == -1) { pos = 0; } else { pos ++; }
		return pos;
	}

	return 0;
}

UArray *UArray_lastPathComponent(const UArray *self)
{
	long pos;
	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	pos = UArray_findLastPathComponent(self);
	if (pos < 0 || (size_t)pos > self->size)
		return NULL;
	return UArray_range(self, (size_t)pos, self->size - (size_t)pos);
}

long UArray_findPathExtension(UArray *self)
{
	UArray dot;
	long path_start, pos;
	if (!UArray_hasValidPathStorage_(self))
		return -1;
	path_start = UArray_findLastPathComponent(self);
	if (path_start < 0)
		return -1;
	dot = UArray_stackAllocedWithCString_(IO_PATH_SEPARATOR_DOT);
	pos = UArray_rFind_(self, &dot);
	/* Dots in parent directories and a filename's leading dot are not extensions. */
	if (pos <= path_start)
		return -1;
	return pos;
}

void UArray_removePathExtension(UArray *self)
{
	long pos;
	if (!UArray_hasValidPathStorage_(self))
		return;
	pos = UArray_findPathExtension(self);
	if (pos != -1)
	{
		UArray_setSize_(self, pos);
	}
}

UArray *UArray_pathExtension(UArray *self)
{
	long pos;
	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	pos = UArray_findPathExtension(self);
	if (pos == -1 || (size_t)pos >= self->size - 1)
	{
		return UArray_newWithCString_copy_("", 1);
	}

	return UArray_range(self, pos + 1, self->size - pos - 1);
}

UArray *UArray_fileName(UArray *self)
{
	long extPos, dotPos;
	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	extPos = UArray_findLastPathComponent(self);
	dotPos = UArray_findPathExtension(self);
	if (extPos < 0)
		return NULL;

	//if (extPos == -1) { extPos = 0; } else { extPos ++; }
	if (dotPos == -1) dotPos = (long)self->size;
	if (dotPos < extPos)
		return NULL;
	return UArray_range(self, (size_t)extPos, (size_t)(dotPos - extPos));
}

// to/from os path - always returns a copy

int UArray_OSPathSeparatorIsUnixSeparator(void)
{
	return strcmp(OS_PATH_SEPARATOR, "/") == 0;
}

UArray *UArray_asOSPath(UArray *self)
{
	UArray *a;
	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	a = UArray_clone(self);
	UArray_replaceCString_withCString_(a, IO_PATH_SEPARATOR, OS_PATH_SEPARATOR);
	return a;
}

UArray *UArray_asUnixPath(UArray *self)
{
	UArray *a;
	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	a = UArray_clone(self);
	UArray_replaceCString_withCString_(a, OS_PATH_SEPARATOR, IO_PATH_SEPARATOR);
	return a;
}

