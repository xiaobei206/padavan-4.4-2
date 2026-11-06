#include "UArray.h"
#include <stdio.h>
#include <stdlib.h>
static int fail(const char *s) { fprintf(stderr, "%s\n", s); return EXIT_FAILURE; }
int main(void)
{
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
