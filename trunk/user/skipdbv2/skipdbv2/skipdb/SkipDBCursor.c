/*#io
SkipDBCursor ioDoc(
			    docCopyright("Steve Dekorte", 2004)
			    docLicense("BSD revised")
			    docObject("SkipDBCursor")
			    docDescription("A cursor for a skipdb.")
			    */

#include "SkipDBCursor.h"

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

SkipDBRecord *SkipDBCursor_goto_(SkipDBCursor *self, Datum key)
{
	if (NULL == self || NULL == self->sdb || !SkipDB_isOpen(self->sdb))
		return NULL;
	return (self->record = SkipDB_goto_(self->sdb, key));
}

SkipDBRecord *SkipDBCursor_first(SkipDBCursor *self)
{
	if (NULL == self || NULL == self->sdb || !SkipDB_isOpen(self->sdb))
		return NULL;
	return (self->record = SkipDB_firstRecord(self->sdb));
}

SkipDBRecord *SkipDBCursor_last(SkipDBCursor *self)
{
	if (NULL == self || NULL == self->sdb || !SkipDB_isOpen(self->sdb))
		return NULL;
	return (self->record = SkipDB_lastRecord(self->sdb));
}

SkipDBRecord *SkipDBCursor_current(SkipDBCursor *self)
{
	return self ? self->record : NULL;
}

SkipDBRecord *SkipDBCursor_previous(SkipDBCursor *self)
{
	if (NULL == self || NULL == self->sdb || !SkipDB_isOpen(self->sdb))
		return NULL;
	if (self->record)
	{
		self->record = SkipDBRecord_previousRecord(self->record);

		if (self->record == SkipDB_headerRecord(self->sdb))
		{
			self->record = NULL;
		}
	}
	return self->record;
}

SkipDBRecord *SkipDBCursor_next(SkipDBCursor *self)
{
	if (NULL == self || NULL == self->sdb || !SkipDB_isOpen(self->sdb) ||
		!self->record)
	{
		return NULL;
	}

	return (self->record = SkipDBRecord_nextRecord(self->record));
}

