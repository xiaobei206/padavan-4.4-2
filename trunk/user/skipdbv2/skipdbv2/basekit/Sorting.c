#include "Sorting.h"

#include <limits.h>

typedef struct
{
    void *context;
    SDSortCompareCallback *comp;
    SDSortSwapCallback *swap;
} SDSort;

static int Sorting_isSorted_(SDSort *self, size_t size);
static void Sorting_quickSort_(SDSort *self, size_t lb, size_t ub);
static size_t Sorting_quickSortRearrange_(SDSort *self, size_t lb, size_t ub);

void Sorting_context_comp_swap_size_type_(void *context,
                                          SDSortCompareCallback *comp,
                                          SDSortSwapCallback *swap,
                                          size_t size,
                                          SDSortType type)
{
    SDSort sort;

    if (type != SDQuickSort || size < 2 || NULL == comp || NULL == swap ||
        size - 1 > (size_t)INT_MAX)
        return;

    sort.context = context;
    sort.comp = comp;
    sort.swap = swap;

    if (!Sorting_isSorted_(&sort, size))
        Sorting_quickSort_(&sort, 0, size - 1);
}

static int Sorting_isSorted_(SDSort *self, size_t size)
{
    size_t i;

    if (NULL == self || NULL == self->comp || size < 2)
        return 1;

    for (i = 0; i < size - 1; ++i) {
        if (self->comp(self->context, (int)i, (int)(i + 1)) > 0)
            return 0;
    }
    return 1;
}

static void Sorting_quickSort_(SDSort *self, size_t lb, size_t ub)
{
    size_t pivot;

    if (NULL == self || lb >= ub)
        return;

    pivot = Sorting_quickSortRearrange_(self, lb, ub);
    if (pivot > lb)
        Sorting_quickSort_(self, lb, pivot - 1);
    if (pivot < ub)
        Sorting_quickSort_(self, pivot + 1, ub);
}

static size_t Sorting_quickSortRearrange_(SDSort *self, size_t lb, size_t ub)
{
    if (NULL == self || NULL == self->comp || NULL == self->swap)
        return lb;

    while (lb < ub) {
        while (lb < ub &&
               self->comp(self->context, (int)ub, (int)lb) >= 0)
            --ub;

        if (lb < ub)
            self->swap(self->context, (int)ub, (int)lb);

        while (lb < ub &&
               self->comp(self->context, (int)lb, (int)ub) <= 0)
            ++lb;

        if (lb < ub)
            self->swap(self->context, (int)lb, (int)ub);
    }
    return lb;
}
