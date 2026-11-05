/*#io
SkipDB ioDoc(
		   docCopyright("Steve Dekorte", 2004)
		   docLicense("BSD revised")
		   docObject("SkipDB")
		   docDescription("BerkeleyDB style database implemented with skip lists instead of a b-tree.")
		   */

#include "SkipDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

// lookups

void SkipDB_clearUpdate(SkipDB *self);

// ops

void SkipDB_readRootRecord(SkipDB *self);

// ----------------------------------------------------------

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

// notifications ---------------------------------

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

// cache -------------------------------------

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

int SkipDB_isOpen(SkipDB *self)
{
	return SkipDB_udb(self) && UDB_isOpen(SkipDB_udb(self));
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
// transactions ---------------------------------------------------

void SkipDB_sync(SkipDB *self)
{
	if (NULL == self || NULL == self->dirtyRecords ||
		NULL == self->pidsToRemove || !SkipDB_isOpen(self) ||
		NULL == self->udb || !self->udb->withinTransaction)
		return;

	if (!self->headerPid) {
		if (NULL == self->header)
			return;
		self->headerPid = SkipDBRecord_pidAllocIfNeeded(self->header);
		if (self->headerPid == 0)
			return;
	}

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
			  self->udb->records->file->maxPos > (long)sizeof(PID_TYPE))) {
			fprintf(stderr, "SkipDB: root record %" PID_FORMAT " is missing or unreadable in a non-empty index\n",
				self->headerPid);
			return;
		}
		record = SkipDBRecord_newWithDB_(self);
		if (NULL == record)
			return;
		SkipDBRecord_keyDatum_(record, Datum_Empty());
		SkipDBRecord_markAsDirty(record);
		self->headerPid = SkipDBRecord_pid(record);
	}
	self->header = record;
}


int SkipDB_level(SkipDB *self)
{
	return self && self->header ? SkipDBRecord_level(self->header) : 0;
}


void SkipDB_level_(SkipDB *self, int level)
{
	if (NULL == self || NULL == self->header ||
		level < 0 || level > SKIPDB_MAX_LEVEL)
		return;
	SkipDBRecord_level_(self->header, level);
	if (SkipDBRecord_level(self->header) == level)
		SkipDBRecord_markAsDirty(self->header);
}


int SkipDB_pickRandomRecordLevel(SkipDB *self)
{
	double random_value;
	int level = 1;
	int current_level;
	if (NULL == self || NULL == self->randomGen || NULL == self->header)
		return 0;
	random_value = RandomGen_randomDouble(self->randomGen);
	while (random_value < self->p && level < SKIPDB_MAX_LEVEL) {
		level++;
		random_value = RandomGen_randomDouble(self->randomGen);
	}
	current_level = SkipDB_level(self);
	if (level > current_level)
		level = current_level + 1;
	if (level < 1)
		level = 1;
	return level;
}


SkipDBRecord *SkipDB_recordAtPid_(SkipDB *self, PID_TYPE pid)
{
	SkipDBRecord *record;
	Datum data;

	if (NULL == self || pid == 0 || NULL == self->pidToRecord ||
		NULL == self->stream || !SkipDB_isOpen(self))
		return NULL;

	record = (SkipDBRecord *)PHash_at_(self->pidToRecord, (void *)pid);
	if (record)
		return record;

	data = UDB_at_(SkipDB_udb(self), pid);
	if (data.size > 0 && data.data != NULL) {
		record = SkipDBRecord_newWithDB_(self);
		if (NULL == record)
			return NULL;
		BStream_setData_length_(self->stream, data.data, data.size);
		if (!SkipDBRecord_fromStream_(record, self->stream)) {
			fprintf(stderr, "SkipDB: invalid serialized record for PID %" PID_FORMAT "\n", pid);
			SkipDBRecord_dealloc(record);
			return NULL;
		}
		SkipDBRecord_pid_(record, pid);
		return record;
	}

	if (pid != 1)
		fprintf(stderr, "SkipDB: missing record for PID %" PID_FORMAT "\n", pid);
	return NULL;
}

