/*
	copyright: Steve Dekorte, 2006. All rights reserved.
	license: See _BSDLicense.txt.
*/

#include "UArray.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stddef.h>

#warning Uncomment the IO_USE_SIMD define to turn on SIMD acceleration
//#define IO_USE_SIMD 1

#ifdef IO_USE_SIMD
#include "simd_cp.h"
#else
#define __UNK__EMU__
#include "simd_cp_emu.h"
#endif

#define VEC_SIZE 4

static int UArray_hasValidNumericStorage_(const UArray *self);
static int UArray_hasUnsafeNegation_(const UArray *self);
static int UArray_integerFloatValueIsOutOfRange_(CTYPE itemType, double value);

#define LLVEC_DUALARG_OP(VNAME, COP, TYPE) \
void v ## TYPE ## _ ## VNAME(TYPE ##_t *aa, TYPE ##_t *bb, size_t size)\
{\
	size_t i = 0;\
	simd_m128 *a = (simd_m128 *)aa;\
	simd_m128 *b = (simd_m128 *)bb;\
	size_t max = (size / VEC_SIZE);\
	for(i = 0; i < max; i ++) { simd_4f_ ## VNAME (a[i], b[i], a[i]); }\
	i = i * VEC_SIZE;\
	while (i < size) { aa[i] COP bb[i]; i ++; }\
}

LLVEC_DUALARG_OP(add,  +=, float32);
LLVEC_DUALARG_OP(sub,  -=, float32);
LLVEC_DUALARG_OP(mult, *=, float32);
LLVEC_DUALARG_OP(div,  /=, float32);

// set

void UArray_round(UArray *self)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0 ||
		(self->itemType != CTYPE_float32_t &&
		 self->itemType != CTYPE_float64_t))
		return;
	UARRAY_FOREACHASSIGN(self, i, v, floor((double)(v+.5)));
	UArray_changed(self);
}

void UArray_clear(UArray *self)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	UARRAY_FOREACHASSIGN(self, i, v, 0);
	UArray_changed(self);
}

void UArray_setItemsToLong_(UArray *self, long x)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	UARRAY_FOREACHASSIGN(self, i, v, x);
	UArray_changed(self);
}

void UArray_setItemsToDouble_(UArray *self, double x)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	/* Out-of-range float-to-integer conversion is undefined; reject before mutating. */
	if (self->itemType != CTYPE_float32_t &&
		self->itemType != CTYPE_float64_t &&
		UArray_integerFloatValueIsOutOfRange_(self->itemType, x))
		return;
	UARRAY_FOREACHASSIGN(self, i, v, x);
	UArray_changed(self);
}

void UArray_rangeFill(UArray *self)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	UARRAY_FOREACHASSIGN(self, i, v, i);
	UArray_changed(self);
}

void UArray_negate(const UArray *self)
{
	UArray *mutableSelf;

	if (!UArray_hasValidNumericStorage_(self))
		return;
	if (!UArray_isSignedType(self)) {
		UArray_error_(self, "UArray_negate not supported on this type");
		return;
	}
	if (self->size == 0 || UArray_hasUnsafeNegation_(self))
		return;

	/* The legacy API is const-qualified despite mutating the array contents. */
	mutableSelf = (UArray *)self;
	UARRAY_FOREACHASSIGN(mutableSelf, i, v, -v);
	UArray_changed(mutableSelf);
}

// basic vector math

static int UArray_hasUnsafeIntegerDivision_(const UArray *self, const UArray *other, size_t count);

void UArray_add_(UArray *self, const UArray *other)
{
	size_t minSize;

	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other))
		return;

	minSize = self->size < other->size ? self->size : other->size;
	if (minSize == 0)
		return;

	if (self->itemType == CTYPE_float32_t && other->itemType == CTYPE_float32_t)
	{
		vfloat32_add((float32_t *)self->data, (float32_t *)other->data, minSize);
	}
	else
	{
		/* UARRAY_BASICOP_TYPES returns from this function after dispatch. */
		UArray_changed(self);
		DUARRAY_OP(UARRAY_BASICOP_TYPES, +=, self, other);
	}

	UArray_changed(self);
}

void UArray_subtract_(UArray *self, const UArray *other)
{
	size_t minSize;

	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other))
		return;

	minSize = self->size < other->size ? self->size : other->size;
	if (minSize == 0)
		return;

	if (self->itemType == CTYPE_float32_t && other->itemType == CTYPE_float32_t)
	{
		vfloat32_sub((float32_t *)self->data, (float32_t *)other->data, minSize);
	}
	else
	{
		/* UARRAY_BASICOP_TYPES returns from this function after dispatch. */
		UArray_changed(self);
		DUARRAY_OP(UARRAY_BASICOP_TYPES, -=, self, other);
	}

	UArray_changed(self);
}

