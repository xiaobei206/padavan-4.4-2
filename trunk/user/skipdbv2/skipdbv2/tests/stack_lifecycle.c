#include "Stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define VALUE_COUNT 1300

static size_t stack_callback_count;


#define STACK_CALLBACK_SEEN_MAX 1024

static Stack *callback_iteration_stack;
static void *callback_iteration_extra;
static size_t callback_iteration_count;
static void *callback_iteration_seen[STACK_CALLBACK_SEEN_MAX];
static int callback_iteration_reallocated;

static void push_during_stack_iteration(void *value)
{
    if (callback_iteration_count < STACK_CALLBACK_SEEN_MAX)
        callback_iteration_seen[callback_iteration_count] = value;
    callback_iteration_count++;

    if (callback_iteration_count == 1) {
        void **original_items = callback_iteration_stack->items;
        Stack_push_(callback_iteration_stack, callback_iteration_extra);
        callback_iteration_reallocated =
            callback_iteration_stack->items != original_items;
    }
}

static void reset_stack_iteration_callback(Stack *stack, void *extra)
{
    callback_iteration_stack = stack;
    callback_iteration_extra = extra;
    callback_iteration_count = 0;
    callback_iteration_reallocated = 0;
}

static int test_callback_iteration_reentrancy(void)
{
    int values[STACK_CALLBACK_SEEN_MAX];
    int extra = -1;
    Stack *all_items = NULL;
    Stack *until_mark = NULL;
    void *all_items_blocker = NULL;
    void *until_mark_blocker = NULL;
    size_t all_count = 0;
    size_t marked_count = 0;
    size_t expected_until_count;
    intptr_t mark;
    size_t i;
    int result = EXIT_FAILURE;

    for (i = 0; i < STACK_CALLBACK_SEEN_MAX; ++i)
        values[i] = (int)i;

    all_items = Stack_new();
    if (NULL == all_items) {
        fprintf(stderr, "Stack callback iteration fixture allocation failed\n");
        goto done;
    }
    all_items_blocker = malloc((size_t)(all_items->memEnd - all_items->items) * sizeof(void *));
    if (NULL == all_items_blocker) {
        fprintf(stderr, "Stack callback iteration realloc blocker allocation failed\n");
        goto done;
    }
    memset(all_items_blocker, 0xa5,
        (size_t)(all_items->memEnd - all_items->items) * sizeof(void *));

    while (all_items->top < all_items->memEnd - 1 &&
        all_count < STACK_CALLBACK_SEEN_MAX) {
        Stack_push_(all_items, &values[all_count]);
        all_count++;
    }
    if (all_items->top != all_items->memEnd - 1 || all_count == 0) {
        fprintf(stderr, "Stack callback iteration fixture did not fill its storage\n");
        goto done;
    }

    reset_stack_iteration_callback(all_items, &extra);
    Stack_do_(all_items, push_during_stack_iteration);
    if (!callback_iteration_reallocated || callback_iteration_count != all_count ||
        Stack_totalSize(all_items) != all_count + 1 ||
        Stack_top(all_items) != &extra) {
        fprintf(stderr, "Stack_do_ was not isolated from callback-time source reallocation\n");
        goto done;
    }
    for (i = 0; i < all_count; ++i) {
        if (callback_iteration_seen[i] != &values[all_count - i - 1]) {
            fprintf(stderr, "Stack_do_ changed reverse snapshot traversal order\n");
            goto done;
        }
    }

    until_mark = Stack_new();
    if (NULL == until_mark) {
        fprintf(stderr, "Stack do-until-mark fixture allocation failed\n");
        goto done;
    }
    until_mark_blocker = malloc((size_t)(until_mark->memEnd - until_mark->items) * sizeof(void *));
    if (NULL == until_mark_blocker) {
        fprintf(stderr, "Stack do-until-mark realloc blocker allocation failed\n");
        goto done;
    }
    memset(until_mark_blocker, 0x5a,
        (size_t)(until_mark->memEnd - until_mark->items) * sizeof(void *));

    Stack_push_(until_mark, &values[0]);
    mark = Stack_pushMarkPoint(until_mark);
    if (mark <= 0) {
        fprintf(stderr, "Stack do-until-mark fixture mark creation failed\n");
        goto done;
    }
    marked_count = 1;
    while (until_mark->top < until_mark->memEnd - 1 &&
        marked_count < STACK_CALLBACK_SEEN_MAX) {
        Stack_push_(until_mark, &values[marked_count]);
        marked_count++;
    }
    if (until_mark->top != until_mark->memEnd - 1) {
        fprintf(stderr, "Stack do-until-mark fixture did not fill its storage\n");
        goto done;
    }
    expected_until_count = Stack_totalSize(until_mark) - (size_t)mark;

    reset_stack_iteration_callback(until_mark, &extra);
    Stack_doUntilMark_(until_mark, push_during_stack_iteration);
    if (!callback_iteration_reallocated ||
        callback_iteration_count != expected_until_count ||
        Stack_totalSize(until_mark) != expected_until_count + (size_t)mark + 1 ||
        Stack_top(until_mark) != &extra) {
        fprintf(stderr, "Stack_doUntilMark_ was not isolated from callback-time source reallocation\n");
        goto done;
    }
    for (i = 0; i < expected_until_count; ++i) {
        if (callback_iteration_seen[i] != &values[marked_count - i - 1]) {
            fprintf(stderr, "Stack_doUntilMark_ changed mark-bounded snapshot traversal order\n");
            goto done;
        }
    }

    result = EXIT_SUCCESS;

done:
    free(until_mark_blocker);
    free(all_items_blocker);
    Stack_free(until_mark);
    Stack_free(all_items);
    callback_iteration_stack = NULL;
    callback_iteration_extra = NULL;
    callback_iteration_count = 0;
    callback_iteration_reallocated = 0;
    return result;
}


