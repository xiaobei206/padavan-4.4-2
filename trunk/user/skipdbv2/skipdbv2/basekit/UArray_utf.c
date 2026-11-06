/*
	copyright: Steve Dekorte, 2006. All rights reserved.
	license: See _BSDLicense.txt.
*/

#include "UArray.h"
#include "ConvertUTF.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <limits.h>

int UArray_MachineIsLittleEndian(void)
{
	unsigned int i = 0x1;
	return ((unsigned char *)(&i))[0] == 1;
}

static int UArray_hasValidEncodingLayout_(const UArray *self)
{
	if (NULL == self || self->itemSize == 0 ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && NULL == self->data))
		return 0;
	switch (self->encoding) {
		case CENCODING_ASCII:
		case CENCODING_UTF8:
			return self->itemSize == 1;
		case CENCODING_UTF16:
			return self->itemSize == sizeof(UTF16);
		case CENCODING_UTF32:
			return self->itemSize == sizeof(UTF32);
		case CENCODING_NUMBER:
			return 1;
		default:
			return 0;
	}
}

static int UArray_SizeOfUTFChar(const uint8_t *s, const uint8_t *end)
{
	uint8_t c;
	int length;
	if (NULL == s || NULL == end || s >= end)
		return -1;
	c = *s;
	if (c <= 0x7f)
		return 1;
	if (c >= 0xc2 && c <= 0xdf)
		length = 2;
	else if (c >= 0xe0 && c <= 0xef)
		length = 3;
	else if (c >= 0xf0 && c <= 0xf4)
		length = 4;
	else
		return -1;
	if ((size_t)(end - s) < (size_t)length ||
		!isLegalUTF8Sequence((const UTF8 *)s, (const UTF8 *)end))
		return -1;
	return length;
}

int UArray_maxCharSize(const UArray *self)
{
	if (NULL == self)
		return -1;
	if (self->encoding == CENCODING_UTF8)
	{
		int maxCharSize = 1;
		size_t i = 0;
		if (self->itemSize != 1 ||
			self->size > (size_t)-1 / self->itemSize ||
			(self->size > 0 && NULL == self->data))
			return -1;
		while (i < self->size)
		{
			int charSize = UArray_SizeOfUTFChar(self->data + i,
				self->data + self->size);
			if (charSize < 1)
				return -1;
			if (charSize > maxCharSize)
				maxCharSize = charSize;
			i += (size_t)charSize;
		}
		return maxCharSize;
	}
	return self->itemSize <= INT_MAX ? (int)self->itemSize : -1;
}

static int UArray_truncateAfterConvertToEncoding_(UArray *self)
{
	long newSize;
	if (NULL == self)
		return 0;
	if (self->encoding == CENCODING_NUMBER)
		return 1;
	if (self->size > (size_t)LONG_MAX)
		return 0;
	{
		UArray tmp = UArray_stackAllocedWithData_type_size_("\0", CTYPE_uint8_t, 1);
		newSize = UArray_find_(self, &tmp);
	}
	if (newSize == -1)
		return 1;
	if (newSize < 0 || (size_t)newSize > self->size)
		return 0;
	UArray_setSize_(self, (size_t)newSize);
	return self->size == (size_t)newSize;
}
int UArray_convertToFixedSizeType(UArray *self)
{
	int maxCharSize;
	if (NULL == self || self->encoding != CENCODING_UTF8)
		return 0;
	maxCharSize = UArray_maxCharSize(self);
	if (maxCharSize < 1)
		return 0;
	if (maxCharSize == 1) {
		self->encoding = CENCODING_ASCII;
		return 1;
	}
	if (maxCharSize == 2) {
		UArray_convertToUTF16(self);
		if (self->encoding != CENCODING_UTF16 ||
			self->itemType != CTYPE_uint16_t ||
			self->itemSize != sizeof(UTF16) ||
			self->size > (size_t)-1 / self->itemSize ||
			(self->size > 0 && NULL == self->data))
			return 0;
		return 1;
	}
	UArray_convertToUTF32(self);
	if (self->encoding != CENCODING_UTF32 ||
		self->itemType != CTYPE_uint32_t ||
		self->itemSize != sizeof(UTF32) ||
		self->size > (size_t)-1 / self->itemSize ||
		(self->size > 0 && NULL == self->data))
		return 0;
	return 1;
}

int UArray_isMultibyte(const UArray *self)
{
	if (NULL == self || (self->size > 0 && NULL == self->data))
		return 0;
	if (self->encoding == CENCODING_UTF8 && self->itemSize == 1)
	{
		UARRAY_INTFOREACH(self, i, v, if (ismbchar((unsigned char)v)) return 1; );
	}
	return 0;
}

