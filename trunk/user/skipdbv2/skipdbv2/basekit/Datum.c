/*#io
docCopyright("Steve Dekorte", 2004)
docLicense("BSD revised")
docObject("Datum")    
*/

#include "Datum.h"
#include "List.h"
#include "UArray.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CTYPE_size is implemented by UArray.c and shared with stream validation. */
extern size_t CTYPE_size(CTYPE type);

static int Datum_hasValidStorage_(const Datum *self)
{
	return NULL != self && (0 == self->size || NULL != self->data);
}

static int Datum_compareStorageState_(const Datum *self, const Datum *other, int *handled)
{
	int selfValid, otherValid;
	*handled = 1;
	if (self == other)
		return 0;
	if (NULL == self)
		return -1;
	if (NULL == other)
		return 1;
	selfValid = Datum_hasValidStorage_(self);
	otherValid = Datum_hasValidStorage_(other);
	if (selfValid != otherValid)
		return selfValid ? 1 : -1;
	if (selfValid) {
		*handled = 0;
		return 0;
	}
	if (self->size == other->size)
		return 0;
	return self->size < other->size ? -1 : 1;
}


PID_TYPE Datum_size(Datum *self)
{
	return self ? self->size : 0;
}

unsigned char *Datum_data(Datum *self)
{
	return self ? self->data : NULL;
}

void *Datum_asUArray(Datum *self)
{
	if (!Datum_hasValidStorage_(self))
		return NULL;
	return UArray_newWithData_type_encoding_size_copy_(self->data,
		CTYPE_uint8_t, CENCODING_UTF8, self->size, 1);
}


Datum Datum_FromData_length_(unsigned char *data, PID_TYPE size)
{
	Datum d;
	d.data = data;
	d.size = data ? size : 0;
	return d;
}


Datum Datum_FromCString_(const char *s)
{
	Datum d;
	d.data = (unsigned char *)s;
	d.size = s ? strlen(s) + 1 : 0;
	return d;
}


/*
Datum Datum_FromPid_(PID_TYPE pid)
{
	Datum d;
	d.data = (unsigned char *)&pid;
	d.size = sizeof(PID_TYPE);
	return d;
}
*/

Datum Datum_FromUArray_(UArray *ba)
{
	Datum d;
	size_t byteCount;
	if (NULL == ba || ba->itemSize == 0 ||
		CTYPE_size(ba->itemType) != ba->itemSize ||
		ba->size > (size_t)-1 / ba->itemSize ||
		(ba->size > 0 && NULL == ba->data))
		return Datum_Empty();

	byteCount = ba->size * ba->itemSize;
	if (byteCount == (size_t)-1)
		return Datum_Empty();

	d.data = (unsigned char *)UArray_bytes(ba);
	d.size = byteCount;
	return d;
}


Datum Datum_Empty(void)
{
	Datum d;
	d.data = NULL;
	d.size = 0;
	return d;
}

Datum Datum_datumAt_(Datum *self, size_t i)
{
	Datum d;
	if (!Datum_hasValidStorage_(self) || i >= self->size)
		return Datum_Empty();
	d.data = self->data + i;
	d.size = self->size - i;
	return d;
}



int Datum_compare_(Datum *self, Datum *other)
{
	size_t l1, l2, min;
	int cmp, handled;
	int state = Datum_compareStorageState_(self, other, &handled);
	if (handled)
		return state;
	l1 = self->size;
	l2 = other->size;
	min = l1 < l2 ? l1 : l2;
	cmp = min ? memcmp(self->data, other->data, min) : 0;
	if (cmp != 0)
		return cmp;
	if (l1 == l2)
		return 0;
	return l1 < l2 ? -1 : 1;
}


int Datum_compare_length_(Datum *self, Datum *other, size_t limit)
{
	size_t min;
	int handled;
	int state = Datum_compareStorageState_(self, other, &handled);
	if (handled)
		return state;
	min = self->size < other->size ? self->size : other->size;
	if (min > limit)
		min = limit;
	return min ? memcmp(self->data, other->data, min) : 0;
}


int Datum_beginsWith_(Datum *self, Datum *other)
{
	if (!Datum_hasValidStorage_(self) || !Datum_hasValidStorage_(other) ||
		self->size < other->size)
		return 0;
	return Datum_compare_length_(self, other, other->size) == 0;
}


int Datum_endsWith_(Datum *self, Datum *other)
{
	Datum d;
	if (!Datum_hasValidStorage_(self) || !Datum_hasValidStorage_(other) ||
		self->size < other->size)
		return 0;
	if (other->size == 0)
		return 1;
	d = Datum_datumAt_(self, self->size - other->size);
	return Datum_compare_(&d, other) == 0;
}


int Datum_compareCString_(Datum *self, const char *s)
{
	Datum sd = Datum_FromCString_(s);
	return Datum_compare_(self, &sd);
}

size_t Datum_matchingPrefixSizeWith_(Datum *self, Datum *other)
{
	size_t min, i;
	const unsigned char *b1, *b2;
	if (!Datum_hasValidStorage_(self) || !Datum_hasValidStorage_(other))
		return 0;
	min = self->size < other->size ? self->size : other->size;
	b1 = self->data;
	b2 = other->data;
	for (i = 0; i < min; i++)
		if (b1[i] != b2[i])
			break;
	return i;
}


