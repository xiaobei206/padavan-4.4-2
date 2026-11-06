//metadoc Stack copyright Steve Dekorte 2002
//metadoc Stack license BSD revised
/*metadoc Stack description
 Stack - array of void pointers
 supports setting marks - when a mark is popped,
 all stack items above it are popped as well
 */

#ifdef STACK_C
#define IO_IN_C_FILE
#endif
#include "Common_inline.h"
#include <limits.h>
#include <stdint.h>
#ifdef IO_DECLARE_INLINES

/* Validate cursor addresses as integers before any pointer subtraction. */
IOINLINE int Stack_hasValidStorage_(const Stack *self, size_t *used, size_t *capacity)
{
	uintptr_t itemsAddress, topAddress, endAddress;
	uintptr_t usedBytes, allocatedBytes;
	size_t active, allocated;

	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd)
		return 0;

	itemsAddress = (uintptr_t)self->items;
	topAddress = (uintptr_t)self->top;
	endAddress = (uintptr_t)self->memEnd;
	if ((itemsAddress % sizeof(void *)) != 0 ||
		endAddress <= itemsAddress || topAddress < itemsAddress ||
		topAddress >= endAddress)
		return 0;

	usedBytes = topAddress - itemsAddress;
	allocatedBytes = endAddress - itemsAddress;
	if ((usedBytes % sizeof(void *)) != 0 ||
		(allocatedBytes % sizeof(void *)) != 0 ||
		usedBytes > (uintptr_t)((size_t)-1) ||
		allocatedBytes > (uintptr_t)((size_t)-1))
		return 0;

	active = (size_t)(usedBytes / sizeof(void *));
	allocated = (size_t)(allocatedBytes / sizeof(void *));
	if (active >= allocated)
		return 0;
	if (used)
		*used = active;
	if (capacity)
		*capacity = allocated;
	return 1;
}


IOINLINE int Stack_hasValidMarkChain_(const Stack *self, size_t used)
{
	intptr_t mark;
	if (NULL == self || self->lastMark < 0)
		return 0;
	mark = self->lastMark;
	while (mark > 0) {
		intptr_t previous;
		if ((size_t)mark > used)
			return 0;
		previous = (intptr_t)self->items[(size_t)mark];
		if (previous < 0 || previous >= mark)
			return 0;
		mark = previous;
	}
	return 1;
}

IOINLINE void Stack_do_(const Stack *self, StackDoCallback *callback)
{
	Stack *snapshot;
	void **itemP;
	intptr_t mark;

	if (NULL == self || NULL == callback)
		return;
	snapshot = Stack_clone(self);
	if (NULL == snapshot)
		return;

	itemP = snapshot->top;
	mark = snapshot->lastMark;
	while (itemP > snapshot->items)
	{
		if (mark > 0 && itemP - snapshot->items == mark)
		{
			intptr_t previous = (intptr_t)(*itemP);
			mark = previous >= 0 && previous < mark ? previous : 0;
		}
		else
		{
			(*callback)(*itemP);
		}
		itemP--;
	}
	Stack_free(snapshot);
}

IOINLINE void Stack_doUntilMark_(const Stack *self, StackDoCallback *callback)
{
	Stack *snapshot;
	void **itemP;
	intptr_t mark;

	if (NULL == self || NULL == callback)
		return;
	snapshot = Stack_clone(self);
	if (NULL == snapshot)
		return;

	itemP = snapshot->top;
	mark = snapshot->lastMark;
	while (itemP > snapshot->items)
	{
		if (mark > 0 && itemP - snapshot->items == mark)
			break;
		(*callback)(*itemP);
		itemP--;
	}
	Stack_free(snapshot);
}

IOINLINE void Stack_clear(Stack *self)
{
	if (!Stack_hasValidStorage_(self, NULL, NULL))
		return;
	self->top = self->items;
	self->lastMark = 0;
}

IOINLINE size_t Stack_totalSize(const Stack *self)
{
	size_t used;
	return Stack_hasValidStorage_(self, &used, NULL) ? used : 0;
}

IOINLINE int Stack_count(const Stack *self)
{
	size_t count = Stack_totalSize(self);
	return count > (size_t)INT_MAX ? INT_MAX : (int)count;
}