static void count_stack_item(void *value)
{
	(void)value;
	++stack_callback_count;
}


static int test_misaligned_pointer_metadata(void)
{
    Stack *source = NULL;
    Stack *destination = NULL;
    Stack malformed;
    void **savedTop;
    uintptr_t malformedTop;
    int values[] = { 17, 29 };
    int result = EXIT_FAILURE;

    source = Stack_new();
    destination = Stack_new();
    if (NULL == source || NULL == destination)
        goto done;
    Stack_push_(source, &values[0]);
    Stack_push_(destination, &values[1]);
    malformed = *source;
    savedTop = malformed.top;
    malformedTop = (uintptr_t)malformed.items + 1u;
    malformed.top = (void **)malformedTop;

    if (Stack_hasValidStorage_(&malformed, NULL, NULL) ||
        Stack_totalSize(&malformed) != 0 ||
        Stack_count(&malformed) != 0 ||
        Stack_memorySize(&malformed) != 0 ||
        Stack_top(&malformed) != NULL ||
        Stack_at_(&malformed, 0) != NULL ||
        Stack_pop(&malformed) != NULL ||
        Stack_popMarkPoint_(&malformed, 1) != 0 ||
        Stack_clone(&malformed) != NULL) {
        fprintf(stderr, "misaligned Stack cursor was accepted\\n");
        goto done;
    }

    Stack_push_(&malformed, &values[1]);
    Stack_clear(&malformed);
    if ((uintptr_t)malformed.top != malformedTop) {
        fprintf(stderr, "invalid Stack cursor was modified by push or clear\\n");
        goto done;
    }

    Stack_copy_(destination, &malformed);
    if (Stack_top(destination) != &values[1] ||
        Stack_totalSize(destination) != 1) {
        fprintf(stderr, "copy from malformed Stack changed the destination\\n");
        goto done;
    }

    malformed.top = savedTop;
    result = EXIT_SUCCESS;

done:
    Stack_free(destination);
    Stack_free(source);
    return result;
}

static int test_copy_preserves_shared_backing(void)
{
	Stack *source = Stack_new();
	Stack alias = {0};
	int values[] = {31, 47};
	int okay = 0;

	if (NULL == source)
		return 0;
	Stack_push_(source, &values[0]);
	Stack_push_(source, &values[1]);
	if (Stack_totalSize(source) != 2 || Stack_top(source) != &values[1])
		goto done;

	/* Copying into a shallow struct alias must not free source->items. */
	alias = *source;
	Stack_copy_(&alias, source);
	if (alias.items == source->items ||
		Stack_totalSize(source) != 2 || Stack_top(source) != &values[1] ||
		Stack_totalSize(&alias) != 2 || Stack_top(&alias) != &values[1] ||
		Stack_at_(source, 0) != &values[0] ||
		Stack_at_(&alias, 0) != &values[0])
		goto done;
	okay = 1;

done:
	if (alias.items && alias.items != source->items)
		io_free(alias.items);
	Stack_free(source);
	return okay;
}


