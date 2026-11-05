/* SkipDB ordered navigation, cursor management, and database merge. */
#include "SkipDBInternal.h"
#include <stdio.h>
#include <limits.h>

/* Share the PID-index bound used to guard ordered scans against corrupt cycles. */
static int SkipDB_queryTraversalLimit_(SkipDB *self, long *max_steps, const char *operation)
{
	if (NULL == self || NULL == max_steps || NULL == self->udb || NULL == self->udb->index)
		return 0;
	*max_steps = UDBIndex_pidCount(self->udb->index);
	if (*max_steps <= 0) {
		fprintf(stderr, "SkipDB: invalid PID index length %s\n", operation);
		SkipDB_close(self);
		return 0;
	}
	return 1;
}

/* Close the database rather than risk an unbounded walk through a corrupt list. */
static int SkipDB_queryTraversalStep_(SkipDB *self, long *steps, long max_steps, const char *context)
{
	if (++*steps <= max_steps)
		return 1;
	fprintf(stderr, "SkipDB: cycle or invalid link %s\n", context);
	SkipDB_close(self);
	return 0;
}

int SkipDB_count(SkipDB *self)
{
	int count = 0;
	long steps = 0, max_steps;
	SkipDBRecord *record;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		NULL == self->udb || NULL == self->udb->index)
		return 0;
	if (!SkipDB_queryTraversalLimit_(self, &max_steps, "while counting records"))
		return -1;
	record = SkipDB_firstRecord(self);
	while (record) {
		/* A valid list cannot contain more rows than the PID index. */
		if (!SkipDB_queryTraversalStep_(self, &steps, max_steps, "in record list"))
			return -1;
		if (count == INT_MAX)
			return INT_MAX;
		count++;
		record = SkipDBRecord_nextRecord(record);
	}
	if (!SkipDB_isOpen(self))
		return -1;
	return count;
}

SkipDBRecord *SkipDB_firstRecord(SkipDB *self)
{
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		SkipDBRecord_level(self->header) == 0)
		return NULL;
	return SkipDBRecord_nextRecord(self->header);
}

SkipDBRecord *SkipDB_lastRecord(SkipDB *self)
{
	SkipDBRecord *last;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		SkipDBRecord_level(self->header) == 0)
		return NULL;
	last = SkipDBRecord_findLastRecord(self->header);
	return last == self->header ? NULL : last;
}

SkipDBRecord *SkipDB_goto_(SkipDB *self, Datum key)
{
	SkipDBRecord *record;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		(key.size > 0 && NULL == key.data))
		return NULL;
	record = SkipDB_recordAt_(self, key);
	if (record)
		return record;
	if (self->update[0])
		return SkipDBRecord_nextRecord(self->update[0]);
	return SkipDB_firstRecord(self);
}

SkipDBCursor *SkipDB_createCursor(SkipDB *self)
{
	SkipDBCursor *cursor;
	if (NULL == self || NULL == self->cursors || !SkipDB_isOpen(self))
		return NULL;
	cursor = SkipDBCursor_newWithDB_(self);
	if (NULL == cursor)
		return NULL;
	if (NULL == List_append_(self->cursors, cursor)) {
		/* An untracked cursor cannot protect its record from cache eviction. */
		SkipDBCursor_release(cursor);
		return NULL;
	}
	return cursor;
}

void SkipDB_removeCursor_(SkipDB *self, SkipDBCursor *cursor)
{
	if (self && self->cursors && cursor)
		List_remove_(self->cursors, cursor);
}

int SkipDB_mergeInto_(SkipDB *self, SkipDB *other)
{
	SkipDBRecord *record;
	long steps = 0, max_steps;
	if (NULL == self || NULL == other || self == other ||
		NULL == self->header || !SkipDB_isOpen(self) || !SkipDB_isOpen(other) ||
		NULL == self->udb || NULL == self->udb->index)
		return 0;
	if (!SkipDB_queryTraversalLimit_(self, &max_steps, "during merge"))
		return 0;

	record = self->header;
	while ((record = SkipDBRecord_nextRecord(record)) != NULL) {
		Datum key, value;
		if (!SkipDB_queryTraversalStep_(self, &steps, max_steps, "during merge"))
			return 0;
		key = SkipDBRecord_keyDatum(record);
		value = SkipDBRecord_valueDatum(record);
		if (!SkipDB_at_put_(other, key, value) ||
			!SkipDB_isOpen(self) || !SkipDB_isOpen(other))
			return 0;
	}
	/* A failed next-pointer read can terminate the loop by closing source. */
	return SkipDB_isOpen(self) && SkipDB_isOpen(other);
}

int SkipDB_exists(SkipDB *self, Datum key)
{
	return SkipDB_recordAt_(self, key) != NULL;
}

int SkipDB_maxPos(SkipDB *self)
{
	long max_pos;

	if (NULL == self || NULL == self->udb || NULL == self->udb->records ||
		NULL == self->udb->records->file)
		return 0;
	max_pos = self->udb->records->file->maxPos;
	if (max_pos < 0)
		return -1;
	/* The daemon only needs a threshold comparison; never wrap large files
	 * into a negative int and accidentally disable database rotation. */
	if (max_pos > INT_MAX)
		return INT_MAX;
	return (int)max_pos;
}