void UArray_multiply_(UArray *self, const UArray *other)
{
	size_t minSize;

	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other))
		return;

	minSize = self->size < other->size ? self->size : other->size;
	if (minSize == 0)
		return;

	if (self->itemType == CTYPE_float32_t && other->itemType == CTYPE_float32_t)
	{
		vfloat32_mult((float32_t *)self->data, (float32_t *)other->data, minSize);
	}
	else
	{
		/* UARRAY_BASICOP_TYPES returns from this function after dispatch. */
		UArray_changed(self);
		DUARRAY_OP(UARRAY_BASICOP_TYPES, *=, self, other);
	}

	UArray_changed(self);
}

void UArray_divide_(UArray *self, const UArray *other)
{
	size_t minSize;

	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other) ||
		self->itemType == CTYPE_uintptr_t ||
		other->itemType == CTYPE_uintptr_t)
		return;

	minSize = self->size < other->size ? self->size : other->size;
	if (minSize == 0)
		return;

	if (self->itemType != CTYPE_float32_t &&
		self->itemType != CTYPE_float64_t &&
		UArray_hasUnsafeIntegerDivision_(self, other, minSize))
		return;

	if (self->itemType == CTYPE_float32_t && other->itemType == CTYPE_float32_t)
	{
		vfloat32_div((float32_t *)self->data, (float32_t *)other->data, minSize);
	}
	else
	{
		/* UARRAY_BASICOP_TYPES returns from this function after dispatch. */
		UArray_changed(self);
		DUARRAY_OP(UARRAY_BASICOP_TYPES, /=, self, other);
	}

	UArray_changed(self);
}

#define UARRAY_DOT(OP2, TYPE1, self, TYPE2, other)\
{\
	double p = 0;\
	size_t i, minSize = self->size < other->size ? self->size : other->size;\
	for(i = 0; i < minSize; i ++)\
	{ p += ((TYPE1 *)self->data)[i] * ((TYPE2 *)other->data)[i]; }\
	return p; \
}


static size_t UArray_numericItemSize_(CTYPE itemType)
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



#define UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(name, type) \
	typedef struct { char leadingByte; type value; } name

UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathUInt8AlignmentProbe, uint8_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathUInt16AlignmentProbe, uint16_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathUInt32AlignmentProbe, uint32_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathUInt64AlignmentProbe, uint64_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathInt8AlignmentProbe, int8_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathInt16AlignmentProbe, int16_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathInt32AlignmentProbe, int32_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathInt64AlignmentProbe, int64_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathFloat32AlignmentProbe, float32_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathFloat64AlignmentProbe, float64_t);
UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_(UArrayMathUintptrAlignmentProbe, uintptr_t);

#undef UARRAY_MATH_DECLARE_ALIGNMENT_PROBE_

static size_t UArray_numericItemAlignment_(CTYPE itemType)
{
	switch (itemType) {
		case CTYPE_uint8_t: return offsetof(UArrayMathUInt8AlignmentProbe, value);
		case CTYPE_uint16_t: return offsetof(UArrayMathUInt16AlignmentProbe, value);
		case CTYPE_uint32_t: return offsetof(UArrayMathUInt32AlignmentProbe, value);
		case CTYPE_uint64_t: return offsetof(UArrayMathUInt64AlignmentProbe, value);
		case CTYPE_int8_t: return offsetof(UArrayMathInt8AlignmentProbe, value);
		case CTYPE_int16_t: return offsetof(UArrayMathInt16AlignmentProbe, value);
		case CTYPE_int32_t: return offsetof(UArrayMathInt32AlignmentProbe, value);
		case CTYPE_int64_t: return offsetof(UArrayMathInt64AlignmentProbe, value);
		case CTYPE_float32_t: return offsetof(UArrayMathFloat32AlignmentProbe, value);
		case CTYPE_float64_t: return offsetof(UArrayMathFloat64AlignmentProbe, value);
		case CTYPE_uintptr_t: return offsetof(UArrayMathUintptrAlignmentProbe, value);
		default: return 0;
	}
}


static int UArray_hasValidNumericStorage_(const UArray *self)
{
	size_t expectedItemSize, expectedItemAlignment;
	if (NULL == self)
		return 0;
	expectedItemSize = UArray_numericItemSize_(self->itemType);
	expectedItemAlignment = UArray_numericItemAlignment_(self->itemType);
	if (expectedItemSize == 0 || expectedItemAlignment == 0 ||
		self->itemSize != expectedItemSize ||
		self->size > (size_t)-1 / expectedItemSize ||
		(self->size > 0 && NULL == self->data) ||
		(self->data &&
			((uintptr_t)self->data % expectedItemAlignment) != 0))
		return 0;
	return 1;
}


