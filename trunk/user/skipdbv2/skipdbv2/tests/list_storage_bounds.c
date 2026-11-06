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


static List *map_callback_list;
static void **map_callback_original_items;
static void *map_callback_replacement;
static size_t map_callback_target_size;
static size_t map_callback_calls;
static int map_callback_storage_moved;

static void *expand_list_during_map(void *item)
{
    (void)item;
    if (map_callback_calls++ == 0) {
        List_preallocateToSize_(map_callback_list, map_callback_target_size);
        map_callback_storage_moved =
            map_callback_list->items != map_callback_original_items;
    }
    return map_callback_replacement;
}

static void *shrink_list_during_map(void *item)
{
    (void)item;
    map_callback_calls++;
    List_setSize_(map_callback_list, 0);
    return map_callback_replacement;
}


static List *filter_callback_list;
static void **filter_callback_original_items;
static void *filter_callback_remove_a;
static void *filter_callback_remove_b;
static size_t filter_callback_target_size;
static size_t filter_callback_calls;
static int filter_callback_storage_moved;

static void *expand_list_during_filter(void *item)
{
    if (filter_callback_calls++ == 0) {
        List_preallocateToSize_(filter_callback_list, filter_callback_target_size);
        filter_callback_storage_moved =
            filter_callback_list->items != filter_callback_original_items;
    }
    return (item == filter_callback_remove_a || item == filter_callback_remove_b)
        ? filter_callback_list->items[0] : NULL;
}

static void *shrink_list_during_filter(void *item)
{
    (void)item;
    filter_callback_calls++;
    List_setSize_(filter_callback_list, 1);
    return NULL;
}

static int test_remove_true_for_reentrancy(void)
{
    int values[] = {10, 20, 30, 40};
    List *filter_list = List_new();
    List *shrink_list = List_new();
    void *blocker = NULL;
    int removed;
    int result = EXIT_FAILURE;

    if (NULL == filter_list || NULL == shrink_list) {
        check(0, "List filter fixture allocation failed");
        goto done;
    }
    if (List_append_(filter_list, &values[0]) != &values[0] ||
        List_append_(filter_list, &values[1]) != &values[1] ||
        List_append_(filter_list, &values[2]) != &values[2] ||
        List_append_(filter_list, &values[3]) != &values[3] ||
        List_append_(shrink_list, &values[0]) != &values[0] ||
        List_append_(shrink_list, &values[1]) != &values[1] ||
        List_append_(shrink_list, &values[2]) != &values[2]) {
        check(0, "List filter fixture append failed");
        goto done;
    }

    blocker = malloc(filter_list->memSize);
    if (NULL == blocker) {
        check(0, "List filter realloc blocker allocation failed");
        goto done;
    }
    memset(blocker, 0x5a, filter_list->memSize);

    filter_callback_list = filter_list;
    filter_callback_original_items = filter_list->items;
    filter_callback_remove_a = &values[1];
    filter_callback_remove_b = &values[3];
    filter_callback_target_size =
        filter_list->memSize / sizeof(void *) + 1024;
    filter_callback_calls = 0;
    filter_callback_storage_moved = 0;

    removed = List_removeTrueFor_(filter_list, expand_list_during_filter);
    if (!check(filter_callback_storage_moved,
        "List predicate-filter callback did not move backing allocation"))
        goto done;
    if (!check(filter_callback_calls == 4,
        "List_removeTrueFor_ did not invoke callback for every nonnull item"))
        goto done;
    if (!check(removed == 2 && filter_list->size == 2 &&
        filter_list->items[0] == &values[0] &&
        filter_list->items[1] == &values[2],
        "List_removeTrueFor_ produced incorrect filtered survivors or removal count"))
        goto done;

    filter_callback_list = shrink_list;
    filter_callback_calls = 0;
    removed = List_removeTrueFor_(shrink_list, shrink_list_during_filter);
    if (!check(removed == 0 && filter_callback_calls == 1 &&
        shrink_list->size == 1 && shrink_list->items[0] == &values[0],
        "List_removeTrueFor_ continued filtering after its callback changed list size")) {
        goto done;
    }

    result = EXIT_SUCCESS;

done:
    free(blocker);
    List_free(filter_list);
    List_free(shrink_list);
    filter_callback_list = NULL;
    filter_callback_original_items = NULL;
    filter_callback_remove_a = NULL;
    filter_callback_remove_b = NULL;
    filter_callback_target_size = 0;
    filter_callback_calls = 0;
    filter_callback_storage_moved = 0;
    return result;
}

