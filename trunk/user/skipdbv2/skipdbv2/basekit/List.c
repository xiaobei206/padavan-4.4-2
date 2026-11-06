//metadoc List copyright Steve Dekorte 2002
//metadoc List license BSD revised

#define LIST_C
#include "List.h"
#undef LIST_C
#include <stdlib.h>

static int List_hasValidStorage_(const List *self)
{
	if (NULL == self ||
		self->size > (size_t)-1 / sizeof(void *) ||
		self->size > self->memSize / sizeof(void *) ||
		(self->memSize > 0 && NULL == self->items) ||
		(self->size > 0 && NULL == self->items))
		return 0;
	return 1;
}


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
	if (!List_hasValidStorage_(self))
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
	size_t start, end, oldSize, retainedSize;
	if (!List_hasValidStorage_(self))
		return;
	oldSize = self->size;
	if (oldSize == 0)
		return;
	start = indexWrap(startIndex, oldSize);
	end = indexWrap(endIndex, oldSize);
	if (start >= oldSize || start > end) {
		memset(self->items, 0, oldSize * sizeof(void *));
		self->size = 0;
		return;
	}
	if (end >= oldSize)
		end = oldSize - 1;
	retainedSize = end - start + 1;
	if (start > 0)
		memmove(self->items, self->items + start, retainedSize * sizeof(void *));
	if (retainedSize < oldSize)
		memset(self->items + retainedSize, 0, (oldSize - retainedSize) * sizeof(void *));
	self->size = retainedSize;
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
	if (!List_hasValidStorage_(self))
		return a;
	a.itemType = CTYPE_uintptr_t;
	a.itemSize = sizeof(uintptr_t);
	a.size = self->size;
	a.data = (uint8_t *)(self->items);
	return a;
}


size_t List_memorySize(const List *self)
{
	if (NULL == self || self->memSize > (size_t)-1 - sizeof(List))
		return 0;
	return sizeof(List) + self->memSize;
}


void List_removeAll(List *self)
{
	if (!List_hasValidStorage_(self))
		return;
	if (self->items && self->size > 0)
		memset(self->items, 0, self->size * sizeof(void *));
	self->size = 0;
	List_compactIfNeeded(self);
}


void List_copy_(List *self, const List *otherList)
{
	size_t oldSize;
	if (self == otherList ||
		!List_hasValidStorage_(self) ||
		!List_hasValidStorage_(otherList))
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
	if (!List_hasValidStorage_(self) ||
		!List_hasValidStorage_(otherList) ||
		self->size != otherList->size)
		return 0;
	if (self->size == 0)
		return 1;
	return memcmp(self->items, otherList->items,
		self->size * sizeof(void *)) == 0;
}


/* --- sizing ------------------------------------------------ */

void List_setSize_(List *self, size_t index)
{
	size_t oldSize;
	if (!List_hasValidStorage_(self) ||
		index > (size_t)-1 / sizeof(void *))
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
	if (!List_hasValidStorage_(self) ||
		index > (size_t)-1 / sizeof(void *))
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
	if (!List_hasValidStorage_(self))
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
	if (!List_hasValidStorage_(self))
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
	if (!List_hasValidStorage_(self) || NULL == callback)
		return;
	LIST_FOREACH(self, i, v, if (v) (*callback)(v));
}

void List_do_with_(List *self, ListDoWithCallback *callback, void *arg)
{
	if (!List_hasValidStorage_(self) || NULL == callback)
		return;
	LIST_FOREACH(self, i, v, if (v) (*callback)(v, arg));
}

void List_mapInPlace_(List *self, ListCollectCallback *callback)
{
	void **items;
	if (!List_hasValidStorage_(self) || NULL == callback)
		return;
	items = self->items;
	LIST_FOREACH(self, i, v, items[i] = (*callback)(v));
}

List *List_map_(List *self, ListCollectCallback *callback)
{
	List *r;
	size_t i;
	if (NULL == callback || (self && !List_hasValidStorage_(self)))
		return NULL;
	r = List_new();
	if (NULL == r)
		return NULL;
	for (i = 0; self && i < self->size; ++i) {
		size_t oldSize = r->size;
		List_append_(r, (*callback)(self->items[i]));
		if (oldSize == (size_t)-1 || r->size != oldSize + 1) {
			List_free(r);
			return NULL;
		}
	}
	return r;
}

List *List_select_(List *self, ListSelectCallback *callback)
{
	List *r;
	size_t i;
	if (NULL == callback || (self && !List_hasValidStorage_(self)))
		return NULL;
	r = List_new();
	if (NULL == r)
		return NULL;
	for (i = 0; self && i < self->size; ++i) {
		if ((*callback)(self->items[i])) {
			size_t oldSize = r->size;
			List_append_(r, self->items[i]);
			if (oldSize == (size_t)-1 || r->size != oldSize + 1) {
				List_free(r);
				return NULL;
			}
		}
	}
	return r;
}

void *List_detect_(List *self, ListDetectCallback *callback)
{
	if (!List_hasValidStorage_(self) || NULL == callback)
		return NULL;
	LIST_FOREACH(self, i, v, if (v && (*callback)(v)) return v; );
	return NULL;
}

void *List_anyOne(const List *self)
{
	size_t i;

	if (!List_hasValidStorage_(self) || self->size == 0)
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
	if (!List_hasValidStorage_(self) || self->size < 2)
		return;
	for (i = 0; i < self->size - 1; i++) {
		j = i + (size_t)rand() % (self->size - i);
		List_swap_with_(self, (long)i, (long)j);
	}
}


void *List_removeLast(List *self)
{
	if (!List_hasValidStorage_(self) || self->size == 0)
		return NULL;
	return List_pop(self);
}