/* Signed minimum values cannot be negated in their own C integer type. */
static int UArray_hasUnsafeNegation_(const UArray *self)
{
	size_t i;

	switch (self->itemType) {
		case CTYPE_int8_t:
			for (i = 0; i < self->size; ++i)
				if (((const int8_t *)self->data)[i] == INT8_MIN) return 1;
			break;
		case CTYPE_int16_t:
			for (i = 0; i < self->size; ++i)
				if (((const int16_t *)self->data)[i] == INT16_MIN) return 1;
			break;
		case CTYPE_int32_t:
			for (i = 0; i < self->size; ++i)
				if (((const int32_t *)self->data)[i] == INT32_MIN) return 1;
			break;
		case CTYPE_int64_t:
			for (i = 0; i < self->size; ++i)
				if (((const int64_t *)self->data)[i] == INT64_MIN) return 1;
			break;
		default:
			break;
	}
	return 0;
}


/* Preflight every affected element before integer division can partially mutate the receiver. */
static float UArray_integerValueAsFloat_(const UArray *self, size_t index)
{
    switch (self->itemType) {
        case CTYPE_uint8_t: return (float)((const uint8_t *)self->data)[index];
        case CTYPE_uint16_t: return (float)((const uint16_t *)self->data)[index];
        case CTYPE_uint32_t: return (float)((const uint32_t *)self->data)[index];
        case CTYPE_uint64_t: return (float)((const uint64_t *)self->data)[index];
        case CTYPE_int8_t: return (float)((const int8_t *)self->data)[index];
        case CTYPE_int16_t: return (float)((const int16_t *)self->data)[index];
        case CTYPE_int32_t: return (float)((const int32_t *)self->data)[index];
        case CTYPE_int64_t: return (float)((const int64_t *)self->data)[index];
        default: return 0.0f;
    }
}

static int UArray_integerFloatValueIsOutOfRange_(CTYPE itemType, double value)
{
    int bits;
    int isSigned;

    if (!isfinite(value))
        return 1;

    switch (itemType) {
        case CTYPE_uint8_t: bits = 8; isSigned = 0; break;
        case CTYPE_uint16_t: bits = 16; isSigned = 0; break;
        case CTYPE_uint32_t: bits = 32; isSigned = 0; break;
        case CTYPE_uint64_t: bits = 64; isSigned = 0; break;
        case CTYPE_int8_t: bits = 8; isSigned = 1; break;
        case CTYPE_int16_t: bits = 16; isSigned = 1; break;
        case CTYPE_int32_t: bits = 32; isSigned = 1; break;
        case CTYPE_int64_t: bits = 64; isSigned = 1; break;
        default: return 1;
    }

    if (!isSigned)
        return value <= -1.0 || value >= ldexp(1.0, bits);

    {
        double upperBound = ldexp(1.0, bits - 1);
        if (bits == 64)
            return value < -upperBound || value >= upperBound;
        return value <= -upperBound - 1.0 || value >= upperBound;
    }
}

static int UArray_hasUnsafeIntegerDivision_(const UArray *self, const UArray *other, size_t count)
{
	size_t i;

	for (i = 0; i < count; ++i) {
		double divisor = UArray_rawDoubleAt_(other, i);

		if (divisor == 0.0)
			return 1;

		if (other->itemType == CTYPE_float32_t ||
			other->itemType == CTYPE_float64_t) {
			double quotient;

			if (other->itemType == CTYPE_float32_t)
				quotient = (double)(UArray_integerValueAsFloat_(self, i) /
					((const float32_t *)other->data)[i]);
			else
				quotient = UArray_rawDoubleAt_(self, i) /
					((const float64_t *)other->data)[i];

			if (UArray_integerFloatValueIsOutOfRange_(self->itemType, quotient))
				return 1;
			continue;
		}

		if (divisor != -1.0)
			continue;

		switch (self->itemType) {
			case CTYPE_int8_t:
				if (((const int8_t *)self->data)[i] == INT8_MIN) return 1;
				break;
			case CTYPE_int16_t:
				if (((const int16_t *)self->data)[i] == INT16_MIN) return 1;
				break;
			case CTYPE_int32_t:
				if (((const int32_t *)self->data)[i] == INT32_MIN) return 1;
				break;
			case CTYPE_int64_t:
				if (((const int64_t *)self->data)[i] == INT64_MIN) return 1;
				break;
			default:
				break;
		}
	}
	return 0;
}


