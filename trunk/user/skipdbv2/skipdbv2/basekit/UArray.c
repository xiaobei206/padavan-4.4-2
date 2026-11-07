/*
copyright: Steve Dekorte, 2006. All rights reserved.
license: See _BSDLicense.txt.
*/

#include "Base.h"

#define UArray_C
#include "UArray.h"
#undef UArray_C

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

size_t CTYPE_size(CTYPE type)
{
	switch (type)
	{
		case CTYPE_uint8_t:   return sizeof(uint8_t);
		case CTYPE_uint16_t:  return sizeof(uint16_t);
		case CTYPE_uint32_t:  return sizeof(uint32_t);
		case CTYPE_uint64_t:  return sizeof(uint64_t);

		case CTYPE_int8_t:    return sizeof(int8_t);
		case CTYPE_int16_t:   return sizeof(int16_t);
		case CTYPE_int32_t:   return sizeof(int32_t);
		case CTYPE_int64_t:   return sizeof(int64_t);

		case CTYPE_float32_t: return sizeof(float32_t);
		case CTYPE_float64_t: return sizeof(float64_t);

		case CTYPE_uintptr_t: return sizeof(uintptr_t);
	}
	return 0;
}

const char *CTYPE_name(CTYPE type)
{
	switch (type)
	{
		case CTYPE_uint8_t:   return "uint8";
		case CTYPE_uint16_t:  return "uint16";
		case CTYPE_uint32_t:  return "uint32";
		case CTYPE_uint64_t:  return "uint64";

		case CTYPE_int8_t:    return "int8";
		case CTYPE_int16_t:   return "int16";
		case CTYPE_int32_t:   return "int32";
		case CTYPE_int64_t:   return "int64";

		case CTYPE_float32_t: return "float32";
		case CTYPE_float64_t: return "float64";

		case CTYPE_uintptr_t: return "intptr";
	}
	return "unknown";
}

int CTYPE_forName(const char *name)
{
	if (NULL == name)
		return -1;
	if(!strcmp(name, "uint8"))   return CTYPE_uint8_t;
	if(!strcmp(name, "uint16"))  return CTYPE_uint16_t;
	if(!strcmp(name, "uint32"))  return CTYPE_uint32_t;
	if(!strcmp(name, "uint64"))  return CTYPE_uint64_t;
	if(!strcmp(name, "int8"))    return CTYPE_int8_t;
	if(!strcmp(name, "int16"))   return CTYPE_int16_t;
	if(!strcmp(name, "int32"))   return CTYPE_int32_t;
	if(!strcmp(name, "int64"))   return CTYPE_int64_t;
	if(!strcmp(name, "float32")) return CTYPE_float32_t;
	if(!strcmp(name, "float64")) return CTYPE_float64_t;
	return -1;
}

int CENCODING_forName(const char *name)
{
	if (NULL == name)
		return -1;
	if(!strcmp(name, "ascii"))  return CENCODING_ASCII;
	if(!strcmp(name, "utf8"))   return CENCODING_UTF8;
	if(!strcmp(name, "utf16"))  return CENCODING_UTF16;
	if(!strcmp(name, "utf32"))  return CENCODING_UTF32;
	if(!strcmp(name, "number")) return CENCODING_NUMBER;
	return -1;
}

const char *CENCODING_name(CENCODING encoding)
{
	switch (encoding)
	{
		case CENCODING_ASCII:  return "ascii";
		case CENCODING_UTF8:   return "utf8";
		case CENCODING_UTF16:  return "utf16";
		case CENCODING_UTF32:  return "utf32";
		case CENCODING_NUMBER: return "number";
	}
	return "unknown";
}

// error

void UArray_unsupported_with_(const UArray *self, const char *methodName, const UArray *other)
{
	//UArray_error_(self, "Error: '%s' not supported between '%s' and '%s'\n");
	printf("Error: '%s' not supported between '%s' and '%s'\n",
		   methodName, CTYPE_name(self->itemType), CTYPE_name(other->itemType));
	exit(-1);
}

void UArray_error_(const UArray *self, char *e)
{
	printf("%s\n", e);
	exit(-1);
}

// new

CTYPE UArray_itemType(const UArray *self)
{
	return self->itemType;
}

size_t UArray_itemSize(const UArray *self)
{
	return self ? self->itemSize : 0;
}

//inline 
size_t UArray_sizeRequiredToContain_(const UArray *self, const UArray *other)
{
	size_t otherBytes;
	if (NULL == self || self->itemSize == 0 || NULL == other)
		return 0;
	otherBytes = UArray_sizeInBytes(other);
	/* Divide before adding the remainder flag to avoid ceil-division overflow. */
	return otherBytes / self->itemSize +
		(otherBytes % self->itemSize != 0);
}

void UArray_rawSetItemType_(UArray *self, CTYPE type)
{
	size_t itemSize;
	if (NULL == self)
		return;
	itemSize = CTYPE_size(type);
	if (itemSize == 0)
		return;
	self->itemType = type;
	self->itemSize = itemSize;
}

void UArray_setItemType_(UArray *self, CTYPE type)
{
	size_t itemSize, oldItemSize, sizeInBytes, newCount, newBytes;
	if (NULL == self)
		return;
	itemSize = CTYPE_size(type);
	oldItemSize = self->itemSize;
	if (itemSize == 0 || oldItemSize == 0 ||
		self->size > (size_t)-1 / oldItemSize)
		return;

	sizeInBytes = self->size * oldItemSize;
	newCount = sizeInBytes / itemSize;
	if (sizeInBytes % itemSize != 0) {
		if (newCount == (size_t)-1)
			return;
		newCount++;
	}
	if (newCount > (size_t)-1 / itemSize)
		return;
	newBytes = newCount * itemSize;
	if (newBytes == (size_t)-1)
		return;

	/* Grow the old-typed array enough to hold the rounded-up byte count.
	 * Check for allocation failure before committing the new type metadata. */
	if (newBytes > sizeInBytes) {
		size_t resizedCount = newBytes / oldItemSize;
		if (newBytes % oldItemSize != 0) {
			if (resizedCount == (size_t)-1)
				return;
			resizedCount++;
		}
		if (resizedCount > (size_t)-1 / oldItemSize ||
			resizedCount * oldItemSize == (size_t)-1)
			return;
		UArray_setSize_(self, resizedCount);
		if (self->size != resizedCount)
			return;
	}

	self->itemType = type;
	self->itemSize = itemSize;
	self->size = newCount;

	/* Ensure encoding is sane for the new type. */
	if (UArray_isFloatType(self))
	{
		self->encoding = CENCODING_NUMBER;
	}
	else if (self->encoding == CENCODING_ASCII)
	{
		switch(self->itemSize)
		{
			case 2: self->encoding = CENCODING_UTF16; break;
			case 4: self->encoding = CENCODING_UTF32; break;
			case 8: self->encoding = CENCODING_NUMBER; break;
		}
	}
}

CENCODING UArray_encoding(const UArray *self)
{
	return self->encoding;
}

void UArray_setEncoding_(UArray *self, CENCODING encoding)
{
	CTYPE targetType;
	if (NULL == self)
		return;

	/* Keep encoding metadata consistent if changing the item type cannot
	 * complete because of invalid state or allocation failure. */
	switch(encoding)
	{
		case CENCODING_ASCII:
		case CENCODING_UTF8:
			targetType = CTYPE_uint8_t;
			break;
		case CENCODING_UTF16:
			targetType = CTYPE_uint16_t;
			break;
		case CENCODING_UTF32:
			targetType = CTYPE_uint32_t;
			break;
		case CENCODING_NUMBER:
			/* Raw numeric encoding preserves the preselected item type. */
			self->encoding = encoding;
			return;
		default:
			return;
	}

	if (self->itemType != targetType)
		UArray_setItemType_(self, targetType);
	if (self->itemType == targetType)
		self->encoding = encoding;
}

