//metadoc List copyright Steve Dekorte 2002
//metadoc List license BSD revised
/*metadoc List description
List is an array of void pointers.
The List is not responsible for io_freeing it's elements.
*/
#ifndef List_inline_DEFINED
#define List_inline_DEFINED 1

#ifdef LIST_C
#define IO_IN_C_FILE
#endif
#include "Common_inline.h"
#include <limits.h>
#ifdef IO_DECLARE_INLINES

#define LIST_FOREACH(list, index, value, code) \
{ \
	const List *foreachList = list; \
	size_t index, foreachMax = foreachList ? foreachList->size : 0; \
	for (index = 0; foreachList && index < foreachMax; index ++) \
	{ \
		void *value = foreachList->items ? foreachList->items[index] : NULL; \
		code; \
	} \
}


#define LIST_SAFEFOREACH(list, index, value, code) \
{ \
	const List *foreachList = list; \
	size_t index; \
	for (index = 0; foreachList && index < List_size(foreachList); index ++) \
	{ \
		void *value = List_at_(foreachList, index); \
		code; \
	} \
}


#define LIST_REVERSEFOREACH(list, index, value, code) \
{ \
	const List *foreachList = list; \
	size_t index = foreachList ? List_size(foreachList) : 0; \
	while (foreachList && index > 0) \
	{ \
		void *value = List_at_(foreachList, --index); \
		code; \
	} \
}


#define LIST_SAFEREVERSEFOREACH(list, index, value, code) \
{ \
	const List *foreachList = list; \
	size_t index = foreachList ? List_size(foreachList) : 0; \
	while (foreachList && index > 0) \
	{ \
		void *value; \
		if (index > List_size(foreachList)) index = List_size(foreachList); \
		if (index == 0) break; \
		value = List_at_(foreachList, --index); \
		code; \
	} \
}


#define LIST_DO_(list, func) \
{ \
	const List *foreachList = list; \
	size_t index, foreachMax = List_size(foreachList); \
	\
	for (index = 0; index < foreachMax; index ++) \
	{ \
		func(List_at_(foreachList, index)); \
	} \
}

#define List_size(self) (self->size)
/*
IOINLINE size_t List_size(const List *self)
{
	return self->size;
}
*/

IOINLINE void List_ifNeededSizeTo_(List *self, size_t newSize)
{
	if (NULL == self || newSize > (size_t)-1 / sizeof(void *))
		return;
	if (newSize * sizeof(void *) > self->memSize)
		List_preallocateToSize_(self, newSize);
}


IOINLINE void *List_rawAt_(List *self, size_t index)
{
	if (NULL == self || index >= self->size || NULL == self->items)
		return NULL;
	return self->items[index];
}



IOINLINE void *List_at_(const List *self, size_t index)
{
	if (NULL == self || index >= self->size || NULL == self->items)
		return NULL;
	return self->items[index];
}


// --------------------------------------------

IOINLINE long List_indexOf_(List *self, void *item)
{
	size_t i;
	if (NULL == self)
		return -1;
	for (i = 0; i < self->size; i++)
		if (self->items && self->items[i] == item)
			return i <= (size_t)LONG_MAX ? (long)i : -1;
	return -1;
}


IOINLINE int List_contains_(List *self, void *item)
{
	return List_indexOf_(self, item) >= 0;
}


IOINLINE void *List_append_(List *self, void *item)
{
	if (NULL == self || self->size == (size_t)-1)
		return NULL;
	List_ifNeededSizeTo_(self, self->size + 1);
	if (self->size + 1 > self->memSize / sizeof(void *) ||
		NULL == self->items)
		return NULL;
	self->items[self->size++] = item;
	return item;
}


IOINLINE void List_appendSeq_(List *self, const List *otherList)
{
	size_t i, oldSize;
	if (NULL == self || NULL == otherList ||
		otherList->size > (size_t)-1 - self->size)
		return;
	if (self == otherList) {
		oldSize = self->size;
		if (oldSize == 0)
			return;
		List_ifNeededSizeTo_(self, oldSize * 2);
		if (oldSize * 2 > self->memSize / sizeof(void *))
			return;
		memmove(self->items + oldSize, self->items,
			oldSize * sizeof(void *));
		self->size += oldSize;
		return;
	}
	oldSize = self->size;
	List_ifNeededSizeTo_(self, oldSize + otherList->size);
	if (oldSize + otherList->size > self->memSize / sizeof(void *))
		return;
	for (i = 0; i < otherList->size; ++i)
		self->items[oldSize + i] = otherList->items[i];
	self->size = oldSize + otherList->size;
}


IOINLINE void List_compactIfNeeded(List *self)
{
	if (self && self->memSize > 1024 &&
		self->size < self->memSize / (sizeof(void *) * 4))
		List_compact(self);
}


IOINLINE void List_removeIndex_(List *self, size_t index)
{
	if (NULL == self || index >= self->size)
		return;
	if (index + 1 < self->size)
		memmove(&self->items[index], &self->items[index + 1],
			(self->size - index - 1) * sizeof(void *));
	self->size--;
	if (self->items)
		self->items[self->size] = NULL;
	List_compactIfNeeded(self);
}


IOINLINE void List_removeIndex_toIndex_(List *self, size_t index1, size_t index2)
{
	size_t count;
	if (NULL == self || self->size == 0)
		return;
	if (index1 > self->size)
		index1 = self->size;
	if (index2 > self->size)
		index2 = self->size;
	if (index1 >= index2)
		return;
	count = index2 - index1;
	memmove(&self->items[index1], &self->items[index2],
		(self->size - index2) * sizeof(void *));
	self->size -= count;
	memset(self->items + self->size, 0, count * sizeof(void *));
	List_compactIfNeeded(self);
}


