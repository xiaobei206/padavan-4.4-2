/*
	copyright: Steve Dekorte, 2006. All rights reserved.
	license: See _BSDLicense.txt.
*/

#include "UArray.h"
#include <math.h>
#include <ctype.h>
#include <limits.h>

/*
 * The C character-classification functions accept only EOF or an unsigned
 * char value. UArray can also contain signed integers, wide integers, and
 * floating-point values, so validate the value before narrowing it.
 */
static int UArray_characterValueIsByte_(long double value)
{
	return value >= 0.0L && value <= (long double)UCHAR_MAX;
}

/* Character predicates produce 0 for values outside the byte domain. */
#define UARRAY_IOP(OP) \
void UArray_ ## OP (UArray *self) \
{ \
	if (NULL == self || (self->size > 0 && NULL == self->data)) return; \
	UARRAY_FOREACHASSIGN(self, i, v, \
		UArray_characterValueIsByte_((long double)v) ? OP((unsigned char)(v)) : 0); \
	UArray_changed(self); \
}

UARRAY_IOP(isalnum);
UARRAY_IOP(isalpha);
UARRAY_IOP(iscntrl);
UARRAY_IOP(isdigit);
UARRAY_IOP(isgraph);
UARRAY_IOP(islower);
UARRAY_IOP(isprint);
UARRAY_IOP(ispunct);
UARRAY_IOP(isspace);
UARRAY_IOP(isupper);
UARRAY_IOP(isxdigit);

/* Case conversion leaves non-byte numeric values unchanged. */
#define UARRAY_TOP(OP) \
void UArray_ ## OP (UArray *self) \
{ \
	if (NULL == self || (self->size > 0 && NULL == self->data)) return; \
	UARRAY_FOREACHASSIGN(self, i, v, \
		UArray_characterValueIsByte_((long double)v) ? OP((unsigned char)(v)) : (v)); \
	UArray_changed(self); \
}

UARRAY_TOP(tolower);
UARRAY_TOP(toupper);

BASEKIT_API int UArray_isLowercase(UArray *self)
{
	if (NULL == self || (self->size > 0 && NULL == self->data))
		return 0;
	UARRAY_INTFOREACH(self, i, v,
		if (!UArray_characterValueIsByte_((long double)v) ||
			v != tolower((unsigned char)v)) return 0);
	return 1;
}

BASEKIT_API int UArray_isUppercase(UArray *self)
{
	if (NULL == self || (self->size > 0 && NULL == self->data))
		return 0;
	UARRAY_INTFOREACH(self, i, v,
		if (!UArray_characterValueIsByte_((long double)v) ||
			v != toupper((unsigned char)v)) return 0);
	return 1;
}