double UArray_dotProduct_(const UArray *self, const UArray *other)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other))
		return 0;
	DUARRAY_OP(UARRAY_DOT, NULL, self, other);
	return 0;
}

// basic scalar math

void UArray_addScalarDouble_(UArray *self, double value)
{
	size_t i;
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	if (self->itemType != CTYPE_float32_t &&
		self->itemType != CTYPE_float64_t) {
		for (i = 0; i < self->size; ++i) {
			double result = UArray_rawDoubleAt_(self, i) + value;
			if (UArray_integerFloatValueIsOutOfRange_(self->itemType, result))
				return;
		}
	}
	UARRAY_FOREACHASSIGN(self, i, v, v + value);
	UArray_changed(self);
}

void UArray_subtractScalarDouble_(UArray *self, double value)
{
	size_t i;
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	if (self->itemType != CTYPE_float32_t &&
		self->itemType != CTYPE_float64_t) {
		for (i = 0; i < self->size; ++i) {
			double result = UArray_rawDoubleAt_(self, i) - value;
			if (UArray_integerFloatValueIsOutOfRange_(self->itemType, result))
				return;
		}
	}
	UARRAY_FOREACHASSIGN(self, i, v, v - value);
	UArray_changed(self);
}

void UArray_multiplyScalarDouble_(UArray *self, double value)
{
	size_t i;
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;
	if (self->itemType != CTYPE_float32_t &&
		self->itemType != CTYPE_float64_t) {
		for (i = 0; i < self->size; ++i) {
			double result = UArray_rawDoubleAt_(self, i) * value;
			if (UArray_integerFloatValueIsOutOfRange_(self->itemType, result))
				return;
		}
	}
	UARRAY_FOREACHASSIGN(self, i, v, v * value);
	UArray_changed(self);
}

void UArray_divideScalarDouble_(UArray *self, double value)
{
	size_t i;

	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0 ||
		value == 0.0)
		return;

	/* Floating-to-integer conversion outside the destination range is undefined. */
	if (self->itemType != CTYPE_float32_t &&
		self->itemType != CTYPE_float64_t) {
		for (i = 0; i < self->size; ++i) {
			double quotient = UArray_rawDoubleAt_(self, i) / value;
			if (UArray_integerFloatValueIsOutOfRange_(self->itemType, quotient))
				return;
		}
	}

	UARRAY_FOREACHASSIGN(self, i, v, v / value);
	UArray_changed(self);
}

// bitwise

static int UArray_hasValidBitStorage_(const UArray *self, size_t *byteCount);

void UArray_bitwiseOr_(UArray *self, const UArray *other)
{
	size_t i, selfByteCount, otherByteCount, max;
	uint8_t *d1, *d2;

	if (!UArray_hasValidBitStorage_(self, &selfByteCount) ||
		!UArray_hasValidBitStorage_(other, &otherByteCount))
		return;

	max = selfByteCount < otherByteCount ? selfByteCount : otherByteCount;
	d1 = self->data;
	d2 = other->data;
	for (i = 0; i < max; ++i)
		d1[i] |= d2[i];
	if (max > 0)
		UArray_changed(self);
}

void UArray_bitwiseAnd_(UArray *self, const UArray *other)
{
	size_t i, selfByteCount, otherByteCount, max;
	uint8_t *d1, *d2;

	if (!UArray_hasValidBitStorage_(self, &selfByteCount) ||
		!UArray_hasValidBitStorage_(other, &otherByteCount))
		return;

	max = selfByteCount < otherByteCount ? selfByteCount : otherByteCount;
	d1 = self->data;
	d2 = other->data;
	for (i = 0; i < max; ++i)
		d1[i] &= d2[i];
	if (max > 0)
		UArray_changed(self);
}

void UArray_bitwiseXor_(UArray *self, const UArray *other)
{
	size_t i, selfByteCount, otherByteCount, max;
	uint8_t *d1, *d2;

	if (!UArray_hasValidBitStorage_(self, &selfByteCount) ||
		!UArray_hasValidBitStorage_(other, &otherByteCount))
		return;

	max = selfByteCount < otherByteCount ? selfByteCount : otherByteCount;
	d1 = self->data;
	d2 = other->data;
	for (i = 0; i < max; ++i)
		d1[i] ^= d2[i];
	if (max > 0)
		UArray_changed(self);
}

void UArray_bitwiseNot(UArray *self)
{
	size_t i, byteCount;
	uint8_t *data;

	if (!UArray_hasValidBitStorage_(self, &byteCount))
		return;

	data = self->data;
	for (i = 0; i < byteCount; ++i)
		data[i] = (uint8_t)~data[i];
	if (byteCount > 0)
		UArray_changed(self);
}

