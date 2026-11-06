#include "UArray.h"
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

static int fail(const char *s) { fprintf(stderr, "%s\n", s); return EXIT_FAILURE; }
static int nearEnough(double a, double b) { return fabs(a - b) <= 1e-12; }

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

int main(void)
{
    if (test_reductions() != EXIT_SUCCESS)
        return EXIT_FAILURE;
    if (test_normalize() != EXIT_SUCCESS)
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
    UArray_free(n); UArray_free(p); UArray_free(empty);
    puts("UArray math extrema test passed");
    return EXIT_SUCCESS;
}
