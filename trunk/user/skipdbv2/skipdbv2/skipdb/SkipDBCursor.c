/*#io
SkipDBCursor ioDoc(
			    docCopyright("Steve Dekorte", 2004)
			    docLicense("BSD revised")
			    docObject("SkipDBCursor")
			    docDescription("A cursor for a skipdb.")
			    */

#include "SkipDBCursor.h"

#include <limits.h>

SkipDBCursor *SkipDBCursor_new(void)
{
	SkipDBCursor *self = (SkipDBCursor *)calloc(1, sizeof(SkipDBCursor));
	if (NULL == self)
		return NULL;
	self->refCount = 1;
	return self;
}

SkipDBCursor *SkipDBCursor_newWithDB_(SkipDB *sdb)
{
	SkipDBCursor *self;
	if (NULL == sdb)
		return NULL;
	self = SkipDBCursor_new();
	if (self)
		self->sdb = sdb;
	return self;
}

void SkipDBCursor_sdb_(SkipDBCursor *self, SkipDB *sdb)
{
	if (self && self->refCount > 0)
		self->sdb = sdb;
}

void SkipDBCursor_retain(SkipDBCursor *self)
{
	if (self && self->refCount > 0 && self->refCount < INT_MAX)
		self->refCount++;
}

void SkipDBCursor_dealloc(SkipDBCursor *self)
{
	if (NULL == self)
		return;
	if (self->sdb)
		SkipDB_removeCursor_(self->sdb, self);
	self->sdb = NULL;
	self->record = NULL;
	free(self);
}

void SkipDBCursor_release(SkipDBCursor *self)
{
	if (NULL == self || self->refCount <= 0)
		return;
	self->refCount--;
	if (self->refCount == 0)
		SkipDBCursor_dealloc(self);
}

void SkipDBCursor_mark(SkipDBCursor *self)
{
	if (self && self->record)
	{
		SkipDBRecord_mark(self->record);
	}
}

/* Validate the cursor's database and reset direction-local traversal history. */
static int SkipDBCursor_preparePositioning_(SkipDBCursor *self)
{
	if (NULL == self || NULL == self->sdb || !SkipDB_isOpen(self->sdb))
		return 0;
	self->nextSteps = 0;
	self->previousSteps = 0;
	return 1;
}

SkipDBRecord *SkipDBCursor_goto_(SkipDBCursor *self, Datum key)
{
	if (!SkipDBCursor_preparePositioning_(self))
		return NULL;
	return (self->record = SkipDB_goto_(self->sdb, key));
}

SkipDBRecord *SkipDBCursor_first(SkipDBCursor *self)
{
	if (!SkipDBCursor_preparePositioning_(self))
		return NULL;
	return (self->record = SkipDB_firstRecord(self->sdb));
}

SkipDBRecord *SkipDBCursor_last(SkipDBCursor *self)
{
	if (!SkipDBCursor_preparePositioning_(self))
		return NULL;
	return (self->record = SkipDB_lastRecord(self->sdb));
}

SkipDBRecord *SkipDBCursor_current(SkipDBCursor *self)
{
	return self ? self->record : NULL;
}

/* Validate and account for one directional traversal step in a shared path. */
static int SkipDBCursor_beginTraversalStep_(SkipDBCursor *self, int reverse)
{
	long max_steps;
	long *step_count;
	long *opposite_count;
	const char *direction;

	if (NULL == self || NULL == self->sdb ||
		!SkipDB_isOpen(self->sdb) || NULL == self->record)
		return 0;

	if (reverse) {
		step_count = &self->previousSteps;
		opposite_count = &self->nextSteps;
		direction = "reverse";
	} else {
		step_count = &self->nextSteps;
		opposite_count = &self->previousSteps;
		direction = "forward";
	}

	max_steps = self->sdb->udb && self->sdb->udb->index
		? UDBIndex_pidCount(self->sdb->udb->index) : -1;
	if (max_steps <= 0 || ++*step_count > max_steps) {
		fprintf(stderr,
			"SkipDBCursor: invalid PID index length or cycle during %s traversal\n",
			direction);
		SkipDB_close(self->sdb);
		self->record = NULL;
		return 0;
	}

	*opposite_count = 0;
	return 1;
}

SkipDBRecord *SkipDBCursor_previous(SkipDBCursor *self)
{
	if (!SkipDBCursor_beginTraversalStep_(self, 1))
		return NULL;

	self->record = SkipDBRecord_previousRecord(self->record);
	if (self->record == SkipDB_headerRecord(self->sdb))
		self->record = NULL;
	return self->record;
}

SkipDBRecord *SkipDBCursor_next(SkipDBCursor *self)
{
	if (!SkipDBCursor_beginTraversalStep_(self, 0))
		return NULL;

	return (self->record = SkipDBRecord_nextRecord(self->record));
}

