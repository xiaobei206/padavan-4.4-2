//metadoc List copyright Steve Dekorte 2002
//metadoc List license BSD revised

#define LIST_C
#include "List.h"
#undef LIST_C
#include <stdlib.h>

List *List_new(void)
{
	List *self = (List *)io_calloc(1, sizeof(List));
	if (NULL == self)
		return NULL;
	self->size = 0;
	self->memSize = sizeof(void *) * LIST_START_SIZE;
	self->items = (void **)io_calloc(1, self->memSize);
	if (NULL == self->items) {
		io_free(self);
		return NULL;
	}
	return self;
}


List *List_clone(const List *self)
{
	List *child;
	if (NULL == self)
		return NULL;
	child = List_new();
	if (NULL == child)
		return NULL;
	List_copy_(child, self);
	if (child->size != self->size) {
		List_free(child);
		return NULL;
	}
	return child;
}


static size_t indexWrap(long index, size_t size)
{
	size_t distance;
	if (index < 0) {
		distance = (size_t)(-(index + 1)) + 1;
		return distance >= size ? 0 : size - distance;
	}
	return (size_t)index > size ? size : (size_t)index;
}


void List_sliceInPlace(List* self, long startIndex, long endIndex)
{
	size_t i, start, end, oldSize;
	List *tmp;
	if (NULL == self)
		return;
	tmp = List_new();
	if (NULL == tmp)
		return;
	oldSize = self->size;
	start = indexWrap(startIndex, oldSize);
	end = indexWrap(endIndex, oldSize);
	if (start <= end) {
		for (i = start; i < oldSize && i <= end; ++i) {
			if (NULL == List_append_(tmp, self->items[i]) &&
				tmp->size == i - start) {
				List_free(tmp);
				return;
			}
		}
	}
	List_copy_(self, tmp);
	List_free(tmp);
}


List *List_cloneSlice(const List *self, long startIndex, long endIndex)
{
	List *child = List_clone(self);
	if (NULL == child)
		return NULL;
	List_sliceInPlace(child, startIndex, endIndex);
	return child;
}


void List_free(List *self)
{
	if (NULL == self)
		return;
	io_free(self->items);
	io_free(self);
}


UArray List_asStackAllocatedUArray(List *self)
{
	UArray a = UArray_stackAllocedEmptyUArray();
	if (NULL == self)
		return a;
	a.itemType = CTYPE_uintptr_t;
	a.itemSize = sizeof(uintptr_t);
	a.size = self->size;
	a.data = (uint8_t *)(self->items);
	return a;
}


size_t List_memorySize(const List *self)
{
	if (NULL == self || self->size > ((size_t)-1 - sizeof(List)) / sizeof(void *))
		return 0;
	return sizeof(List) + (self->size * sizeof(void *));
}


void List_removeAll(List *self)
{
	if (NULL == self)
		return;
	if (self->items && self->size > 0)
		memset(self->items, 0, self->size * sizeof(void *));
	self->size = 0;
	List_compactIfNeeded(self);
}


void List_copy_(List *self, const List *otherList)
{
	size_t oldSize;
	if (NULL == self || NULL == otherList || self == otherList)
		return;
	if (otherList->size > (size_t)-1 / sizeof(void *))
		return;
	oldSize = self->size;
	if (otherList->size > 0) {
		List_preallocateToSize_(self, otherList->size);
		if (otherList->size > self->memSize / sizeof(void *) ||
			NULL == self->items || NULL == otherList->items)
			return;
		memmove(self->items, otherList->items,
			otherList->size * sizeof(void *));
	}
	if (oldSize > otherList->size && self->items)
		memset(self->items + otherList->size, 0,
			(oldSize - otherList->size) * sizeof(void *));
	self->size = otherList->size;
}


int List_equals_(const List *self, const List *otherList)
{
	if (NULL == self || NULL == otherList || self->size != otherList->size)
		return 0;
	if (self->size == 0)
		return 1;
	if (NULL == self->items || NULL == otherList->items)
		return 0;
	return memcmp(self->items, otherList->items,
		self->size * sizeof(void *)) == 0;
}


/* --- sizing ------------------------------------------------ */

