/*#io
SkipDB ioDoc(
    docCopyright("Steve Dekorte", 2004)
    docLicense("BSD revised")
    docObject("SkipDB")
    docDescription("Lifecycle and public facade for the sorted key/value database.")
    */

#include "SkipDBInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

void SkipDBCursor_dealloc(SkipDBCursor *self);

SkipDB *SkipDB_new(void)
{
	SkipDB *self = (SkipDB *)calloc(1, sizeof(SkipDB));
	if (NULL == self)
		return NULL;

	self->udb = UDB_new();
	self->stream = BStream_new();
	self->cursors = List_new();
	self->pidsToRemove = List_new();
	self->dirtyRecords = List_new();
	self->pidToRecord = PHash_new();
	self->randomGen = RandomGen_new();

	if (NULL == self->udb || NULL == self->stream ||
		NULL == self->cursors || NULL == self->pidsToRemove ||
		NULL == self->dirtyRecords || NULL == self->pidToRecord ||
		NULL == self->randomGen) {
		SkipDB_dealloc(self);
		return NULL;
	}

	self->p = SKIPDB_PROBABILITY_DISTRIBUTION;
	self->cacheHighWaterMark = 10000;
	self->cacheLowWaterMark = 500;
	self->headerPid = 1;
	return self;
}

void SkipDB_dealloc(SkipDB *self)
{
	if (NULL == self)
		return;

	SkipDB_clearUpdate(self);
	if (self->stream)
		BStream_free(self->stream);
	SkipDB_freeAllCachedRecords(self);

	/* Detach cursors before destruction; their deallocators remove themselves. */
	if (self->cursors) {
		while (self->cursors->size > 0) {
			SkipDBCursor *cursor = (SkipDBCursor *)List_removeLast(self->cursors);
			if (cursor) {
				cursor->sdb = NULL;
				SkipDBCursor_dealloc(cursor);
			}
		}
		List_free(self->cursors);
	}
	if (self->udb)
		UDB_free(self->udb);
	if (self->pidsToRemove)
		List_free(self->pidsToRemove);
	if (self->dirtyRecords)
		List_free(self->dirtyRecords);
	if (self->pidToRecord)
		PHash_free(self->pidToRecord);
	if (self->randomGen)
		RandomGen_free(self->randomGen);
	free(self);
}

void SkipDB_free(SkipDB *self)
{
	SkipDB_dealloc(self);
}

void SkipDB_setPath_(SkipDB *self, char *path)
{
	if (self && self->udb && path && !UDB_isOpen(self->udb))
		UDB_setPath_(self->udb, path);
}

SKIPDB_API int SkipDB_open(SkipDB *self)
{
	if (NULL == self || NULL == self->udb)
		return 0;
	if (SkipDB_isOpen(self))
		return (int)self->headerPid;

	/* Drop stale in-memory state after close or failed transaction. */
	if (self->dirtyRecords)
		List_removeAll(self->dirtyRecords);
	if (self->pidsToRemove)
		List_removeAll(self->pidsToRemove);
	SkipDB_clearUpdate(self);
	if (self->cursors) {
		size_t i;
		for (i = 0; i < self->cursors->size; ++i) {
			SkipDBCursor *cursor = (SkipDBCursor *)self->cursors->items[i];
			if (cursor)
				cursor->record = NULL;
		}
	}
	if (self->youngestRecord || self->header)
		SkipDB_freeAllCachedRecords(self);
	self->headerPid = 1;

	UDB_open(self->udb);
	if (!UDB_isOpen(self->udb))
		return 0;

	SkipDB_readRootRecord(self);
	if (NULL == self->header) {
		UDB_close(self->udb);
		return 0;
	}

#if 0
        if(0 == self->headerPid) {
	    SkipDB_beginTransaction(self);
            key = Datum_FromCString_("__inner__");
            value = Datum_FromCString_("init");
	    SkipDB_at_put_(self, key, value);
	    SkipDB_commitTransaction(self);
	    SkipDBRecord_markAsDirty(self->header);
        }
#endif

        return self->headerPid;
}

SKIPDB_API void SkipDB_close(SkipDB *self)
{
	if (self && self->udb)
		UDB_close(self->udb);
}

void SkipDB_headerPid_(SkipDB *self, PID_TYPE pid)
{
	if (self)
		self->headerPid = pid;
}

PID_TYPE SkipDB_headerPid(SkipDB *self)
{
	return self ? self->headerPid : 0;
}

SkipDBRecord *SkipDB_headerRecord(SkipDB *self)
{
	return self ? self->header : NULL;
}

UDB *SkipDB_udb(SkipDB *self)
{
	return self ? self->udb : NULL;
}

BStream *SkipDB_tmpStream(SkipDB *self)
{
	return self ? self->stream : NULL;
}

void SkipDB_delete(SkipDB *self)
{
	if (NULL == self)
		return;
	SkipDB_close(self);
	if (self->udb)
		UDB_delete(self->udb);
/*
	int count = 0;
	SkipDBRecord *r = self->header;

	while (r)
	{
		PID_TYPE pid = SkipDBRecord_pid(r);
		SkipDBRecord *next = SkipDBRecord_nextRecord(self->header);

		if (pid)
		{
			UDB_removeAt_(SkipDB_udb(self), pid);
		}

		r = next;
		count ++;
	}

	return count;
	*/
}

int SkipDB_isOpen(SkipDB *self)
{
	return SkipDB_udb(self) && UDB_isOpen(SkipDB_udb(self));
}

void SkipDB_objectMarkFunc_(SkipDB *self, SkipDBObjectMarkFunc *func)
{
	if (self)
		self->objectMarkFunc = func;
}

void SkipDB_freeObjectCallback_(SkipDB *self, SkipDBFreeObjectFunc *func)
{
	if (self)
		self->objectFreeFunc = func;
}

void SkipDB_showUpdate(SkipDB *self)
{
	int i;
	if (NULL == self || NULL == self->header) {
		printf("SkipDB update vector: unavailable\n");
		return;
	}
	i = SkipDBRecord_level(self->header);
	if (i > SKIPDB_MAX_LEVEL)
		i = SKIPDB_MAX_LEVEL;
	printf("SkipDB update vector:\n");
	while (i-- > 0) {
		SkipDBRecord *record = self->update[i];
		Datum key;
		if (NULL == record) {
			printf("  %i : (null)\n", i);
			continue;
		}
		key = SkipDBRecord_keyDatum(record);
		printf("  %i : %.*s\n", i,
			(int)(key.size > INT_MAX ? INT_MAX : key.size),
			key.data ? (const char *)key.data : "");
	}
	printf("\n");
}

void SkipDB_show(SkipDB *self)
{
	SkipDBRecord *record;
	size_t guard = 0;
	if (NULL == self || NULL == self->header) {
		printf("SkipDB: unavailable\n");
		return;
	}
	printf("\nSkipDB:\n");
	record = self->header;
	while (record && guard++ < (size_t)1000000) {
		SkipDBRecord_show(record);
		record = SkipDBRecord_recordAtLevel_(record, 0);
	}
	if (record)
		printf("SkipDB: traversal stopped (cycle or corrupt linkage)\n");
	printf("\n\n");
}
