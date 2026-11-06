#include "Sorting.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    int *values;
    unsigned int comparisons;
    unsigned int swaps;
} SortContext;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "Sorting boundary test: %s\n", message); \
        return 1; \
    } \
} while (0)

static int compare_values(void *context, int left, int right)
{
    SortContext *sort = (SortContext *)context;
    sort->comparisons++;
    if (sort->values[left] < sort->values[right])
        return -1;
    if (sort->values[left] > sort->values[right])
        return 1;
    return 0;
}

static void swap_values(void *context, int left, int right)
{
    SortContext *sort = (SortContext *)context;
    int value = sort->values[left];
    sort->values[left] = sort->values[right];
    sort->values[right] = value;
    sort->swaps++;
}

static int is_sorted(const int *values, size_t size)
{
    size_t i;
    for (i = 1; i < size; ++i) {
        if (values[i - 1] > values[i])
            return 0;
    }
    return 1;
}

int main(void)
{
    int ascending[] = {-4, -1, 0, 3, 8};
    int descending[] = {9, 7, 5, 3, 1, -1};
    int duplicates[] = {4, -2, 4, 0, -2, 9, 1};
    int guarded[] = {3, 1, 2};
    int original[] = {3, 1, 2};
    SortContext sort;
    SortContext guard;
    size_t oversized = (size_t)INT_MAX + 2u;

    sort.values = ascending;
    sort.comparisons = 0;
    sort.swaps = 0;
    Sorting_context_comp_swap_size_type_(&sort, compare_values, swap_values,
                                         sizeof(ascending) / sizeof(ascending[0]),
                                         SDQuickSort);
    CHECK(is_sorted(ascending, sizeof(ascending) / sizeof(ascending[0])),
          "already-sorted input was changed to an invalid order");
    CHECK(sort.comparisons == 4 && sort.swaps == 0,
          "sorted fast path performed unexpected operations");

    sort.values = descending;
    sort.comparisons = 0;
    sort.swaps = 0;
    Sorting_context_comp_swap_size_type_(&sort, compare_values, swap_values,
                                         sizeof(descending) / sizeof(descending[0]),
                                         SDQuickSort);
    CHECK(is_sorted(descending, sizeof(descending) / sizeof(descending[0])),
          "reverse-order input was not sorted");

    sort.values = duplicates;
    sort.comparisons = 0;
    sort.swaps = 0;
    Sorting_context_comp_swap_size_type_(&sort, compare_values, swap_values,
                                         sizeof(duplicates) / sizeof(duplicates[0]),
                                         SDQuickSort);
    CHECK(is_sorted(duplicates, sizeof(duplicates) / sizeof(duplicates[0])),
          "duplicate and negative values were not sorted");

    guard.values = guarded;
    guard.comparisons = 0;
    guard.swaps = 0;
    Sorting_context_comp_swap_size_type_(&guard, NULL, swap_values, 3, SDQuickSort);
    Sorting_context_comp_swap_size_type_(&guard, compare_values, NULL, 3, SDQuickSort);
    Sorting_context_comp_swap_size_type_(&guard, compare_values, swap_values,
                                         oversized, SDQuickSort);
    Sorting_context_comp_swap_size_type_(&guard, NULL, NULL, 0, SDQuickSort);
    Sorting_context_comp_swap_size_type_(&guard, NULL, NULL, 1, SDQuickSort);
    CHECK(guard.comparisons == 0 && guard.swaps == 0,
          "invalid callbacks or unsupported index range invoked a callback");
    CHECK(0 == memcmp(guarded, original, sizeof(guarded)),
          "invalid arguments modified the input array");

    puts("Sorting boundary test passed");
    return 0;
}