void UArray_convertToEncoding_(UArray *self, CENCODING encoding)
{
	UArray *converted = NULL;
	if (NULL == self)
		return;

	switch (encoding)
	{
		case CENCODING_ASCII:
		case CENCODING_UTF8:
			converted = UArray_asUTF8(self);
			break;
		case CENCODING_UTF16:
			converted = UArray_asUTF16(self);
			break;
		case CENCODING_UTF32:
			converted = UArray_asUTF32(self);
			break;
		case CENCODING_NUMBER:
			converted = UArray_clone(self);
			if (NULL == converted)
				return;
			UArray_setItemType_(converted, CTYPE_uint8_t);
			if (converted->itemType != CTYPE_uint8_t ||
				converted->itemSize != sizeof(uint8_t)) {
				UArray_free(converted);
				return;
			}
			converted->encoding = encoding;
			UArray_swapWith_(self, converted);
			UArray_free(converted);
			UArray_changed(self);
			return;
		default:
			return;
	}

	/* Conversion routines return NULL on invalid input or allocation failure.
	 * Keep the original bytes and encoding unchanged in that case. */
	if (NULL == converted)
		return;
	UArray_swapWith_(self, converted);
	UArray_free(converted);
	if (encoding == CENCODING_ASCII)
		self->encoding = CENCODING_ASCII;
	UArray_changed(self);
}

UArray *UArray_newWithData_type_encoding_size_copy_(void *bytes, CTYPE type, CENCODING encoding, size_t size, int copy)
{
	UArray *self;
	size_t itemSize = CTYPE_size(type);
	if (itemSize == 0 || size > (size_t)-1 / itemSize ||
		size * itemSize == (size_t)-1 || (size > 0 && NULL == bytes))
		return NULL;
	self = (UArray *)io_calloc(1, sizeof(UArray));
	if (NULL == self)
		return NULL;
	UArray_setData_type_size_copy_(self, bytes, type, size, copy);
	if (self->itemSize != itemSize || self->size != size ||
		(copy && NULL == self->data)) {
		if (self->data)
			io_free(self->data);
		io_free(self);
		return NULL;
	}
	self->encoding = encoding;
	return self;
}

UArray *UArray_newWithData_type_size_copy_(void *bytes, CTYPE type, size_t size, int copy)
{
	return UArray_newWithData_type_encoding_size_copy_(bytes, type,
		CENCODING_ASCII, size, copy);
}

UArray *UArray_new(void)
{
	return UArray_newWithData_type_size_copy_("", CTYPE_uint8_t, 0, 1);
}

UArray *UArray_clone(const UArray *self)
{
	UArray *out;
	if (NULL == self)
		return NULL;
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_copy_(out, self);
	if (out->itemType != self->itemType || out->encoding != self->encoding ||
		out->size != self->size || (self->size > 0 && NULL == out->data)) {
		UArray_free(out);
		return NULL;
	}
	return out;
}

void UArray_show(const UArray *self)
{
	if (NULL == self) {
		printf("UArray <null>\n");
		return;
	}
	printf("UArray_%p %s\t", (const void *)self, CTYPE_name(self->itemType));
	printf("size: %zu ", self->size);
	printf("itemSize: %zu ", self->itemSize);
	printf("data: ");
	UArray_print(self);
	printf("\n");
}

void UArray_print(const UArray *self)
{
	if (NULL == self) {
		printf("<null>");
		return;
	}
	if (self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && self->data == NULL)) {
		printf("<invalid>");
		return;
	}
	if (self->encoding == CENCODING_ASCII || self->encoding == CENCODING_UTF8)
	{
		if (self->size > 0)
			fwrite(self->data, 1, self->size * self->itemSize, stdout);
	}
	else if (self->encoding != CENCODING_NUMBER)
	{
		UARRAY_FOREACH(self, i, v, printf("%c", (int)v); );
	}
	else if(UArray_isFloatType(self))
	{
		printf("[");
		UARRAY_FOREACH(self, i, v,
					printf("%f", (double)v);
					if(i != self->size - 1) printf(", ");
					);
		printf("]");
	}
	else
	{
		printf("[");
		UARRAY_FOREACH(self, i, v,
					printf("%ld", (long)v);
					if(i != self->size - 1) printf(", ");
					);
		printf("]");
	}
}

UArray UArray_stackAllocedWithData_type_size_(void *data, CTYPE type, size_t size)
{
	UArray self;
	memset(&self, 0, sizeof(UArray));

#ifdef UARRAY_DEBUG
	self.stackAllocated = 1;
#endif

	self.itemType = type;
	self.itemSize = CTYPE_size(type);
	self.size = size;
	self.data = data;
	return self;
}

BASEKIT_API UArray UArray_stackAllocedEmptyUArray(void)
{
	UArray self;
	memset(&self, 0, sizeof(UArray));

#ifdef UARRAY_DEBUG
	self.stackAllocated = 1;
#endif

	self.itemType = CTYPE_int32_t;
	self.itemSize = 4;
	self.size = 0;
	self.data = 0x0;
	return self;
}

UArray *UArray_newWithCString_copy_(char *s, int copy)
{
	if (NULL == s)
		return NULL;
	return UArray_newWithData_type_size_copy_(s, CTYPE_uint8_t, strlen(s), copy);
}

UArray *UArray_newWithCString_(const char *s)
{
	if (NULL == s)
		return NULL;
	return UArray_newWithData_type_size_copy_((uint8_t *)s, CTYPE_uint8_t, strlen(s), 1);
}

void UArray_empty(UArray *self)
{
	UArray_setSize_(self, 0);
}

void UArray_setCString_(UArray *self, const char *s)
{
	if (NULL == self || NULL == s)
		return;
	UArray_empty(self);
	UArray_setItemType_(self, CTYPE_uint8_t);
	if (self->itemType == CTYPE_uint8_t)
		UArray_appendCString_(self, s);
}

#ifdef UARRAY_DEBUG
void UArray_checkIfOkToRelloc(UArray *self)
{
	if(self->stackAllocated)
	{
		printf("UArray debug error: attempt to io_realloc UArray data that this UArray does not own");
		exit(-1);
	}
}
#endif

