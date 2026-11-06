//metadoc Stack copyright Steve Dekorte 2002
//metadoc Stack license BSD revised
/*metadoc Stack description
Notes: first element of items is always 0x0.
*/

#define STACK_C
#include "Stack.h"
#undef STACK_C
#if !defined(_MSC_VER)
#include <inttypes.h>
#endif

static int Stack_isValid_(const Stack *self, size_t *used, size_t *capacity)
{
	size_t active, allocated;
	/* mark-chain validation is shared with inline mark operations. */

	if (!Stack_hasValidStorage_(self, &active, &allocated) ||
		!Stack_hasValidMarkChain_(self, active))
		return 0;

	if (used)
		*used = active;
	if (capacity)
		*capacity = allocated;
	return 1;
}

static int Stack_copyState_(Stack *self, const Stack *other)
{
	Stack copy;
	void **newItems, **oldItems;
	size_t used, slots;

	if (NULL == self || !Stack_isValid_(other, &used, NULL) ||
		used == (size_t)-1 || used + 1 > (size_t)-1 / sizeof(void *))
		return 0;

	slots = used + 1;
	newItems = (void **)io_malloc(slots * sizeof(void *));
	if (NULL == newItems)
		return 0;
	memcpy(newItems, other->items, slots * sizeof(void *));

	copy = *other;
	copy.items = newItems;
	copy.memEnd = newItems + slots;
	copy.top = newItems + used;
	oldItems = Stack_hasValidStorage_(self, NULL, NULL) ? self->items : NULL;
	*self = copy;
	io_free(oldItems);
	return 1;
}

Stack *Stack_new(void)
{
	Stack *self;
	size_t slots = STACK_START_SIZE;

	if (slots == 0 || slots > (size_t)-1 / sizeof(void *))
		return NULL;
	self = (Stack *)io_calloc(1, sizeof(Stack));
	if (NULL == self)
		return NULL;

	self->items = (void **)io_calloc(slots, sizeof(void *));
	if (NULL == self->items) {
		io_free(self);
		return NULL;
	}
	self->memEnd = self->items + slots;
	self->top = self->items;
	return self;
}

void Stack_free(Stack *self)
{
	if (NULL == self)
		return;
	io_free(self->items);
	io_free(self);
}

Stack *Stack_clone(const Stack *self)
{
	Stack *child = (Stack *)io_calloc(1, sizeof(Stack));
	if (NULL == child)
		return NULL;
	if (!Stack_copyState_(child, self)) {
		io_free(child);
		return NULL;
	}
	return child;
}

void Stack_copy_(Stack *self, const Stack *other)
{
	if (NULL == self || self == other)
		return;
	(void)Stack_copyState_(self, other);
}

// stack --------------------------------------------------

size_t Stack_memorySize(const Stack *self)
{
	size_t capacity;
	if (!Stack_isValid_(self, NULL, &capacity) ||
		capacity > ((size_t)-1 - sizeof(Stack)) / sizeof(void *))
		return 0;
	return sizeof(Stack) + capacity * sizeof(void *);
}

void Stack_compact(Stack *self)
{
	size_t used, capacity, required;
	void **newItems;

	if (!Stack_isValid_(self, &used, &capacity))
		return;
	required = used + 1;
	if (required == capacity ||
		required > (size_t)-1 / sizeof(void *))
		return;

	newItems = (void **)io_realloc(self->items, required * sizeof(void *));
	if (NULL == newItems)
		return;
	self->items = newItems;
	self->top = newItems + used;
	self->memEnd = newItems + required;
}

void Stack_resize(Stack *self)
{
	size_t used, capacity, newCapacity;
	void **newItems;

	if (!Stack_isValid_(self, &used, &capacity) ||
		STACK_RESIZE_FACTOR < 2 ||
		capacity > (size_t)-1 / STACK_RESIZE_FACTOR)
		return;
	newCapacity = capacity * STACK_RESIZE_FACTOR;
	if (newCapacity > (size_t)-1 / sizeof(void *))
		return;

	newItems = (void **)io_realloc(self->items,
		newCapacity * sizeof(void *));
	if (NULL == newItems)
		return;
	self->items = newItems;
	self->top = newItems + used;
	self->memEnd = newItems + newCapacity;
}

// sizing ------------------------------------------------

void Stack_do_on_(const Stack *self, StackDoOnCallback *callback, void *target)
{
	Stack *stack;
	size_t i, count;

	if (NULL == self || NULL == callback)
		return;
	stack = Stack_newCopyWithNullMarks(self);
	if (NULL == stack)
		return;

	count = Stack_totalSize(stack);
	for (i = 0; i < count; ++i) {
		void *value = stack->items[i + 1];
		if (value)
			(*callback)(target, value);
	}
	Stack_free(stack);
}

void Stack_makeMarksNull(Stack *self)
{
	intptr_t mark;
	size_t used;

	if (!Stack_isValid_(self, &used, NULL))
		return;
	mark = self->lastMark;
	while (mark > 0 && (size_t)mark <= used) {
		intptr_t previous = (intptr_t)self->items[mark];
		self->items[mark] = NULL;
		mark = previous;
	}
	self->lastMark = 0;
}

Stack *Stack_newCopyWithNullMarks(const Stack *self)
{
	Stack *newStack = Stack_clone(self);
	if (NULL == newStack)
		return NULL;
	Stack_makeMarksNull(newStack);
	return newStack;
}

void Stack_popToMark_(Stack *self, intptr_t mark)
{
	if (NULL == self)
		return;
	if (!Stack_popMarkPoint_(self, mark)) {
		printf("Stack error: unable to find mark %p in %p\\n",
			(void *)mark, (void *)self);
		exit(1);
	}
}

List *Stack_asList(const Stack *self) // slow
{
	List *list;
	if (NULL == self)
		return NULL;
	list = List_new();
	if (NULL == list)
		return NULL;
	Stack_do_on_(self, (StackDoOnCallback *)List_append_, list);
	return list;
}