// bitwise ops

void UArray_setAllBitsTo_(UArray *self, uint8_t aBool)
{
	size_t i, byteCount;
	uint8_t *data;
	uint8_t bits = aBool ? 0xff : 0;

	if (!UArray_hasValidBitStorage_(self, &byteCount))
		return;

	data = self->data;
	for (i = 0; i < byteCount; ++i)
		data[i] = bits;
	if (byteCount > 0)
		UArray_changed(self);
}

// adjust for endianess?

static int UArray_hasValidBitStorage_(const UArray *self, size_t *byteCount)
{
    if (NULL == self || NULL == byteCount || self->itemSize == 0 ||
        self->size > ((size_t)-1 - 1) / self->itemSize ||
        (self->size > 0 && NULL == self->data))
        return 0;
    *byteCount = self->size * self->itemSize;
    return 1;
}

int UArray_bitAt_(UArray *self, size_t i)
{
    size_t byteCount, bytePosition, bitPosition;
    if (!UArray_hasValidBitStorage_(self, &byteCount))
        return 0;
    bytePosition = i / 8;
    if (bytePosition >= byteCount)
        return 0;
    bitPosition = i % 8;
    return (self->data[bytePosition] >> bitPosition) & 0x1;
}

uint8_t UArray_byteAt_(UArray *self, size_t i)
{
	size_t byteCount;
	if (!UArray_hasValidBitStorage_(self, &byteCount) || i >= byteCount)
		return 0;
	return self->data[i];
}

void UArray_setBit_at_(UArray *self, int aBool, size_t i)
{
    size_t byteCount, bytePosition, bitPosition;
    uint8_t mask;
    if (!UArray_hasValidBitStorage_(self, &byteCount))
        return;
    bytePosition = i / 8;
    if (bytePosition >= byteCount)
        return;
    bitPosition = i % 8;
    mask = (uint8_t)(1u << bitPosition);
    if (aBool)
        self->data[bytePosition] |= mask;
    else
        self->data[bytePosition] &= (uint8_t)~mask;
    UArray_changed(self);
}

UArray *UArray_asBits(const UArray *self)
{
	UArray *out;
	size_t byteCount, bitCount, i;
	const uint8_t *data;

	if (!UArray_hasValidBitStorage_(self, &byteCount) ||
		byteCount > ((size_t)-1 - 1) / 8)
		return NULL;
	bitCount = byteCount * 8;
	out = UArray_new();
	if (NULL == out)
		return NULL;
	UArray_setSize_(out, bitCount);
	if (out->size != bitCount || (bitCount > 0 && NULL == out->data)) {
		UArray_free(out);
		return NULL;
	}

	data = self->data;
	for (i = 0; i < byteCount; ++i) {
		unsigned int bit;
		for (bit = 0; bit < 8; ++bit)
			out->data[i * 8 + bit] =
				(data[i] & (uint8_t)(1u << bit)) ? '1' : '0';
	}
	return out;
}

size_t UArray_bitCount(UArray *self)
{
	const unsigned char map[] =
	{
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
	};

	size_t i, byteCount, total = 0;

	if (!UArray_hasValidBitStorage_(self, &byteCount) ||
		byteCount > (size_t)-1 / 8)
		return 0;
	for (i = 0; i < byteCount; ++i)
		total += map[self->data[i]];
	return total;
}

// logic

static int UArray_hasSupportedLogicalTypes_(const UArray *self, const UArray *other)
{
	int receiverSupported, operandSupported;

	switch (self->itemType) {
		case CTYPE_uint8_t:
		case CTYPE_uint16_t:
		case CTYPE_uint32_t:
		case CTYPE_int8_t:
		case CTYPE_int16_t:
		case CTYPE_int32_t:
			receiverSupported = 1;
			break;
		default:
			receiverSupported = 0;
			break;
	}

	switch (other->itemType) {
		case CTYPE_uint8_t:
		case CTYPE_uint16_t:
		case CTYPE_uint32_t:
		case CTYPE_int8_t:
		case CTYPE_int16_t:
		case CTYPE_int32_t:
		case CTYPE_uintptr_t:
			operandSupported = 1;
			break;
		default:
			operandSupported = 0;
			break;
	}

	return receiverSupported && operandSupported;
}

#define UARRAY_LOGICOP_TYPES(OP2, TYPE1, self, TYPE2, other)\
{\
	size_t i, minSize = self->size < other->size ? self->size : other->size;\
	if (minSize == 0)\
		return;\
	for (i = 0; i < minSize; ++i)\
	{\
		((TYPE1 *)self->data)[i] = ((TYPE1 *)self->data)[i] OP2 ((TYPE2 *)other->data)[i];\
	}\
	UArray_changed(self);\
	return; \
}

