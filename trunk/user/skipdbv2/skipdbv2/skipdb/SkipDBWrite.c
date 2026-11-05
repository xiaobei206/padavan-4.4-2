/* SkipDB skip-list insert, update, and removal operations. */
#include "SkipDBInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

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