void UArray_setData_type_size_copy_(UArray *self, void *data, CTYPE type, size_t size, int copy)
{
	size_t itemSize, sizeInBytes;
	void *sourceCopy = NULL;
	uint8_t *newData;

	if (NULL == self)
		return;
	itemSize = CTYPE_size(type);
	if (itemSize == 0 || size > (size_t)-1 / itemSize)
		return;
	sizeInBytes = size * itemSize;
	if (sizeInBytes == (size_t)-1 || (sizeInBytes > 0 && NULL == data))
		return;

	if (copy)
	{
		/* Snapshot any overlapping source before reallocating or detaching. */
		if (sizeInBytes > 0 && self->data && self->itemSize > 0 &&
			self->size <= (size_t)-1 / self->itemSize)
		{
			size_t oldSizeInBytes = self->size * self->itemSize;
			uintptr_t base = (uintptr_t)self->data;
			uintptr_t src = (uintptr_t)data;
			size_t sourceOffset;
			int startsInside = src >= base && src - base <= oldSizeInBytes;
			int overlaps = startsInside || (src < base &&
				base - src < sizeInBytes);
			if (startsInside) {
				sourceOffset = (size_t)(src - base);
				/* The single byte at oldSizeInBytes is the trailing terminator. */
				if ((sourceOffset == oldSizeInBytes && sizeInBytes != 1) ||
					(sourceOffset < oldSizeInBytes &&
					 sizeInBytes > oldSizeInBytes - sourceOffset))
					return;
			}
			if (overlaps)
			{
				sourceCopy = io_malloc(sizeInBytes);
				if (NULL == sourceCopy)
					return;
				memmove(sourceCopy, data, sizeInBytes);
				data = sourceCopy;
			}
		}

		/* A stack-allocated UArray borrows its buffer; never realloc it. */
		if (self->stackAllocated)
			newData = (uint8_t *)io_malloc(sizeInBytes + 1);
		else
			newData = (uint8_t *)io_realloc(self->data, sizeInBytes + 1);
		if (NULL == newData) {
			if (sourceCopy)
				io_free(sourceCopy);
			return;
		}
		if (sizeInBytes > 0)
			memmove(newData, data, sizeInBytes);
		newData[sizeInBytes] = 0;
		self->data = newData;
		self->stackAllocated = 0;
		if (sourceCopy)
			io_free(sourceCopy);
	}
	else
	{
		if (self->data == data && self->data && self->itemSize > 0 &&
			self->size <= (size_t)-1 / self->itemSize &&
			sizeInBytes > self->size * self->itemSize)
			return;
		if (self->data != data) {
			/* A pointer into the current allocation cannot become an owner. */
			if (self->data && data) {
				size_t oldSizeInBytes = self->itemSize > 0 &&
					self->size <= (size_t)-1 / self->itemSize ?
					self->size * self->itemSize : 0;
				uintptr_t base = (uintptr_t)self->data;
				uintptr_t src = (uintptr_t)data;
				if (src >= base ? src - base <= oldSizeInBytes :
					base - src < sizeInBytes)
					return;
			}
			if (self->data && !self->stackAllocated)
				io_free(self->data);
			/* copy=0 transfers ownership of the supplied buffer. */
			self->stackAllocated = 0;
		}
		self->data = (uint8_t *)data;
	}

	self->itemType = type;
	self->itemSize = itemSize;
	self->size = size;
}
UArray UArray_stackAllocedWithCString_(char *s)
{
	if (NULL == s)
		return UArray_stackAllocedWithData_type_size_(NULL, CTYPE_uint8_t, 0);
	return UArray_stackAllocedWithData_type_size_(s, CTYPE_uint8_t, strlen(s));
}

const void *UArray_data(const UArray *self)
{
	return self ? self->data : NULL;
}

const uint8_t *UArray_bytes(const UArray *self)
{
	return self ? self->data : NULL;
}

uint8_t *UArray_mutableBytes(UArray *self)
{
	if (NULL == self)
		return NULL;
	UArray_changed(self);
	return self->data;
}

const char *UArray_asCString(const UArray *self)
{
	return self ? (const char *)(self->data) : NULL;
}

void UArray_stackFree(UArray *self)
{
	if (NULL == self)
		return;
	if (self->data && !self->stackAllocated)
		io_free(self->data);
	self->data = NULL;
	self->size = 0;
	self->hash = 0;
	self->stackAllocated = 0;
}

void UArray_free(UArray *self)
{
	if (NULL == self)
		return;
	if (self->data && !self->stackAllocated)
		io_free(self->data);
	io_free(self);
}

// size

void UArray_setSize_(UArray *self, size_t size)
{
	size_t oldSizeInBytes;
	size_t newSizeInBytes;
	uint8_t *newData;
	if (NULL == self || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		size > (size_t)-1 / self->itemSize)
		return;
	oldSizeInBytes = self->size * self->itemSize;
	newSizeInBytes = self->itemSize * size;
	if (newSizeInBytes == (size_t)-1 ||
		(oldSizeInBytes > 0 && NULL == self->data))
		return;
	if (size != self->size)
	{
		if (self->stackAllocated) {
			size_t copyBytes = oldSizeInBytes < newSizeInBytes ?
				oldSizeInBytes : newSizeInBytes;
			newData = (uint8_t *)io_malloc(newSizeInBytes + 1);
			if (NULL == newData)
				return;
			if (copyBytes > 0)
				memcpy(newData, self->data, copyBytes);
		} else {
#ifdef UARRAY_DEBUG
			UArray_checkIfOkToRelloc(self);
#endif
			newData = (uint8_t *)io_realloc(self->data, newSizeInBytes + 1);
			if (NULL == newData)
				return;
		}
		self->data = newData;
		self->stackAllocated = 0;
		self->data[newSizeInBytes] = 0x0;
		self->size = size;

		if (newSizeInBytes > oldSizeInBytes)
			memset(self->data + oldSizeInBytes, 0, newSizeInBytes - oldSizeInBytes);

		UArray_changed(self);
	}
}

size_t UArray_size(const UArray *self)
{
	return self ? self->size : 0;
}

size_t UArray_sizeInBytes(const UArray *self)
{
	if (NULL == self || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize)
		return 0;
	return self->size * self->itemSize;
}

void UArray_sizeTo_(UArray *self, size_t size)
{
	UArray_setSize_(self, size);
}


// copy


#define UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(name, type) \
	typedef struct { char leadingByte; type value; } name

UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayUInt8AlignmentProbe, uint8_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayUInt16AlignmentProbe, uint16_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayUInt32AlignmentProbe, uint32_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayUInt64AlignmentProbe, uint64_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayInt8AlignmentProbe, int8_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayInt16AlignmentProbe, int16_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayInt32AlignmentProbe, int32_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayInt64AlignmentProbe, int64_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayFloat32AlignmentProbe, float32_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayFloat64AlignmentProbe, float64_t);
UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_(UArrayUintptrAlignmentProbe, uintptr_t);

#undef UARRAY_DECLARE_ITEM_ALIGNMENT_PROBE_

static size_t UArray_itemAlignment_(CTYPE itemType)
{
	switch (itemType) {
		case CTYPE_uint8_t: return offsetof(UArrayUInt8AlignmentProbe, value);
		case CTYPE_uint16_t: return offsetof(UArrayUInt16AlignmentProbe, value);
		case CTYPE_uint32_t: return offsetof(UArrayUInt32AlignmentProbe, value);
		case CTYPE_uint64_t: return offsetof(UArrayUInt64AlignmentProbe, value);
		case CTYPE_int8_t: return offsetof(UArrayInt8AlignmentProbe, value);
		case CTYPE_int16_t: return offsetof(UArrayInt16AlignmentProbe, value);
		case CTYPE_int32_t: return offsetof(UArrayInt32AlignmentProbe, value);
		case CTYPE_int64_t: return offsetof(UArrayInt64AlignmentProbe, value);
		case CTYPE_float32_t: return offsetof(UArrayFloat32AlignmentProbe, value);
		case CTYPE_float64_t: return offsetof(UArrayFloat64AlignmentProbe, value);
		case CTYPE_uintptr_t: return offsetof(UArrayUintptrAlignmentProbe, value);
		default: return 0;
	}
}


static int UArray_hasValidItemLayout_(const UArray *self)
{
	size_t expectedItemSize, expectedAlignment;

	if (NULL == self)
		return 0;
	expectedItemSize = CTYPE_size(self->itemType);
	expectedAlignment = UArray_itemAlignment_(self->itemType);
	if (expectedItemSize == 0 || expectedAlignment == 0 ||
		self->itemSize != expectedItemSize ||
		self->size > (size_t)-1 / expectedItemSize ||
		self->size * expectedItemSize == (size_t)-1 ||
		(self->size > 0 && NULL == self->data) ||
		(self->data && ((uintptr_t)self->data % expectedAlignment) != 0))
		return 0;
	return 1;
}

