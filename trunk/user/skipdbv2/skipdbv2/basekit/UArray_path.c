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


static int UArray_pathComponentBounds_(const UArray *self, size_t *componentStart, size_t *componentEnd)
{
	UArray component;
	UArray separators;
	long separatorPosition;

	if (NULL == componentStart || NULL == componentEnd ||
		!UArray_hasValidPathStorage_(self))
		return 0;

	component = UArray_stackRange(self, 0, self->size);
	while (component.size > 0 && IS_PATH_SEPERATOR(UArray_lastLong(&component)))
		component.size--;

	*componentEnd = component.size;
	if (component.size == 0) {
		*componentStart = 0;
		return 1;
	}

	separators = UArray_stackAllocedWithCString_(IO_PATH_SEPARATORS);
	separatorPosition = UArray_rFindAnyValue_(&component, &separators);
	*componentStart = separatorPosition < 0 ? 0 : (size_t)separatorPosition + 1;
	return 1;
}


void UArray_appendPath_(UArray *self, const UArray *path)
{
	const UArray sep = UArray_stackAllocedWithCString_(IO_PATH_SEPARATOR);
	int selfEndsWithSep, pathStartsWithSep;

	if (!UArray_hasValidPathStorage_(self) ||
		!UArray_hasValidPathStorage_(path))
		return;
	/* An empty component must not turn a plain path into a trailing-slash path. */
	if (path->size == 0)
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
	size_t componentStart, componentEnd;

	if (!UArray_hasValidPathStorage_(self))
		return;
	if (!UArray_pathComponentBounds_(self, &componentStart, &componentEnd) ||
		componentStart == componentEnd)
		return;

	if (componentStart == 0)
		UArray_setSize_(self, 0);
	else if (componentStart == 1)
		UArray_setSize_(self, 1);
	else
		UArray_setSize_(self, componentStart - 1);
}

void UArray_clipBeforeLastPathComponent(UArray *self)
{
	size_t componentStart, componentEnd;

	if (!UArray_hasValidPathStorage_(self))
		return;
	if (UArray_pathComponentBounds_(self, &componentStart, &componentEnd) &&
		componentStart != componentEnd) {
		UArray_setSize_(self, componentEnd);
		UArray_removeRange(self, 0, componentStart);
	}
}

long UArray_findLastPathComponent(const UArray *self)
{
	size_t componentStart, componentEnd;

	if (!UArray_hasValidPathStorage_(self))
		return -1;
	if (!UArray_pathComponentBounds_(self, &componentStart, &componentEnd))
		return -1;
	return (long)componentStart;
}

UArray *UArray_lastPathComponent(const UArray *self)
{
	size_t componentStart, componentEnd;

	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	if (!UArray_pathComponentBounds_(self, &componentStart, &componentEnd))
		return NULL;
	return UArray_range(self, componentStart, componentEnd - componentStart);
}

long UArray_findPathExtension(UArray *self)
{
	UArray component, dot;
	size_t componentStart, componentEnd;
	long pos;

	if (!UArray_hasValidPathStorage_(self))
		return -1;
	if (!UArray_pathComponentBounds_(self, &componentStart, &componentEnd))
		return -1;
	if (componentStart >= componentEnd)
		return -1;

	component = UArray_stackRange(self, componentStart, componentEnd - componentStart);
	dot = UArray_stackAllocedWithCString_(IO_PATH_SEPARATOR_DOT);
	pos = UArray_rFind_(&component, &dot);
	/* Dots in parent directories and a filename's leading dot are not extensions. */
	if (pos <= 0)
		return -1;
	return (long)componentStart + pos;
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
	size_t componentStart, componentEnd;

	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	if (!UArray_pathComponentBounds_(self, &componentStart, &componentEnd))
		return NULL;
	pos = UArray_findPathExtension(self);
	if (pos == -1 || (size_t)pos < componentStart ||
		(size_t)pos + 1 >= componentEnd)
		return UArray_newWithCString_copy_("", 1);

	return UArray_range(self, (size_t)pos + 1, componentEnd - ((size_t)pos + 1));
}

UArray *UArray_fileName(UArray *self)
{
	long dotPos;
	size_t componentStart, componentEnd;

	if (!UArray_hasValidPathStorage_(self))
		return NULL;
	if (!UArray_pathComponentBounds_(self, &componentStart, &componentEnd))
		return NULL;
	dotPos = UArray_findPathExtension(self);
	if (dotPos == -1)
		dotPos = (long)componentEnd;
	if ((size_t)dotPos < componentStart || (size_t)dotPos > componentEnd)
		return NULL;
	return UArray_range(self, componentStart, (size_t)dotPos - componentStart);
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

