/* SkipDB record cache, age list, and eviction policy. */
#include "SkipDBInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

void SkipDB_noteNewRecord_(SkipDB *self, SkipDBRecord *r)
{
	if (NULL == self || NULL == r)
		return;
	self->cachedRecordCount++;
	SkipDB_noteAccessedRecord_(self, r);
}

void SkipDB_noteAccessedRecord_(SkipDB *self, SkipDBRecord *r)
{
	SkipDBRecord *y;
	if (NULL == self || NULL == r)
		return;
	y = self->youngestRecord;
	if (y != r) {
		if (y) {
			SkipDBRecord_removeFromAgeList(r);
			SkipDBRecord_setOlderRecord_(r, y);
			SkipDBRecord_setYoungerRecord_(y, r);
		}
		self->youngestRecord = r;
	}
}

void SkipDB_noteDirtyRecord_(SkipDB *self, SkipDBRecord *r)
{
	if (NULL == self || NULL == r || !SkipDB_isOpen(self))
		return;
	/* A dirty record omitted from this queue would be silently skipped at sync. */
	if (NULL == self->dirtyRecords || NULL == self->udb ||
		NULL == List_append_(self->dirtyRecords, r)) {
		fprintf(stderr, "SkipDB: unable to queue dirty record; aborting transaction\n");
		if (self->udb)
			UDB_close(self->udb);
	}
}

void SkipDB_noteAssignedPidToRecord_(SkipDB *self, SkipDBRecord *r)
{
	PID_TYPE pid;

	if (NULL == self || NULL == r || !SkipDB_isOpen(self))
		return;
	pid = SkipDBRecord_pid(r);
	if (pid == 0)
		return;
	/* Cache insertion can fail during allocation/rehash; duplicate loads are unsafe. */
	if (NULL == self->pidToRecord) {
		fprintf(stderr, "SkipDB: PID cache unavailable; aborting transaction\n");
		if (self->udb)
			UDB_close(self->udb);
		return;
	}
	PHash_at_put_(self->pidToRecord, (void *)pid, r);
	if (PHash_at_(self->pidToRecord, (void *)pid) != r) {
		fprintf(stderr, "SkipDB: unable to cache assigned PID; aborting transaction\n");
		if (self->udb)
			UDB_close(self->udb);
	}
}

void SkipDB_noteWillFreeRecord_(SkipDB *self, SkipDBRecord *r)
{
	SkipDBFreeObjectFunc *f;
	void *object;
	if (NULL == self || NULL == r)
		return;
	f = self->objectFreeFunc;
	object = SkipDBRecord_object(r);
	if (f && object)
		(*f)(object);
	if (self->cachedRecordCount > 0)
		self->cachedRecordCount--;
	if (r == self->youngestRecord)
		self->youngestRecord = SkipDBRecord_olderRecord(r);
	SkipDBRecord_removeFromAgeList(r);
	if (self->pidToRecord && SkipDBRecord_pid(r) != 0)
		PHash_removeKey_(self->pidToRecord, (void *)SkipDBRecord_pid(r));
}

void SkipDB_freeAllCachedRecords(SkipDB *self)
{
	if (NULL == self)
		return;
	SkipDB_clearUpdate(self);
	//SkipDBRecord *r = self->youngestRecord;
	//self->headerPid = SkipDBRecord_pid(self->header);

	//SkipDBRecord_showAgeList(self->youngestRecord);

	while (self->youngestRecord)
	{
		SkipDBRecord_dealloc(self->youngestRecord);
	}

	self->header = NULL;
}

void SkipDB_setCacheHighWaterMark_(SkipDB *self, size_t recordCount)
{
	if (NULL == self)
		return;
	self->cacheHighWaterMark = recordCount;
	if (recordCount < self->cacheLowWaterMark)
		self->cacheLowWaterMark = recordCount / 2;
}

size_t SkipDB_cacheHighWaterMark(SkipDB *self)
{
	return self ? self->cacheHighWaterMark : 0;
}

void SkipDB_setCacheLowWaterMark_(SkipDB *self, size_t recordCount)
{
	if (NULL == self)
		return;
	self->cacheLowWaterMark = recordCount;
	if (recordCount > self->cacheHighWaterMark)
		self->cacheHighWaterMark = recordCount > (size_t)-1 / 2
			? (size_t)-1 : recordCount * 2;
}

size_t SkipDB_cacheLowWaterMark(SkipDB *self)
{
	return self ? self->cacheLowWaterMark : 0;
}

int SkipDB_headerIsEmpty(SkipDB *self)
{
	return self && self->header
		? SkipDBRecord_pointersAreEmpty(self->header) : 1;
}

void SkipDB_freeExcessCachedRecords(SkipDB *self)
{
	if (NULL == self || NULL == self->cursors || NULL == self->header)
		return;

	if (self->cachedRecordCount > self->cacheHighWaterMark)
	{
		SkipDBRecord *r;
		size_t kept = 0;
		size_t lowMark = self->cacheLowWaterMark;

		/* Keep the root in the cache and clear marks left by earlier passes
		 * before applying the current cursor/recent-record protection set. */
		SkipDB_noteAccessedRecord_(self, self->header);
		r = self->youngestRecord;
		while (r)
		{
			r->mark = 0;
			r = r->olderRecord;
		}

		List_do_(self->cursors, (ListDoCallback *)SkipDBCursor_mark);
		r = self->youngestRecord;
		while (r && kept < lowMark)
		{
			r->mark = 1;
			kept++;
			r = r->olderRecord;
		}
		self->header->mark = 1;

		/* Drop cached links from every record we keep to any record about
		 * to be evicted; persistent PIDs remain available for lazy reload. */
		for (r = self->youngestRecord; r; r = r->olderRecord)
		{
			if (r->mark)
				SkipDBRecord_removeReferencesToUnmarked(r);
		}

		/* Remove unmarked records in-place. Keep marked cursor records linked
		 * in the age list, even when they are older than an evicted gap. */
		r = self->youngestRecord;
		while (r)
		{
			SkipDBRecord *next = r->olderRecord;
			if (r->mark)
				r->mark = 0;
			else
				SkipDBRecord_dealloc(r);
			r = next;
		}
	}
}

void SkipDB_clearCache(SkipDB *self)
{
	size_t i;
	if (NULL == self || !SkipDB_isOpen(self))
		return;
	if (self->cursors) {
		for (i = 0; i < self->cursors->size; ++i) {
			SkipDBCursor *cursor = (SkipDBCursor *)self->cursors->items[i];
			if (cursor)
				cursor->record = NULL;
		}
	}
	if (self->header) {
		SkipDB_freeAllCachedRecords(self);
		SkipDB_readRootRecord(self);
	}
}
