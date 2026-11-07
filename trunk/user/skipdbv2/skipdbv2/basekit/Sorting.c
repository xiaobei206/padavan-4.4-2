#include "Sorting.h"

#include <limits.h>

typedef struct
{
    void *context;
    SDSortCompareCallback *comp;
    SDSortSwapCallback *swap;
} SDSort;

static size_t Sorting_depthLimit_(size_t size);
static int Sorting_isSorted_(SDSort *self, size_t size);
static void Sorting_quickSort_(SDSort *self, size_t lb, size_t ub, size_t depthLimit);
static void Sorting_heapSort_(SDSort *self, size_t lb, size_t ub);
static void Sorting_siftDown_(SDSort *self, size_t lb, size_t root, size_t count);
static void Sorting_quickSortRearrange_(SDSort *self, size_t lb, size_t ub, size_t *equalStart, size_t *equalEnd);

static size_t Sorting_depthLimit_(size_t size)
{
    size_t depth = 0;

    while (size > 1) {
        size >>= 1;
        ++depth;
    }
    return depth * 2;
}


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
        Sorting_quickSort_(&sort, 0, size - 1, Sorting_depthLimit_(size));
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

static void Sorting_quickSort_(SDSort *self, size_t lb, size_t ub, size_t depthLimit)
{
    if (NULL == self)
        return;

    while (lb < ub) {
        size_t equalStart, equalEnd;
        size_t leftSize, rightSize;

        /* Median-of-three is a heuristic; heapsort closes its worst-case gap. */
        if (depthLimit == 0) {
            Sorting_heapSort_(self, lb, ub);
            return;
        }
        --depthLimit;

        Sorting_quickSortRearrange_(self, lb, ub, &equalStart, &equalEnd);
        leftSize = equalStart - lb;
        rightSize = ub - equalEnd;

        /* Recurse only into the smaller strict partition; skip the equal band. */
        if (leftSize < rightSize) {
            if (leftSize > 0)
                Sorting_quickSort_(self, lb, equalStart - 1, depthLimit);
            lb = equalEnd + 1;
        } else {
            if (rightSize > 0)
                Sorting_quickSort_(self, equalEnd + 1, ub, depthLimit);
            if (equalStart == 0)
                return;
            ub = equalStart - 1;
        }
    }
}


static void Sorting_heapSort_(SDSort *self, size_t lb, size_t ub)
{
    size_t count, start, end;

    if (NULL == self || NULL == self->comp || NULL == self->swap || lb >= ub)
        return;
    count = ub - lb + 1;

    /* Build a max heap, then move its root to the end of the active range. */
    start = count / 2;
    while (start > 0) {
        --start;
        Sorting_siftDown_(self, lb, start, count);
    }

    end = count;
    while (end > 1) {
        --end;
        self->swap(self->context, (int)lb, (int)(lb + end));
        Sorting_siftDown_(self, lb, 0, end);
    }
}


static void Sorting_siftDown_(SDSort *self, size_t lb, size_t root, size_t count)
{
    if (NULL == self || NULL == self->comp || NULL == self->swap)
        return;

    /* root < count / 2 guarantees root * 2 + 1 is an in-range child. */
    while (root < count / 2) {
        size_t child = root * 2 + 1;

        if (child + 1 < count &&
            self->comp(self->context, (int)(lb + child), (int)(lb + child + 1)) < 0)
            ++child;
        if (self->comp(self->context, (int)(lb + root), (int)(lb + child)) >= 0)
            return;
        self->swap(self->context, (int)(lb + root), (int)(lb + child));
        root = child;
    }
}
static void Sorting_quickSortRearrange_(SDSort *self, size_t lb, size_t ub,
                                           size_t *equalStart, size_t *equalEnd)
{
    size_t firstEqual = lb;
    size_t firstGreater;
    size_t i;

    if (NULL == equalStart || NULL == equalEnd)
        return;
    *equalStart = lb;
    *equalEnd = lb;
    if (NULL == self || NULL == self->comp || NULL == self->swap || lb >= ub)
        return;

    /*
     * Keep the median-of-three pivot at ub while partitioning so comparisons
     * can classify opaque context values without copying them.
     */
    if (ub - lb > 1) {
        size_t mid = lb + (ub - lb) / 2;

        if (self->comp(self->context, (int)mid, (int)lb) < 0)
            self->swap(self->context, (int)mid, (int)lb);
        if (self->comp(self->context, (int)ub, (int)lb) < 0)
            self->swap(self->context, (int)ub, (int)lb);
        if (self->comp(self->context, (int)ub, (int)mid) < 0)
            self->swap(self->context, (int)ub, (int)mid);

        self->swap(self->context, (int)mid, (int)ub);
    }

    /* First move every element smaller than the pivot to the left. */
    for (i = lb; i < ub; ++i) {
        if (self->comp(self->context, (int)i, (int)ub) < 0) {
            if (i != firstEqual)
                self->swap(self->context, (int)i, (int)firstEqual);
            ++firstEqual;
        }
    }

    /* Gather equal elements next to the pivot, leaving larger values right. */
    firstGreater = firstEqual;
    for (i = firstEqual; i < ub; ++i) {
        if (self->comp(self->context, (int)i, (int)ub) == 0) {
            if (i != firstGreater)
                self->swap(self->context, (int)i, (int)firstGreater);
            ++firstGreater;
        }
    }

    if (firstGreater != ub)
        self->swap(self->context, (int)firstGreater, (int)ub);
    *equalStart = firstEqual;
    *equalEnd = firstGreater;
}
