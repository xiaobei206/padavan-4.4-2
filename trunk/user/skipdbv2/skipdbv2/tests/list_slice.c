#include "List.h"
#include <stdio.h>
#include <stdlib.h>

static int append_values(List *list, int *values, size_t count)
{
	size_t i;
	for (i = 0; i < count; ++i) {
		if (List_append_(list, &values[i]) != &values[i])
			return 0;
	}
	return 1;
}

static int matches_slice(const List *list, void *const *expected,
	size_t count, size_t previous_size)
{
	size_t i;
	if (NULL == list || list->size != count ||
		previous_size > list->memSize / sizeof(void *) ||
		(count > 0 && NULL == list->items))
		return 0;
	for (i = 0; i < count; ++i) {
		if (list->items[i] != expected[i])
			return 0;
	}
	for (i = count; i < previous_size; ++i) {
		if (list->items[i] != NULL)
			return 0;
	}
	return 1;
}

int main(void)
{
	int values[6] = {0, 1, 2, 3, 4, 5};
	void *middle_expected[3] = {&values[1], &values[2], &values[3]};
	void *bounded_expected[5] = {&values[1], &values[2], &values[3], &values[4], &values[5]};
	void *tail_expected[3] = {&values[3], &values[4], &values[5]};
	List *list = List_new();
	size_t capacity, previous_size;
	int result = EXIT_FAILURE;

	List_sliceInPlace(NULL, 0, 0);
	if (NULL == list) {
		fprintf(stderr, "list allocation failed\n");
		return EXIT_FAILURE;
	}
	if (!append_values(list, values, sizeof(values) / sizeof(values[0]))) {
		fprintf(stderr, "list append failed\n");
		goto done;
	}

	capacity = list->memSize;
	previous_size = list->size;
	List_sliceInPlace(list, 1, 3);
	if (!matches_slice(list, middle_expected, 3, previous_size) ||
		list->memSize != capacity) {
		fprintf(stderr, "middle slice changed bounds, tail storage, or capacity\n");
		goto done;
	}

	List_removeAll(list);
	if (!append_values(list, values, sizeof(values) / sizeof(values[0]))) {
		fprintf(stderr, "list refill failed\n");
		goto done;
	}
	previous_size = list->size;
	List_sliceInPlace(list, 1, 6);
	if (!matches_slice(list, bounded_expected, 5, previous_size) ||
		list->memSize != capacity) {
		fprintf(stderr, "end index equal to size was not clamped to the final item\n");
		goto done;
	}

	List_removeAll(list);
	if (!append_values(list, values, sizeof(values) / sizeof(values[0]))) {
		fprintf(stderr, "list refill for negative slice failed\n");
		goto done;
	}
	previous_size = list->size;
	List_sliceInPlace(list, -3, -1);
	if (!matches_slice(list, tail_expected, 3, previous_size) ||
		list->memSize != capacity) {
		fprintf(stderr, "negative-index slice returned the wrong range\n");
		goto done;
	}

	previous_size = list->size;
	List_sliceInPlace(list, 3, 3);
	if (!matches_slice(list, NULL, 0, previous_size) ||
		list->memSize != capacity) {
		fprintf(stderr, "a start index equal to size did not clear the slice\n");
		goto done;
	}
	List_removeAll(list);
	if (!append_values(list, values, sizeof(values) / sizeof(values[0]))) {
		fprintf(stderr, "list refill for reversed slice failed\n");
		goto done;
	}
	previous_size = list->size;
	List_sliceInPlace(list, 2, 1);
	if (!matches_slice(list, NULL, 0, previous_size) ||
		list->memSize != capacity) {
		fprintf(stderr, "reversed slice did not clear the old range safely\n");
		goto done;
	}
	List_sliceInPlace(list, 0, -1);
	if (list->size != 0) {
		fprintf(stderr, "slicing an empty list changed its size\n");
		goto done;
	}

	result = EXIT_SUCCESS;
	puts("list slice test passed");

done:
	List_free(list);
	return result;
}