int UArray_isLegalUTF8(const UArray *self)
{
	size_t i = 0;
	if (NULL == self || self->itemSize != 1 ||
		(self->size > 0 && NULL == self->data))
		return 0;
	while (i < self->size) {
		int length = UArray_SizeOfUTFChar(self->data + i,
			self->data + self->size);
		if (length < 1)
			return 0;
		i += (size_t)length;
	}
	return 1;
}

UArray *UArray_asNumberArrayString(const UArray *self)
{
	UArray *out;
	if (NULL == self || (self->size > 0 && NULL == self->data))
		return NULL;
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_setEncoding_(out, CENCODING_ASCII);

	UARRAY_INTFOREACH(self, i, v,
		char s[512];
		int n;
		if (UArray_isFloatType(self)) {
			n = snprintf(s, sizeof(s), "%f", (double)v);
		} else if (UArray_isSignedType(self)) {
			n = snprintf(s, sizeof(s), "%lld", (long long)v);
		} else {
			n = snprintf(s, sizeof(s), "%llu", (unsigned long long)v);
		}
		if (n < 0 || (size_t)n >= sizeof(s)) {
			UArray_free(out);
			out = NULL;
			break;
		}
		if (i + 1 < self->size) {
			if ((size_t)n + 2 >= sizeof(s)) {
				UArray_free(out);
				out = NULL;
				break;
			}
			s[n++] = ',';
			s[n++] = ' ';
		}
		UArray_appendBytes_size_(out, (const unsigned char *)s, (size_t)n);
	);
	return out;
}


UArray *UArray_asUTF8(const UArray *self)
{
	UArray *out;
	UArray *numbers;
	size_t sourceBytes, capacity, outLength;
	const uint8_t *sourceData;
	void *sourceStart, *sourceEnd;
	UTF8 *targetStart, *targetEnd;
	ConversionResult r = conversionOK;
	ConversionFlags options = lenientConversion;

	if (!UArray_hasValidEncodingLayout_(self))
		return NULL;
	if (self->encoding == CENCODING_NUMBER) {
		numbers = UArray_asNumberArrayString(self);
		if (NULL == numbers)
			return NULL;
		out = UArray_asUTF8(numbers);
		UArray_free(numbers);
		return out;
	}
	if (self->size > (size_t)-1 / 4)
		return NULL;
	sourceBytes = self->size * self->itemSize;
	capacity = self->size * 4;
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_setItemType_(out, CTYPE_uint8_t);
	UArray_setEncoding_(out, CENCODING_UTF8);
	UArray_setSize_(out, capacity);
	if (out->itemSize != 1 || out->size != capacity ||
		(capacity > 0 && NULL == out->data)) {
		UArray_free(out);
		return NULL;
	}

	sourceData = self->data ? self->data : (const uint8_t *)"";
	sourceStart = (void *)sourceData;
	sourceEnd = (void *)(sourceData + sourceBytes);
	targetStart = out->data;
	targetEnd = out->data + out->size;

	switch (self->encoding) {
		case CENCODING_ASCII:
		case CENCODING_UTF8:
			UArray_copy_(out, self);
			if (out->size != self->size ||
				(self->size > 0 && NULL == out->data)) {
				UArray_free(out);
				return NULL;
			}
			break;
		case CENCODING_UTF16:
			r = ConvertUTF16toUTF8((const UTF16 **)&sourceStart,
				(const UTF16 *)sourceEnd, &targetStart, targetEnd, options);
			break;
		case CENCODING_UTF32:
			r = ConvertUTF32toUTF8((const UTF32 **)&sourceStart,
				(const UTF32 *)sourceEnd, &targetStart, targetEnd, options);
			break;
		default:
			UArray_free(out);
			return NULL;
	}
	if (r != conversionOK) {
		UArray_free(out);
		return NULL;
	}
	if (NULL == out->data) {
		UArray_free(out);
		return NULL;
	}
	outLength = strlen((const char *)out->data);
	UArray_setSize_(out, outLength);
	if (out->size != outLength) {
		UArray_free(out);
		return NULL;
	}
	UArray_setEncoding_(out, CENCODING_UTF8);
	return out;
}


