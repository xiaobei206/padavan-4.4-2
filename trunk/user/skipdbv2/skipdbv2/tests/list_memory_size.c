#include "List.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    List *list = List_new();
    int values[] = { 10, 20, 30 };
    size_t expected;
    int result = EXIT_FAILURE;
    size_t i;

    if (NULL == list) {
        fprintf(stderr, "list allocation failed\n");
        return EXIT_FAILURE;
    }

    expected = sizeof(List) + list->memSize;
    if (List_memorySize(list) != expected) {
        fprintf(stderr, "empty list memory size omitted its allocated buffer\n");
        goto done;
    }

    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (List_append_(list, &values[i]) != &values[i]) {
            fprintf(stderr, "list append failed\n");
            goto done;
        }
    }

    if (list->memSize <= list->size * sizeof(void *)) {
        fprintf(stderr, "list fixture did not retain spare capacity\n");
        goto done;
    }
    expected = sizeof(List) + list->memSize;
    if (List_memorySize(list) != expected) {
        fprintf(stderr, "list memory size reported logical contents instead of allocated capacity\n");
        goto done;
    }

    List_compact(list);
    expected = sizeof(List) + list->memSize;
    if (List_memorySize(list) != expected) {
        fprintf(stderr, "compacted list memory size does not match its allocation\n");
        goto done;
    }

    result = EXIT_SUCCESS;
    puts("list memory size test passed");

done:
    List_free(list);
    return result;
}
