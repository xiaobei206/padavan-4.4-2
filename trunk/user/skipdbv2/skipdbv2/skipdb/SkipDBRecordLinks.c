/* SkipDB skip-list links, bounded traversal, and predecessor repair. */
#include "SkipDB.h"
#include "SkipDBRecordInternal.h"
#include <stdio.h>

static void SkipDBPointer_setRecord_(SkipDBPointer *self, SkipDBRecord *r)
{
	self->record = r;
}

/* A valid level always has a matching pointer array when nonempty. */
static int SkipDBRecord_hasValidLevel_(SkipDBRecord *self)
{
	return self != NULL && self->level >= 0 &&
		self->level <= SKIPDB_MAX_LEVEL &&
		(self->level == 0 || self->pointers != NULL);
}

/* Validate the shared PID-population bound before walking forward links. */
static int SkipDBRecord_pidCountForTraversal_(SkipDB *db, long *max_steps)
{
	if (NULL == db || NULL == max_steps || NULL == db->udb ||
		NULL == db->udb->index || !SkipDB_isOpen(db))
		return 0;
	*max_steps = UDBIndex_pidCount(db->udb->index);
	if (*max_steps <= 0) {
		/* A usable SkipDB always has at least the reserved PID-0 slot. */
		UDB_close(db->udb);
		return 0;
	}
	return 1;
}

static int SkipDBRecord_checkLevel_(SkipDBRecord *self, int level)
{
	return SkipDBRecord_hasValidLevel_(self) && self->level > 0 &&
		level >= 0 && level < self->level;
}

SkipDBPointer *SkipDBRecord_pointerAtLevel_(SkipDBRecord *self, int level)
{
	if (!SkipDBRecord_checkLevel_(self, level))
		return NULL;
	return self->pointers + level;
}

void SkipDBRecord_atLevel_setPid_setSuffix_(SkipDBRecord *self, int level, PID_TYPE pid, uint8_t sMatch)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	if (NULL == p)
		return;
	p->pid = pid;
	SkipDBPointer_setRecord_(p, NULL);
	p->matchingPrefixSize = sMatch;
}

void SkipDBRecord_atLevel_setPid_(SkipDBRecord *self, int level, PID_TYPE pid)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	if (NULL == p)
		return;
	p->pid = pid;
	SkipDBPointer_setRecord_(p, NULL);
}

PID_TYPE SkipDBRecord_allocedPidAtLevel_(SkipDBRecord *self, int level)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	if (NULL == p)
		return 0;
	if (!p->pid && p->record)
		p->pid = SkipDBRecord_pidAllocIfNeeded(p->record);
	return p->pid;
}

PID_TYPE SkipDBRecord_pidAtLevel_(SkipDBRecord *self, int level)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	if (NULL == p)
		return 0;
	/* Resolve a cached pointer to its own PID, not the current record's PID. */
	if (!p->pid && p->record)
		p->pid = SkipDBRecord_pid(p->record);
	return p->pid;
}

unsigned char SkipDBRecord_matchingPrefixSizeWith_(SkipDBRecord *self, SkipDBRecord *r)
{
	Datum k1, k2;
	if (NULL == self || NULL == r || NULL == self->key || NULL == r->key)
		return 0;
	k1 = Datum_FromUArray_(self->key);
	k2 = Datum_FromUArray_(r->key);
	return Datum_matchingPrefixSizeWith_(&k1, &k2);
	//return UArray_matchingPrefixSizeWith_(self->key, r->key);
}

void SkipDBRecord_atLevel_setRecord_(SkipDBRecord *self, int level, SkipDBRecord *r)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	if (NULL == p || NULL == r)
		return;
	p->pid = SkipDBRecord_pid(r);
	p->record = r;
	p->matchingPrefixSize = SkipDBRecord_matchingPrefixSizeWith_(self, r);
}

SkipDBRecord *SkipDBRecord_cachedRecordAtLevel_(SkipDBRecord *self, int level)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	return p ? p->record : NULL;
}

SkipDBRecord *SkipDBRecord_recordAtLevel_(SkipDBRecord *self, int level)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	SkipDBRecord *r;
	if (NULL == p)
		return NULL;
	r = p->record;
	if (r && self->sdb) {
		SkipDB *db = (SkipDB *)self->sdb;
		if (r->sdb != db ||
			(p->pid != 0 && p->pid != SkipDBRecord_pid(r))) {
			/* The cached object must agree with its owning database and PID. */
			fprintf(stderr, "SkipDB: cached forward reference belongs to another database or PID\n");
			UDB_close(SkipDB_udb(db));
			return NULL;
		}
	}
	if (NULL == r && p->pid && self->sdb) {
		SkipDB *db = (SkipDB *)self->sdb;
		r = SkipDB_recordAtPid_(db, p->pid);
		if (NULL == r) {
			/* A nonzero forward PID is authoritative; treating a missing row
			 * as an ordinary end-of-list would silently hide later records. */
			fprintf(stderr, "SkipDB: invalid forward reference to PID %lu\n",
				(unsigned long)p->pid);
			UDB_close(SkipDB_udb(db));
			return NULL;
		}
		p->record = r;
	}
	return r;
}

