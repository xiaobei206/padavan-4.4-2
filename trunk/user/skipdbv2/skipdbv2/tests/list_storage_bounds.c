#include "List.h"

#include <stdio.h>
#include <stdlib.h>

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "%s\n", message);
	return 0;
}

int main(void)
{
	int values[] = { 10, 20, 30 };
	List *source = List_new();
	List *target = List_new();
	List *clone = NULL;
	List malformed_source;
	List malformed_target;
	UArray invalid_view;
	void *saved_target_item;

	if (NULL == source || NULL == target) {
		fprintf(stderr, "list fixture allocation failed\n");
		List_free(source);
		List_free(target);
		return EXIT_FAILURE;
	}
	List_append_(source, &values[0]);
	List_append_(source, &values[1]);
	List_append_(target, &values[2]);
	if (!check(source->size == 2 && target->size == 1,
		"list fixtures were not initialized"))
		goto failure;
	clone = List_clone(source);
	if (!check(clone != NULL && List_equals_(source, clone),
		"valid list clone/equality changed unexpectedly"))
		goto failure;

	malformed_source = *source;
	malformed_source.memSize = sizeof(void *);
	List_copy_(target, &malformed_source);
	if (!check(target->size == 1 && target->items[0] == &values[2],
		"copy read from a source whose capacity is smaller than its size") ||
		!check(List_equals_(source, &malformed_source) == 0,
		"equality accepted source metadata beyond its capacity") ||
		!check(List_clone(&malformed_source) == NULL,
		"clone accepted source metadata beyond its capacity"))
		goto failure;

	malformed_target = *target;
	malformed_target.memSize = sizeof(void *);
	malformed_target.size = 2;
	saved_target_item = malformed_target.items[0];
	List_copy_(&malformed_target, source);
	List_setSize_(&malformed_target, 0);
	List_preallocateToSize_(&malformed_target, 8);
	List_compact(&malformed_target);
	List_removeAll(&malformed_target);
	List_sliceInPlace(&malformed_target, 0, 0);
	if (!check(malformed_target.size == 2 &&
		malformed_target.items[0] == saved_target_item,
		"mutating list operations modified a destination with invalid capacity"))
		goto failure;

	malformed_source.items = NULL;
	invalid_view = List_asStackAllocatedUArray(&malformed_source);
	if (!check(invalid_view.size == 0 && invalid_view.data == NULL,
		"invalid list storage was exposed as a UArray view"))
		goto failure;

	List_free(clone);
	List_free(source);
	List_free(target);
	puts("list storage bounds test passed");
	return EXIT_SUCCESS;

failure:
	List_free(clone);
	List_free(source);
	List_free(target);
	return EXIT_FAILURE;
}