// lookups -----------------------------

void SkipDB_updateAt_put_(SkipDB *self, int level, SkipDBRecord *r)
{
	if (NULL == self || level < 0 || level >= SKIPDB_MAX_LEVEL)
		return;
	self->update[level] = r;
}

void SkipDB_clearUpdate(SkipDB *self)
{
	int i;
	if (NULL == self)
		return;
	for (i = 0; i < SKIPDB_MAX_LEVEL; ++i)
		self->update[i] = NULL;
}


// Record API -------------------------------------

SkipDBRecord *SkipDB_recordAt_(SkipDB *self, Datum k)
{
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		(k.size > 0 && NULL == k.data))
		return NULL;
	SkipDB_clearUpdate(self);
	/* Use the full walk so every level's predecessor is populated. */
	return SkipDBRecord_find_quick_(self->header, k, 0);
}


int SkipDB_replace_put_(SkipDB *self, Datum k, Datum v)
{
	SkipDBRecord *record;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		NULL == self->udb || !self->udb->withinTransaction ||
		(k.size > 0 && NULL == k.data) ||
		(v.size > 0 && NULL == v.data))
		return 0;
	record = SkipDB_recordAt_(self, k);
	if (NULL == record)
		return 0;
	SkipDBRecord_valueDatum_(record, v);
	SkipDBRecord_markAsDirty(record);
	return 1;
}


static int SkipDB_recordAt_put_inner(SkipDB *self, Datum k, Datum v, SkipDBRecord **pp)
{
	SkipDBRecord *record;
	int level, old_header_level;
	if (pp)
		*pp = NULL;
	/* RAM-only callers may update the in-memory index without a disk transaction. */
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		(k.size > 0 && NULL == k.data) ||
		(v.size > 0 && NULL == v.data))
		return 0;

	record = SkipDB_recordAt_(self, k);
	/* A corrupt lookup closes the database; never turn its NULL result into
	 * an apparent insert or mutate detached in-memory records. */
	if (!SkipDB_isOpen(self)) {
		SkipDB_clearUpdate(self);
		return 0;
	}
	if (record) {
		SkipDBRecord_valueDatum_(record, v);
		SkipDBRecord_markAsDirty(record);
		if (pp)
			*pp = record;
		return 1;
	}

	record = SkipDBRecord_newWithDB_(self);
	if (NULL == record)
		return 0;
	SkipDBRecord_keyDatum_(record, k);
	SkipDBRecord_valueDatum_(record, v);

	level = SkipDB_pickRandomRecordLevel(self);
	if (level < 1 || level > SKIPDB_MAX_LEVEL) {
		SkipDBRecord_dealloc(record);
		SkipDB_clearUpdate(self);
		return 0;
	}
	SkipDBRecord_level_(record, level);
	if (SkipDBRecord_level(record) != level) {
		SkipDBRecord_dealloc(record);
		SkipDB_clearUpdate(self);
		return 0;
	}

	old_header_level = SkipDB_level(self);
	if (level > old_header_level) {
		int i;
		for (i = old_header_level; i < level; ++i)
			SkipDB_updateAt_put_(self, i, self->header);
		SkipDB_level_(self, level);
		if (SkipDB_level(self) != level) {
			SkipDBRecord_dealloc(record);
			SkipDB_clearUpdate(self);
			return 0;
		}
	}

	/* Never link a new row through an incomplete update path. */
	{
		int i;
		for (i = 0; i < level; ++i) {
			if (NULL == self->update[i]) {
				SkipDBRecord_dealloc(record);
				SkipDB_clearUpdate(self);
				return 0;
			}
		}
	}

	SkipDBRecord_markAsDirty(record);
	for (level--; level >= 0; --level) {
		SkipDBRecord *previous = self->update[level];
		if (NULL == previous) {
			SkipDBRecord_dealloc(record);
			SkipDB_clearUpdate(self);
			return 0;
		}
		SkipDBRecord_copyLevel_from_(record, level, previous);
		if (level == 0) {
			SkipDBRecord *next = SkipDBRecord_nextRecord(previous);
			SkipDBRecord_previousRecord_(record, previous);
			if (next) {
				SkipDBRecord_previousRecord_(next, record);
				SkipDBRecord_markAsDirty(next);
			}
		}
		SkipDBRecord_atLevel_setRecord_(previous, level, record);
		SkipDBRecord_markAsDirty(previous);
	}
	SkipDBRecord_markAsDirty(record);
	if (pp)
		*pp = record;
	return 1;
}
SkipDBRecord *SkipDB_recordAt_put_(SkipDB *self, Datum k, Datum v) {
    SkipDBRecord *p = NULL;
    SkipDB_recordAt_put_inner(self, k, v, &p);
    return p;
}

