#include "Stack.h"
#include <stdio.h>
#include <stdlib.h>

#define VALUE_COUNT 1300

static size_t stack_callback_count;

static void count_stack_item(void *value)
{
	(void)value;
	++stack_callback_count;
}

int main(void)
{
	Stack *source = NULL;
	Stack *destination = NULL;
	Stack *clone = NULL;
	Stack *marks = NULL;
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
	Stack_free(clone);
	Stack_free(marks);
	Stack_free(destination);
	Stack_free(source);
	return result;
}