UArray *UArray_asUTF16(const UArray *self)
{
	UArray *out;
	UArray *numbers;
	size_t sourceBytes, capacity;
	const uint8_t *sourceData;
	void *sourceStart, *sourceEnd;
	UTF16 *targetStart, *targetEnd;
	ConversionResult r = conversionOK;
	ConversionFlags options = lenientConversion;

	if (!UArray_hasValidEncodingLayout_(self))
		return NULL;
	if (self->encoding == CENCODING_NUMBER) {
		numbers = UArray_asNumberArrayString(self);
		if (NULL == numbers)
			return NULL;
		out = UArray_asUTF16(numbers);
		UArray_free(numbers);
		return out;
	}
	if (self->size > (size_t)-1 / 2)
		return NULL;
	sourceBytes = self->size * self->itemSize;
	capacity = self->size * 2; /* UTF-32 scalars may require surrogate pairs. */
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_setItemType_(out, CTYPE_uint16_t);
	UArray_setEncoding_(out, CENCODING_UTF16);
	UArray_setSize_(out, capacity);
	if (out->itemSize != sizeof(UTF16) || out->size != capacity ||
		(capacity > 0 && NULL == out->data)) {
		UArray_free(out);
		return NULL;
	}

	sourceData = self->data ? self->data : (const uint8_t *)"";
	sourceStart = (void *)sourceData;
	sourceEnd = (void *)(sourceData + sourceBytes);
	targetStart = (UTF16 *)out->data;
	targetEnd = (UTF16 *)(out->data + out->size * out->itemSize);

	switch (self->encoding) {
		case CENCODING_ASCII:
		case CENCODING_UTF8:
			r = ConvertUTF8toUTF16((const UTF8 **)&sourceStart,
				(const UTF8 *)sourceEnd, &targetStart, targetEnd, options);
			break;
		case CENCODING_UTF16:
			UArray_copy_(out, self);
			if (out->size != self->size ||
				(self->size > 0 && NULL == out->data)) {
				UArray_free(out);
				return NULL;
			}
			break;
		case CENCODING_UTF32:
			r = ConvertUTF32toUTF16((const UTF32 **)&sourceStart,
				(const UTF32 *)sourceEnd, &targetStart, targetEnd, options);
			break;
		default:
			UArray_free(out);
			return NULL;
	}
	if (r != conversionOK) {
		UArray_free(out);
		return NULL;
	}
	if (!UArray_truncateAfterConvertToEncoding_(out)) {
		UArray_free(out);
		return NULL;
	}
	return out;
}


UArray *UArray_asUTF32(const UArray *self)
{
	UArray *out;
	UArray *numbers;
	size_t sourceBytes, capacity;
	const uint8_t *sourceData;
	void *sourceStart, *sourceEnd;
	UTF32 *targetStart, *targetEnd;
	ConversionResult r = conversionOK;
	ConversionFlags options = lenientConversion;

	if (!UArray_hasValidEncodingLayout_(self))
		return NULL;
	if (self->encoding == CENCODING_NUMBER) {
		numbers = UArray_asNumberArrayString(self);
		if (NULL == numbers)
			return NULL;
		out = UArray_asUTF32(numbers);
		UArray_free(numbers);
		return out;
	}
	capacity = self->size;
	sourceBytes = self->size * self->itemSize;
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_setItemType_(out, CTYPE_uint32_t);
	UArray_setEncoding_(out, CENCODING_UTF32);
	UArray_setSize_(out, capacity);
	if (out->itemSize != sizeof(UTF32) || out->size != capacity ||
		(capacity > 0 && NULL == out->data)) {
		UArray_free(out);
		return NULL;
	}

	sourceData = self->data ? self->data : (const uint8_t *)"";
	sourceStart = (void *)sourceData;
	sourceEnd = (void *)(sourceData + sourceBytes);
	targetStart = (UTF32 *)out->data;
	targetEnd = (UTF32 *)(out->data + out->size * out->itemSize);

	switch (self->encoding) {
		case CENCODING_ASCII:
		case CENCODING_UTF8:
			r = ConvertUTF8toUTF32((const UTF8 **)&sourceStart,
				(const UTF8 *)sourceEnd, &targetStart, targetEnd, options);
			break;
		case CENCODING_UTF16:
			r = ConvertUTF16toUTF32((const UTF16 **)&sourceStart,
				(const UTF16 *)sourceEnd, &targetStart, targetEnd, options);
			break;
		case CENCODING_UTF32:
			UArray_copy_(out, self);
			if (out->size != self->size ||
				(self->size > 0 && NULL == out->data)) {
				UArray_free(out);
				return NULL;
			}
			break;
		default:
			UArray_free(out);
			return NULL;
	}
	if (r != conversionOK) {
		UArray_free(out);
		return NULL;
	}
	if (!UArray_truncateAfterConvertToEncoding_(out)) {
		UArray_free(out);
		return NULL;
	}
	return out;
}


void UArray_convertToUTF8(UArray *self)
{
	UArray *a = UArray_asUTF8(self);
	if (NULL == a)
		return;
	UArray_swapWith_(self, a);
	UArray_free(a);
}

void UArray_convertToUTF16(UArray *self)
{
	UArray *a = UArray_asUTF16(self);
	if (NULL == a)
		return;
	UArray_swapWith_(self, a);
	UArray_free(a);
}

void UArray_convertToUTF32(UArray *self)
{
	UArray *a = UArray_asUTF32(self);
	if (NULL == a)
		return;
	UArray_swapWith_(self, a);
	UArray_free(a);
}


// ----------------------------------------------------