// Datum API -------------------------------------

Datum SkipDB_at_(SkipDB *self, Datum k)
{
	SkipDBRecord *record;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		(k.size > 0 && NULL == k.data))
		return Datum_Empty();
	SkipDB_clearUpdate(self);
	record = SkipDBRecord_find_quick_(self->header, k, 1);
	if (record)
		return Datum_FromUArray_(SkipDBRecord_value(record));
	return Datum_Empty();
}


int SkipDB_at_put_(SkipDB *self, Datum k, Datum v)
{
	//SkipDB_recordAt_put_(self, k, v);
        return SkipDB_recordAt_put_inner(self, k, v, NULL);
}

void SkipDB_removeAt_(SkipDB *self, Datum k)
{
	SkipDBRecord *record, *last_update = NULL;
	int i;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		NULL == self->udb || !self->udb->withinTransaction ||
		(k.size > 0 && NULL == k.data))
		return;

	record = SkipDB_recordAt_(self, k);
	if (NULL == record)
		return;

	for (i = 0; i < SKIPDB_MAX_LEVEL; ++i) {
		SkipDBRecord *update = self->update[i];
		if (update == record)
			break;
		if (update && update != last_update)
			SkipDBRecord_willRemove_(update, record);
		if (i == 0) {
			SkipDBRecord *next = SkipDBRecord_nextRecord(record);
			SkipDBRecord *previous = SkipDBRecord_previousRecord(record);
			if (next) {
				SkipDBRecord_previousRecord_(next, previous);
				SkipDBRecord_markAsDirty(next);
			}
		}
		last_update = update;
	}

	{
		PID_TYPE pid = SkipDBRecord_pid(record);
		size_t i;
		if (pid != 0 && (NULL == self->pidsToRemove ||
			NULL == List_append_(self->pidsToRemove, (void *)pid))) {
			/* Otherwise the unlinked row would remain live in the on-disk UDB. */
			fprintf(stderr, "SkipDB: unable to queue removed PID; aborting transaction\n");
			if (self->udb)
				UDB_close(self->udb);
		}
		if (self->dirtyRecords)
			List_remove_(self->dirtyRecords, record);
		/* Cursors hold raw record pointers; invalidate them before freeing it. */
		if (self->cursors) {
			for (i = 0; i < self->cursors->size; ++i) {
				SkipDBCursor *cursor = (SkipDBCursor *)self->cursors->items[i];
				if (cursor && cursor->record == record)
					cursor->record = NULL;
			}
		}
	}

	SkipDBRecord_dealloc(record);
	SkipDB_clearUpdate(self);
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


// objects --------------------------------------------------

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


// cursor ---------------------------------

