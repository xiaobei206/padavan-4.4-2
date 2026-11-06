#include "UArray.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <stdint.h>

static int fail(const char *s) { fprintf(stderr, "%s\n", s); return EXIT_FAILURE; }
static int nearEnough(double a, double b) { return fabs(a - b) <= 1e-12; }

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

int main(void)
{
    if (test_scalar_mutations() != EXIT_SUCCESS)
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
