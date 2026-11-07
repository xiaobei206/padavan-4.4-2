#include "UArray.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

static int fail(const char *s) { fprintf(stderr, "%s\n", s); return EXIT_FAILURE; }
static int nearEnough(double a, double b) { return fabs(a - b) <= 1e-12; }

typedef void (*ScalarDoubleOperation)(UArray *, double);

static int expect_scalar_integer_range_rejection(
    const int8_t initial[2], double operand, ScalarDoubleOperation operation)
{
    UArray *values = UArray_newWithData_type_size_copy_(
        (void *)initial, CTYPE_int8_t, 2, 1);
    const uintptr_t original_hash = (uintptr_t)0x5a5a;
    if (NULL == values)
        return fail("UArray scalar integer arithmetic fixture allocation failed");
    values->hash = original_hash;
    operation(values, operand);
    if (memcmp(values->data, initial, 2 * sizeof(initial[0])) != 0 ||
        values->hash != original_hash) {
        UArray_free(values);
        return fail("UArray scalar integer arithmetic committed an out-of-range result");
    }
    UArray_free(values);
    return EXIT_SUCCESS;
}

static int test_scalar_mutations(void)
{
    const int32_t initial[] = {2, -4, 6};
    const int32_t expectedAdd[] = {4, -2, 8};
    const int32_t expectedSubtract[] = {3, -3, 7};
    const int32_t expectedMultiply[] = {9, -9, 21};
    const int32_t expectedDivide[] = {3, -3, 7};
    UArray *values = UArray_newWithData_type_size_copy_((void *)initial, CTYPE_int32_t, 3, 1);
    UArray *empty = UArray_new();
    UArray invalid;
    int result = EXIT_FAILURE;

    if (NULL == values || NULL == empty) {
        fail("UArray scalar mutation allocation failed");
        goto cleanup;
    }

    values->hash = 0x1111;
    UArray_addScalarDouble_(values, 2.0);
    if (memcmp(values->data, expectedAdd, sizeof(expectedAdd)) != 0 || values->hash != 0) {
        fail("UArray scalar add changed values or retained the cached hash");
        goto cleanup;
    }

    values->hash = 0x2222;
    UArray_subtractScalarDouble_(values, 1.0);
    if (memcmp(values->data, expectedSubtract, sizeof(expectedSubtract)) != 0 || values->hash != 0) {
        fail("UArray scalar subtract changed values or retained the cached hash");
        goto cleanup;
    }

    values->hash = 0x3333;
    UArray_multiplyScalarDouble_(values, 3.0);
    if (memcmp(values->data, expectedMultiply, sizeof(expectedMultiply)) != 0 || values->hash != 0) {
        fail("UArray scalar multiply changed values or retained the cached hash");
        goto cleanup;
    }

    values->hash = 0x4444;
    UArray_divideScalarDouble_(values, 3.0);
    if (memcmp(values->data, expectedDivide, sizeof(expectedDivide)) != 0 || values->hash != 0) {
        fail("UArray scalar divide changed values or retained the cached hash");
        goto cleanup;
    }

    values->hash = 0x5555;
    UArray_divideScalarDouble_(values, 0.0);
    if (memcmp(values->data, expectedDivide, sizeof(expectedDivide)) != 0 || values->hash != 0x5555) {
        fail("UArray scalar divide-by-zero was not a no-op");
        goto cleanup;
    }

    {
        const int8_t boundaryValues[] = {4, 64};
        UArray *boundary = UArray_newWithData_type_size_copy_(
            (void *)boundaryValues, CTYPE_int8_t, 2, 1);
        if (NULL == boundary) {
            fail("UArray scalar integer boundary fixture allocation failed");
            goto cleanup;
        }
        boundary->hash = (uintptr_t)0x5a5a;
        UArray_divideScalarDouble_(boundary, 0.5);
        if (memcmp(boundary->data, boundaryValues, sizeof(boundaryValues)) != 0 ||
            boundary->hash != (uintptr_t)0x5a5a) {
            UArray_free(boundary);
            fail("UArray scalar integer division mutated the receiver before rejecting an out-of-range quotient");
            goto cleanup;
        }
        UArray_free(boundary);
    }

    {
        const int16_t finiteValues[] = {12, -8};
        UArray *nonFinite = UArray_newWithData_type_size_copy_(
            (void *)finiteValues, CTYPE_int16_t, 2, 1);
        if (NULL == nonFinite) {
            fail("UArray scalar non-finite fixture allocation failed");
            goto cleanup;
        }
        nonFinite->hash = (uintptr_t)0x6b6b;
        UArray_divideScalarDouble_(nonFinite, NAN);
        if (memcmp(nonFinite->data, finiteValues, sizeof(finiteValues)) != 0 ||
            nonFinite->hash != (uintptr_t)0x6b6b) {
            UArray_free(nonFinite);
            fail("UArray scalar integer division accepted a non-finite quotient");
            goto cleanup;
        }
        UArray_free(nonFinite);
    }

    {
        const int8_t addOverflow[] = {INT8_MAX, -5};
        const int8_t subtractOverflow[] = {INT8_MIN, 5};
        const int8_t multiplyOverflow[] = {64, -10};
        const int8_t finiteValues[] = {1, 2};
        if (expect_scalar_integer_range_rejection(addOverflow, 1.0, UArray_addScalarDouble_) ||
            expect_scalar_integer_range_rejection(subtractOverflow, 1.0, UArray_subtractScalarDouble_) ||
            expect_scalar_integer_range_rejection(multiplyOverflow, 2.0, UArray_multiplyScalarDouble_) ||
            expect_scalar_integer_range_rejection(finiteValues, NAN, UArray_addScalarDouble_))
            goto cleanup;
    }
    UArray_addScalarDouble_(NULL, 1.0);
    UArray_subtractScalarDouble_(NULL, 1.0);
    UArray_multiplyScalarDouble_(NULL, 1.0);
    UArray_divideScalarDouble_(NULL, 1.0);

    empty->hash = 0x6666;
    UArray_addScalarDouble_(empty, 1.0);
    UArray_subtractScalarDouble_(empty, 1.0);
    UArray_multiplyScalarDouble_(empty, 1.0);
    UArray_divideScalarDouble_(empty, 1.0);
    if (empty->size != 0 || empty->hash != 0x6666) {
        fail("UArray empty scalar operations changed receiver state");
        goto cleanup;
    }

    invalid = *values;
    invalid.data = NULL;
    invalid.hash = 0x7777;
    UArray_addScalarDouble_(&invalid, 1.0);
    UArray_subtractScalarDouble_(&invalid, 1.0);
    UArray_multiplyScalarDouble_(&invalid, 2.0);
    UArray_divideScalarDouble_(&invalid, 2.0);
    if (invalid.data != NULL || invalid.hash != 0x7777) {
        fail("UArray scalar operations accepted missing backing storage");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemSize = 0;
    invalid.hash = 0x8888;
    UArray_addScalarDouble_(&invalid, 1.0);
    UArray_subtractScalarDouble_(&invalid, 1.0);
    UArray_multiplyScalarDouble_(&invalid, 2.0);
    UArray_divideScalarDouble_(&invalid, 2.0);
    if (invalid.itemSize != 0 || invalid.hash != 0x8888) {
        fail("UArray scalar operations accepted mismatched item width");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemType = (CTYPE)9999;
    invalid.hash = 0x9999;
    UArray_addScalarDouble_(&invalid, 1.0);
    UArray_subtractScalarDouble_(&invalid, 1.0);
    UArray_multiplyScalarDouble_(&invalid, 2.0);
    UArray_divideScalarDouble_(&invalid, 2.0);
    if (invalid.itemType != (CTYPE)9999 || invalid.hash != 0x9999) {
        fail("UArray scalar operations accepted unsupported item type");
        goto cleanup;
    }

    invalid = *values;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0xaaaa;
    UArray_addScalarDouble_(&invalid, 1.0);
    UArray_subtractScalarDouble_(&invalid, 1.0);
    UArray_multiplyScalarDouble_(&invalid, 2.0);
    UArray_divideScalarDouble_(&invalid, 2.0);
    if (invalid.hash != 0xaaaa) {
        fail("UArray scalar operations accepted overflowing storage geometry");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemType = CTYPE_uintptr_t;
    invalid.itemSize = sizeof(uintptr_t);
    invalid.hash = 0xbbbb;
    UArray_addScalarDouble_(&invalid, 1.0);
    UArray_subtractScalarDouble_(&invalid, 1.0);
    UArray_multiplyScalarDouble_(&invalid, 2.0);
    UArray_divideScalarDouble_(&invalid, 2.0);
    if (invalid.hash != 0xbbbb) {
        fail("UArray scalar operations accepted pointer-sized item type");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(values);
    UArray_free(empty);
    return result;
}

static int test_indexed_double_writes(void)
{
    const int8_t signedInitial = 7;
    const uint8_t unsignedInitial = 9;
    const int64_t wideSignedInitial = 11;
    const uint64_t wideUnsignedInitial = 13;
    const float32_t floatInitial = 1.5f;
    UArray *signedValues = UArray_newWithData_type_size_copy_(
        (void *)&signedInitial, CTYPE_int8_t, 1, 1);
    UArray *unsignedValues = UArray_newWithData_type_size_copy_(
        (void *)&unsignedInitial, CTYPE_uint8_t, 1, 1);
    UArray *wideSigned = UArray_newWithData_type_size_copy_(
        (void *)&wideSignedInitial, CTYPE_int64_t, 1, 1);
    UArray *wideUnsigned = UArray_newWithData_type_size_copy_(
        (void *)&wideUnsignedInitial, CTYPE_uint64_t, 1, 1);
    UArray *floatValues = UArray_newWithData_type_size_copy_(
        (void *)&floatInitial, CTYPE_float32_t, 1, 1);
    UArray invalid;
    int8_t original;
    int result = EXIT_FAILURE;

    if (!signedValues || !unsignedValues || !wideSigned ||
        !wideUnsigned || !floatValues) {
        fail("UArray indexed double-write allocation failed");
        goto cleanup;
    }

    signedValues->hash = (uintptr_t)0x101;
    UArray_at_putDouble_(signedValues, 0, NAN);
    UArray_at_putDouble_(signedValues, 0, INFINITY);
    UArray_at_putDouble_(signedValues, 0, -INFINITY);
    UArray_at_putDouble_(signedValues, 0, 128.0);
    UArray_at_putDouble_(signedValues, 0, -129.0);
    UArray_appendDouble_(signedValues, DBL_MAX);
    if (signedValues->size != 1 ||
        ((int8_t *)signedValues->data)[0] != signedInitial ||
        signedValues->hash != (uintptr_t)0x101) {
        fail("UArray signed scalar write accepted a non-finite or out-of-range value");
        goto cleanup;
    }

    signedValues->hash = (uintptr_t)0x111;
    UArray_at_putDouble_(signedValues, 0, 127.75);
    if (((int8_t *)signedValues->data)[0] != INT8_MAX ||
        signedValues->hash != 0) {
        fail("UArray in-place indexed double write did not invalidate the cached hash");
        goto cleanup;
    }
    UArray_at_putDouble_(signedValues, 2, -128.75);
    if (signedValues->size != 3 ||
        ((int8_t *)signedValues->data)[0] != INT8_MAX ||
        ((int8_t *)signedValues->data)[1] != 0 ||
        ((int8_t *)signedValues->data)[2] != INT8_MIN ||
        signedValues->hash != 0) {
        fail("UArray signed scalar write changed truncation or growth semantics");
        goto cleanup;
    }

    unsignedValues->hash = (uintptr_t)0x202;
    UArray_at_putDouble_(unsignedValues, 0, -1.0);
    UArray_at_putDouble_(unsignedValues, 0, 256.0);
    UArray_appendDouble_(unsignedValues, INFINITY);
    if (unsignedValues->size != 1 ||
        ((uint8_t *)unsignedValues->data)[0] != unsignedInitial ||
        unsignedValues->hash != (uintptr_t)0x202) {
        fail("UArray unsigned scalar write accepted a non-finite or out-of-range value");
        goto cleanup;
    }
    UArray_at_putDouble_(unsignedValues, 0, -0.75);
    UArray_at_putDouble_(unsignedValues, 2, 255.75);
    if (unsignedValues->size != 3 ||
        ((uint8_t *)unsignedValues->data)[0] != 0 ||
        ((uint8_t *)unsignedValues->data)[1] != 0 ||
        ((uint8_t *)unsignedValues->data)[2] != UINT8_MAX) {
        fail("UArray unsigned scalar write changed truncation or growth semantics");
        goto cleanup;
    }

    wideSigned->hash = (uintptr_t)0x303;
    UArray_at_putDouble_(wideSigned, 0, ldexp(1.0, 63));
    if (((int64_t *)wideSigned->data)[0] != wideSignedInitial ||
        wideSigned->hash != (uintptr_t)0x303) {
        fail("UArray int64 scalar write accepted the exclusive upper boundary");
        goto cleanup;
    }
    UArray_at_putDouble_(wideSigned, 0, -ldexp(1.0, 63));
    if (((int64_t *)wideSigned->data)[0] != INT64_MIN ||
        wideSigned->hash != 0) {
        fail("UArray int64 scalar write rejected its inclusive lower boundary");
        goto cleanup;
    }

    wideUnsigned->hash = (uintptr_t)0x404;
    UArray_at_putDouble_(wideUnsigned, 0, ldexp(1.0, 64));
    if (((uint64_t *)wideUnsigned->data)[0] != wideUnsignedInitial ||
        wideUnsigned->hash != (uintptr_t)0x404) {
        fail("UArray uint64 scalar write accepted the exclusive upper boundary");
        goto cleanup;
    }
    UArray_at_putDouble_(wideUnsigned, 0, ldexp(1.0, 64) - 2048.0);
    if (((uint64_t *)wideUnsigned->data)[0] != UINT64_MAX - UINT64_C(2047) ||
        wideUnsigned->hash != 0) {
        fail("UArray uint64 scalar write rejected the largest exactly representable in-range value");
        goto cleanup;
    }

    UArray_at_putDouble_(floatValues, 0, INFINITY);
    if (!isinf(((float32_t *)floatValues->data)[0])) {
        fail("UArray floating scalar writes no longer preserve non-finite values");
        goto cleanup;
    }

    signedValues->hash = (uintptr_t)0x113;
    UArray_at_putLong_(signedValues, 1, 42);
    if (((int8_t *)signedValues->data)[1] != 42 ||
        signedValues->hash != 0) {
        fail("UArray in-place indexed long write did not invalidate the cached hash");
        goto cleanup;
    }

    invalid = *signedValues;
    invalid.itemSize = 0;
    invalid.hash = (uintptr_t)0x505;
    original = ((int8_t *)invalid.data)[0];
    UArray_at_putDouble_(&invalid, 0, 1.0);
    UArray_at_putLong_(&invalid, 0, 1);
    if (invalid.hash != (uintptr_t)0x505 ||
        ((int8_t *)invalid.data)[0] != original) {
        fail("UArray scalar indexed writes accepted malformed item geometry");
        goto cleanup;
    }

    invalid = *signedValues;
    invalid.itemType = (CTYPE)9999;
    invalid.hash = (uintptr_t)0x606;
    original = ((int8_t *)invalid.data)[0];
    UArray_at_putDouble_(&invalid, 0, 1.0);
    UArray_at_putLong_(&invalid, 0, 1);
    if (invalid.hash != (uintptr_t)0x606 ||
        ((int8_t *)invalid.data)[0] != original) {
        fail("UArray scalar indexed writes accepted an unsupported item type");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(signedValues);
    UArray_free(unsignedValues);
    UArray_free(wideSigned);
    UArray_free(wideUnsigned);
    UArray_free(floatValues);
    return result;
}


static int test_elementwise_mutations(void)
{
    const int32_t initial[] = {2, 3, 4};
    const int16_t factors[] = {5, 7};
    const int32_t expectedAdd[] = {7, 10, 4};
    const int32_t expectedSubtract[] = {-3, -4, 4};
    const int32_t expectedMultiply[] = {10, 21, 4};
    const float32_t floatInitial[] = {1.5f, -2.0f};
    const float32_t floatOther[] = {0.5f, 4.0f};
    UArray *left = UArray_newWithData_type_size_copy_((void *)initial, CTYPE_int32_t, 3, 1);
    UArray *right = UArray_newWithData_type_size_copy_((void *)factors, CTYPE_int16_t, 2, 1);
    UArray *floatLeft = UArray_newWithData_type_size_copy_((void *)floatInitial, CTYPE_float32_t, 2, 1);
    UArray *floatRight = UArray_newWithData_type_size_copy_((void *)floatOther, CTYPE_float32_t, 2, 1);
    UArray invalid;
    int result = EXIT_FAILURE;

    if (!left || !right || !floatLeft || !floatRight) {
        fail("UArray elementwise mutation allocation failed");
        goto cleanup;
    }

    left->hash = 0x1010;
    UArray_add_(left, right);
    if (memcmp(left->data, expectedAdd, sizeof(expectedAdd)) != 0 || left->hash != 0) {
        fail("UArray elementwise add changed prefix behavior or retained the cached hash");
        goto cleanup;
    }

    memcpy(left->data, initial, sizeof(initial));
    left->hash = 0x2020;
    UArray_subtract_(left, right);
    if (memcmp(left->data, expectedSubtract, sizeof(expectedSubtract)) != 0 || left->hash != 0) {
        fail("UArray elementwise subtract changed prefix behavior or retained the cached hash");
        goto cleanup;
    }

    memcpy(left->data, initial, sizeof(initial));
    left->hash = 0x3030;
    UArray_multiply_(left, right);
    if (memcmp(left->data, expectedMultiply, sizeof(expectedMultiply)) != 0 || left->hash != 0) {
        fail("UArray elementwise multiply changed prefix behavior or retained the cached hash");
        goto cleanup;
    }

    floatLeft->hash = 0x4040;
    UArray_add_(floatLeft, floatRight);
    if (!nearEnough(((float32_t *)floatLeft->data)[0], 2.0) ||
        !nearEnough(((float32_t *)floatLeft->data)[1], 2.0) ||
        floatLeft->hash != 0) {
        fail("UArray float32 fast-path add changed results or retained the cached hash");
        goto cleanup;
    }

    UArray_add_(NULL, right);
    UArray_subtract_(left, NULL);
    UArray_multiply_(NULL, NULL);

    invalid = *left;
    invalid.data = NULL;
    invalid.hash = 0x5050;
    UArray_add_(&invalid, right);
    UArray_subtract_(&invalid, right);
    UArray_multiply_(&invalid, right);
    if (invalid.data != NULL || invalid.hash != 0x5050) {
        fail("UArray elementwise operations accepted missing receiver storage");
        goto cleanup;
    }

    invalid = *right;
    invalid.data = NULL;
    left->hash = 0x6060;
    UArray_add_(left, &invalid);
    UArray_subtract_(left, &invalid);
    UArray_multiply_(left, &invalid);
    if (left->hash != 0x6060 ||
        memcmp(left->data, expectedMultiply, sizeof(expectedMultiply)) != 0) {
        fail("UArray elementwise operations accepted missing operand storage");
        goto cleanup;
    }

    invalid = *right;
    invalid.itemSize = 0;
    left->hash = 0x7070;
    UArray_add_(left, &invalid);
    UArray_subtract_(left, &invalid);
    UArray_multiply_(left, &invalid);
    if (left->hash != 0x7070 ||
        memcmp(left->data, expectedMultiply, sizeof(expectedMultiply)) != 0) {
        fail("UArray elementwise operations accepted mismatched item width");
        goto cleanup;
    }

    invalid = *right;
    invalid.itemType = (CTYPE)9999;
    left->hash = 0x8080;
    UArray_add_(left, &invalid);
    UArray_subtract_(left, &invalid);
    UArray_multiply_(left, &invalid);
    if (left->hash != 0x8080 ||
        memcmp(left->data, expectedMultiply, sizeof(expectedMultiply)) != 0) {
        fail("UArray elementwise operations accepted unsupported item type");
        goto cleanup;
    }

    invalid = *right;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    left->hash = 0x9090;
    UArray_add_(left, &invalid);
    UArray_subtract_(left, &invalid);
    UArray_multiply_(left, &invalid);
    if (left->hash != 0x9090 ||
        memcmp(left->data, expectedMultiply, sizeof(expectedMultiply)) != 0) {
        fail("UArray elementwise operations accepted overflowing storage geometry");
        goto cleanup;
    }

    invalid = *right;
    invalid.size = 0;
    left->hash = 0xa0a0;
    UArray_add_(left, &invalid);
    UArray_subtract_(left, &invalid);
    UArray_multiply_(left, &invalid);
    if (left->hash != 0xa0a0 ||
        memcmp(left->data, expectedMultiply, sizeof(expectedMultiply)) != 0) {
        fail("UArray zero-length elementwise operation changed receiver state");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(left);
    UArray_free(right);
    UArray_free(floatLeft);
    UArray_free(floatRight);
    return result;
}

static int test_reductions(void)
{
    const int32_t positiveValues[] = {5, 12, 8};
    const int32_t negativeValues[] = {-11, -4, -19};
    UArray *positive = UArray_newWithData_type_size_copy_((void *)positiveValues, CTYPE_int32_t, 3, 1);
    UArray *negative = UArray_newWithData_type_size_copy_((void *)negativeValues, CTYPE_int32_t, 3, 1);
    UArray *empty = UArray_new();
    int result = EXIT_FAILURE;

    if (!positive || !negative || !empty) {
        fail("UArray reduction allocation failed");
        goto cleanup;
    }

    positive->hash = 0x1357;
    if (UArray_sumAsDouble(positive) != 25.0 ||
        UArray_productAsDouble(positive) != 480.0 ||
        !nearEnough(UArray_arithmeticMeanAsDouble(positive), 25.0 / 3.0) ||
        !nearEnough(UArray_arithmeticMeanSquareAsDouble(positive), 233.0 / 3.0) ||
        positive->hash != 0x1357) {
        fail("UArray valid reductions changed their results or mutated the source hash");
        goto cleanup;
    }

    if (UArray_sumAsDouble(negative) != -34.0 ||
        UArray_productAsDouble(negative) != -836.0 ||
        !nearEnough(UArray_arithmeticMeanAsDouble(negative), -34.0 / 3.0) ||
        !nearEnough(UArray_arithmeticMeanSquareAsDouble(negative), 498.0 / 3.0)) {
        fail("UArray negative reductions changed their results");
        goto cleanup;
    }

    if (UArray_sumAsDouble(empty) != 0.0 ||
        UArray_productAsDouble(empty) != 1.0 ||
        UArray_arithmeticMeanAsDouble(empty) != 0.0 ||
        UArray_arithmeticMeanSquareAsDouble(empty) != 0.0 ||
        UArray_sumAsDouble(NULL) != 0.0 ||
        UArray_productAsDouble(NULL) != 0.0 ||
        UArray_arithmeticMeanAsDouble(NULL) != 0.0 ||
        UArray_arithmeticMeanSquareAsDouble(NULL) != 0.0) {
        fail("UArray empty or NULL reduction behavior is invalid");
        goto cleanup;
    }

    {
        uint8_t *originalData = positive->data;
        positive->data = NULL;
        if (UArray_sumAsDouble(positive) != 0.0 ||
            UArray_productAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanSquareAsDouble(positive) != 0.0) {
            positive->data = originalData;
            fail("UArray reductions accepted missing backing storage");
            goto cleanup;
        }
        positive->data = originalData;
    }

    {
        size_t originalItemSize = positive->itemSize;
        positive->itemSize = 0;
        if (UArray_sumAsDouble(positive) != 0.0 ||
            UArray_productAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanSquareAsDouble(positive) != 0.0) {
            positive->itemSize = originalItemSize;
            fail("UArray reductions accepted mismatched item width");
            goto cleanup;
        }
        positive->itemSize = originalItemSize;
    }

    {
        size_t originalSize = positive->size;
        positive->size = (size_t)-1 / positive->itemSize + 1;
        if (UArray_sumAsDouble(positive) != 0.0 ||
            UArray_productAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanSquareAsDouble(positive) != 0.0) {
            positive->size = originalSize;
            fail("UArray reductions accepted overflowing storage geometry");
            goto cleanup;
        }
        positive->size = originalSize;
    }

    {
        CTYPE originalItemType = positive->itemType;
        positive->itemType = (CTYPE)9999;
        if (UArray_sumAsDouble(positive) != 0.0 ||
            UArray_productAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanAsDouble(positive) != 0.0 ||
            UArray_arithmeticMeanSquareAsDouble(positive) != 0.0) {
            positive->itemType = originalItemType;
            fail("UArray reductions accepted unsupported item type");
            goto cleanup;
        }
        positive->itemType = originalItemType;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(positive);
    UArray_free(negative);
    UArray_free(empty);
    return result;
}

static int test_normalize(void)
{
    const double normalValues[] = {3.0, 4.0};
    const double zeroValues[] = {0.0, 0.0};
    const double largeValues[] = {DBL_MAX, DBL_MAX};
    const double tinyValues[] = {DBL_MIN, DBL_MIN};
    const double nonfiniteValues[] = {1.0, INFINITY};
    UArray *normal = UArray_newWithData_type_size_copy_((void *)normalValues, CTYPE_float64_t, 2, 1);
    UArray *zero = UArray_newWithData_type_size_copy_((void *)zeroValues, CTYPE_float64_t, 2, 1);
    UArray *large = UArray_newWithData_type_size_copy_((void *)largeValues, CTYPE_float64_t, 2, 1);
    UArray *tiny = UArray_newWithData_type_size_copy_((void *)tinyValues, CTYPE_float64_t, 2, 1);
    UArray *nonfinite = UArray_newWithData_type_size_copy_((void *)nonfiniteValues, CTYPE_float64_t, 2, 1);
    UArray *empty = UArray_new();
    int result = EXIT_FAILURE;

    if (!normal || !zero || !large || !tiny || !nonfinite || !empty) {
        fail("UArray normalization allocation failed");
        goto cleanup;
    }

    normal->hash = 0x1234;
    UArray_normalize(normal);
    if (!nearEnough(UArray_rawDoubleAt_(normal, 0), 0.6) ||
        !nearEnough(UArray_rawDoubleAt_(normal, 1), 0.8) ||
        normal->hash != 0) {
        fail("UArray normalization failed for the 3-4 vector or retained its cached hash");
        goto cleanup;
    }

    zero->hash = 0x2345;
    UArray_normalize(zero);
    if (UArray_rawDoubleAt_(zero, 0) != 0.0 ||
        UArray_rawDoubleAt_(zero, 1) != 0.0 ||
        zero->hash != 0x2345) {
        fail("UArray normalization changed a zero vector");
        goto cleanup;
    }

    UArray_normalize(large);
    if (!nearEnough(UArray_rawDoubleAt_(large, 0), 0.7071067811865475) ||
        !nearEnough(UArray_rawDoubleAt_(large, 1), 0.7071067811865475)) {
        fail("UArray normalization failed for DBL_MAX values");
        goto cleanup;
    }

    UArray_normalize(tiny);
    if (!nearEnough(UArray_rawDoubleAt_(tiny, 0), 0.7071067811865475) ||
        !nearEnough(UArray_rawDoubleAt_(tiny, 1), 0.7071067811865475)) {
        fail("UArray normalization failed for DBL_MIN values");
        goto cleanup;
    }

    nonfinite->hash = 0x3456;
    UArray_normalize(nonfinite);
    if (UArray_rawDoubleAt_(nonfinite, 0) != 1.0 ||
        !isinf(UArray_rawDoubleAt_(nonfinite, 1)) ||
        nonfinite->hash != 0x3456) {
        fail("UArray normalization modified non-finite input");
        goto cleanup;
    }

    {
        uint8_t *originalData = normal->data;
        normal->hash = 0x4567;
        normal->data = NULL;
        UArray_normalize(normal);
        normal->data = originalData;
        if (normal->hash != 0x4567 ||
            !nearEnough(UArray_rawDoubleAt_(normal, 0), 0.6)) {
            fail("UArray normalization accepted missing backing storage");
            goto cleanup;
        }
    }

    {
        size_t originalItemSize = normal->itemSize;
        normal->hash = 0x5678;
        normal->itemSize = 0;
        UArray_normalize(normal);
        normal->itemSize = originalItemSize;
        if (normal->hash != 0x5678 ||
            !nearEnough(UArray_rawDoubleAt_(normal, 1), 0.8)) {
            fail("UArray normalization accepted mismatched item width");
            goto cleanup;
        }
    }

    {
        size_t originalSize = normal->size;
        normal->hash = 0x6789;
        normal->size = (size_t)-1 / normal->itemSize + 1;
        UArray_normalize(normal);
        normal->size = originalSize;
        if (normal->hash != 0x6789 ||
            !nearEnough(UArray_rawDoubleAt_(normal, 0), 0.6)) {
            fail("UArray normalization accepted overflowing geometry");
            goto cleanup;
        }
    }

    empty->hash = 0x789a;
    UArray_normalize(empty);
    UArray_normalize(NULL);
    if (empty->size != 0 || empty->hash != 0x789a) {
        fail("UArray normalization changed an empty array");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(normal);
    UArray_free(zero);
    UArray_free(large);
    UArray_free(tiny);
    UArray_free(nonfinite);
    UArray_free(empty);
    return result;
}



static int test_elementwise_division(void)
{
    const int32_t integerNumerators[] = {12, -15, 8};
    const int16_t integerDivisors[] = {3, 5};
    const int32_t integerExpected[] = {4, -3, 8};
    const float32_t float32Numerators[] = {6.0f, -8.0f};
    const float32_t float32Divisors[] = {2.0f, 4.0f};
    const float32_t float32Expected[] = {3.0f, -2.0f};
    const double float64Numerators[] = {9.0, 16.0};
    const double float64Divisors[] = {3.0, 4.0};
    const int32_t atomicNumerators[] = {12, 15};
    const int32_t zeroDivisors[] = {3, 0};
    const int32_t signedMinimum[] = {INT32_MIN};
    const int32_t negativeOne[] = {-1};
    const float32_t ieeeNumerators[] = {1.0f, 0.0f};
    const float32_t ieeeZeroDivisors[] = {0.0f, 0.0f};
    UArray *integerLeft = UArray_newWithData_type_size_copy_(
        (void *)integerNumerators, CTYPE_int32_t, 3, 1);
    UArray *integerRight = UArray_newWithData_type_size_copy_(
        (void *)integerDivisors, CTYPE_int16_t, 2, 1);
    UArray *float32Left = UArray_newWithData_type_size_copy_(
        (void *)float32Numerators, CTYPE_float32_t, 2, 1);
    UArray *float32Right = UArray_newWithData_type_size_copy_(
        (void *)float32Divisors, CTYPE_float32_t, 2, 1);
    UArray *float64Left = UArray_newWithData_type_size_copy_(
        (void *)float64Numerators, CTYPE_float64_t, 2, 1);
    UArray *float64Right = UArray_newWithData_type_size_copy_(
        (void *)float64Divisors, CTYPE_float64_t, 2, 1);
    UArray *atomicLeft = UArray_newWithData_type_size_copy_(
        (void *)atomicNumerators, CTYPE_int32_t, 2, 1);
    UArray *zeroRight = UArray_newWithData_type_size_copy_(
        (void *)zeroDivisors, CTYPE_int32_t, 2, 1);
    UArray *minimumLeft = UArray_newWithData_type_size_copy_(
        (void *)signedMinimum, CTYPE_int32_t, 1, 1);
    UArray *negativeOneRight = UArray_newWithData_type_size_copy_(
        (void *)negativeOne, CTYPE_int32_t, 1, 1);
    UArray *ieeeLeft = UArray_newWithData_type_size_copy_(
        (void *)ieeeNumerators, CTYPE_float32_t, 2, 1);
    UArray *ieeeRight = UArray_newWithData_type_size_copy_(
        (void *)ieeeZeroDivisors, CTYPE_float32_t, 2, 1);
    UArray *empty = UArray_new();
    const uintptr_t pointerValues[] = {3, 5};
    UArray *pointerArray = UArray_newWithData_type_size_copy_(
        (void *)pointerValues, CTYPE_uintptr_t, 2, 1);
    UArray invalid;
    int result = EXIT_FAILURE;

    if (!integerLeft || !integerRight || !float32Left || !float32Right ||
        !float64Left || !float64Right || !atomicLeft || !zeroRight ||
        !minimumLeft || !negativeOneRight || !ieeeLeft || !ieeeRight ||
        !empty || !pointerArray) {
        fail("UArray division allocation failed");
        goto cleanup;
    }

    integerLeft->hash = 0x1111;
    UArray_divide_(integerLeft, integerRight);
    if (memcmp(integerLeft->data, integerExpected, sizeof(integerExpected)) != 0 ||
        integerLeft->hash != 0) {
        fail("UArray integer division changed mixed-width prefix results or retained the cached hash");
        goto cleanup;
    }

    float32Left->hash = 0x2222;
    UArray_divide_(float32Left, float32Right);
    if (memcmp(float32Left->data, float32Expected, sizeof(float32Expected)) != 0 ||
        float32Left->hash != 0) {
        fail("UArray float32 division changed results or retained the cached hash");
        goto cleanup;
    }

    float64Left->hash = 0x3333;
    UArray_divide_(float64Left, float64Right);
    if (UArray_rawDoubleAt_(float64Left, 0) != 3.0 ||
        UArray_rawDoubleAt_(float64Left, 1) != 4.0 ||
        float64Left->hash != 0) {
        fail("UArray float64 division changed results or retained the cached hash");
        goto cleanup;
    }

    {
        const int32_t mixedNumerators[] = {12, -15};
        const double mixedDivisors[] = {0.5, 3.0};
        const int32_t mixedExpected[] = {24, -5};
        UArray mixedLeft = UArray_stackAllocedWithData_type_size_(
            (void *)mixedNumerators, CTYPE_int32_t, 2);
        UArray mixedRight = UArray_stackAllocedWithData_type_size_(
            (void *)mixedDivisors, CTYPE_float64_t, 2);

        mixedLeft.hash = 0x7777;
        UArray_divide_(&mixedLeft, &mixedRight);
        if (memcmp(mixedLeft.data, mixedExpected, sizeof(mixedExpected)) != 0 ||
            mixedLeft.hash != 0) {
            fail("UArray integer division changed valid mixed float64 results");
            goto cleanup;
        }
    }

    {
        const int32_t beforeOverflow[] = {12, INT32_MAX};
        const float32_t overflowingDivisors[] = {3.0f, 0.5f};
        const double nanDivisors[] = {NAN};
        const int32_t nanNumerator[] = {1};
        UArray overflowLeft = UArray_stackAllocedWithData_type_size_(
            (void *)beforeOverflow, CTYPE_int32_t, 2);
        UArray overflowRight = UArray_stackAllocedWithData_type_size_(
            (void *)overflowingDivisors, CTYPE_float32_t, 2);
        UArray nanLeft = UArray_stackAllocedWithData_type_size_(
            (void *)nanNumerator, CTYPE_int32_t, 1);
        UArray nanRight = UArray_stackAllocedWithData_type_size_(
            (void *)nanDivisors, CTYPE_float64_t, 1);

        overflowLeft.hash = 0x8888;
        UArray_divide_(&overflowLeft, &overflowRight);
        if (memcmp(overflowLeft.data, beforeOverflow, sizeof(beforeOverflow)) != 0 ||
            overflowLeft.hash != 0x8888) {
            fail("UArray integer division committed a float quotient outside receiver range");
            goto cleanup;
        }

        nanLeft.hash = 0x9999;
        UArray_divide_(&nanLeft, &nanRight);
        if (memcmp(nanLeft.data, nanNumerator, sizeof(nanNumerator)) != 0 ||
            nanLeft.hash != 0x9999) {
            fail("UArray integer division committed a non-finite float quotient");
            goto cleanup;
        }
    }

    atomicLeft->hash = 0x4444;
    UArray_divide_(atomicLeft, zeroRight);
    if (memcmp(atomicLeft->data, atomicNumerators, sizeof(atomicNumerators)) != 0 ||
        atomicLeft->hash != 0x4444) {
        fail("UArray integer division partially mutated around a zero divisor");
        goto cleanup;
    }

    minimumLeft->hash = 0x5555;
    UArray_divide_(minimumLeft, negativeOneRight);
    if (memcmp(minimumLeft->data, signedMinimum, sizeof(signedMinimum)) != 0 ||
        minimumLeft->hash != 0x5555) {
        fail("UArray signed minimum divided by negative one was not rejected");
        goto cleanup;
    }

    ieeeLeft->hash = 0x6666;
    UArray_divide_(ieeeLeft, ieeeRight);
    if (!isinf(((float32_t *)ieeeLeft->data)[0]) ||
        !isnan(((float32_t *)ieeeLeft->data)[1]) ||
        ieeeLeft->hash != 0) {
        fail("UArray floating-point division lost IEEE zero-divisor behavior");
        goto cleanup;
    }

    empty->hash = 0x7777;
    UArray_divide_(empty, integerRight);
    UArray_divide_(NULL, integerRight);
    UArray_divide_(integerLeft, NULL);
    if (empty->size != 0 || empty->hash != 0x7777) {
        fail("UArray empty division changed receiver state");
        goto cleanup;
    }

    invalid = *integerLeft;
    invalid.data = NULL;
    invalid.hash = 0x8888;
    UArray_divide_(&invalid, integerRight);
    if (invalid.data != NULL || invalid.hash != 0x8888) {
        fail("UArray division accepted missing receiver storage");
        goto cleanup;
    }

    invalid = *integerRight;
    invalid.data = NULL;
    integerLeft->hash = 0x9999;
    UArray_divide_(integerLeft, &invalid);
    if (integerLeft->hash != 0x9999 ||
        memcmp(integerLeft->data, integerExpected, sizeof(integerExpected)) != 0) {
        fail("UArray division accepted missing operand storage");
        goto cleanup;
    }

    invalid = *integerLeft;
    invalid.itemSize = 0;
    invalid.hash = 0xaaaa;
    UArray_divide_(&invalid, integerRight);
    if (invalid.hash != 0xaaaa) {
        fail("UArray division accepted malformed item width");
        goto cleanup;
    }

    invalid = *integerLeft;
    invalid.itemType = (CTYPE)9999;
    invalid.hash = 0xbbbb;
    UArray_divide_(&invalid, integerRight);
    if (invalid.hash != 0xbbbb) {
        fail("UArray division accepted unsupported item type");
        goto cleanup;
    }

    invalid = *integerLeft;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0xcccc;
    UArray_divide_(&invalid, integerRight);
    if (invalid.hash != 0xcccc) {
        fail("UArray division accepted overflowing storage geometry");
        goto cleanup;
    }

    pointerArray->hash = 0xdddd;
    integerLeft->hash = 0xeeee;
    UArray_divide_(pointerArray, integerRight);
    UArray_divide_(integerLeft, pointerArray);
    if (pointerArray->hash != 0xdddd || integerLeft->hash != 0xeeee ||
        memcmp(pointerArray->data, pointerValues, sizeof(pointerValues)) != 0 ||
        memcmp(integerLeft->data, integerExpected, sizeof(integerExpected)) != 0) {
        fail("UArray division accepted pointer-sized item type");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(integerLeft);
    UArray_free(integerRight);
    UArray_free(float32Left);
    UArray_free(float32Right);
    UArray_free(float64Left);
    UArray_free(float64Right);
    UArray_free(atomicLeft);
    UArray_free(zeroRight);
    UArray_free(minimumLeft);
    UArray_free(negativeOneRight);
    UArray_free(ieeeLeft);
    UArray_free(ieeeRight);
    UArray_free(empty);
    UArray_free(pointerArray);
    return result;
}


static int test_pairwise_extrema(void)
{
    const int32_t initial[] = {3, -5, 8, 10};
    const double candidates[] = {4.0, -6.0, 2.0};
    const int32_t expectedMax[] = {4, -5, 8, 10};
    const int32_t expectedMin[] = {3, -6, 2, 10};
    const uintptr_t pointerValues[] = {4, 6, 2};
    UArray *values = UArray_newWithData_type_size_copy_(
        (void *)initial, CTYPE_int32_t, 4, 1);
    UArray *other = UArray_newWithData_type_size_copy_(
        (void *)candidates, CTYPE_float64_t, 3, 1);
    UArray *empty = UArray_new();
    UArray *pointerArray = UArray_newWithData_type_size_copy_(
        (void *)pointerValues, CTYPE_uintptr_t, 3, 1);
    UArray invalid;
    int result = EXIT_FAILURE;

    if (NULL == values || NULL == other || NULL == empty || NULL == pointerArray) {
        fail("UArray pairwise extrema allocation failed");
        goto cleanup;
    }

    values->hash = 0x1111;
    UArray_Max(values, other);
    if (memcmp(values->data, expectedMax, sizeof(expectedMax)) != 0 ||
        values->hash != 0) {
        fail("UArray pairwise maximum changed prefix results or retained the cached hash");
        goto cleanup;
    }

    memcpy(values->data, initial, sizeof(initial));
    values->hash = 0x2222;
    UArray_Min(values, other);
    if (memcmp(values->data, expectedMin, sizeof(expectedMin)) != 0 ||
        values->hash != 0) {
        fail("UArray pairwise minimum changed prefix results or retained the cached hash");
        goto cleanup;
    }

    empty->hash = 0x3333;
    UArray_Max(empty, other);
    UArray_Min(empty, other);
    UArray_Max(NULL, other);
    UArray_Min(values, NULL);
    if (empty->size != 0 || empty->hash != 0x3333) {
        fail("UArray pairwise extrema changed empty receiver state");
        goto cleanup;
    }

    invalid = *values;
    invalid.data = NULL;
    invalid.hash = 0x4444;
    UArray_Max(&invalid, other);
    UArray_Min(&invalid, other);
    if (invalid.data != NULL || invalid.hash != 0x4444) {
        fail("UArray pairwise extrema accepted missing receiver storage");
        goto cleanup;
    }

    invalid = *other;
    invalid.data = NULL;
    values->hash = 0x5555;
    UArray_Max(values, &invalid);
    UArray_Min(values, &invalid);
    if (values->hash != 0x5555 ||
        memcmp(values->data, expectedMin, sizeof(expectedMin)) != 0) {
        fail("UArray pairwise extrema accepted missing operand storage");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemSize = 0;
    invalid.hash = 0x6666;
    UArray_Max(&invalid, other);
    UArray_Min(&invalid, other);
    if (invalid.hash != 0x6666) {
        fail("UArray pairwise extrema accepted malformed item width");
        goto cleanup;
    }

    invalid = *other;
    invalid.itemType = (CTYPE)9999;
    values->hash = 0x7777;
    UArray_Max(values, &invalid);
    UArray_Min(values, &invalid);
    if (values->hash != 0x7777) {
        fail("UArray pairwise extrema accepted unsupported operand type");
        goto cleanup;
    }

    invalid = *values;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0x8888;
    UArray_Max(&invalid, other);
    UArray_Min(&invalid, other);
    if (invalid.hash != 0x8888) {
        fail("UArray pairwise extrema accepted overflowing receiver geometry");
        goto cleanup;
    }

    invalid = *other;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    values->hash = 0x9999;
    UArray_Max(values, &invalid);
    UArray_Min(values, &invalid);
    if (values->hash != 0x9999) {
        fail("UArray pairwise extrema accepted overflowing operand geometry");
        goto cleanup;
    }

    pointerArray->hash = 0xaaaa;
    values->hash = 0xbbbb;
    UArray_Max(pointerArray, other);
    UArray_Min(values, pointerArray);
    if (pointerArray->hash != 0xaaaa || values->hash != 0xbbbb ||
        memcmp(pointerArray->data, pointerValues, sizeof(pointerValues)) != 0 ||
        memcmp(values->data, expectedMin, sizeof(expectedMin)) != 0) {
        fail("UArray pairwise extrema accepted pointer-sized item type");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(values);
    UArray_free(other);
    UArray_free(empty);
    UArray_free(pointerArray);
    return result;
}


static int test_logical_mutations(void)
{
    const int32_t initial[] = {0, 2, 0, 5};
    const int16_t operands[] = {0, -3, 4};
    const int32_t expectedOr[] = {0, 1, 1, 5};
    const int32_t expectedAnd[] = {0, 1, 0, 5};
    const uintptr_t pointerOperands[] = {0, 1, 0};
    UArray *values = UArray_newWithData_type_size_copy_(
        (void *)initial, CTYPE_int32_t, 4, 1);
    UArray *other = UArray_newWithData_type_size_copy_(
        (void *)operands, CTYPE_int16_t, 3, 1);
    UArray *pointerOperand = UArray_newWithData_type_size_copy_(
        (void *)pointerOperands, CTYPE_uintptr_t, 3, 1);
    UArray *empty = UArray_new();
    UArray invalid;
    int result = EXIT_FAILURE;

    if (NULL == values || NULL == other || NULL == pointerOperand || NULL == empty) {
        fail("UArray logical mutation allocation failed");
        goto cleanup;
    }

    values->hash = 0x1111;
    UArray_logicalOr_(values, other);
    if (memcmp(values->data, expectedOr, sizeof(expectedOr)) != 0 ||
        values->hash != 0) {
        fail("UArray logical OR changed prefix results or retained the cached hash");
        goto cleanup;
    }

    memcpy(values->data, initial, sizeof(initial));
    values->hash = 0x2222;
    UArray_logicalAnd_(values, other);
    if (memcmp(values->data, expectedAnd, sizeof(expectedAnd)) != 0 ||
        values->hash != 0) {
        fail("UArray logical AND changed prefix results or retained the cached hash");
        goto cleanup;
    }

    memcpy(values->data, initial, sizeof(initial));
    values->hash = 0x3333;
    UArray_logicalOr_(values, pointerOperand);
    if (((int32_t *)values->data)[0] != 0 ||
        ((int32_t *)values->data)[1] != 1 ||
        ((int32_t *)values->data)[2] != 0 ||
        ((int32_t *)values->data)[3] != 5 ||
        values->hash != 0) {
        fail("UArray logical operations rejected a supported pointer-sized operand");
        goto cleanup;
    }

    empty->hash = 0x4444;
    UArray_logicalOr_(empty, other);
    UArray_logicalAnd_(empty, other);
    UArray_logicalOr_(NULL, other);
    UArray_logicalAnd_(values, NULL);
    if (empty->size != 0 || empty->hash != 0x4444) {
        fail("UArray empty logical operations changed receiver state");
        goto cleanup;
    }

    invalid = *values;
    invalid.data = NULL;
    invalid.hash = 0x5555;
    UArray_logicalOr_(&invalid, other);
    UArray_logicalAnd_(&invalid, other);
    if (invalid.data != NULL || invalid.hash != 0x5555) {
        fail("UArray logical operations accepted missing receiver storage");
        goto cleanup;
    }

    invalid = *other;
    invalid.data = NULL;
    values->hash = 0x6666;
    UArray_logicalOr_(values, &invalid);
    UArray_logicalAnd_(values, &invalid);
    if (values->hash != 0x6666) {
        fail("UArray logical operations accepted missing operand storage");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemSize = 0;
    invalid.hash = 0x7777;
    UArray_logicalOr_(&invalid, other);
    UArray_logicalAnd_(&invalid, other);
    if (invalid.hash != 0x7777) {
        fail("UArray logical operations accepted malformed receiver width");
        goto cleanup;
    }

    invalid = *other;
    invalid.itemType = CTYPE_float64_t;
    values->hash = 0x8888;
    UArray_logicalOr_(values, &invalid);
    UArray_logicalAnd_(values, &invalid);
    if (values->hash != 0x8888) {
        fail("UArray logical operations accepted unsupported operand type");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemType = CTYPE_uint64_t;
    invalid.itemSize = sizeof(uint64_t);
    invalid.hash = 0x9999;
    UArray_logicalOr_(&invalid, other);
    UArray_logicalAnd_(&invalid, other);
    if (invalid.hash != 0x9999) {
        fail("UArray logical operations accepted unsupported receiver type");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemType = CTYPE_int32_t;
    invalid.itemSize = sizeof(int32_t);
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0xaaaa;
    UArray_logicalOr_(&invalid, other);
    UArray_logicalAnd_(&invalid, other);
    if (invalid.hash != 0xaaaa) {
        fail("UArray logical operations accepted overflowing receiver geometry");
        goto cleanup;
    }

    invalid = *other;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    values->hash = 0xbbbb;
    UArray_logicalOr_(values, &invalid);
    UArray_logicalAnd_(values, &invalid);
    if (values->hash != 0xbbbb) {
        fail("UArray logical operations accepted overflowing operand geometry");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(values);
    UArray_free(other);
    UArray_free(pointerOperand);
    UArray_free(empty);
    return result;
}


static int test_assignment_transforms(void)
{
    const int16_t clearValues[] = {7, -2, 9};
    const double roundValues[] = {1.2, 1.8, -1.8};
    const int32_t fillValues[] = {9, 9, 9};
    const int32_t expectedRange[] = {0, 1, 2};
    const double expectedRound[] = {1.0, 2.0, -2.0};
    const uintptr_t pointerValues[] = {7, 8, 9};
    UArray *cleared = UArray_newWithData_type_size_copy_(
        (void *)clearValues, CTYPE_int16_t, 3, 1);
    UArray *longFilled = UArray_newWithData_type_size_copy_(
        (void *)fillValues, CTYPE_int32_t, 3, 1);
    UArray *doubleFilled = UArray_newWithData_type_size_copy_(
        (void *)roundValues, CTYPE_float64_t, 3, 1);
    UArray *range = UArray_newWithData_type_size_copy_(
        (void *)fillValues, CTYPE_int32_t, 3, 1);
    UArray *rounded = UArray_newWithData_type_size_copy_(
        (void *)roundValues, CTYPE_float64_t, 3, 1);
    UArray *pointerArray = UArray_newWithData_type_size_copy_(
        (void *)pointerValues, CTYPE_uintptr_t, 3, 1);
    UArray *empty = UArray_new();
    UArray invalid;
    int result = EXIT_FAILURE;

    if (!cleared || !longFilled || !doubleFilled || !range ||
        !rounded || !pointerArray || !empty) {
        fail("UArray assignment transform allocation failed");
        goto cleanup;
    }

    cleared->hash = 0x1111;
    UArray_clear(cleared);
    if (UArray_rawDoubleAt_(cleared, 0) != 0.0 ||
        UArray_rawDoubleAt_(cleared, 1) != 0.0 ||
        UArray_rawDoubleAt_(cleared, 2) != 0.0 || cleared->hash != 0) {
        fail("UArray clear changed values incorrectly or retained the cached hash");
        goto cleanup;
    }

    longFilled->hash = 0x2222;
    UArray_setItemsToLong_(longFilled, 6);
    if (UArray_rawDoubleAt_(longFilled, 0) != 6.0 ||
        UArray_rawDoubleAt_(longFilled, 2) != 6.0 || longFilled->hash != 0) {
        fail("UArray long fill changed values incorrectly or retained the cached hash");
        goto cleanup;
    }

    longFilled->hash = 0x2323;
    UArray_setItemsToDouble_(longFilled, INFINITY);
    if (UArray_rawDoubleAt_(longFilled, 0) != 6.0 ||
        UArray_rawDoubleAt_(longFilled, 1) != 6.0 ||
        UArray_rawDoubleAt_(longFilled, 2) != 6.0 || longFilled->hash != 0x2323) {
        fail("UArray double fill accepted a non-finite integer value");
        goto cleanup;
    }

    longFilled->hash = 0x2424;
    UArray_setItemsToDouble_(longFilled, ldexp(1.0, 31));
    if (UArray_rawDoubleAt_(longFilled, 0) != 6.0 ||
        UArray_rawDoubleAt_(longFilled, 1) != 6.0 ||
        UArray_rawDoubleAt_(longFilled, 2) != 6.0 || longFilled->hash != 0x2424) {
        fail("UArray double fill accepted an out-of-range int32 value");
        goto cleanup;
    }

    doubleFilled->hash = 0x3333;
    UArray_setItemsToDouble_(doubleFilled, 2.75);
    if (UArray_rawDoubleAt_(doubleFilled, 0) != 2.75 ||
        UArray_rawDoubleAt_(doubleFilled, 2) != 2.75 || doubleFilled->hash != 0) {
        fail("UArray double fill changed values incorrectly or retained the cached hash");
        goto cleanup;
    }

    range->hash = 0x4444;
    UArray_rangeFill(range);
    if (memcmp(range->data, expectedRange, sizeof(expectedRange)) != 0 ||
        range->hash != 0) {
        fail("UArray range fill changed values incorrectly or retained the cached hash");
        goto cleanup;
    }

    rounded->hash = 0x5555;
    UArray_round(rounded);
    if (UArray_rawDoubleAt_(rounded, 0) != expectedRound[0] ||
        UArray_rawDoubleAt_(rounded, 1) != expectedRound[1] ||
        UArray_rawDoubleAt_(rounded, 2) != expectedRound[2] || rounded->hash != 0) {
        fail("UArray round changed results or retained the cached hash");
        goto cleanup;
    }

    {
        const int64_t signedValues[] = {INT64_MIN, INT64_MAX, INT64_C(9007199254740993)};
        const uint64_t unsignedValues[] = {0, UINT64_MAX, UINT64_MAX - 1};
        UArray *signedArray = UArray_newWithData_type_size_copy_(
            (void *)signedValues, CTYPE_int64_t, 3, 1);
        UArray *unsignedArray = UArray_newWithData_type_size_copy_(
            (void *)unsignedValues, CTYPE_uint64_t, 3, 1);
        if (NULL == signedArray || NULL == unsignedArray) {
            UArray_free(signedArray);
            UArray_free(unsignedArray);
            fail("UArray integer round boundary fixture allocation failed");
            goto cleanup;
        }
        signedArray->hash = (uintptr_t)0x5151;
        unsignedArray->hash = (uintptr_t)0x6161;
        UArray_round(signedArray);
        UArray_round(unsignedArray);
        if (memcmp(signedArray->data, signedValues, sizeof(signedValues)) != 0 ||
            memcmp(unsignedArray->data, unsignedValues, sizeof(unsignedValues)) != 0 ||
            signedArray->hash != (uintptr_t)0x5151 ||
            unsignedArray->hash != (uintptr_t)0x6161) {
            UArray_free(signedArray);
            UArray_free(unsignedArray);
            fail("UArray round changed integer backing storage or invalidated its hash");
            goto cleanup;
        }
        UArray_free(signedArray);
        UArray_free(unsignedArray);
    }

    empty->hash = 0x6666;
    UArray_clear(empty);
    UArray_setItemsToLong_(empty, 1);
    UArray_setItemsToDouble_(empty, 2.0);
    UArray_rangeFill(empty);
    UArray_round(empty);
    UArray_clear(NULL);
    UArray_setItemsToLong_(NULL, 1);
    UArray_setItemsToDouble_(NULL, 2.0);
    UArray_rangeFill(NULL);
    UArray_round(NULL);
    if (empty->size != 0 || empty->hash != 0x6666) {
        fail("UArray empty assignment transforms changed receiver state");
        goto cleanup;
    }

    invalid = *longFilled;
    invalid.data = NULL;
    invalid.hash = 0x7777;
    UArray_clear(&invalid);
    UArray_setItemsToLong_(&invalid, 1);
    UArray_setItemsToDouble_(&invalid, 2.0);
    UArray_rangeFill(&invalid);
    UArray_round(&invalid);
    if (invalid.data != NULL || invalid.hash != 0x7777) {
        fail("UArray assignment transforms accepted missing backing storage");
        goto cleanup;
    }

    invalid = *longFilled;
    invalid.itemSize = 0;
    invalid.hash = 0x8888;
    UArray_clear(&invalid);
    UArray_setItemsToLong_(&invalid, 1);
    UArray_setItemsToDouble_(&invalid, 2.0);
    UArray_rangeFill(&invalid);
    UArray_round(&invalid);
    if (invalid.hash != 0x8888) {
        fail("UArray assignment transforms accepted malformed item width");
        goto cleanup;
    }

    invalid = *longFilled;
    invalid.itemType = (CTYPE)9999;
    invalid.hash = 0x9999;
    UArray_clear(&invalid);
    UArray_setItemsToLong_(&invalid, 1);
    UArray_setItemsToDouble_(&invalid, 2.0);
    UArray_rangeFill(&invalid);
    UArray_round(&invalid);
    if (invalid.hash != 0x9999) {
        fail("UArray assignment transforms accepted unsupported item type");
        goto cleanup;
    }

    invalid = *longFilled;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0xaaaa;
    UArray_clear(&invalid);
    UArray_setItemsToLong_(&invalid, 1);
    UArray_setItemsToDouble_(&invalid, 2.0);
    UArray_rangeFill(&invalid);
    UArray_round(&invalid);
    if (invalid.hash != 0xaaaa) {
        fail("UArray assignment transforms accepted overflowing storage geometry");
        goto cleanup;
    }

    pointerArray->hash = 0xbbbb;
    UArray_clear(pointerArray);
    UArray_setItemsToLong_(pointerArray, 1);
    UArray_setItemsToDouble_(pointerArray, 2.0);
    UArray_rangeFill(pointerArray);
    UArray_round(pointerArray);
    if (pointerArray->hash != 0xbbbb ||
        memcmp(pointerArray->data, pointerValues, sizeof(pointerValues)) != 0) {
        fail("UArray assignment transforms accepted pointer-sized item type");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(cleared);
    UArray_free(longFilled);
    UArray_free(doubleFilled);
    UArray_free(range);
    UArray_free(rounded);
    UArray_free(pointerArray);
    UArray_free(empty);
    return result;
}


static int test_negate(void)
{
    const int32_t valuesData[] = {1, -2, 3};
    const int32_t expected[] = {-1, 2, -3};
    const double floatData[] = {1.5, -2.25};
    const double floatExpected[] = {-1.5, 2.25};
    const int8_t min8[] = {INT8_MIN};
    const int16_t min16[] = {INT16_MIN};
    const int32_t min32[] = {INT32_MIN};
    const int64_t min64[] = {INT64_MIN};
    UArray *values = UArray_newWithData_type_size_copy_(
        (void *)valuesData, CTYPE_int32_t, 3, 1);
    UArray *floats = UArray_newWithData_type_size_copy_(
        (void *)floatData, CTYPE_float64_t, 2, 1);
    UArray *minimum8 = UArray_newWithData_type_size_copy_(
        (void *)min8, CTYPE_int8_t, 1, 1);
    UArray *minimum16 = UArray_newWithData_type_size_copy_(
        (void *)min16, CTYPE_int16_t, 1, 1);
    UArray *minimum32 = UArray_newWithData_type_size_copy_(
        (void *)min32, CTYPE_int32_t, 1, 1);
    UArray *minimum64 = UArray_newWithData_type_size_copy_(
        (void *)min64, CTYPE_int64_t, 1, 1);
    UArray invalid;
    int result = EXIT_FAILURE;

    if (!values || !floats || !minimum8 || !minimum16 || !minimum32 || !minimum64) {
        fail("UArray negate allocation failed");
        goto cleanup;
    }

    values->hash = 0x1111;
    UArray_negate(values);
    if (memcmp(values->data, expected, sizeof(expected)) != 0 || values->hash != 0) {
        fail("UArray integer negate changed values incorrectly or retained the cached hash");
        goto cleanup;
    }

    floats->hash = 0x2222;
    UArray_negate(floats);
    if (memcmp(floats->data, floatExpected, sizeof(floatExpected)) != 0 || floats->hash != 0) {
        fail("UArray floating negate changed values incorrectly or retained the cached hash");
        goto cleanup;
    }

    minimum8->hash = 0x3333;
    minimum16->hash = 0x4444;
    minimum32->hash = 0x5555;
    minimum64->hash = 0x6666;
    UArray_negate(minimum8);
    UArray_negate(minimum16);
    UArray_negate(minimum32);
    UArray_negate(minimum64);
    if (memcmp(minimum8->data, min8, sizeof(min8)) != 0 || minimum8->hash != 0x3333 ||
        memcmp(minimum16->data, min16, sizeof(min16)) != 0 || minimum16->hash != 0x4444 ||
        memcmp(minimum32->data, min32, sizeof(min32)) != 0 || minimum32->hash != 0x5555 ||
        memcmp(minimum64->data, min64, sizeof(min64)) != 0 || minimum64->hash != 0x6666) {
        fail("UArray negate did not reject signed minimum values atomically");
        goto cleanup;
    }

    UArray_negate(NULL);
    invalid = *values;
    invalid.data = NULL;
    invalid.hash = 0x7777;
    UArray_negate(&invalid);
    if (invalid.data != NULL || invalid.hash != 0x7777) {
        fail("UArray negate accepted missing backing storage");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemSize = 0;
    invalid.hash = 0x8888;
    UArray_negate(&invalid);
    if (invalid.hash != 0x8888) {
        fail("UArray negate accepted malformed item width");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemType = (CTYPE)9999;
    invalid.hash = 0x9999;
    UArray_negate(&invalid);
    if (invalid.hash != 0x9999) {
        fail("UArray negate accepted unsupported item type");
        goto cleanup;
    }

    invalid = *values;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0xaaaa;
    UArray_negate(&invalid);
    if (invalid.hash != 0xaaaa) {
        fail("UArray negate accepted overflowing storage geometry");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(values);
    UArray_free(floats);
    UArray_free(minimum8);
    UArray_free(minimum16);
    UArray_free(minimum32);
    UArray_free(minimum64);
    return result;
}

typedef void (*UArrayUnaryTransform_)(UArray *);
typedef double (*UArrayUnaryMathFunction_)(double);

typedef struct {
    UArrayUnaryTransform_ transform;
    UArrayUnaryMathFunction_ expected;
} UArrayUnaryTransformCase_;

static int test_unary_transforms(void)
{
    static const double initial[] = {0.25, 0.5};
    static const uintptr_t pointerValues[] = {1, 2};
    static const UArrayUnaryTransformCase_ transforms[] = {
        {UArray_sin, sin},
        {UArray_cos, cos},
        {UArray_tan, tan},
        {UArray_asin, asin},
        {UArray_acos, acos},
        {UArray_atan, atan},
        {UArray_sinh, sinh},
        {UArray_cosh, cosh},
        {UArray_tanh, tanh},
        {UArray_exp, exp},
        {UArray_log, log},
        {UArray_log10, log10},
        {UArray_sqrt, sqrt},
        {UArray_ceil, ceil},
        {UArray_floor, floor},
        {UArray_abs, fabs}
    };
    UArray *values = UArray_newWithData_type_size_copy_(
        (void *)initial, CTYPE_float64_t, 2, 1);
    UArray *empty = UArray_new();
    UArray *pointerArray = UArray_newWithData_type_size_copy_(
        (void *)pointerValues, CTYPE_uintptr_t, 2, 1);
    UArray invalid;
    size_t i;
    int result = EXIT_FAILURE;

    if (NULL == values || NULL == empty || NULL == pointerArray) {
        fail("UArray unary transform allocation failed");
        goto cleanup;
    }

    for (i = 0; i < sizeof(transforms) / sizeof(transforms[0]); ++i) {
        memcpy(values->data, initial, sizeof(initial));
        values->hash = (uintptr_t)(0x1000u + i);
        transforms[i].transform(values);
        transforms[i].transform(NULL);
        if (values->hash != 0 ||
            !nearEnough(UArray_rawDoubleAt_(values, 0),
                        transforms[i].expected(initial[0]))) {
            fail("UArray unary transform changed values or retained the cached hash");
            goto cleanup;
        }
    }

    {
        const int32_t signedInput[] = {-12, 3};
        const int32_t signedExpected[] = {12, 3};
        const int64_t minimumInput[] = {INT64_MIN, -7};
        const uint64_t unsignedInput[] = {UINT64_MAX, UINT64_MAX - 1};
        UArray *signedArray = UArray_newWithData_type_size_copy_(
            (void *)signedInput, CTYPE_int32_t, 2, 1);
        UArray *minimumArray = UArray_newWithData_type_size_copy_(
            (void *)minimumInput, CTYPE_int64_t, 2, 1);
        UArray *unsignedArray = UArray_newWithData_type_size_copy_(
            (void *)unsignedInput, CTYPE_uint64_t, 2, 1);
        if (NULL == signedArray || NULL == minimumArray || NULL == unsignedArray) {
            UArray_free(signedArray);
            UArray_free(minimumArray);
            UArray_free(unsignedArray);
            fail("UArray integer absolute-value fixture allocation failed");
            goto cleanup;
        }

        signedArray->hash = (uintptr_t)0x7171;
        UArray_abs(signedArray);
        if (memcmp(signedArray->data, signedExpected, sizeof(signedExpected)) != 0 ||
            signedArray->hash != 0) {
            UArray_free(signedArray);
            UArray_free(minimumArray);
            UArray_free(unsignedArray);
            fail("UArray fabs did not safely normalize signed integer values");
            goto cleanup;
        }

        minimumArray->hash = (uintptr_t)0x8181;
        UArray_abs(minimumArray);
        if (memcmp(minimumArray->data, minimumInput, sizeof(minimumInput)) != 0 ||
            minimumArray->hash != (uintptr_t)0x8181) {
            UArray_free(signedArray);
            UArray_free(minimumArray);
            UArray_free(unsignedArray);
            fail("UArray absolute value did not reject signed minimum values atomically");
            goto cleanup;
        }

        unsignedArray->hash = (uintptr_t)0x9191;
        UArray_abs(unsignedArray);
        if (memcmp(unsignedArray->data, unsignedInput, sizeof(unsignedInput)) != 0 ||
            unsignedArray->hash != (uintptr_t)0x9191) {
            UArray_free(signedArray);
            UArray_free(minimumArray);
            UArray_free(unsignedArray);
            fail("UArray fabs changed unsigned integer storage");
            goto cleanup;
        }
        UArray_free(signedArray);
        UArray_free(minimumArray);
        UArray_free(unsignedArray);
    }

    empty->hash = 0x5151;
    pointerArray->hash = 0x6161;
    for (i = 0; i < sizeof(transforms) / sizeof(transforms[0]); ++i) {
        transforms[i].transform(empty);
        transforms[i].transform(pointerArray);
    }
    if (empty->hash != 0x5151 || empty->size != 0) {
        fail("UArray unary transforms changed empty receiver state");
        goto cleanup;
    }
    if (pointerArray->hash != 0x6161 ||
        memcmp(pointerArray->data, pointerValues, sizeof(pointerValues)) != 0) {
        fail("UArray unary transforms accepted pointer-sized item type");
        goto cleanup;
    }

    invalid = *values;
    invalid.data = NULL;
    invalid.hash = 0x7171;
    for (i = 0; i < sizeof(transforms) / sizeof(transforms[0]); ++i)
        transforms[i].transform(&invalid);
    if (invalid.data != NULL || invalid.hash != 0x7171) {
        fail("UArray unary transforms accepted missing backing storage");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemSize = 0;
    invalid.hash = 0x8181;
    for (i = 0; i < sizeof(transforms) / sizeof(transforms[0]); ++i)
        transforms[i].transform(&invalid);
    if (invalid.itemSize != 0 || invalid.hash != 0x8181) {
        fail("UArray unary transforms accepted zero-width storage");
        goto cleanup;
    }

    invalid = *values;
    invalid.itemType = (CTYPE)9999;
    invalid.hash = 0x9191;
    for (i = 0; i < sizeof(transforms) / sizeof(transforms[0]); ++i)
        transforms[i].transform(&invalid);
    if (invalid.itemType != (CTYPE)9999 || invalid.hash != 0x9191) {
        fail("UArray unary transforms accepted unsupported item type");
        goto cleanup;
    }

    invalid = *values;
    invalid.size = (size_t)-1 / invalid.itemSize + 1;
    invalid.hash = 0xa1a1;
    for (i = 0; i < sizeof(transforms) / sizeof(transforms[0]); ++i)
        transforms[i].transform(&invalid);
    if (invalid.hash != 0xa1a1) {
        fail("UArray unary transforms accepted overflowing storage geometry");
        goto cleanup;
    }

    result = EXIT_SUCCESS;
cleanup:
    UArray_free(values);
    UArray_free(empty);
    UArray_free(pointerArray);
    return result;
}


static int test_misaligned_numeric_storage(void)
{
	union {
		uint64_t alignment;
		unsigned char bytes[sizeof(uint64_t) + 1];
	} storage;
	uint64_t original = UINT64_C(0x123456789abcdef0);
	unsigned char snapshot[sizeof(original)];
	UArray malformed;

	memset(storage.bytes, 0, sizeof(storage.bytes));
	memcpy(storage.bytes + 1, &original, sizeof(original));
	memcpy(snapshot, &original, sizeof(snapshot));
	malformed = UArray_stackAllocedWithData_type_size_(
		storage.bytes + 1, CTYPE_uint64_t, 1);

	UArray_clear(&malformed);
	if (memcmp(storage.bytes + 1, snapshot, sizeof(snapshot)) != 0)
		return fail("UArray math mutated misaligned numeric storage");
	UArray_setItemsToLong_(&malformed, 0);
	if (memcmp(storage.bytes + 1, snapshot, sizeof(snapshot)) != 0)
		return fail("UArray math fill accepted misaligned numeric storage");
	if (UArray_dotProduct_(&malformed, &malformed) != 0.0)
		return fail("UArray dot product accepted misaligned numeric storage");
	return EXIT_SUCCESS;
}


int main(void)
{
    if (test_scalar_mutations() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_indexed_double_writes() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_elementwise_mutations() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_elementwise_division() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_reductions() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_normalize() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_unary_transforms() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_pairwise_extrema() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_logical_mutations() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_assignment_transforms() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_negate() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_misaligned_numeric_storage() != EXIT_SUCCESS)
        return EXIT_FAILURE;


    const int32_t negatives[] = {-11, -4, -19};
    const int32_t positives[] = {5, 12, 8};
    UArray *n = UArray_newWithData_type_size_copy_((void *)negatives, CTYPE_int32_t, 3, 1);
    UArray *p = UArray_newWithData_type_size_copy_((void *)positives, CTYPE_int32_t, 3, 1);
    UArray *empty = UArray_new();
    unsigned char *data;
    size_t size, itemSize;
    if (!n || !p || !empty) { UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray extrema allocation failed"); }
    {
        const int16_t factors[] = { 2, 3 };
        UArray *factorArray = UArray_newWithData_type_size_copy_(
            (void *)factors, CTYPE_int16_t, 2, 1);
        double dot;
        if (NULL == factorArray) {
            UArray_free(n); UArray_free(p); UArray_free(empty);
            return fail("UArray dot product fixture allocation failed");
        }
        dot = UArray_dotProduct_(p, factorArray);
        if (dot != 46.0) {
            UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
            return fail("UArray mixed-width dot product changed");
        }
        if (UArray_dotProduct_(NULL, factorArray) != 0.0 ||
            UArray_dotProduct_(p, NULL) != 0.0) {
            UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
            return fail("UArray dot product accepted NULL input");
        }
        {
            unsigned char *originalData = p->data;
            p->data = NULL;
            dot = UArray_dotProduct_(p, factorArray);
            p->data = originalData;
            if (dot != 0.0) {
                UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
                return fail("UArray dot product accepted missing backing storage");
            }
        }
        {
            size_t originalItemSize = p->itemSize;
            p->itemSize = sizeof(uint32_t) - 1;
            dot = UArray_dotProduct_(p, factorArray);
            p->itemSize = originalItemSize;
            if (dot != 0.0) {
                UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
                return fail("UArray dot product accepted mismatched item width");
            }
        }
        {
            CTYPE originalItemType = p->itemType;
            p->itemType = (CTYPE)9999;
            dot = UArray_dotProduct_(p, factorArray);
            p->itemType = originalItemType;
            if (dot != 0.0) {
                UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
                return fail("UArray dot product accepted an unsupported item type");
            }
        }
        {
            size_t originalSize = p->size;
            p->size = (size_t)-1 / sizeof(uint32_t) + 1;
            dot = UArray_dotProduct_(p, factorArray);
            p->size = originalSize;
            if (dot != 0.0) {
                UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
                return fail("UArray dot product accepted overflowing item geometry");
            }
        }
        if (UArray_dotProduct_(empty, factorArray) != 0.0) {
            UArray_free(factorArray); UArray_free(n); UArray_free(p); UArray_free(empty);
            return fail("UArray empty dot product behavior changed");
        }
        UArray_free(factorArray);
    }
    if (UArray_maxAsDouble(n) != -4.0 || UArray_minAsDouble(n) != -19.0) { UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray negative-only extrema returned a finite sentinel"); }
    if (UArray_maxAsDouble(p) != 12.0 || UArray_minAsDouble(p) != 5.0) { UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray positive extrema changed"); }
    if (UArray_maxAsDouble(empty) != 0.0 || UArray_minAsDouble(empty) != 0.0 || UArray_maxAsDouble(NULL) != 0.0 || UArray_minAsDouble(NULL) != 0.0) { UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray extrema empty/NULL behavior changed"); }
    data=n->data; size=n->size; n->data=NULL;
    if (UArray_maxAsDouble(n)!=0.0 || UArray_minAsDouble(n)!=0.0) { n->data=data; UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray extrema accepted missing backing storage"); }
    n->data=data; n->size=size;
    itemSize=n->itemSize; n->itemSize=0;
    if (UArray_maxAsDouble(n)!=0.0 || UArray_minAsDouble(n)!=0.0) { n->itemSize=itemSize; UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray extrema accepted zero-width storage"); }
    n->itemSize=itemSize;
    size=n->size; n->itemSize=2; n->size=(size_t)-1/2+1;
    if (UArray_maxAsDouble(n)!=0.0 || UArray_minAsDouble(n)!=0.0) { n->size=size; n->itemSize=itemSize; UArray_free(n); UArray_free(p); UArray_free(empty); return fail("UArray extrema accepted overflowing geometry"); }
    n->size=size; n->itemSize=itemSize;
    {
        size_t originalItemSize = n->itemSize;
        n->itemSize = sizeof(int32_t) - 1;
        if (UArray_maxAsDouble(n) != 0.0 || UArray_minAsDouble(n) != 0.0) {
            n->itemSize = originalItemSize;
            UArray_free(n); UArray_free(p); UArray_free(empty);
            return fail("UArray extrema accepted mismatched item width");
        }
        n->itemSize = originalItemSize;
    }
    {
        CTYPE originalItemType = n->itemType;
        n->itemType = CTYPE_float64_t;
        if (UArray_maxAsDouble(n) != 0.0 || UArray_minAsDouble(n) != 0.0) {
            n->itemType = originalItemType;
            UArray_free(n); UArray_free(p); UArray_free(empty);
            return fail("UArray extrema accepted mismatched item type");
        }
        n->itemType = originalItemType;
    }
    UArray_free(n); UArray_free(p); UArray_free(empty);
    puts("UArray math extrema test passed");
    return EXIT_SUCCESS;
}