int SkipDB_count(SkipDB *self)
{
	int count = 0;
	long steps = 0, max_steps;
	SkipDBRecord *record;
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		NULL == self->udb || NULL == self->udb->index)
		return 0;
	max_steps = UDBIndex_pidCount(self->udb->index);
	if (max_steps <= 0) {
		fprintf(stderr, "SkipDB: invalid PID index length while counting records\n");
		SkipDB_close(self);
		return -1;
	}
	record = SkipDB_firstRecord(self);
	while (record) {
		/* A valid list cannot contain more rows than the PID index. */
		if (++steps > max_steps) {
			fprintf(stderr, "SkipDB: cycle or invalid link in record list\n");
			SkipDB_close(self);
			return -1;
		}
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

// moving from in-memory to on-disk ---------------------------------

int SkipDB_mergeInto_(SkipDB *self, SkipDB *other)
{
	SkipDBRecord *record;
	long steps = 0, max_steps;
	if (NULL == self || NULL == other || self == other ||
		NULL == self->header || !SkipDB_isOpen(self) || !SkipDB_isOpen(other) ||
		NULL == self->udb || NULL == self->udb->index)
		return 0;
	max_steps = UDBIndex_pidCount(self->udb->index);
	if (max_steps <= 0) {
		fprintf(stderr, "SkipDB: invalid PID index length during merge\n");
		SkipDB_close(self);
		return 0;
	}

	record = self->header;
	while ((record = SkipDBRecord_nextRecord(record)) != NULL) {
		Datum key, value;
		if (++steps > max_steps) {
			fprintf(stderr, "SkipDB: cycle or invalid link during merge\n");
			SkipDB_close(self);
			return 0;
		}
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


static int SkipDB_keyHasPrefix_(Datum key, Datum prefix)
{
	size_t prefix_len;
	if (prefix.size == 0 || NULL == prefix.data ||
		NULL == key.data || key.size < prefix.size)
		return 0;
	/* Datums from the string API include the terminating NUL in size. */
	prefix_len = prefix.size - 1;
	return prefix_len == 0 || memcmp(key.data, prefix.data, prefix_len) == 0;
}

void SkipDB_list_prefix(SkipDB *self, Datum k, void *ctx,
                        skipdb_list_callback callback)
{
	SkipDBCursor *cursor;
	SkipDBRecord *record;
	long steps = 0, max_steps;
	if (NULL == self || NULL == callback || k.size == 0 || NULL == k.data ||
		!SkipDB_isOpen(self) || NULL == self->udb || NULL == self->udb->index)
		return;
	cursor = SkipDB_createCursor(self);
	if (NULL == cursor)
		return;
	max_steps = UDBIndex_pidCount(self->udb->index);
	if (max_steps <= 0) {
		fprintf(stderr, "SkipDB: invalid PID index length during prefix scan\n");
		SkipDB_close(self);
		SkipDBCursor_release(cursor);
		return;
	}
	record = SkipDBCursor_goto_(cursor, k);
	while (record) {
		Datum key;
		if (++steps > max_steps) {
			fprintf(stderr, "SkipDB: cycle or invalid link during prefix scan\n");
			SkipDB_close(self);
			break;
		}
		key = SkipDBRecord_keyDatum(record);
		if (!SkipDB_keyHasPrefix_(key, k))
			break;
		callback(self, record, ctx);
		record = SkipDBCursor_next(cursor);
	}
	SkipDBCursor_release(cursor);
}

/* TODO howto make better */
SkipDBRecord *SkipDB_list_next(SkipDB *self, Datum k, SkipDBCursor *cursor)
{
	SkipDBRecord *record;
	Datum key;
	if (NULL == self || NULL == cursor || k.size == 0 || NULL == k.data)
		return NULL;
	record = SkipDBCursor_next(cursor);
	if (record) {
		key = SkipDBRecord_keyDatum(record);
		if (!SkipDB_keyHasPrefix_(key, k))
			return NULL;
	}
	return record;
}

SkipDBRecord *SkipDB_list_first(SkipDB *self, Datum k, SkipDBCursor **pcur)
{
	SkipDBCursor *cursor;
	SkipDBRecord *record;
	Datum key;
	if (pcur)
		*pcur = NULL;
	if (NULL == self || NULL == pcur || k.size == 0 || NULL == k.data ||
		!SkipDB_isOpen(self))
		return NULL;
	cursor = SkipDB_createCursor(self);
	if (NULL == cursor)
		return NULL;
	*pcur = cursor;
	record = SkipDBCursor_goto_(cursor, k);
	if (NULL == record)
		return NULL;
	key = SkipDBRecord_keyDatum(record);
	if (SkipDB_keyHasPrefix_(key, k))
		return record;
	return SkipDB_list_next(self, k, cursor);
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
