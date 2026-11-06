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
#ifdef IO_DECLARE_INLINES

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
	if (NULL == self || NULL == self->items)
		return;
	self->top = self->items;
	self->lastMark = 0;
}

IOINLINE size_t Stack_totalSize(const Stack *self)
{
	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->memEnd <= self->items ||
		self->top < self->items || self->top >= self->memEnd)
		return 0;
	return (size_t)(self->top - self->items);
}

IOINLINE int Stack_count(const Stack *self)
{
	size_t count = Stack_totalSize(self);
	return count > (size_t)INT_MAX ? INT_MAX : (int)count;
}

IOINLINE void Stack_push_(Stack *self, void *item)
{
	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->memEnd <= self->items ||
		self->top < self->items || self->top >= self->memEnd)
		return;

	if (self->top >= self->memEnd - 1)
		Stack_resize(self);
	if (self->top >= self->memEnd - 1)
		return;

	self->top++;
	*(self->top) = item;
}

IOINLINE intptr_t Stack_pushMarkPoint(Stack *self)
{
	size_t oldUsed;

	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->memEnd <= self->items ||
		self->top < self->items || self->top >= self->memEnd ||
		self->lastMark < 0 ||
		(size_t)self->lastMark > (size_t)(self->top - self->items))
		return 0;

	oldUsed = (size_t)(self->top - self->items);
	Stack_push_(self, (void *)self->lastMark);
	if ((size_t)(self->top - self->items) == oldUsed)
		return 0;
	self->lastMark = (intptr_t)(self->top - self->items);
	return self->lastMark;
}

IOINLINE void Stack_pushMark(Stack *self)
{
	(void)Stack_pushMarkPoint(self);
}

IOINLINE void *Stack_pop(Stack *self)
{
	void *top;

	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->top <= self->items ||
		self->top >= self->memEnd)
		return NULL;
	top = *(self->top);
#ifdef STACK_POP_CALLBACK
	if (self->popCallback)
		self->popCallback(top);
#endif
	self->top--;
	return top;
}

IOINLINE void Stack_popMark(Stack *self)
{
	intptr_t mark, previous;

	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->top < self->items ||
		self->top >= self->memEnd)
		return;
	mark = self->lastMark;
	if (mark <= 0 || (size_t)mark > (size_t)(self->top - self->items))
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

	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->memEnd <= self->items ||
		self->top < self->items || self->top >= self->memEnd ||
		mark <= 0)
		return 0;
	used = (size_t)(self->top - self->items);
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
	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->top <= self->items ||
		self->top >= self->memEnd)
		return NULL;
	return *(self->top);
}

IOINLINE void *Stack_at_(const Stack *self, int i)
{
	if (NULL == self || NULL == self->items || NULL == self->top ||
		NULL == self->memEnd || self->memEnd <= self->items ||
		self->top < self->items || self->top >= self->memEnd ||
		i < 0 || (size_t)i >= (size_t)(self->top - self->items))
		return NULL;
	return self->items[(size_t)i + 1];
}

#undef IO_IN_C_FILE
#endif
