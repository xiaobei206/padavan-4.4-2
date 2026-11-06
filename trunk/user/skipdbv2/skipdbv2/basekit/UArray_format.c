/*
	copyright: Steve Dekorte, 2006. All rights reserved.
	license: See _BSDLicense.txt.
*/

#include "UArray.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

UArray *UArray_newWithFormat_(const char *format, ...)
{
	UArray *self;
	va_list ap;
	if (NULL == format)
		return NULL;
	va_start(ap, format);
	self = UArray_newWithVargs_(format, ap);
	va_end(ap);
	return self;
}

UArray *UArray_newWithVargs_(const char *format, va_list ap)
{
	UArray *self;
	if (NULL == format)
		return NULL;
	self = UArray_new();
	if (NULL == self)
		return NULL;
	UArray_fromVargs_(self, format, ap);
	return self;
}

UArray *UArray_fromFormat_(UArray *self, const char *format, ...)
{
	va_list ap;
	if (NULL == self || NULL == format)
		return self;
	va_start(ap, format);
	UArray_fromVargs_(self, format, ap);
	va_end(ap);
	return self;
}

void UArray_fromVargs_(UArray *self, const char *format, va_list ap)
{
	if (NULL == self || NULL == format)
		return;
	while (*format)
	{
		if (*format == '%')
		{
			format ++;

			/* A trailing format marker must not advance past the terminator. */
			if (*format == 0)
			{
				UArray_appendCString_(self, "%");
				return;
			}

			if (*format == 's')
			{
				char *s = va_arg(ap, char *);
				if (!s) { printf("UArray_fromVargs_ missing param"); return; }
				UArray_appendCString_(self, s);
			}
			else if (*format == 'i' || *format == 'd')
			{
				int i = va_arg(ap, int);
				char s[100];

				snprintf(s, 100, "%i", i);
				UArray_appendCString_(self, s);
			}
			else if (*format == 'f')
			{
				double d = va_arg(ap, double);
				int formattedLength = snprintf(NULL, 0, "%f", d);
				size_t formattedSize;
				char *formatted;

				if (formattedLength < 0)
					return;
				formattedSize = (size_t)formattedLength + 1;
				formatted = (char *)malloc(formattedSize);
				if (NULL == formatted)
					return;
				if (snprintf(formatted, formattedSize, "%f", d) == formattedLength)
					UArray_appendCString_(self, formatted);
				free(formatted);
			}
			else if (*format == 'p')
			{
				void *p = va_arg(ap, void *);
				char s[100];

				snprintf(s, 100, "%p", p);
				UArray_appendCString_(self, s);
			}
			// new format command for a given number adding spaces
			else if (*format == '#')
			{
				int n, i = va_arg(ap, int);
				char *s = " ";

				for (n = 0; n < i; n ++)
				{
					UArray_appendCString_(self, s);
				}
			}
		}
		else
		{
			char s[2];

			snprintf(s, 2, "%c", *format);
			UArray_appendCString_(self, s);
		}

		format ++;
	}
}

UArray *UArray_asNewHexStringUArray(UArray *self)
{
	static const char digits[] = "0123456789abcdef";
	UArray *out;
	size_t i, newSize;
	if (NULL == self ||
		(self->itemType != CTYPE_uint8_t && self->itemType != CTYPE_int8_t) ||
		self->itemSize != sizeof(uint8_t) ||
		self->size > (size_t)-1 / 2 ||
		(self->size > 0 && NULL == self->data))
		return NULL;
	newSize = self->size * 2;
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_setEncoding_(out, CENCODING_ASCII);
	UArray_setSize_(out, newSize);
	if (out->size != newSize || (newSize > 0 && NULL == out->data)) {
		UArray_free(out);
		return NULL;
	}
	for (i = 0; i < self->size; ++i) {
		unsigned char value = self->data[i];
		out->data[i * 2] = (uint8_t)digits[value >> 4];
		out->data[i * 2 + 1] = (uint8_t)digits[value & 0x0f];
	}
	return out;
}