void UArray_logicalOr_(UArray *self, const UArray *other)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other) ||
		!UArray_hasSupportedLogicalTypes_(self, other))
		return;

	DUARRAY_INTOP(UARRAY_LOGICOP_TYPES, ||, self, other);
}

void UArray_logicalAnd_(UArray *self, const UArray *other)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other) ||
		!UArray_hasSupportedLogicalTypes_(self, other))
		return;

	DUARRAY_INTOP(UARRAY_LOGICOP_TYPES, &&, self, other);
}

// trigonometry

#define UARRAY_DOP(OP) \
void UArray_ ## OP (UArray *self) \
{ \
	if (!UArray_hasValidNumericStorage_(self) || \
		self->itemType == CTYPE_uintptr_t || self->size == 0) \
		return; \
	UARRAY_FOREACHASSIGN(self, i, v, OP((double)v)); \
	UArray_changed(self); \
}

UARRAY_DOP(sin);
UARRAY_DOP(cos);
UARRAY_DOP(tan);
UARRAY_DOP(asin);
UARRAY_DOP(acos);
UARRAY_DOP(atan);
UARRAY_DOP(sinh);
UARRAY_DOP(cosh);
UARRAY_DOP(tanh);
UARRAY_DOP(exp);
UARRAY_DOP(log);
UARRAY_DOP(log10);

UARRAY_DOP(sqrt);
UARRAY_DOP(ceil);
UARRAY_DOP(floor);

void UArray_fabs(UArray *self)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t || self->size == 0)
		return;

	if (self->itemType == CTYPE_float32_t ||
		self->itemType == CTYPE_float64_t) {
		UARRAY_FOREACHASSIGN(self, i, v, fabs((double)v));
		UArray_changed(self);
		return;
	}

	if (!UArray_isSignedType(self) || UArray_hasUnsafeNegation_(self))
		return;

	/* Avoid double rounding and reject signed minima before native negation. */
	UARRAY_FOREACHASSIGN(self, i, v, (v < 0 ? -v : v));
	UArray_changed(self);
}

void UArray_abs(UArray *self)
{
	UArray_fabs(self);
}

void UArray_square(UArray *self)
{
	UArray_multiply_(self, self);
}

// extras

double UArray_sumAsDouble(const UArray *self)
{
	double sum = 0;

	if (!UArray_hasValidNumericStorage_(self))
		return 0;

	UARRAY_FOREACH(self, i, v, sum += (double)v);
	return sum;
}

double UArray_productAsDouble(const UArray *self)
{
	double product = 1;

	if (!UArray_hasValidNumericStorage_(self))
		return 0;

	UARRAY_FOREACH(self, i, v, product *= (double)v);
	return product;
}

double UArray_arithmeticMeanAsDouble(const UArray *self)
{
	if (!UArray_hasValidNumericStorage_(self) || self->size == 0)
		return 0;

	return UArray_sumAsDouble(self) / (double)self->size;
}

double UArray_arithmeticMeanSquareAsDouble(const UArray *self)
{
	double meanSquare = 0;

	if (!UArray_hasValidNumericStorage_(self) || self->size == 0)
		return 0;

	UARRAY_FOREACH(self, i, v, meanSquare += (double)v * (double)v);
	return meanSquare / (double)self->size;
}

double UArray_maxAsDouble(const UArray *self)
{
    size_t i;
    double maximum;

    if (!UArray_hasValidNumericStorage_(self) || self->size == 0)
        return 0;

    maximum = UArray_rawDoubleAt_(self, 0);
    for (i = 1; i < self->size; ++i) {
        double value = UArray_rawDoubleAt_(self, i);
        if (value > maximum)
            maximum = value;
    }
    return maximum;
}

double UArray_minAsDouble(const UArray *self)
{
    size_t i;
    double minimum;

    if (!UArray_hasValidNumericStorage_(self) || self->size == 0)
        return 0;

    minimum = UArray_rawDoubleAt_(self, 0);
    for (i = 1; i < self->size; ++i) {
        double value = UArray_rawDoubleAt_(self, i);
        if (value < minimum)
            minimum = value;
    }
    return minimum;
}

BASEKIT_API void UArray_Max(UArray *self, const UArray *other)
{
	size_t i, minSize;

	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other) ||
		self->itemType == CTYPE_uintptr_t ||
		other->itemType == CTYPE_uintptr_t)
		return;

	minSize = self->size < other->size ? self->size : other->size;
	if (minSize == 0)
		return;

	for (i = 0; i < minSize; ++i)
	{
		double v1 = UArray_rawDoubleAt_(self, i);
		double v2 = UArray_rawDoubleAt_(other, i);
		double m = v1 > v2 ? v1 : v2;
		UArray_at_putDouble_(self, i, m);
	}

	/* The setter's type-dispatch macro returns before its hash invalidation. */
	UArray_changed(self);
}