/* Reject undefined floating-to-integer conversions before a scalar write. */
static int UArray_doubleFitsItemType_(CTYPE itemType, double value)
{
	int bits;
	int isSigned;

	if (itemType == CTYPE_float32_t || itemType == CTYPE_float64_t)
		return 1;
	if (!isfinite(value))
		return 0;

	switch (itemType) {
		case CTYPE_uint8_t: bits = 8; isSigned = 0; break;
		case CTYPE_uint16_t: bits = 16; isSigned = 0; break;
		case CTYPE_uint32_t: bits = 32; isSigned = 0; break;
		case CTYPE_uint64_t: bits = 64; isSigned = 0; break;
		case CTYPE_int8_t: bits = 8; isSigned = 1; break;
		case CTYPE_int16_t: bits = 16; isSigned = 1; break;
		case CTYPE_int32_t: bits = 32; isSigned = 1; break;
		case CTYPE_int64_t: bits = 64; isSigned = 1; break;
		case CTYPE_uintptr_t:
			bits = (int)(sizeof(uintptr_t) * CHAR_BIT);
			isSigned = 0;
			break;
		default:
			return 0;
	}

	if (!isSigned)
		return value > -1.0 && value < ldexp(1.0, bits);

	{
		double upperBound = ldexp(1.0, bits - 1);
		if (bits == 64)
			return value >= -upperBound && value < upperBound;
		return value > -upperBound - 1.0 && value < upperBound;
	}
}

void UArray_copy_(UArray *self, const UArray *other)
{
	if (NULL == self || NULL == other || self == other ||
		!UArray_hasValidItemLayout_(other))
		return;

	UArray_setItemType_(self, other->itemType);
	if (self->itemType != other->itemType)
		return;
	UArray_setEncoding_(self, (CENCODING)other->encoding);
	if (self->encoding != other->encoding ||
		self->itemType != other->itemType)
		return;
	UArray_setSize_(self, other->size);
	if (self->size != other->size ||
		(other->size > 0 && NULL == self->data))
		return;
	UArray_copyItems_(self, other);
}

void UArray_copyItems_(UArray *self, const UArray *other)
{
	if (NULL == self || NULL == other ||
		!UArray_hasValidItemLayout_(self) ||
		!UArray_hasValidItemLayout_(other))
		return;
	if (self->size != other->size)
	{
		printf("UArray_copyItems_ error - arrays not of same size\n");
		exit(-1);
	}

	if (self->itemType == other->itemType)
	{
		UArray_copyData_(self, other);
	}
	else
	{
		DUARRAY_OP(UARRAY_BASICOP_TYPES, =, self, other);
	}
	UArray_changed(self);
}

void UArray_copyData_(UArray *self, const UArray *other)
{
	size_t sourceBytes, selfBytes, newSize;
	uintptr_t selfStart, sourceStart;
	const uint8_t *source;
	void *snapshot = NULL;
	int overlaps = 0;

	if (NULL == self || NULL == other || self == other ||
		self->itemSize == 0 || other->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		other->size > (size_t)-1 / other->itemSize ||
		(other->size > 0 && NULL == other->data))
		return;

	sourceBytes = other->size * other->itemSize;
	selfBytes = self->size * self->itemSize;
	if (sourceBytes > (size_t)-1 - (self->itemSize - 1))
		return;
	newSize = (sourceBytes + self->itemSize - 1) / self->itemSize;
	if (newSize > (size_t)-1 / self->itemSize ||
		newSize * self->itemSize == (size_t)-1)
		return;

	/*
	 * Include the trailing NUL in the owned storage span. A borrowed source
	 * may start at self->data + selfBytes (also when selfBytes is zero), and
	 * resizing before snapshotting would invalidate that source pointer.
	 */
	if (sourceBytes > 0 && self->data && other->data) {
		selfStart = (uintptr_t)self->data;
		sourceStart = (uintptr_t)other->data;
		overlaps = (sourceStart >= selfStart &&
			sourceStart - selfStart <= selfBytes) ||
			(selfStart >= sourceStart &&
			selfStart - sourceStart < sourceBytes);
	}
	if (overlaps) {
		snapshot = io_malloc(sourceBytes);
		if (NULL == snapshot)
			return;
		memmove(snapshot, other->data, sourceBytes);
		source = (const uint8_t *)snapshot;
	} else {
		source = other->data;
	}

	UArray_setSize_(self, newSize);
	if (self->size != newSize ||
		(newSize > 0 && NULL == self->data)) {
		if (snapshot)
			io_free(snapshot);
		return;
	}
	if (sourceBytes > 0)
		memmove(self->data, source, sourceBytes);
	if (snapshot)
		io_free(snapshot);
}

void UArray_convertToItemType_(UArray *self, CTYPE newItemType)
{
	UArray *tmp;
	if (NULL == self || self->itemType == newItemType ||
		CTYPE_size(newItemType) == 0)
		return;
	tmp = UArray_new();
	if (NULL == tmp)
		return;
	UArray_setItemType_(tmp, newItemType);
	UArray_setEncoding_(tmp, (CENCODING)self->encoding);
	if (tmp->itemType != newItemType || tmp->encoding != self->encoding) {
		UArray_free(tmp);
		return;
	}
	UArray_setSize_(tmp, self->size);
	if (tmp->size != self->size ||
		(self->size > 0 && NULL == tmp->data)) {
		UArray_free(tmp);
		return;
	}
	UArray_copyItems_(tmp, self);
	if (tmp->size == self->size && tmp->itemType == newItemType) {
		UArray_copy_(self, tmp);
	}
	UArray_free(tmp);
}

// slice

UArray UArray_stackRange(const UArray *self, size_t start, size_t size)
{
	UArray s;
	if (NULL == self) {
		memset(&s, 0, sizeof(UArray));
		s.itemType = CTYPE_uint8_t;
		s.itemSize = sizeof(uint8_t);
#ifdef UARRAY_DEBUG
		s.stackAllocated = 1;
#endif
		return s;
	}

	memcpy(&s, self, sizeof(UArray));
	s.hash = 0;
#ifdef UARRAY_DEBUG
	s.stackAllocated = 1;
#endif

	if (!UArray_hasValidItemLayout_(self)) {
		memset(&s, 0, sizeof(UArray));
		s.itemType = CTYPE_uint8_t;
		s.itemSize = sizeof(uint8_t);
#ifdef UARRAY_DEBUG
		s.stackAllocated = 1;
#endif
		return s;
	}
	if (start > self->size || size > self->size - start) {
		s.data = NULL;
		s.size = 0;
		return s;
	}

	s.data = self->data ? self->data + self->itemSize * start : NULL;
	s.size = size;
	return s;
}

UArray *UArray_range(const UArray *self, size_t start, size_t size)
{
	UArray out = UArray_stackRange(self, start, size);
	return UArray_clone(&out);
}

UArray UArray_stackSlice(const UArray *self, long start, long end)
{
	start = UArray_wrapPos_(self, start);
	end   = UArray_wrapPos_(self, end);
	if (end < start) end = start;
	return UArray_stackRange(self, start, end - start);
}

BASEKIT_API UArray *UArray_slice(const UArray *self, long start, long end)
{
	UArray out = UArray_stackSlice(self, start, end);
	return UArray_clone(&out);
}

// at, without bounds check

typedef struct
{
    char leadingByte;
    void *value;
} UArrayPointerAlignmentProbe;

static int UArray_hasValidPointerStorage_(const UArray *self)
{
    size_t alignment = offsetof(UArrayPointerAlignmentProbe, value);

    if (NULL == self || self->itemType != CTYPE_uintptr_t ||
        self->itemSize != sizeof(void *) ||
        self->itemSize != sizeof(uintptr_t) ||
        self->size > (size_t)-1 / self->itemSize ||
        (self->size > 0 && NULL == self->data) ||
        (self->data && ((uintptr_t)self->data % alignment) != 0))
        return 0;
    return 1;
}