int main(void)
{
	if (!test_copy_preserves_shared_backing()) {
		fprintf(stderr, "Stack shallow aliases must not free the source backing allocation\\n");
		return EXIT_FAILURE;
	}
	if (test_callback_iteration_reentrancy() != EXIT_SUCCESS ||
		test_misaligned_pointer_metadata() != EXIT_SUCCESS)
		return EXIT_FAILURE;


	Stack *source = NULL;
	Stack *destination = NULL;
	Stack *clone = NULL;
	Stack *marks = NULL;
	Stack *invalid_chain = NULL;
	int values[VALUE_COUNT];
	intptr_t outer_mark;
	size_t i;
	int result = EXIT_FAILURE;

	Stack_free(NULL);
	Stack_copy_(NULL, NULL);
	Stack_resize(NULL);
	Stack_compact(NULL);
	Stack_clear(NULL);
	Stack_push_(NULL, NULL);
	if (Stack_pop(NULL) != NULL || Stack_top(NULL) != NULL ||
		Stack_at_(NULL, 0) != NULL || Stack_popMarkPoint_(NULL, 0) != 0 ||
		Stack_newCopyWithNullMarks(NULL) != NULL) {
		fprintf(stderr, "null Stack operations were not safe\\n");
		goto done;
	}

	source = Stack_new();
	destination = Stack_new();
	marks = Stack_new();
	if (NULL == source || NULL == destination || NULL == marks) {
		fprintf(stderr, "Stack allocation failed\\n");
		goto done;
	}

	for (i = 0; i < VALUE_COUNT; ++i) {
		values[i] = (int)i;
		Stack_push_(source, &values[i]);
	}
	if (Stack_totalSize(source) != VALUE_COUNT ||
		Stack_count(source) != VALUE_COUNT ||
		Stack_top(source) != &values[VALUE_COUNT - 1] ||
		Stack_at_(source, 0) != &values[0] ||
		Stack_at_(source, VALUE_COUNT - 1) != &values[VALUE_COUNT - 1]) {
		fprintf(stderr, "stack growth or indexed access failed\\n");
		goto done;
	}

	for (i = 0; i < 17; ++i)
		Stack_push_(destination, &values[i]);
	Stack_copy_(destination, source);
	if (Stack_totalSize(destination) != VALUE_COUNT ||
		Stack_top(destination) != &values[VALUE_COUNT - 1] ||
		Stack_at_(destination, 0) != &values[0] ||
		Stack_at_(destination, VALUE_COUNT - 1) != &values[VALUE_COUNT - 1]) {
		fprintf(stderr, "copy did not adopt the source stack state\\n");
		goto done;
	}

	clone = Stack_clone(source);
	if (NULL == clone || Stack_totalSize(clone) != VALUE_COUNT ||
		Stack_top(clone) != &values[VALUE_COUNT - 1] ||
		Stack_at_(clone, VALUE_COUNT - 1) != &values[VALUE_COUNT - 1]) {
		fprintf(stderr, "stack clone failed\\n");
		goto done;
	}

	Stack_compact(source);
	if (Stack_memorySize(source) != sizeof(Stack) +
		(size_t)(source->memEnd - source->items) * sizeof(void *)) {
		fprintf(stderr, "stack memory size does not match allocated storage\\n");
		goto done;
	}
	for (i = VALUE_COUNT; i > 0; --i) {
		if (Stack_pop(source) != &values[i - 1]) {
			fprintf(stderr, "stack pop order changed after compaction\\n");
			goto done;
		}
	}
	if (Stack_pop(source) != NULL || Stack_totalSize(source) != 0) {
		fprintf(stderr, "empty stack pop was not safe\\n");
		goto done;
	}

	Stack_push_(marks, &values[0]);
	outer_mark = Stack_pushMarkPoint(marks);
	Stack_push_(marks, &values[1]);
	if (outer_mark <= 0 || Stack_pushMarkPoint(marks) <= outer_mark) {
		fprintf(stderr, "nested stack mark creation failed\\n");
		goto done;
	}
	Stack_push_(marks, &values[2]);
	if (Stack_popMarkPoint_(marks, 0) != 0 ||
		Stack_totalSize(marks) != 5) {
		fprintf(stderr, "invalid zero mark changed stack state\\n");
		goto done;
	}
	{
		Stack invalid_bounds = *marks;
		invalid_bounds.top = invalid_bounds.memEnd;
		invalid_bounds.lastMark = outer_mark;
		stack_callback_count = 0;
		if (Stack_totalSize(&invalid_bounds) != 0 ||
			Stack_count(&invalid_bounds) != 0 ||
			Stack_at_(&invalid_bounds, 0) != NULL ||
			Stack_top(&invalid_bounds) != NULL ||
			Stack_popMarkPoint_(&invalid_bounds, outer_mark) != 0) {
			fprintf(stderr, "one-past-end stack cursor was accepted\\n");
			goto done;
		}
		Stack_do_(&invalid_bounds, count_stack_item);
		Stack_doUntilMark_(&invalid_bounds, count_stack_item);
		if (stack_callback_count != 0) {
			fprintf(stderr, "stack bounds callback ran on invalid storage\\n");
			goto done;
		}
	}
	{
		Stack invalid_mark = *marks;
		void **saved_items = invalid_mark.items;
		void **saved_top = invalid_mark.top;
		void **saved_memEnd = invalid_mark.memEnd;
		Stack *invalid_clone;

		invalid_mark.lastMark = -1;
		invalid_clone = Stack_clone(&invalid_mark);
		if (invalid_clone != NULL) {
			Stack_free(invalid_clone);
			fprintf(stderr, "negative stack mark was accepted by clone\\n");
			goto done;
		}
		Stack_compact(&invalid_mark);
		if (Stack_memorySize(&invalid_mark) != 0 ||
			invalid_mark.items != saved_items || invalid_mark.top != saved_top ||
			invalid_mark.memEnd != saved_memEnd || invalid_mark.lastMark != -1) {
			fprintf(stderr, "negative stack mark changed state during validation\\n");
			goto done;
		}
	}
	invalid_chain = Stack_clone(marks);
	if (NULL == invalid_chain) {
		fprintf(stderr, "malformed mark-chain clone failed\\n");
		goto done;
	}
	{
		intptr_t inner_mark = invalid_chain->lastMark;
		void **saved_top = invalid_chain->top;
		intptr_t saved_last_mark = invalid_chain->lastMark;

		/* Corrupt the target mark's predecessor, not the newer mark. */
		invalid_chain->items[outer_mark] = (void *)outer_mark;
		if (Stack_popMarkPoint_(invalid_chain, outer_mark) != 0 ||
			invalid_chain->top != saved_top ||
			invalid_chain->lastMark != saved_last_mark ||
			Stack_totalSize(invalid_chain) != 5 ||
			Stack_top(invalid_chain) != &values[2]) {
			fprintf(stderr, "invalid target mark partially changed the stack\\n");
			goto done;
		}

		/* A malformed current mark must also be rejected before popping. */
		invalid_chain->items[inner_mark] = (void *)inner_mark;
		saved_top = invalid_chain->top;
		saved_last_mark = invalid_chain->lastMark;
		Stack_popMark(invalid_chain);
		if (invalid_chain->top != saved_top ||
			invalid_chain->lastMark != saved_last_mark) {
			fprintf(stderr, "invalid current mark changed the stack\\n");
			goto done;
		}
	}
	Stack_free(invalid_chain);
	invalid_chain = NULL;

	if (!Stack_popMarkPoint_(marks, outer_mark) ||
		Stack_totalSize(marks) != 1 || Stack_top(marks) != &values[0]) {
		fprintf(stderr, "nested mark rollback failed\\n");
		goto done;
	}

	Stack_pushMark(marks);
	Stack_push_(marks, &values[3]);
	Stack_clearTop(marks);
	if (Stack_totalSize(marks) != 2 || Stack_top(marks) != NULL) {
		fprintf(stderr, "clear-top did not retain an empty mark\\n");
		goto done;
	}
	Stack_copy_(destination, marks);
	Stack_popMark(destination);
	if (Stack_totalSize(destination) != 1 ||
		Stack_top(destination) != &values[0]) {
		fprintf(stderr, "copied mark chain was not usable\\n");
		goto done;
	}

	result = EXIT_SUCCESS;
	puts("stack lifecycle test passed");

done:
	Stack_free(invalid_chain);
	Stack_free(clone);
	Stack_free(marks);
	Stack_free(destination);
	Stack_free(source);
	return result;
}