static int SkipDBRecord_compareKey_(SkipDBRecord *self, Datum key)
{
	Datum d = Datum_FromUArray_(self->key);
	return Datum_compare_(&d, &key);
}

SkipDBRecord *SkipDBRecord_find_quick_(SkipDBRecord *self, Datum key, int quick)
{
	SkipDB *db;
	SkipDBRecord *current;
	int level;
	size_t steps = 0;
	size_t step_limit;
	long max_steps;

	if (NULL == self || NULL == self->key ||
		(key.size > 0 && NULL == key.data))
		return NULL;
	db = (SkipDB *)self->sdb;
	if (NULL == db || NULL == db->udb || NULL == db->udb->index ||
		!SkipDB_isOpen(db))
		return NULL;
	if (!SkipDBRecord_pidCountForTraversal_(db, &max_steps))
		return NULL;
	/* Each live record may require a horizontal step, plus up to one
	 * downward step per skip-list level. Do not count vertical descents
	 * against the PID population or valid tail searches can stop early. */
	step_limit = (size_t)max_steps;
	if (step_limit > (size_t)-1 - SKIPDB_MAX_LEVEL)
		step_limit = (size_t)-1;
	else
		step_limit += SKIPDB_MAX_LEVEL;
	current = self;
	/* A corrupt start level must not turn a failed search into a false miss. */
	if (!SkipDBRecord_hasValidLevel_(current)) {
		fprintf(stderr, "SkipDB: invalid record level during key traversal\n");
		SkipDB_clearUpdate(db);
		UDB_close(db->udb);
		return NULL;
	}

#ifdef USE_SUFFIX_MATCHING
	{
		Datum kDatum = Datum_FromUArray_(self->key);
		unsigned char skMatchSize = Datum_matchingPrefixSizeWith_(&kDatum, &key);
#endif
		level = current->level - 1;
		while (current && level >= 0) {
			SkipDBRecord *r;
			int c;

			if (++steps > step_limit || level >= SKIPDB_MAX_LEVEL ||
				!SkipDBRecord_hasValidLevel_(current)) {
				SkipDB_clearUpdate(db);
				UDB_close(db->udb);
				return NULL;
			}
			r = SkipDBRecord_recordAtLevel_(current, level);
			if (!SkipDB_isOpen(db)) {
				SkipDB_clearUpdate(db);
				return NULL;
			}
			SkipDB_updateAt_put_(db, level, current);
			if (NULL == r) {
				level--;
				continue;
			}
			/* A non-root forward target must have a usable pointer level. */
			if (!SkipDBRecord_hasValidLevel_(r) || r->level == 0) {
				fprintf(stderr, "SkipDB: invalid forward record level during key traversal\n");
				SkipDB_clearUpdate(db);
				UDB_close(db->udb);
				return NULL;
			}
			if (NULL == r->key) {
				SkipDB_clearUpdate(db);
				UDB_close(db->udb);
				return NULL;
			}
#ifdef USE_SUFFIX_MATCHING
			{
				unsigned char pkMatchSize = current->pointers[level].matchingPrefixSize;
				if (pkMatchSize > skMatchSize) {
					current = r;
					level = current->level - 1;
					continue;
				}
			}
#endif
			c = SkipDBRecord_compareKey_(r, key);
			if (c == 0) {
				if (level == 0 || quick)
					return r;
			} else if (c < 0) {
				current = r;
				level = current->level - 1;
				continue;
			}
			level--;
		}
#ifdef USE_SUFFIX_MATCHING
	}
#endif
	return NULL;
}

SkipDBRecord *SkipDBRecord_findLastRecord(SkipDBRecord *self)
{
	SkipDBRecord *current = self;
	SkipDBRecord *next;
	SkipDB *db = self ? (SkipDB *)self->sdb : NULL;
	long max_steps;
	int level;
	size_t steps = 0;

	if (NULL == db || NULL == db->udb || NULL == db->udb->index ||
		!SkipDB_isOpen(db))
		return NULL;
	if (!SkipDBRecord_pidCountForTraversal_(db, &max_steps))
		return NULL;

	while (current)
	{
		if (++steps > (size_t)max_steps ||
			!SkipDBRecord_hasValidLevel_(current) ||
			(current != db->header && current->level == 0)) {
			fprintf(stderr, "SkipDB: invalid linkage while finding last record\n");
			SkipDB_clearUpdate(db);
			UDB_close(db->udb);
			return NULL;
		}

		next = NULL;
		for (level = current->level - 1; level >= 0; --level)
		{
			next = SkipDBRecord_recordAtLevel_(current, level);
			if (!SkipDB_isOpen(db))
				return NULL;
			if (next)
				break;
		}
		if (NULL == next)
			return current;
		current = next;
	}
	return NULL;
}