void *UArray_rawPointerAt_(const UArray *self, size_t i)
{
    if (!UArray_hasValidPointerStorage_(self) || NULL == self->data)
        return NULL;
    return ((void **)self->data)[i];
}

long UArray_rawLongAt_(const UArray *self, size_t i)
{
	UARRAY_RAWAT_(self, i);
	UArray_error_(self, "UArray_rawLongAt_ not supported on this type");
	return 0;
}

double UArray_rawDoubleAt_(const UArray *self, size_t i)
{
	UARRAY_RAWAT_(self, i);
	UArray_error_(self, "UArray_doubleAt_ not supported on this type");
	return 0;
}

// at, with bounds check

void *UArray_pointerAt_(const UArray *self, size_t i)
{
    if (NULL == self || i >= self->size ||
        !UArray_hasValidPointerStorage_(self))
        return NULL;
    return UArray_rawPointerAt_(self, i);
}

long UArray_longAt_(const UArray *self, size_t i)
{
	if (NULL == self || i >= self->size || NULL == self->data)
		return 0;
	return UArray_rawLongAt_(self, i);
}

double UArray_doubleAt_(const UArray *self, size_t i)
{
	if (NULL == self || i >= self->size || NULL == self->data)
		return 0.0;
	return UArray_rawDoubleAt_(self, i);
}

// at, extras

long UArray_firstLong(const UArray *self)
{
	if (NULL == self || self->size == 0 || NULL == self->data)
		return 0;
	return UArray_rawLongAt_(self, 0);
}

long UArray_lastLong(const UArray *self)
{
	if (NULL == self || self->size == 0 || NULL == self->data)
		return 0;
	return UArray_rawLongAt_(self, self->size - 1);
}

// remove

void UArray_removeRange(UArray *self, size_t start, size_t removeSize)
{
	size_t originalSize, newSize, remainder, remainderSize, newBytes;
	if (NULL == self || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && NULL == self->data))
		return;
	originalSize = self->size;
	if (originalSize * self->itemSize == (size_t)-1)
		return;
	if (start >= originalSize || removeSize == 0)
		return;
	if (removeSize > originalSize - start)
		removeSize = originalSize - start;
	newSize = originalSize - removeSize;

	if (start < newSize) {
		remainder = start + removeSize;
		remainderSize = originalSize - remainder;
		if (remainderSize > (size_t)-1 / self->itemSize)
			return;
		memmove(UARRAY_BYTESAT_(self, start), UARRAY_BYTESAT_(self, remainder),
			self->itemSize * remainderSize);
	}
	newBytes = newSize * self->itemSize;
	if (newBytes == (size_t)-1)
		return;
	UArray_setSize_(self, newSize);
	/* Shrinking reallocations may fail; the old allocation is still large
	 * enough, so keep the logical removal successful without reallocating. */
	if (self->size != newSize) {
		self->size = newSize;
		self->data[newBytes] = 0;
		UArray_changed(self);
	}
	UArray_changed(self);
}

void UArray_leave_thenRemove_(UArray *self, size_t itemsToLeave, size_t itemsToRemove)
{
	size_t size, itemSize, period, remainder, chunkCount;
	size_t tailCount, newItemCount, newBytes, chunkBytes, chunkPos;
	uint8_t *newData;

	if (NULL == self || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && NULL == self->data))
		return;
	if (itemsToLeave == 0) {
		UArray_clear(self);
		UArray_setSize_(self, 0);
		return;
	}
	if (itemsToRemove == 0 || self->size == 0)
		return;
	if (itemsToLeave > (size_t)-1 - itemsToRemove)
		return;

	size = self->size;
	itemSize = self->itemSize;
	period = itemsToLeave + itemsToRemove;
	remainder = size % period;
	chunkCount = size / period;
	tailCount = remainder < itemsToLeave ? remainder : itemsToLeave;

	if (itemsToLeave > (size_t)-1 / itemSize)
		return;
	chunkBytes = itemsToLeave * itemSize;
	if (chunkCount > ((size_t)-1 - tailCount) / itemsToLeave)
		return;
	newItemCount = chunkCount * itemsToLeave + tailCount;
	if (newItemCount > (size_t)-1 / itemSize)
		return;
	newBytes = newItemCount * itemSize;
	if (newBytes == (size_t)-1)
		return;

	newData = (uint8_t *)io_malloc(newBytes + 1);
	if (NULL == newData)
		return;
	for (chunkPos = 0; chunkPos < chunkCount; chunkPos++) {
		memmove(newData + chunkPos * chunkBytes,
			UARRAY_BYTESAT_(self, chunkPos * period), chunkBytes);
	}
	if (tailCount > 0) {
		memmove(newData + chunkPos * chunkBytes,
			UARRAY_BYTESAT_(self, chunkPos * period), tailCount * itemSize);
	}
	newData[newBytes] = 0;
	UArray_setData_type_size_copy_(self, newData, UArray_itemType(self), newItemCount, 0);
	if (self->size != newItemCount || self->data != newData) {
		/* The setter validates all bounds before transferring ownership. */
		io_free(newData);
		return;
	}
	UArray_changed(self);
}
BASEKIT_API void UArray_removeFirst(UArray *self)
{
	UArray_removeRange(self, 0, 1);
}

BASEKIT_API void UArray_removeLast(UArray *self)
{
	size_t newSize, newBytes;
	if (NULL == self || self->size == 0 || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize || NULL == self->data)
		return;
	newSize = self->size - 1;
	newBytes = newSize * self->itemSize;
	UArray_setSize_(self, newSize);
	if (self->size != newSize) {
		self->size = newSize;
		self->data[newBytes] = 0;
		UArray_changed(self);
	}
}

// insert

#define UARRAY_RAWAT_PUT_(self, pos, v) \
switch (self->itemType)\
{\
	case CTYPE_uint8_t:   ((uint8_t   *)self->data)[pos] = v; break;\
	case CTYPE_uint16_t:  ((uint16_t  *)self->data)[pos] = v; break;\
	case CTYPE_uint32_t:  ((uint32_t  *)self->data)[pos] = v; break;\
	case CTYPE_uint64_t:  ((uint64_t  *)self->data)[pos] = v; break;\
	case CTYPE_int8_t:    ((int8_t    *)self->data)[pos] = v; break;\
	case CTYPE_int16_t:   ((int16_t   *)self->data)[pos] = v; break;\
	case CTYPE_int32_t:   ((int32_t   *)self->data)[pos] = v; break;\
	case CTYPE_int64_t:   ((int64_t   *)self->data)[pos] = v; break;\
	case CTYPE_float32_t: ((float32_t *)self->data)[pos] = v; break;\
	case CTYPE_float64_t: ((float64_t *)self->data)[pos] = v; break;\
	case CTYPE_uintptr_t: ((uintptr_t *)self->data)[pos] = v; break;\
}

void UArray_at_putLong_(UArray *self, size_t pos, long v)
{
	if (NULL == self || pos == (size_t)-1 ||
		!UArray_hasValidItemLayout_(self))
		return;
	if (pos >= self->size)
		UArray_setSize_(self, pos + 1);
	if (pos >= self->size || NULL == self->data)
		return;
	UARRAY_RAWAT_PUT_(self, pos, v);
	UArray_changed(self);
}

void UArray_at_putDouble_(UArray *self, size_t pos, double v)
{
	if (NULL == self || pos == (size_t)-1 ||
		!UArray_hasValidItemLayout_(self) ||
		!UArray_doubleFitsItemType_(self->itemType, v))
		return;
	if (pos >= self->size)
		UArray_setSize_(self, pos + 1);
	if (pos >= self->size || NULL == self->data)
		return;
	UARRAY_RAWAT_PUT_(self, pos, v);
	UArray_changed(self);
}

