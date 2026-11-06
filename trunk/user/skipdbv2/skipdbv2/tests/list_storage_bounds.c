#include "List.h"

#include <stdio.h>
#include <stdlib.h>

static size_t callback_count;
static List *safe_iteration_list;
static size_t safe_iteration_count;

static void count_visit(void *item)
{
	(void)item;
	callback_count++;
}

static void shrink_safe_iteration(void *item)
{
	(void)item;
	safe_iteration_count++;
	if (safe_iteration_count == 1)
		List_setSize_(safe_iteration_list, 1);
}

static void *remove_everything(void *item)
{
	callback_count++;
	return item;
}

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
	void *saved_target_items[2];

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

	safe_iteration_list = source;
	safe_iteration_count = 0;
	LIST_SAFEFOREACH(source, live_index, live_value, shrink_safe_iteration(live_value));
	if (!check(safe_iteration_count == 1 && source->size == 1,
		"safe List iteration ignored a live size reduction"))
		goto failure;
	List_setSize_(source, 2);
	List_at_put_(source, 1, &values[1]);


	malformed_source = *source;
	malformed_source.memSize = sizeof(void *);
	callback_count = 0;
	if (!check(List_rawAt_(&malformed_source, 0) == NULL &&
		List_at_(&malformed_source, 0) == NULL &&
		List_indexOf_(&malformed_source, &values[0]) == -1 &&
		!List_contains_(&malformed_source, &values[0]) &&
		List_first(&malformed_source) == NULL,
		"inline list readers accepted metadata beyond capacity"))
		goto failure;
	LIST_FOREACH(&malformed_source, inline_index, inline_value, ++callback_count);
	LIST_SAFEFOREACH(&malformed_source, safe_index, safe_value, ++callback_count);
	LIST_REVERSEFOREACH(&malformed_source, reverse_index, reverse_value, ++callback_count);
	LIST_SAFEREVERSEFOREACH(&malformed_source, safe_reverse_index, safe_reverse_value, ++callback_count);
	LIST_DO_(&malformed_source, count_visit);
	List_do_(&malformed_source, count_visit);
	if (!check(callback_count == 0,
		"list iteration visited entries beyond declared capacity"))
		goto failure;
	List_appendSeq_(target, &malformed_source);
	if (!check(target->size == 1 && target->items[0] == &values[2],
		"append sequence accepted a source whose capacity is smaller than its size"))
		goto failure;
	List_copy_(target, &malformed_source);
	if (!check(target->size == 1 && target->items[0] == &values[2],
		"copy read from a source whose capacity is smaller than its size") ||
		!check(List_equals_(source, &malformed_source) == 0,
		"equality accepted source metadata beyond its capacity") ||
		!check(List_clone(&malformed_source) == NULL,
		"clone accepted source metadata beyond its capacity"))
		goto failure;


	malformed_target = *source;
	malformed_target.memSize = sizeof(void *);
	saved_target_items[0] = malformed_target.items[0];
	saved_target_items[1] = malformed_target.items[1];

	callback_count = 0;
	if (!check(List_rawAt_(&malformed_target, 0) == NULL &&
		List_at_(&malformed_target, 0) == NULL &&
		List_indexOf_(&malformed_target, &values[0]) == -1 &&
		!List_contains_(&malformed_target, &values[0]) &&
		List_top(&malformed_target) == NULL &&
		List_last(&malformed_target) == NULL,
		"inline list readers accepted a malformed destination"))
		goto failure;
	LIST_FOREACH(&malformed_target, inline_index, inline_value, ++callback_count);
	LIST_SAFEFOREACH(&malformed_target, safe_index, safe_value, ++callback_count);
	LIST_REVERSEFOREACH(&malformed_target, reverse_index, reverse_value, ++callback_count);
	LIST_SAFEREVERSEFOREACH(&malformed_target, safe_reverse_index, safe_reverse_value, ++callback_count);
	LIST_DO_(&malformed_target, count_visit);
	List_do_(&malformed_target, count_visit);
	List_append_(&malformed_target, &values[2]);
	List_push_(&malformed_target, &values[2]);
	List_at_insert_(&malformed_target, 0, &values[2]);
	List_at_put_(&malformed_target, 0, &values[2]);
	List_removeIndex_(&malformed_target, 0);
	List_removeIndex_toIndex_(&malformed_target, 0, 2);
	List_remove_(&malformed_target, &values[0]);
	List_removeFirst_(&malformed_target, &values[0]);
	List_removeLast_(&malformed_target, &values[1]);
	List_removeItems_(&malformed_target, target);
	List_removeTrueFor_(&malformed_target, remove_everything);
	List_reverse(&malformed_target);
	(void)List_pop(&malformed_target);
	if (!check(callback_count == 0 &&
		malformed_target.size == 2 &&
		malformed_target.memSize == sizeof(void *) &&
		malformed_target.items[0] == saved_target_items[0] &&
		malformed_target.items[1] == saved_target_items[1],
		"inline list mutation or traversal changed malformed backing storage"))
		goto failure;
	List_copy_(&malformed_target, source);
	List_setSize_(&malformed_target, 0);
	List_preallocateToSize_(&malformed_target, 8);
	List_compact(&malformed_target);
	List_removeAll(&malformed_target);
	List_sliceInPlace(&malformed_target, 0, 0);
	if (!check(malformed_target.size == 2 &&
		malformed_target.items[0] == saved_target_items[0] &&
		malformed_target.items[1] == saved_target_items[1],
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