BASEKIT_API void UArray_Min(UArray *self, const UArray *other)
{
	size_t i, minSize;

	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other) ||
		self->itemType == CTYPE_uintptr_t ||
		other->itemType == CTYPE_uintptr_t)
		return;

	minSize = self->size < other->size ? self->size : other->size;
	if (minSize == 0)
		return;

	for (i = 0; i < minSize; ++i)
	{
		double v1 = UArray_rawDoubleAt_(self, i);
		double v2 = UArray_rawDoubleAt_(other, i);
		double m = v1 < v2 ? v1 : v2;
		UArray_at_putDouble_(self, i, m);
	}

	/* The setter's type-dispatch macro returns before its hash invalidation. */
	UArray_changed(self);
}


void UArray_normalize(UArray *self)
{
	size_t i;
	double scale = 0.0;
	double sum = 0.0;
	double inverseNorm;

	if (!UArray_hasValidNumericStorage_(self) ||
		self->itemType == CTYPE_uintptr_t)
		return;

	for (i = 0; i < self->size; ++i)
	{
		double value = UArray_rawDoubleAt_(self, i);
		double magnitude;

		if (!isfinite(value))
			return;
		magnitude = fabs(value);
		if (magnitude > scale)
			scale = magnitude;
	}

	if (scale == 0.0)
		return;

	for (i = 0; i < self->size; ++i)
	{
		double scaledValue = UArray_rawDoubleAt_(self, i) / scale;
		sum += scaledValue * scaledValue;
	}

	if (!(sum > 0.0) || !isfinite(sum))
		return;

	inverseNorm = 1.0 / sqrt(sum);
	UARRAY_FOREACHASSIGN(self, i, v, ((double)v / scale) * inverseNorm);
	UArray_changed(self);
}

void UArray_crossProduct_(UArray *self, const UArray *other)
{
	if (!UArray_hasValidNumericStorage_(self) ||
		!UArray_hasValidNumericStorage_(other) ||
		self->itemType != CTYPE_float32_t ||
		other->itemType != CTYPE_float32_t ||
		self->size < 3 || other->size < 3)
		return;

	float32_t *a = (float32_t *)self->data;
	float32_t *b = (float32_t *)other->data;

	float32_t i = (a[1]*b[2]) - (a[2]*b[1]);
	float32_t j = (a[2]*b[0]) - (a[0]*b[2]);
	float32_t k = (a[0]*b[1]) - (a[1]*b[0]);

	a[0] = i;
	a[1] = j;
	a[2] = k;

	UArray_changed(self);
}


static int UArray_hasValidDistanceVector_(const UArray *self)
{
	size_t expectedItemSize, expectedItemAlignment;

	if (NULL == self)
		return 0;
	if (CTYPE_float32_t == self->itemType)
		expectedItemSize = sizeof(float32_t);
	else if (CTYPE_float64_t == self->itemType)
		expectedItemSize = sizeof(float64_t);
	else
		return 0;
	expectedItemAlignment = UArray_numericItemAlignment_(self->itemType);

	if (self->itemSize != expectedItemSize ||
		expectedItemAlignment == 0 ||
		self->size > (size_t)-1 / expectedItemSize ||
		(self->size > 0 && NULL == self->data) ||
		(self->data &&
			((uintptr_t)self->data % expectedItemAlignment) != 0))
		return 0;
	return 1;
}

double UArray_distanceTo_(const UArray *self, const UArray *other)
{
	size_t i;
	double sum = 0;

	if (!UArray_hasValidDistanceVector_(self) ||
		!UArray_hasValidDistanceVector_(other) ||
		self->itemType != other->itemType ||
		self->size != other->size)
		return 0;

	if (self->itemType == CTYPE_float32_t)
	{
		const float32_t *a = (const float32_t *)self->data;
		const float32_t *b = (const float32_t *)other->data;

		for (i = 0; i < self->size; ++i)
		{
			float32_t d = a[i] - b[i];
			sum = hypot(sum, (double)d);
		}
		return sum;
	}

	if (self->itemType == CTYPE_float64_t)
	{
		const float64_t *a = (const float64_t *)self->data;
		const float64_t *b = (const float64_t *)other->data;

		for (i = 0; i < self->size; ++i)
		{
			float64_t d = a[i] - b[i];
			sum = hypot(sum, (double)d);
		}
		return sum;
	}

	return 0;
}