void UArray_at_putPointer_(UArray *self, size_t pos, void *v)
{
    if (NULL == self || pos == (size_t)-1)
        return;
    if (!UArray_hasValidPointerStorage_(self))
        return;
    if (pos >= self->size)
        UArray_setSize_(self, pos + 1);
    if (pos >= self->size || !UArray_hasValidPointerStorage_(self) ||
        NULL == self->data)
        return;

    if (((void **)self->data)[pos] != v) {
        ((void **)self->data)[pos] = v;
        UArray_changed(self);
    }
}

void UArray_appendLong_(UArray *self, long v)
{
	if (self)
		UArray_at_putLong_(self, self->size, v);
}

void UArray_appendDouble_(UArray *self, double v)
{
	if (self)
		UArray_at_putDouble_(self, self->size, v);
}

void UArray_appendPointer_(UArray *self, void *v)
{
	if (self)
		UArray_at_putPointer_(self, self->size, v);
}

void UArray_appendBytes_size_(UArray *self, const uint8_t *bytes, size_t size)
{
	UArray a = UArray_stackAllocedWithData_type_size_((uint8_t *)bytes, CTYPE_uint8_t, size);
	UArray_append_(self, &a);
}

/*
void UArray_appendByte_(UArray *self, uint8_t byte)
{
	UArray a = UArray_stackAllocedWithData_type_size_(&byte, CTYPE_uint8_t, 1);
	UArray_append_(self, &a);	
}
*/

void UArray_insert_every_(UArray *self, UArray *other, size_t itemCount)
{
	UArray *convertedOther = NULL;
	const UArray *insert = other;
	size_t selfBytes, otherBytes, itemSize, chunkBytes;
	size_t chunkCount, tailBytes, newBytes, newCount;
	size_t inPos = 0, outPos = 0, chunkPos;
	uint8_t *newData;

	if (NULL == self || NULL == other || itemCount == 0 ||
		self->itemSize == 0 || other->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		other->size > (size_t)-1 / other->itemSize ||
		(self->size > 0 && NULL == self->data) ||
		(other->size > 0 && NULL == other->data) ||
		itemCount > (size_t)-1 / self->itemSize)
		return;

	itemSize = self->itemSize;
	selfBytes = self->size * itemSize;
	chunkBytes = itemCount * itemSize;
	if (chunkBytes == 0)
		return;

	if (self->itemType != other->itemType) {
		convertedOther = UArray_clone(other);
		if (NULL == convertedOther)
			return;
		UArray_convertToItemType_(convertedOther, self->itemType);
		if (convertedOther->itemType != self->itemType ||
			convertedOther->itemSize != itemSize) {
			UArray_free(convertedOther);
			return;
		}
		insert = convertedOther;
	}
	otherBytes = insert->size * insert->itemSize;
	if (otherBytes == 0) {
		if (convertedOther)
			UArray_free(convertedOther);
		return;
	}

	chunkCount = selfBytes / chunkBytes;
	tailBytes = selfBytes % chunkBytes;
	if (chunkCount > ((size_t)-1 - selfBytes) / otherBytes) {
		if (convertedOther)
			UArray_free(convertedOther);
		return;
	}
	newBytes = selfBytes + chunkCount * otherBytes;
	if (newBytes == (size_t)-1 || newBytes % itemSize != 0) {
		if (convertedOther)
			UArray_free(convertedOther);
		return;
	}
	newCount = newBytes / itemSize;
	newData = (uint8_t *)io_malloc(newBytes + 1);
	if (NULL == newData) {
		if (convertedOther)
			UArray_free(convertedOther);
		return;
	}

	for (chunkPos = 0; chunkPos < chunkCount; chunkPos++) {
		memmove(newData + outPos, self->data + inPos, chunkBytes);
		inPos += chunkBytes;
		outPos += chunkBytes;
		memmove(newData + outPos, insert->data, otherBytes);
		outPos += otherBytes;
	}
	if (tailBytes > 0)
		memmove(newData + outPos, self->data + inPos, tailBytes);
	newData[newBytes] = 0;

	UArray_setData_type_size_copy_(self, newData, self->itemType, newCount, 0);
	if (self->data == newData && self->size == newCount)
		UArray_changed(self);
	else
		io_free(newData);
	if (convertedOther)
		UArray_free(convertedOther);
}
void UArray_at_putAll_(UArray *self, size_t pos, const UArray *other)
{
	UArray *snapshot = NULL;
	const UArray *source = other;
	size_t originalSize, effectiveSize, itemSize, sourceBytes;
	size_t selfBytes, newSize, tailCount, tailBytes, newBytes;
	uintptr_t selfStart, sourceStart;
	int overlaps = 0;
	UArray insertChunk;

	if (NULL == self || NULL == other || other->size == 0 ||
		!UArray_hasValidItemLayout_(self) ||
		!UArray_hasValidItemLayout_(other))
		return;

	itemSize = self->itemSize;
	originalSize = self->size;
	selfBytes = originalSize * itemSize;
	sourceBytes = other->size * other->itemSize;
	if (self == other)
		overlaps = 1;
	else if (sourceBytes > 0 && self->data && source->data) {
		selfStart = (uintptr_t)self->data;
		sourceStart = (uintptr_t)source->data;
		/*
		 * Include the trailing NUL at selfBytes: a borrowed UArray view may
		 * legally begin there, and a tail move or realloc would invalidate it.
		 */
		overlaps = (sourceStart >= selfStart &&
			sourceStart - selfStart <= selfBytes) ||
			(selfStart >= sourceStart &&
			selfStart - sourceStart < sourceBytes);
	}
	if (overlaps) {
		snapshot = UArray_clone(other);
		if (NULL == snapshot)
			return;
		source = snapshot;
	}

	effectiveSize = pos > originalSize ? pos : originalSize;
	if (source->size > (size_t)-1 - effectiveSize) {
		if (snapshot)
			UArray_free(snapshot);
		return;
	}
	newSize = effectiveSize + source->size;
	if (newSize > (size_t)-1 / itemSize) {
		if (snapshot)
			UArray_free(snapshot);
		return;
	}
	newBytes = newSize * itemSize;
	if (newBytes == (size_t)-1) {
		if (snapshot)
			UArray_free(snapshot);
		return;
	}

	/* Grow once so failure leaves all existing contents untouched. */
	UArray_setSize_(self, newSize);
	if (self->size != newSize || (newSize > 0 && NULL == self->data)) {
		if (snapshot)
			UArray_free(snapshot);
		return;
	}

	tailCount = pos < originalSize ? originalSize - pos : 0;
	if (tailCount > (size_t)-1 / itemSize) {
		if (snapshot)
			UArray_free(snapshot);
		return;
	}
	tailBytes = tailCount * itemSize;
	if (tailBytes > 0) {
		memmove(self->data + (pos + source->size) * itemSize,
			self->data + pos * itemSize, tailBytes);
	}

	insertChunk = UArray_stackRange(self, pos, source->size);
	if (insertChunk.size != source->size ||
		(source->size > 0 && NULL == insertChunk.data)) {
		if (snapshot)
			UArray_free(snapshot);
		return;
	}
	UArray_copyItems_(&insertChunk, source);
	UArray_changed(self);
	if (snapshot)
		UArray_free(snapshot);
}
// compare

#define UARRAY_COMPARE_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, minSize = self->size < other->size ? self->size : other->size;\
	for(i = 0; i < minSize; i ++)\
	{\
		TYPE1 v1 = ((TYPE1 *)self->data)[i];\
		TYPE2 v2 = ((TYPE2 *)other->data)[i];\
		if (v1 > v2) return 1;\
		if (v1 < v2) return -1;\
	}\
	if(self->size != other->size)\
	{\
		return self->size < other->size ? -1 : 1;\
	}\
	return 0;\
}