IOINLINE void Stack_push_(Stack *self, void *item)
{
	size_t used, capacity;
	if (!Stack_hasValidStorage_(self, &used, &capacity) ||
		!Stack_hasValidMarkChain_(self, used))
		return;

	if (used >= capacity - 1) {
		Stack_resize(self);
		if (!Stack_hasValidStorage_(self, &used, &capacity) ||
			!Stack_hasValidMarkChain_(self, used))
			return;
	}
	if (used >= capacity - 1)
		return;

	self->top = self->items + (used + 1);
	*self->top = item;
}

IOINLINE intptr_t Stack_pushMarkPoint(Stack *self)
{
	size_t oldUsed;

	size_t newUsed;
	if (!Stack_hasValidStorage_(self, &oldUsed, NULL) ||
		!Stack_hasValidMarkChain_(self, oldUsed))
		return 0;

	Stack_push_(self, (void *)self->lastMark);
	if (!Stack_hasValidStorage_(self, &newUsed, NULL) || newUsed == oldUsed)
		return 0;
	self->lastMark = (intptr_t)newUsed;
	return self->lastMark;
}

IOINLINE void Stack_pushMark(Stack *self)
{
	(void)Stack_pushMarkPoint(self);
}

IOINLINE void *Stack_pop(Stack *self)
{
	void *top;
	size_t used;

	if (!Stack_hasValidStorage_(self, &used, NULL) || used == 0 ||
		!Stack_hasValidMarkChain_(self, used))
		return NULL;
	top = *(self->top);
#ifdef STACK_POP_CALLBACK
	if (self->popCallback)
		self->popCallback(top);
#endif
	/* Popping a mark entry must unlink it from the nested-mark chain. */
	if (self->lastMark > 0 && (size_t)self->lastMark == used)
		self->lastMark = (intptr_t)top;
	self->top--;
	return top;
}

IOINLINE void Stack_popMark(Stack *self)
{
	intptr_t mark, previous;

	size_t used;
	if (!Stack_hasValidStorage_(self, &used, NULL) ||
		!Stack_hasValidMarkChain_(self, used))
		return;
	mark = self->lastMark;
	if (mark <= 0 || (size_t)mark > used)
		return;
	previous = (intptr_t)self->items[mark];
	if (previous < 0 || previous >= mark)
		return;

#ifdef STACK_POP_CALLBACK
	if (self->popCallback)
		Stack_doUntilMark_(self, self->popCallback);
#endif
	self->top = self->items + (size_t)mark - 1;
	self->lastMark = previous;
}

IOINLINE int Stack_popMarkPoint_(Stack *self, intptr_t mark)
{
	intptr_t cursor;
	size_t used;

	if (!Stack_hasValidStorage_(self, &used, NULL) ||
		!Stack_hasValidMarkChain_(self, used) || mark <= 0)
		return 0;
	cursor = self->lastMark;

	/* Validate the complete chain before mutating the stack. */
	while (cursor > 0) {
		intptr_t previous;
		if ((size_t)cursor > used)
			return 0;
		previous = (intptr_t)self->items[(size_t)cursor];
		if (previous < 0 || previous >= cursor)
			return 0;
		if (cursor == mark)
			break;
		cursor = previous;
	}
	if (cursor != mark)
		return 0;

	while (self->lastMark != mark)
		Stack_popMark(self);
	Stack_popMark(self);
	return 1;
}

IOINLINE void Stack_clearTop(Stack *self)
{
	if (NULL == self || self->lastMark <= 0)
		return;
	Stack_popMark(self);
	Stack_pushMark(self);
}

IOINLINE void *Stack_top(const Stack *self)
{
	size_t used;
	if (!Stack_hasValidStorage_(self, &used, NULL) || used == 0)
		return NULL;
	return *(self->top);
}

IOINLINE void *Stack_at_(const Stack *self, int i)
{
	size_t used;
	if (!Stack_hasValidStorage_(self, &used, NULL) ||
		i < 0 || (size_t)i >= used)
		return NULL;
	return self->items[(size_t)i + 1];
}

#undef IO_IN_C_FILE
#endif