IOINLINE void List_remove_(List *self, void *item)
{
	size_t index = 0;
	if (NULL == self)
		return;
	while (index < self->size) {
		if (self->items[index] == item)
			List_removeIndex_(self, index);
		else
			index++;
	}
}


IOINLINE int List_removeFirst_(List *self, void *item)
{
	size_t i;
	if (NULL == self)
		return 0;
	for (i = 0; i < self->size; ++i) {
		if (self->items[i] == item) {
			List_removeIndex_(self, i);
			return 1;
		}
	}
	return 0;
}


IOINLINE void List_removeLast_(List *self, void *item)
{
	size_t index;
	if (NULL == self)
		return;
	index = self->size;
	while (index > 0) {
		--index;
		if (self->items[index] == item) {
			List_removeIndex_(self, index);
			return;
		}
	}
}


IOINLINE void List_removeItems_(List *self, List *other)
{
	if (NULL == self || NULL == other)
		return;
	if (self == other) {
		List_removeAll(self);
		return;
	}
	LIST_FOREACH(other, i, v, List_remove_(self, v));
}


IOINLINE void List_at_insert_(List *self, size_t index, void *item)
{
	size_t oldSize;
	if (NULL == self || self->size == (size_t)-1)
		return;
	oldSize = self->size;
	if (index > oldSize)
		index = oldSize;
	List_ifNeededSizeTo_(self, oldSize + 1);
	if (oldSize + 1 > self->memSize / sizeof(void *) ||
		NULL == self->items)
		return;
	memmove(&self->items[index + 1], &self->items[index],
		(oldSize - index) * sizeof(void *));
	self->items[index] = item;
	self->size = oldSize + 1;
}


IOINLINE void List_at_put_(List *self, size_t index, void *item)
{
	size_t oldSize, needed;
	if (NULL == self || index == (size_t)-1)
		return;
	if (index < self->size) {
		self->items[index] = item;
		return;
	}
	needed = index + 1;
	oldSize = self->size;
	List_ifNeededSizeTo_(self, needed);
	if (needed > self->memSize / sizeof(void *) || NULL == self->items)
		return;
	if (needed > oldSize)
		memset(self->items + oldSize, 0, (needed - oldSize) * sizeof(void *));
	self->items[index] = item;
	self->size = needed;
}


IOINLINE void List_swap_with_(List *self, long index1, long index2)
{
	void **items;
	void *v1;
	if (NULL == self || index1 < 0 || index2 < 0 ||
		(size_t)index1 >= self->size || (size_t)index2 >= self->size ||
		index1 == index2)
		return;
	items = self->items;
	v1 = items[index1];
	items[index1] = items[index2];
	items[index2] = v1;
}


IOINLINE void List_reverse(List *self)
{
	size_t i, j;
	void *value;
	if (NULL == self || self->size < 2 || NULL == self->items)
		return;
	i = 0;
	j = self->size - 1;
	while (i < j) {
		value = self->items[i];
		self->items[i] = self->items[j];
		self->items[j] = value;
		++i;
		--j;
	}
}


// stack --------------------------------------------------

IOINLINE void List_push_(List *self, void *item)
{
	if (NULL == self || self->size == (size_t)-1)
		return;
	List_ifNeededSizeTo_(self, self->size + 1);
	if (self->size + 1 > self->memSize / sizeof(void *) ||
		NULL == self->items)
		return;
	self->items[self->size++] = item;
}


IOINLINE void *List_pop(List *self)
{
	void *item;
	if (NULL == self || self->size == 0 || NULL == self->items)
		return NULL;
	item = self->items[--self->size];
	self->items[self->size] = NULL;
	List_compactIfNeeded(self);
	return item;
}


IOINLINE void *List_top(const List *self)
{
	if (NULL == self || !self->size || NULL == self->items)
		return NULL;
	return self->items[self->size - 1];
}


/* --- perform -------------------------------------------------- */

IOINLINE int List_removeTrueFor_(List *self, ListCollectCallback* callback)
{
	size_t getIndex = 0;
	size_t putIndex = 0;
	size_t count = self->size;
	void **items = self->items;

	while (getIndex < count)
	{
		void *item = items[getIndex];

		if (item && !((*callback)(item)))
		{
			if (getIndex!=putIndex)
			{
				items[putIndex] = item;
			}

			putIndex ++;
		}

		getIndex ++;
	}

	self->size = putIndex;

	return getIndex - putIndex;
}

IOINLINE void List_qsort(List *self, ListSortCallback *callback)
{
	if (NULL == self || NULL == callback ||
		(self->size > 0 && NULL == self->items))
		return;
	if (self->size > 1)
		qsort(self->items, self->size, sizeof(void *), *callback);
}


IOINLINE void *List_bsearch(List *self, const void *key, ListSortCallback *callback)
{
	if (NULL == self || NULL == callback ||
		(self->size > 0 && NULL == self->items))
		return NULL;
	return bsearch(key, self->items, self->size, sizeof(void *), callback);
}


IOINLINE void *List_first(const List *self)
{
	return List_at_(self, 0);
}

IOINLINE void *List_last(List *self)
{
	if (NULL == self || self->size == 0 || NULL == self->items)
		return NULL;
	return self->items[self->size - 1];
}


#undef IO_IN_C_FILE
#endif
#endif