Datum *Datum_newFrom_to_(Datum *self, size_t start, size_t end)
{
	Datum *d;
	if (!Datum_hasValidStorage_(self) || start > end || end > self->size)
		return NULL;
	d = (Datum *)malloc(sizeof(Datum));
	if (NULL == d)
		return NULL;
	d->data = self->data ? self->data + start : NULL;
	d->size = end - start;
	return d;
}



int Datum_detect_with_(Datum *self, DatumDetectWithFunc *func, void *target)
{
	size_t i, size;
	unsigned char *data;

	if (!Datum_hasValidStorage_(self) || NULL == func ||
		self->size > (size_t)INT_MAX)
		return -1;

	/* Keep the original search extent stable across callback invocations. */
	size = self->size;
	data = self->data;
	for (i = 0; i < size; ++i) {
		Datum suffix;
		int result;
		suffix.data = data + i;
		suffix.size = size - i;
		result = (*func)(target, &suffix);
		if (self->data != data || self->size != size)
			return -1;
		if (1 == result)
			return (int)i;
		if (-1 == result)
			return -1;
	}
	return -1;
}


long Datum_find_(Datum *self, void *delimsList, size_t startIndex)
{
	List *delims = (List *)delimsList;
	size_t i;
	if (!Datum_hasValidStorage_(self) || NULL == delims ||
		self->size > (size_t)LONG_MAX ||
		startIndex > self->size)
		return -1;
	for (i = startIndex; i < self->size; i++) {
		Datum d = Datum_datumAt_(self, i);
		size_t j;
		for (j = 0; j < (size_t)List_size(delims); j++) {
			Datum *delim = (Datum *)List_at_(delims, (long)j);
			if (delim && Datum_beginsWith_(&d, delim))
				return (long)i;
		}
	}
	return -1;
}



static void Datum_freeSplitResults_(List *results)
{
	size_t i;
	if (NULL == results)
		return;
	for (i = 0; i < (size_t)List_size(results); ++i)
		free(List_at_(results, (long)i));
	List_free(results);
}

void *Datum_split_(Datum *self, void *delimsList) /* returns a List */
{
	List *delims = (List *)delimsList;
	List *results;
	size_t i, last = 0;
	if (!Datum_hasValidStorage_(self) || NULL == delims)
		return NULL;
	results = List_new();
	if (NULL == results)
		return NULL;

	for (i = 0; i < self->size; ++i) {
		Datum d = Datum_datumAt_(self, i);
		size_t j;
		for (j = 0; j < (size_t)List_size(delims); ++j) {
			Datum *delim = (Datum *)List_at_(delims, (long)j);
			Datum *part;
			size_t oldCount;
			if (NULL == delim || delim->size == 0 ||
				(delim->size > 0 && NULL == delim->data))
				continue;
			if (!Datum_beginsWith_(&d, delim))
				continue;
			part = Datum_newFrom_to_(self, last, i);
			if (NULL == part) {
				Datum_freeSplitResults_(results);
				return NULL;
			}
			oldCount = (size_t)List_size(results);
			List_append_(results, part);
			if ((size_t)List_size(results) != oldCount + 1) {
				free(part);
				Datum_freeSplitResults_(results);
				return NULL;
			}
			last = i + delim->size;
			i = last - 1;
			break;
		}
	}
	if (last != self->size) {
		Datum *part = Datum_newFrom_to_(self, last, self->size);
		size_t oldCount;
		if (NULL == part) {
			Datum_freeSplitResults_(results);
			return NULL;
		}
		oldCount = (size_t)List_size(results);
		List_append_(results, part);
		if ((size_t)List_size(results) != oldCount + 1) {
			free(part);
			Datum_freeSplitResults_(results);
			return NULL;
		}
	}
	return results;
}



// enumeration 

/*
 int Datum_next(Datum *self)
 {
	 if (self->size)
	 {
		 self->data ++;
		 self->size --;
		 return 1;
	 }
	 return 0;
 }
 */

unsigned int Datum_hash(Datum *self)
{
	PID_TYPE length;
	const unsigned char *key;
	unsigned int h = 5381;
	if (!Datum_hasValidStorage_(self))
		return 0;
	length = self->size;
	key = self->data;
	while (length-- > 0) {
		h += (h << 5);
		h ^= *key++;
	}
	return h;
}


/*
 typedef int (DatumDetectWithFunc)(void *, Datum *); // 1 = match, -1 = break 
 
 int Datum_detect_withTarget_(Datum *self, DatumDetectWithFunc *func, void *target)
 {
	 unsigned char *s   = self->data;
	 unsigned char *end = self->data + self->size;
	 
	 Datum d;
	 int result;
	 
	 while (s < end)
	 {
		 d.data = s;
		 d.size = end - s;
		 result = (*func)(target, &d);
		 
		 if (result == 1) 
		 {
			 return s - self->bytes;
		 } 
		 else if (result == -1)
		 {
			 return -1;
		 }
		 
		 s ++;
	 }
	 
	 return -1;
 }
 */

/*
 Datum Datum_datumUntil_withTarget_(Datum *self, DatumDetectWithFunc *func, void *target)
 {
	 int index = Datum_detect_withTarget_(self, func, target);
	 Datum chunk;
	 
	 if (index == -1) 
	 {
		 index = length; 
	 }
	 
	 d.data = self->data + index;
	 d.size = self->size - index;
	 return d;
 }
 */