void List_setSize_(List *self, size_t index)
{
	size_t oldSize;
	if (NULL == self || index > (size_t)-1 / sizeof(void *))
		return;
	oldSize = self->size;
	if (index > oldSize) {
		List_preallocateToSize_(self, index);
		if (index > self->memSize / sizeof(void *) || NULL == self->items)
			return;
		memset(self->items + oldSize, 0,
			(index - oldSize) * sizeof(void *));
	} else if (index < oldSize && self->items) {
		memset(self->items + index, 0, (oldSize - index) * sizeof(void *));
	}
	self->size = index;
	List_compactIfNeeded(self);
}


void List_preallocateToSize_(List *self, size_t index)
{
	size_t required, newSize, oldMemSize;
	void **newItems;
	if (NULL == self || index > (size_t)-1 / sizeof(void *))
		return;
	required = index * sizeof(void *);
	if (required <= self->memSize && (required == 0 || self->items))
		return;
	oldMemSize = self->memSize;
	newSize = oldMemSize ? oldMemSize : sizeof(void *);
	while (newSize < required) {
		if (newSize > (size_t)-1 / LIST_RESIZE_FACTOR) {
			newSize = required;
			break;
		}
		newSize *= LIST_RESIZE_FACTOR;
	}
	newItems = (void **)io_realloc(self->items, newSize);
	if (NULL == newItems)
		return;
	if (newSize > oldMemSize)
		memset((uint8_t *)newItems + oldMemSize, 0, newSize - oldMemSize);
	self->items = newItems;
	self->memSize = newSize;
}


void List_compact(List *self)
{
	size_t newSize;
	void **newItems;
	if (NULL == self)
		return;
	if (self->size > (size_t)-1 / sizeof(void *))
		return;
	newSize = self->size * sizeof(void *);
	if (newSize == 0) {
		io_free(self->items);
		self->items = NULL;
		self->memSize = 0;
		return;
	}
	newItems = (void **)io_realloc(self->items, newSize);
	if (NULL == newItems)
		return;
	self->items = newItems;
	self->memSize = newSize;
}


// -----------------------------------------------------------

void List_print(const List *self)
{
	size_t i;
	if (NULL == self)
		return;
	printf("List <%p> [%lu bytes]\n", (const void *)self,
		(unsigned long)self->memSize);
	for (i = 0; i < self->size; i++)
		printf("%lu: %p\n", (unsigned long)i, self->items ? self->items[i] : NULL);
	printf("\n");
}


// enumeration -----------------------------------------

void List_do_(List *self, ListDoCallback *callback)
{
	LIST_FOREACH(self, i, v, if (v) (*callback)(v));
}

void List_do_with_(List *self, ListDoWithCallback *callback, void *arg)
{
	LIST_FOREACH(self, i, v, if (v) (*callback)(v, arg));
}

void List_mapInPlace_(List *self, ListCollectCallback *callback)
{
	void **items = self->items;
	LIST_FOREACH(self, i, v, items[i] = (*callback)(v));
}

List *List_map_(List *self, ListCollectCallback *callback)
{
	List *r = List_new();
	LIST_FOREACH(self, i, v, List_append_(r, (*callback)(v)););
	return r;
}

List *List_select_(List *self, ListSelectCallback *callback)
{
	List *r = List_new();
	LIST_FOREACH(self, i, v, if ((*callback)(v)) List_append_(r, v));
	return r;
}

void *List_detect_(List *self, ListDetectCallback *callback)
{
	LIST_FOREACH(self, i, v, if (v && (*callback)(v)) return v; );
	return NULL;
}

void *List_anyOne(const List *self)
{
	size_t i;

	if (self->size == 0)
	{
		return (void *)NULL;
	}

	if (self->size == 1)
	{
		return LIST_AT_(self, 0);
	}

	i = (rand() >> 4) % (self->size); // without the shift, just get a sequence!

	return LIST_AT_(self, i);
}

void List_shuffle(List *self)
{
	size_t i, j;
	if (NULL == self || self->size < 2)
		return;
	for (i = 0; i < self->size - 1; i++) {
		j = i + (size_t)rand() % (self->size - i);
		List_swap_with_(self, (long)i, (long)j);
	}
}


void *List_removeLast(List *self)
{
	if (NULL == self || self->size == 0)
		return NULL;
	return List_pop(self);
}


{
	void *item = List_at_(self, self->size - 1);

	if (item)
	{
		self->size --;
		List_compactIfNeeded(self);
	}

	return item;
}