static int test_map_in_place_reentrancy(void)
{
    int values[] = {10, 20, 30};
    List *map_list = List_new();
    List *shrink_list = List_new();
    void *blocker = NULL;
    size_t i;
    int result = EXIT_FAILURE;

    if (NULL == map_list || NULL == shrink_list) {
        check(0, "List map-in-place fixture allocation failed");
        goto done;
    }

    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (List_append_(map_list, &values[i]) != &values[i] ||
            List_append_(shrink_list, &values[i]) != &values[i]) {
            check(0, "List map-in-place fixture append failed");
            goto done;
        }
    }

    /* Occupy the following heap block so the callback's larger realloc moves. */
    blocker = malloc(map_list->memSize);
    if (NULL == blocker) {
        check(0, "List map-in-place realloc blocker allocation failed");
        goto done;
    }
    memset(blocker, 0xa5, map_list->memSize);

    map_callback_list = map_list;
    map_callback_original_items = map_list->items;
    map_callback_replacement = &values[2];
    map_callback_target_size =
        map_list->memSize / sizeof(void *) + 1024;
    map_callback_calls = 0;
    map_callback_storage_moved = 0;

    List_mapInPlace_(map_list, expand_list_during_map);
    if (!check(map_callback_storage_moved && map_callback_calls == 3 &&
        map_list->size == 3 &&
        map_list->items[0] == map_callback_replacement &&
        map_list->items[1] == map_callback_replacement &&
        map_list->items[2] == map_callback_replacement,
        "List map-in-place wrote through a stale backing pointer after callback realloc")) {
        goto done;
    }

    map_callback_list = shrink_list;
    map_callback_calls = 0;
    List_mapInPlace_(shrink_list, shrink_list_during_map);
    if (!check(map_callback_calls == 1 && shrink_list->size == 0 &&
        (NULL == shrink_list->items || NULL == shrink_list->items[0]),
        "List map-in-place wrote an item after its callback changed the list size")) {
        goto done;
    }

    List_mapInPlace_(NULL, expand_list_during_map);
    List_mapInPlace_(map_list, NULL);
    result = EXIT_SUCCESS;

done:
    free(blocker);
    List_free(map_list);
    List_free(shrink_list);
    map_callback_list = NULL;
    map_callback_original_items = NULL;
    map_callback_replacement = NULL;
    map_callback_target_size = 0;
    map_callback_calls = 0;
    map_callback_storage_moved = 0;
    return result;
}


static List *traversal_callback_list;
static size_t traversal_callback_calls;

static void shrink_metadata_for_do(void *item)
{
    (void)item;
    traversal_callback_calls++;
    if (traversal_callback_calls == 1) {
        traversal_callback_list->size = 1;
        traversal_callback_list->memSize = sizeof(void *);
    }
}

static void shrink_metadata_for_do_with(void *item, void *arg)
{
    (void)item;
    (void)arg;
    traversal_callback_calls++;
    if (traversal_callback_calls == 1) {
        traversal_callback_list->size = 1;
        traversal_callback_list->memSize = sizeof(void *);
    }
}

static int shrink_metadata_for_detect(void *item)
{
    (void)item;
    traversal_callback_calls++;
    if (traversal_callback_calls == 1) {
        traversal_callback_list->size = 1;
        traversal_callback_list->memSize = sizeof(void *);
    }
    return 0;
}

static int append_three_values(List *list, int *values)
{
    return List_append_(list, &values[0]) == &values[0] &&
        List_append_(list, &values[1]) == &values[1] &&
        List_append_(list, &values[2]) == &values[2];
}

static int test_live_callback_traversals(void)
{
    int values[] = {10, 20, 30};
    List *do_list = List_new();
    List *do_with_list = List_new();
    List *detect_list = List_new();
    int result = EXIT_FAILURE;

    if (!do_list || !do_with_list || !detect_list) {
        check(0, "List live callback traversal allocation failed");
        goto done;
    }
    if (!append_three_values(do_list, values) ||
        !append_three_values(do_with_list, values) ||
        !append_three_values(detect_list, values)) {
        check(0, "List live callback traversal fixture append failed");
        goto done;
    }

    traversal_callback_list = do_list;
    traversal_callback_calls = 0;
    List_do_(do_list, shrink_metadata_for_do);
    if (!check(traversal_callback_calls == 1 && do_list->size == 1 &&
        do_list->memSize == sizeof(void *),
        "List_do_ visited entries beyond callback-reduced capacity"))
        goto done;

    traversal_callback_list = do_with_list;
    traversal_callback_calls = 0;
    List_do_with_(do_with_list, shrink_metadata_for_do_with, NULL);
    if (!check(traversal_callback_calls == 1 && do_with_list->size == 1 &&
        do_with_list->memSize == sizeof(void *),
        "List_do_with_ visited entries beyond callback-reduced capacity"))
        goto done;

    traversal_callback_list = detect_list;
    traversal_callback_calls = 0;
    if (!check(List_detect_(detect_list, shrink_metadata_for_detect) == NULL &&
        traversal_callback_calls == 1 && detect_list->size == 1 &&
        detect_list->memSize == sizeof(void *),
        "List_detect_ visited entries beyond callback-reduced capacity"))
        goto done;

    result = EXIT_SUCCESS;

done:
    traversal_callback_list = NULL;
    traversal_callback_calls = 0;
    List_free(do_list);
    List_free(do_with_list);
    List_free(detect_list);
    return result;
}

int main(void)
{
	if (test_live_callback_traversals() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (test_remove_true_for_reentrancy() != EXIT_SUCCESS)
		return EXIT_FAILURE;
	if (test_map_in_place_reentrancy() != EXIT_SUCCESS)
		return EXIT_FAILURE;


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
