/*
	copyright: Steve Dekorte, 2006. All rights reserved.
	license: See _BSDLicense.txt.
*/

#include "UArray.h"
#include <math.h>
#include <ctype.h>

// set

static int UArray_characterByteArrayIsValid_(const UArray *self)
{
	if (NULL == self ||
		(self->itemType != CTYPE_uint8_t && self->itemType != CTYPE_int8_t) ||
		self->itemSize != sizeof(uint8_t) ||
		self->size > self->memSize / sizeof(uint8_t) ||
		(self->size > 0 && NULL == self->data))
		return 0;
	return 1;
}

#define UARRAY_IOP_PREDICATE(OP) \
BASEKIT_API void UArray_ ## OP (UArray *self) { \
	if (!UArray_characterByteArrayIsValid_(self)) return; \
	UARRAY_FOREACHASSIGN(self, i, v, (!!OP((unsigned char)v))); \
	UArray_changed(self); \
}

#define UARRAY_IOP_CASEMAP(OP) \
BASEKIT_API void UArray_ ## OP (UArray *self) { \
	if (!UArray_characterByteArrayIsValid_(self)) return; \
	UARRAY_FOREACHASSIGN(self, i, v, OP((unsigned char)v)); \
	UArray_changed(self); \
}

UARRAY_IOP_PREDICATE(isalnum);
UARRAY_IOP_PREDICATE(isalpha);
UARRAY_IOP_PREDICATE(iscntrl);
UARRAY_IOP_PREDICATE(isdigit);
UARRAY_IOP_PREDICATE(isgraph);
UARRAY_IOP_PREDICATE(islower);
UARRAY_IOP_PREDICATE(isprint);
UARRAY_IOP_PREDICATE(ispunct);
UARRAY_IOP_PREDICATE(isspace);
UARRAY_IOP_PREDICATE(isupper);
UARRAY_IOP_PREDICATE(isxdigit);

UARRAY_IOP_CASEMAP(tolower);
UARRAY_IOP_CASEMAP(toupper);

BASEKIT_API int UArray_isLowercase(UArray *self)
{
	if (!UArray_characterByteArrayIsValid_(self))
		return 0;
	UARRAY_INTFOREACH(self, i, v,
		if ((unsigned char)v != (unsigned char)tolower((unsigned char)v)) return 0);
	return 1;
}

BASEKIT_API int UArray_isUppercase(UArray *self)
{
	if (!UArray_characterByteArrayIsValid_(self))
		return 0;
	UARRAY_INTFOREACH(self, i, v,
		if ((unsigned char)v != (unsigned char)toupper((unsigned char)v)) return 0);
	return 1;
}