// hash

void UArray_changed(UArray *self)
{
	self->hash = 0;
}

uintptr_t UArray_calcHash(UArray *self)
{
	uintptr_t h = 5381;

	int i, max = UArray_sizeInBytes(self);
	uint8_t *data = self->data;

	for(i = 0; i < max; i ++)
	{
		h += (h << 5);
		h ^= data[i];
	}

	//printf("UArray_%p %i %s\n", self, h, (char *)self->data);

/*
	// I *think* this should hash to the same value for ASCII, UTF16 and UTF32 types, but not UTF8

	UARRAY_FOREACH(self, i, v,
		h += (h << 5);
		h ^= (uintptr_t)v;
	);
*/

	return h;
}

uintptr_t UArray_hash(UArray *self)
{
	if (!self->hash)
	{
		self->hash = UArray_calcHash(self);
		if(self->hash == 0x0) self->hash = 0x1;
	}

	return self->hash;
}

int UArray_equalsWithHashCheck_(UArray *self, UArray *other)
{
	if (self == other)
	{
		return 1;
	}
	else
	{
		uintptr_t h1 = UArray_hash(self);
		uintptr_t h2 = UArray_hash(other);

		if (h1 != h2)
		{
			return 0;
		}

		/*
		if(strcmp(self->data, other->data) != 0)
		{
			printf("[%s] %i == %i [%s]\n", self->data, h1, h2, other->data);
		}
		*/
	}

	return UArray_equals_(self, other);
}

// indexes

BASEKIT_API void UArray_duplicateIndexes(UArray *self)
{
	size_t size, doubledSize, itemSize, di, si;
	uint8_t *b;

	if (NULL == self)
		return;
	size = self->size;
	itemSize = self->itemSize;
	if (size == 0 || itemSize == 0 || NULL == self->data ||
		size > ((size_t)-1 - 1) / itemSize ||
		size > (size_t)-1 / 2)
		return;

	doubledSize = size * 2;
	if (doubledSize > ((size_t)-1 - 1) / itemSize)
		return;

	UArray_setSize_(self, doubledSize);
	if (self->size != doubledSize || NULL == self->data)
		return;

	b = self->data;
	si = size - 1;
	di = doubledSize - 1;
	for (;;) {
		uint8_t *src = b + si * itemSize;
		uint8_t *dest = b + di * itemSize;

		memcpy(dest, src, itemSize);
		memcpy(dest - itemSize, src, itemSize);
		if (si == 0)
			break;
		di -= 2;
		--si;
	}
}

void UArray_removeOddIndexes(UArray *self)
{
	size_t byteCount;
	size_t itemSize, di = 1, si = 2, max;
	uint8_t *b;

	if (!UArray_hasValidBitStorage_(self, &byteCount))
		return;

	itemSize = self->itemSize;
	max = byteCount / itemSize;
	if (max <= 1)
		return;

	b = self->data;
	while (si < max)
	{
		uint8_t *src = b + (si * itemSize);
		uint8_t *dest = b + (di * itemSize);
		memcpy(dest, src, itemSize);
		si += 2;
		++di;
	}

	/* Compaction changes bytes even if the following shrink allocation fails. */
	UArray_changed(self);
	UArray_setSize_(self, di);
}

void UArray_removeEvenIndexes(UArray *self)
{
	size_t byteCount;
	size_t itemSize, di = 0, si = 1, max;
	uint8_t *b;

	if (!UArray_hasValidBitStorage_(self, &byteCount))
		return;

	itemSize = self->itemSize;
	max = byteCount / itemSize;
	if (max == 0)
		return;

	b = self->data;
	while (si < max)
	{
		uint8_t *src = b + (si * itemSize);
		uint8_t *dest = b + (di * itemSize);
		memcpy(dest, src, itemSize);
		si += 2;
		++di;
	}

	/* Compaction changes bytes even if the following shrink allocation fails. */
	UArray_changed(self);
	UArray_setSize_(self, di);
}

void UArray_reverseItemByteOrders(UArray *self)
{
	size_t itemSize, i;

	if (NULL == self || self->itemSize <= 1 || self->size == 0 ||
		NULL == self->data ||
		self->size > (size_t)-1 / self->itemSize)
		return;

	itemSize = self->itemSize;
	for (i = 0; i < self->size; ++i) {
		uint8_t *item = self->data + i * itemSize;
		size_t j;

		for (j = 0; j < itemSize / 2; ++j) {
			uint8_t value = item[j];
			item[j] = item[itemSize - 1 - j];
			item[itemSize - 1 - j] = value;
		}
	}

	UArray_changed(self);
}