#define UARRAY_EQ_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, minSize = self->size < other->size ? self->size : other->size;\
	for(i = 0; i < minSize; i ++)\
	{\
		TYPE1 v1 = ((TYPE1 *)self->data)[i];\
		TYPE2 v2 = ((TYPE2 *)other->data)[i];\
		if (v1 != v2) return 0;\
	}\
	return 1;\
}

#define UARRAY_GT_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, minSize = self->size < other->size ? self->size : other->size;\
	for(i = 0; i < minSize; i ++)\
	{\
		TYPE1 v1 = ((TYPE1 *)self->data)[i];\
		TYPE2 v2 = ((TYPE2 *)other->data)[i];\
		if (v1 < v2) return 0;\
	}\
	return 1;\
}

#define UARRAY_LT_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, minSize = self->size < other->size ? self->size : other->size;\
	for(i = 0; i < minSize; i ++)\
	{\
		TYPE1 v1 = ((TYPE1 *)self->data)[i];\
		TYPE2 v2 = ((TYPE2 *)other->data)[i];\
		if (v1 > v2) return 0;\
	}\
	return 1;\
}

int UArray_compare_(const UArray *self, const UArray *other)
{
	DUARRAY_OP(UARRAY_COMPARE_TYPES, NULL, self, other);
	return 0;
}

int UArray_equals_(const UArray *self, const UArray *other)
{
	if (self->size != other->size) return 0;
	DUARRAY_OP(UARRAY_EQ_TYPES, NULL, self, other);
	return 0;
}

int UArray_greaterThan_(const UArray *self, const UArray *other)
{
	if(self->encoding == CENCODING_NUMBER)
	{ DUARRAY_OP(UARRAY_GT_TYPES, NULL, self, other); }

	return UArray_compare_(self, other) > 0;
}

int UArray_lessThan_(const UArray *self, const UArray *other)
{
	if(self->encoding == CENCODING_NUMBER)
	{ DUARRAY_OP(UARRAY_LT_TYPES, NULL, self, other); }

	return UArray_compare_(self, other) < 0;
}

int UArray_greaterThanOrEqualTo_(const UArray *self, const UArray *other)
{
	if(self->encoding == CENCODING_NUMBER)
	{
		if (UArray_greaterThan_(self, other) | UArray_equals_(self, other))
		{ return 1; } else { return 0; }
	}

	return UArray_compare_(self, other) >= 0;
}

int UArray_lessThanOrEqualTo_(const UArray *self, const UArray *other)
{
	if(self->encoding == CENCODING_NUMBER)
	{
		if (UArray_lessThan_(self, other) | UArray_equals_(self, other))
		{ return 1; } else { return 0; }
	}

	return UArray_compare_(self, other) <= 0;
}

int UArray_isZero(const UArray *self)
{
	UARRAY_FOREACH(self, i, v, if (v) return 0;)
	return 1;
}

// find

// printf("i %i %c j %i %c\n", i, v1, j, v2);\
// printf("j%i == %i\n", i, other->size);\

#define UARRAY_FIND_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i;\
			if(self->size < other->size || self->size == 0) return -1;\
			for(i = 0; i < self->size - other->size + 1; i ++)\
			{\
				size_t j;\
					int match = 1;\
					for(j = 0; j < other->size; j ++)\
					{\
						TYPE1 v1 = ((TYPE1 *)self->data)[i + j];\
							TYPE2 v2 = ((TYPE2 *)other->data)[j];\
								if (v1 != v2) { match = 0; break; }\
					}\
					if (match) return i;\
			}\
			return -1;\
}

static int UArray_searchArrayIsValid_(const UArray *self)
{
	size_t itemSize;

	if (NULL == self)
		return 0;

	itemSize = CTYPE_size(self->itemType);
	if (itemSize == 0 ||
		self->itemSize != itemSize ||
		self->size > (size_t)LONG_MAX ||
		self->size > (size_t)-1 / itemSize ||
		(self->size > 0 && NULL == self->data))
		return 0;

	return 1;
}

long UArray_find_(const UArray *self, const UArray *other)
{
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other))
		return -1;
	DUARRAY_OP(UARRAY_FIND_TYPES, NULL, self, other);
	return -1;
}

int UArray_contains_(const UArray *self, const UArray *other)
{
	return UArray_find_(self, other) != -1;
}

long UArray_find_from_(const UArray *self, const UArray *other, size_t from)
{
	UArray s;
	long i;
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other) ||
		from > self->size)
		return -1;
	if (from == self->size)
		return -1;
	s = UArray_stackRange(self, from, self->size - from);
	i = UArray_find_(&s, other);
	if (i < 0 || (size_t)i > (size_t)LONG_MAX - from)
		return -1;
	return (long)(from + (size_t)i);
}

#define UARRAY_FINDANYCASE_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i;\
		if(self->size < other->size) return -1;\
			for(i = 0; i < self->size - other->size + 1; i ++)\
			{\
				size_t j;\
				int match = 1;\
					for(j = 0; j < other->size; j ++)\
					{\
						TYPE1 v1 = ((TYPE1 *)self->data)[i + j];\
						TYPE2 v2 = ((TYPE2 *)other->data)[j];\
						if (tolower((unsigned char)v1) != tolower((unsigned char)v2)) { match = 0; break; }\
					}\
					if(match) { return i; }\
			}\
			return -1;\
}

long UArray_findAnyCase_(const UArray *self, const UArray *other)
{
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other))
		return -1;
	DUARRAY_OP(UARRAY_FINDANYCASE_TYPES, NULL, self, other);
	return -1;
}

int UArray_containsAnyCase_(const UArray *self, const UArray *other)
{
	long i = UArray_findAnyCase_(self, other);
	return i != -1;
}

long UArray_findLongValue_(const UArray *self, long value)
{
	if (!UArray_searchArrayIsValid_(self))
		return -1;
	UARRAY_FOREACH(self, i, v, if(v == value) return (long)i);
	return -1;
}

int UArray_containsLong_(const UArray *self, long value)
{
	return UArray_findLongValue_(self, value) != -1;
}

long UArray_findDoubleValue_(const UArray *self, double value)
{
	if (!UArray_searchArrayIsValid_(self))
		return -1;
	UARRAY_FOREACH(self, i, v, if(v == value) return (long)i);
	return -1;
}

int UArray_containsDouble_(const UArray *self, double value)
{
	return UArray_findDoubleValue_(self, value) != -1;
}

#define UARRAY_RFIND_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, j;\
	if (self->size < other->size || self->size > (size_t)LONG_MAX) return -1;\
	i = self->size - other->size;\
	for (;;) {\
		int match = 1;\
		for (j = 0; j < other->size; j++) {\
			TYPE1 v1 = ((TYPE1 *)self->data)[i + j];\
			TYPE2 v2 = ((TYPE2 *)other->data)[j];\
			if (v1 != v2) { match = 0; break; }\
		}\
		if (match) return (long)i;\
		if (i == 0) break;\
		i--;\
	}\
	return -1;\
}

long UArray_rFind_(const UArray *self, const UArray *other)
{
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other))
		return -1;
	DUARRAY_OP(UARRAY_RFIND_TYPES, NULL, self, other);
	return -1;
}

BASEKIT_API long UArray_rFind_from_(const UArray *self, const UArray *other, size_t from)
{
	UArray s;
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other))
		return -1;
	if (from > self->size)
		from = self->size;
	if (from == 0) {
		s = *self;
		s.size = 0;
		return UArray_rFind_(&s, other);
	}
	s = UArray_stackRange(self, 0, from);
	return UArray_rFind_(&s, other);
}

