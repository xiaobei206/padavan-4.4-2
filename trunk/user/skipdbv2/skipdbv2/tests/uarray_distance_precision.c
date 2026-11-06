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
    const float32_t crossX[] = { 1.0f, 0.0f, 0.0f };
    const float32_t crossY[] = { 0.0f, 1.0f, 0.0f };
    const float64_t tinyValues[] = { 0.0, 1e-50 };
    const float64_t tinyZeros[] = { 0.0, 0.0 };
    const float64_t largeValues[] = { 0.0, 1e40 };
    const float64_t largeZeros[] = { 0.0, 0.0 };
    const float64_t extremeValues[] = { 1e200, -1e200 };
    const float64_t extremeZeros[] = { 0.0, 0.0 };
    const float64_t oneValue[] = { 0.0 };
    UArray *floatA = UArray_newWithData_type_size_copy_(
        (void *)floatValues, CTYPE_float32_t, 3, 1);
    UArray *floatB = UArray_newWithData_type_size_copy_(
        (void *)floatZeros, CTYPE_float32_t, 3, 1);
    UArray *crossA = UArray_newWithData_type_size_copy_(
        (void *)crossX, CTYPE_float32_t, 3, 1);
    UArray *crossB = UArray_newWithData_type_size_copy_(
        (void *)crossY, CTYPE_float32_t, 3, 1);
    UArray *tinyA = UArray_newWithData_type_size_copy_(
        (void *)tinyValues, CTYPE_float64_t, 2, 1);
    UArray *tinyB = UArray_newWithData_type_size_copy_(
        (void *)tinyZeros, CTYPE_float64_t, 2, 1);
    UArray *largeA = UArray_newWithData_type_size_copy_(
        (void *)largeValues, CTYPE_float64_t, 2, 1);
    UArray *largeB = UArray_newWithData_type_size_copy_(
        (void *)largeZeros, CTYPE_float64_t, 2, 1);
    UArray *extremeA = UArray_newWithData_type_size_copy_(
        (void *)extremeValues, CTYPE_float64_t, 2, 1);
    UArray *extremeB = UArray_newWithData_type_size_copy_(
        (void *)extremeZeros, CTYPE_float64_t, 2, 1);
    UArray *shortA = UArray_newWithData_type_size_copy_(
        (void *)oneValue, CTYPE_float64_t, 1, 1);
    double distance;

    if (!floatA || !floatB || !crossA || !crossB ||
        !tinyA || !tinyB || !largeA || !largeB || !extremeA || !extremeB || !shortA) {
        UArray_free(floatA); UArray_free(floatB);
        UArray_free(crossA); UArray_free(crossB);
        UArray_free(tinyA); UArray_free(tinyB);
        UArray_free(largeA); UArray_free(largeB);
        UArray_free(extremeA); UArray_free(extremeB);
        UArray_free(shortA);
        return fail("UArray distance test allocation failed");
    }

    UArray_crossProduct_(crossA, crossB);
    if (!closeEnough(((float32_t *)crossA->data)[0], 0.0) ||
        !closeEnough(((float32_t *)crossA->data)[1], 0.0) ||
        !closeEnough(((float32_t *)crossA->data)[2], 1.0)) {
        fail("UArray float32 cross product changed");
        goto failed;
    }

    UArray_crossProduct_(NULL, crossB);
    UArray_crossProduct_(crossA, NULL);
    {
        unsigned char *originalData = crossA->data;
        crossA->data = NULL;
        UArray_crossProduct_(crossA, crossB);
        crossA->data = originalData;
        if (!closeEnough(((float32_t *)crossA->data)[2], 1.0)) {
            fail("UArray cross product accepted missing receiver storage");
            goto failed;
        }
    }

    {
        unsigned char *originalData = crossB->data;
        crossB->data = NULL;
        UArray_crossProduct_(crossA, crossB);
        crossB->data = originalData;
        if (!closeEnough(((float32_t *)crossA->data)[2], 1.0)) {
            fail("UArray cross product accepted missing operand storage");
            goto failed;
        }
    }

    {
        size_t originalItemSize = crossA->itemSize;
        crossA->itemSize = sizeof(float32_t) - 1;
        UArray_crossProduct_(crossA, crossB);
        crossA->itemSize = originalItemSize;
        if (!closeEnough(((float32_t *)crossA->data)[2], 1.0)) {
            fail("UArray cross product accepted malformed item width");
            goto failed;
        }
    }

    {
        size_t originalSize = crossA->size;
        crossA->size = (size_t)-1 / sizeof(float32_t) + 1;
        UArray_crossProduct_(crossA, crossB);
        crossA->size = originalSize;
        if (!closeEnough(((float32_t *)crossA->data)[2], 1.0)) {
            fail("UArray cross product accepted overflowing item geometry");
            goto failed;
        }
    }

    {
        CTYPE originalItemType = crossA->itemType;
        crossA->itemType = CTYPE_int32_t;
        UArray_crossProduct_(crossA, crossB);
        crossA->itemType = originalItemType;
        if (!closeEnough(((float32_t *)crossA->data)[2], 1.0)) {
            fail("UArray cross product accepted a non-float32 receiver");
            goto failed;
        }
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

    distance = UArray_distanceTo_(extremeA, extremeB);
    if (!isfinite(distance) || !closeEnough(distance, sqrt(2.0) * 1e200)) {
        fail("UArray float64 distance overflowed intermediate squares for a finite result");
        goto failed;
    }

    if (UArray_distanceTo_(tinyA, shortA) != 0.0) {
        fail("UArray distance changed unequal-length behavior");
        goto failed;
    }

    if (UArray_distanceTo_(NULL, floatB) != 0.0 ||
        UArray_distanceTo_(floatA, NULL) != 0.0) {
        fail("UArray distance accepted NULL input");
        goto failed;
    }

    {
        unsigned char *originalData = floatA->data;
        floatA->data = NULL;
        distance = UArray_distanceTo_(floatA, floatB);
        floatA->data = originalData;
        if (distance != 0.0) {
            fail("UArray distance accepted missing backing storage");
            goto failed;
        }
    }

    {
        size_t originalItemSize = floatA->itemSize;
        floatA->itemSize = sizeof(float32_t) - 1;
        distance = UArray_distanceTo_(floatA, floatB);
        floatA->itemSize = originalItemSize;
        if (distance != 0.0) {
            fail("UArray distance accepted malformed item width");
            goto failed;
        }
    }

    {
        size_t originalSize = floatA->size;
        floatA->size = (size_t)-1 / sizeof(float32_t) + 1;
        distance = UArray_distanceTo_(floatA, floatB);
        floatA->size = originalSize;
        if (distance != 0.0) {
            fail("UArray distance accepted overflowing item geometry");
            goto failed;
        }
    }

    {
        floatA->itemType = CTYPE_int32_t;
        distance = UArray_distanceTo_(floatA, floatB);
        floatA->itemType = CTYPE_float32_t;
        if (distance != 0.0) {
            fail("UArray distance accepted non-floating item type");
            goto failed;
        }
    }

    UArray_free(floatA); UArray_free(floatB);
    UArray_free(crossA); UArray_free(crossB);
    UArray_free(tinyA); UArray_free(tinyB);
    UArray_free(largeA); UArray_free(largeB);
    UArray_free(extremeA); UArray_free(extremeB);
    UArray_free(shortA);
    puts("UArray distance precision test passed");
    return EXIT_SUCCESS;

failed:
    UArray_free(floatA); UArray_free(floatB);
    UArray_free(crossA); UArray_free(crossB);
    UArray_free(tinyA); UArray_free(tinyB);
    UArray_free(largeA); UArray_free(largeB);
    UArray_free(extremeA); UArray_free(extremeB);
    UArray_free(shortA);
    return EXIT_FAILURE;
}