void SkipDBRecord_copyLevel_from_(SkipDBRecord *self, int level, SkipDBRecord *other)
{
	PID_TYPE new_pid;
	SkipDBRecord *cached;
	if (NULL == self || NULL == other ||
		!SkipDBRecord_checkLevel_(self, level) ||
		!SkipDBRecord_checkLevel_(other, level))
		return;
	new_pid = SkipDBRecord_pidAtLevel_(other, level);
	cached = SkipDBRecord_cachedRecordAtLevel_(other, level);
	if (cached)
		SkipDBRecord_atLevel_setRecord_(self, level, cached);
	else
		SkipDBRecord_atLevel_setPid_(self, level, new_pid);
}

void SkipDBRecord_willRemove_(SkipDBRecord *self, SkipDBRecord *other)
{
	PID_TYPE otherPid;
	int level;

	if (NULL == other || !SkipDBRecord_hasValidLevel_(self) ||
		self->level == 0)
		return;
	otherPid = SkipDBRecord_pid(other);
	level = self->level;

	while (level --)
	{
		if ((otherPid && SkipDBRecord_pidAtLevel_(self, level) == otherPid) ||
		    (SkipDBRecord_recordAtLevel_(self, level) == other))
		{
			SkipDBRecord_copyLevel_from_(self, level, other);
			SkipDBRecord_markAsDirty(self);
		}
	}

	/*
	 if (SkipDBRecord_hasRecord_(self, other))
	 {
		 printf("error - has record after remove\n");
		 exit(-1);
	 }
	 */
}

int SkipDBRecord_hasRecord_(SkipDBRecord *self, SkipDBRecord *other)
{
	int i;

	if (NULL == other || !SkipDBRecord_hasValidLevel_(self))
		return 0;

	for (i = 0; i < self->level; i ++)
	{
		if (self->pointers[i].record == other)
		{
			return 1;
		}
	}
	return 0;
}

void SkipDBRecord_show(SkipDBRecord *self)
{
	int i;
	//printf("SRec %i '%s' '%s' (", self->pid,
	//	UArray_asCString(self->key), UArray_asCString(self->value));
	//printf("SRec %i '%s' (", self->pid, UArray_asCString(self->key));
	printf("  record %i '%s'\t:\t", (int)self->pid, UArray_asCString(self->key));

	for (i = 0; i < self->level; i ++)
	{
		SkipDBRecord *r = self->pointers[i].record;

		if (r)
		{
			printf("'%s'", UArray_asCString(r->key));
		}
		else
		{
			printf("%i", (int)(self->pointers[i].pid));
		}

		printf("\t");

		//if (i != self->level - 1) printf(", ");
	}

	printf("\n");
}

SkipDBRecord *SkipDBRecord_nextRecord(SkipDBRecord *self)
{
	return self ? SkipDBRecord_recordAtLevel_(self, 0) : NULL;
}

void SkipDBRecord_previousPid_(SkipDBRecord *self, PID_TYPE pid)
{
	if (NULL == self)
		return;
	self->previousPid = pid;
	self->previousRecord = NULL;
}

PID_TYPE SkipDBRecord_previousPid(SkipDBRecord *self)
{
	return self ? self->previousPid : 0;
}

void SkipDBRecord_previousRecord_(SkipDBRecord *self, SkipDBRecord *r)
{
	if (NULL == self || r == self)
		return;
	if (r != self->previousRecord ||
		(NULL == r && self->previousPid != 0))
	{
		self->previousRecord = r;
		self->previousPid = r ? SkipDBRecord_pid(r) : 0;
		SkipDBRecord_markAsDirty(self);
	}
}

SkipDBRecord *SkipDBRecord_previousRecord(SkipDBRecord *self)
{
	SkipDB *db;
	SkipDBRecord *found;
	SkipDBRecord *previous;

	if (NULL == self)
		return NULL;

	db = (SkipDB *)self->sdb;
	if (NULL == db || NULL == self->key || !SkipDB_isOpen(db))
		return NULL;

	/*
	 * Reverse links are cache-only and are not serialized. Always reconstruct
	 * the predecessor from the authoritative forward skip-list instead of
	 * trusting a potentially stale previousRecord/previousPid after reload,
	 * cache eviction, or PID reuse.
	 */
	found = SkipDB_recordAt_(db, SkipDBRecord_keyDatum(self));
	if (!SkipDB_isOpen(db))
		return NULL;
	if (found != self) {
		fprintf(stderr, "SkipDB: reverse traversal target is not in the active index\n");
		SkipDB_close(db);
		return NULL;
	}

	previous = db->update[0];
	if (previous == self) {
		fprintf(stderr, "SkipDB: invalid self predecessor during reverse traversal\n");
		SkipDB_close(db);
		return NULL;
	}
	self->previousRecord = previous;
	self->previousPid = previous ? SkipDBRecord_pid(previous) : 0;
	return previous;
}