#define UARRAY_RFINDANYCASE_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, j;\
	if (self->size < other->size || self->size > (size_t)LONG_MAX) return -1;\
	i = self->size - other->size;\
	for (;;) {\
		int match = 1;\
		for (j = 0; j < other->size; j++) {\
			TYPE1 v1 = ((TYPE1 *)self->data)[i + j];\
			TYPE2 v2 = ((TYPE2 *)other->data)[j];\
			if (tolower((unsigned char)v1) != tolower((unsigned char)v2)) { match = 0; break; }\
		}\
		if (match) return (long)i;\
		if (i == 0) break;\
		i--;\
	}\
	return -1;\
}
long UArray_rFindAnyCase_(const UArray *self, const UArray *other)
{
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other))
		return -1;
	DUARRAY_OP(UARRAY_RFINDANYCASE_TYPES, NULL, self, other);
	return -1;
}

#define UARRAY_RFINDANYVALUE_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, j;\
	if (self->size == 0 || self->size < other->size ||\
		self->size > (size_t)LONG_MAX) return -1;\
	for (i = self->size; i > 0; i--) {\
		TYPE1 v1 = ((TYPE1 *)self->data)[i - 1];\
		for (j = 0; j < other->size; j++) {\
			TYPE2 v2 = ((TYPE2 *)other->data)[j];\
			if (v1 == v2) return (long)(i - 1);\
		}\
	}\
	return -1;\
}
long UArray_rFindAnyValue_(const UArray *self, const UArray *other)
{
	if (!UArray_searchArrayIsValid_(self) ||
		!UArray_searchArrayIsValid_(other))
		return -1;
	DUARRAY_OP(UARRAY_RFINDANYVALUE_TYPES, NULL, self, other);
	return -1;
}

// types

int UArray_isFloatType(const UArray *self)
{
	return self->itemType == CTYPE_float32_t || self->itemType == CTYPE_float64_t;
}

int UArray_isSignedType(const UArray *self)
{
	switch (self->itemType)
	{
		case CTYPE_uint8_t:   return 0;
		case CTYPE_uint16_t:  return 0;
		case CTYPE_uint32_t:  return 0;
		case CTYPE_uint64_t:  return 0;
		case CTYPE_int8_t:    return 1;
		case CTYPE_int16_t:   return 1;
		case CTYPE_int32_t:   return 1;
		case CTYPE_int64_t:   return 1;
		case CTYPE_float32_t: return 1;
		case CTYPE_float64_t: return 1;
	}
	return 0;
}

size_t UArray_wrapPos_(const UArray *self, long pos)
{
	size_t size, distance;
	if (NULL == self)
		return 0;
	size = self->size;

	if (pos >= 0)
		return (size_t)pos >= size ? size : (size_t)pos;

	/* Avoid casting a large size_t to long or negating LONG_MIN. */
	distance = (size_t)(-(pos + 1)) + 1;
	if (distance >= size)
		return 0;
	return size - distance;
}

#define DEFINE_UARRAY_COMPARATOR(name, type) \
static int name(const void *va, const void *vb) \
{ \
	const type a = *(const type *)va; \
	const type b = *(const type *)vb; \
	return a == b ? 0 : (a < b ? -1 : 1); \
}

DEFINE_UARRAY_COMPARATOR(cmp_uint8_t, uint8_t)
DEFINE_UARRAY_COMPARATOR(cmp_uint16_t, uint16_t)
DEFINE_UARRAY_COMPARATOR(cmp_uint32_t, uint32_t)
DEFINE_UARRAY_COMPARATOR(cmp_uint64_t, uint64_t)
DEFINE_UARRAY_COMPARATOR(cmp_int8_t, int8_t)
DEFINE_UARRAY_COMPARATOR(cmp_int16_t, int16_t)
DEFINE_UARRAY_COMPARATOR(cmp_int32_t, int32_t)
DEFINE_UARRAY_COMPARATOR(cmp_int64_t, int64_t)
DEFINE_UARRAY_COMPARATOR(cmp_uintptr_t, uintptr_t)

static int cmp_float32_t(const void *va, const void *vb)
{
	float32_t a = *(const float32_t *)va;
	float32_t b = *(const float32_t *)vb;
	if (isnan(a)) return isnan(b) ? 0 : 1;
	if (isnan(b)) return -1;
	return a < b ? -1 : (a > b ? 1 : 0);
}

static int cmp_float64_t(const void *va, const void *vb)
{
	float64_t a = *(const float64_t *)va;
	float64_t b = *(const float64_t *)vb;
	if (isnan(a)) return isnan(b) ? 0 : 1;
	if (isnan(b)) return -1;
	return a < b ? -1 : (a > b ? 1 : 0);
}

#undef DEFINE_UARRAY_COMPARATOR

void UArray_sort(UArray *self)
{
	void *base;
	size_t size;
	if (NULL == self || !UArray_hasValidItemLayout_(self))
		return;
	base = self->data;
	size = self->size;
	UArray_changed(self);
	if (size < 2)
		return;

	switch(self->itemType)
	{
		case CTYPE_uint8_t:   qsort(base, size,  sizeof(uint8_t),  cmp_uint8_t);   return;
		case CTYPE_uint16_t:  qsort(base, size, sizeof(uint16_t),  cmp_uint16_t);  return;
		case CTYPE_uint32_t:  qsort(base, size, sizeof(uint32_t),  cmp_uint32_t);  return;
		case CTYPE_uint64_t:  qsort(base, size, sizeof(uint64_t),  cmp_uint64_t);  return;

		case CTYPE_int8_t:    qsort(base, size,  sizeof(int8_t),   cmp_int8_t);    return;
		case CTYPE_int16_t:   qsort(base, size, sizeof(int16_t),   cmp_int16_t);   return;
		case CTYPE_int32_t:   qsort(base, size, sizeof(int32_t),   cmp_int32_t);   return;
		case CTYPE_int64_t:   qsort(base, size, sizeof(int64_t),   cmp_int64_t);   return;

		case CTYPE_float32_t: qsort(base, size, sizeof(float32_t), cmp_float32_t); return;
		case CTYPE_float64_t: qsort(base, size, sizeof(float64_t), cmp_float64_t); return;
		case CTYPE_uintptr_t: qsort(base, size, sizeof(uintptr_t), cmp_uintptr_t); return;
	}
}

void UArray_sortBy_(UArray *self, UArraySortCallback *cmp)
{
	void *base;
	size_t size;
	if (NULL == self || NULL == cmp || !UArray_hasValidItemLayout_(self))
		return;
	base = self->data;
	size = self->size;
	UArray_changed(self);
	if (size < 2)
		return;

	switch(self->itemType)
	{
		case CTYPE_uint8_t:   qsort(base, size,  sizeof(uint8_t),  cmp); return;
		case CTYPE_uint16_t:  qsort(base, size, sizeof(uint16_t),  cmp); return;
		case CTYPE_uint32_t:  qsort(base, size, sizeof(uint32_t),  cmp); return;
		case CTYPE_uint64_t:  qsort(base, size, sizeof(uint64_t),  cmp); return;

		case CTYPE_int8_t:    qsort(base, size,  sizeof(int8_t),   cmp); return;
		case CTYPE_int16_t:   qsort(base, size, sizeof(int16_t),   cmp); return;
		case CTYPE_int32_t:   qsort(base, size, sizeof(int32_t),   cmp); return;
		case CTYPE_int64_t:   qsort(base, size, sizeof(int64_t),   cmp); return;

		case CTYPE_float32_t: qsort(base, size, sizeof(float32_t), cmp); return;
		case CTYPE_float64_t: qsort(base, size, sizeof(float64_t), cmp); return;
		case CTYPE_uintptr_t: qsort(base, size, sizeof(uintptr_t), cmp); return;
	}
}


