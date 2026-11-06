#include "UArray.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    return EXIT_FAILURE;
}

static int closeEnough(double actual, double expected)
{
    return fabs(actual - expected) <= fabs(expected) * 1e-12;
}

int main(void)
{
    const float32_t floatValues[] = { 0.0f, 3.0f, 4.0f };
    const float32_t floatZeros[] = { 0.0f, 0.0f, 0.0f };
    const float64_t tinyValues[] = { 0.0, 1e-50 };
    const float64_t tinyZeros[] = { 0.0, 0.0 };
    const float64_t largeValues[] = { 0.0, 1e40 };
    const float64_t largeZeros[] = { 0.0, 0.0 };
    const float64_t oneValue[] = { 0.0 };
    UArray *floatA = UArray_newWithData_type_size_copy_(
        (void *)floatValues, CTYPE_float32_t, 3, 1);
    UArray *floatB = UArray_newWithData_type_size_copy_(
        (void *)floatZeros, CTYPE_float32_t, 3, 1);
    UArray *tinyA = UArray_newWithData_type_size_copy_(
        (void *)tinyValues, CTYPE_float64_t, 2, 1);
    UArray *tinyB = UArray_newWithData_type_size_copy_(
        (void *)tinyZeros, CTYPE_float64_t, 2, 1);
    UArray *largeA = UArray_newWithData_type_size_copy_(
        (void *)largeValues, CTYPE_float64_t, 2, 1);
    UArray *largeB = UArray_newWithData_type_size_copy_(
        (void *)largeZeros, CTYPE_float64_t, 2, 1);
    UArray *shortA = UArray_newWithData_type_size_copy_(
        (void *)oneValue, CTYPE_float64_t, 1, 1);
    double distance;

    if (!floatA || !floatB || !tinyA || !tinyB ||
        !largeA || !largeB || !shortA) {
        UArray_free(floatA); UArray_free(floatB);
        UArray_free(tinyA); UArray_free(tinyB);
        UArray_free(largeA); UArray_free(largeB);
        UArray_free(shortA);
        return fail("UArray distance test allocation failed");
    }

    distance = UArray_distanceTo_(floatA, floatB);
    if (!closeEnough(distance, 5.0)) {
        fail("UArray float32 distance changed");
        goto failed;
    }

    distance = UArray_distanceTo_(tinyA, tinyB);
    if (!isfinite(distance) || !closeEnough(distance, 1e-50)) {
        fail("UArray float64 distance underflowed a representable delta");
        goto failed;
    }

    distance = UArray_distanceTo_(largeA, largeB);
    if (!isfinite(distance) || !closeEnough(distance, 1e40)) {
        fail("UArray float64 distance overflowed a representable delta");
        goto failed;
    }

    if (UArray_distanceTo_(tinyA, shortA) != 0.0) {
        fail("UArray distance changed unequal-length behavior");
        goto failed;
    }

    UArray_free(floatA); UArray_free(floatB);
    UArray_free(tinyA); UArray_free(tinyB);
    UArray_free(largeA); UArray_free(largeB);
    UArray_free(shortA);
    puts("UArray distance precision test passed");
    return EXIT_SUCCESS;

failed:
    UArray_free(floatA); UArray_free(floatB);
    UArray_free(tinyA); UArray_free(tinyB);
    UArray_free(largeA); UArray_free(largeB);
    UArray_free(shortA);
    return EXIT_FAILURE;
}
