/* SkipDB transaction synchronization and root-record recovery. */
#include "SkipDBInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

void SkipDB_sync(SkipDB *self)
{
	if (NULL == self || NULL == self->dirtyRecords ||
		NULL == self->pidsToRemove || !SkipDB_isOpen(self) ||
		NULL == self->udb || !self->udb->withinTransaction)
		return;

	/*
	 * The in-memory root can have no PID even though headerPid is preseeded
	 * to the reserved root ID (1). Reserve the root before saving any dirty
	 * record so insertion order cannot let the first user record claim PID 1.
	 */
	if (NULL == self->header)
		return;
	self->headerPid = SkipDBRecord_pid(self->header);
	if (self->headerPid == 0)
		self->headerPid = SkipDBRecord_pidAllocIfNeeded(self->header);
	if (self->headerPid == 0)
		return;

	SkipDB_saveDirtyRecords(self);
	/* A low-level write failure closes UDB to force rollback/recovery. */
	if (!SkipDB_isOpen(self))
		return;

	SkipDB_deleteRecordsToRemove(self);
	if (!SkipDB_isOpen(self))
		return;

	List_removeAll(self->dirtyRecords);
	List_removeAll(self->pidsToRemove);
	SkipDB_freeExcessCachedRecords(self);
}

SKIPDB_API void SkipDB_beginTransaction(SkipDB *self)
{
	if (self && SkipDB_isOpen(self))
		UDB_beginTransaction(self->udb);
}

SKIPDB_API void SkipDB_commitTransaction(SkipDB *self)
{
	if (NULL == self || !SkipDB_isOpen(self))
		return;
	SkipDB_sync(self);
	if (SkipDB_isOpen(self))
		UDB_commitTransaction(self->udb);
}

void SkipDB_saveDirtyRecords(SkipDB *self)
{
	size_t i;
	if (NULL == self || NULL == self->dirtyRecords || !SkipDB_isOpen(self) ||
		NULL == self->udb || !self->udb->withinTransaction)
		return;

	List_do_(self->dirtyRecords, (ListDoCallback *)SkipDBRecord_save);
	if (!SkipDB_isOpen(self))
		return;

	/* The callback is void; remaining dirty rows mean persistence failed. */
	for (i = 0; i < self->dirtyRecords->size; ++i) {
		SkipDBRecord *record = (SkipDBRecord *)self->dirtyRecords->items[i];
		if (record && SkipDBRecord_isDirty(record)) {
			fprintf(stderr, "SkipDB: refusing commit after a record save failed\n");
			UDB_close(self->udb);
			return;
		}
	}
}

void SkipDB_deleteRecordsToRemove(SkipDB *self)
{
	size_t i;
	UDB *udb;
	if (NULL == self || NULL == self->pidsToRemove ||
		!SkipDB_isOpen(self) || NULL == self->udb ||
		!self->udb->withinTransaction)
		return;
	udb = SkipDB_udb(self);
	for (i = 0; i < self->pidsToRemove->size && SkipDB_isOpen(self); ++i)
		UDB_removeAt_(udb, (PID_TYPE)self->pidsToRemove->items[i]);
}

void SkipDB_readRootRecord(SkipDB *self)
{
	SkipDBRecord *record;
	if (NULL == self || !SkipDB_isOpen(self) || NULL == self->pidToRecord)
		return;

	record = SkipDB_recordAtPid_(self, self->headerPid);
	if (NULL == record) {
		/* Only a genuinely new index (reserved PID 0 only) may create a root. */
		if (self->headerPid != 0 && self->udb && self->udb->index &&
			(UDBIndex_posForPid_(self->udb->index, self->headerPid) != 0 ||
			 UDBIndex_pidCount(self->udb->index) != 1 ||
			 ferror(self->udb->index->file->file) ||
			 (self->udb->records && self->udb->records->file &&
			  self->udb->records->file->maxPos > (long)sizeof(PID_TYPE)))) {
			fprintf(stderr, "SkipDB: root record %" PID_FORMAT " is missing or unreadable in a non-empty index\n",
				self->headerPid);
			return;
		}
		record = SkipDBRecord_newWithDB_(self);
		if (NULL == record)
			return;
		SkipDBRecord_keyDatum_(record, Datum_Empty());
		SkipDBRecord_markAsDirty(record);
		/* A new root has no PID until its first transactional save. Keep
		 * the reserved root PID rather than replacing it with the zero sentinel. */
		if (SkipDBRecord_pid(record) != 0)
			self->headerPid = SkipDBRecord_pid(record);
	}
	self->header = record;
}
