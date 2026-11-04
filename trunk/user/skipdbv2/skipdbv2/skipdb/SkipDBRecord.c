

#include "SkipDB.h"
#include "SkipDBRecord.h"

static void SkipDBPointer_setRecord_(SkipDBPointer *self, SkipDBRecord *r)
{
	self->record = r;
}


static PID_TYPE SkipDBPointer_pid(SkipDBPointer *self)
{
	return self->pid;
}

//-----------------------------------------------------

SkipDBRecord *SkipDBRecord_new(void)
{
	SkipDBRecord *self = (SkipDBRecord *)calloc(1, sizeof(SkipDBRecord));
	if (NULL == self)
		return NULL;
	self->key = UArray_new();
	self->value = UArray_new();
	self->ownsKey = 1;
	if (NULL == self->key || NULL == self->value) {
		if (self->key) UArray_free(self->key);
		if (self->value) UArray_free(self->value);
		free(self);
		return NULL;
	}
	return self;
}

SkipDBRecord *SkipDBRecord_newWithDB_(void *db)
{
	SkipDB *sdb = (SkipDB *)db;
	SkipDBRecord *self = SkipDBRecord_new();
	if (NULL == self)
		return NULL;
	SkipDBRecord_db_(self, sdb);
	if (sdb)
		SkipDB_noteNewRecord_(sdb, self);
	return self;
}

// older/younger ------------------------

void SkipDBRecord_setOlderRecord_(SkipDBRecord *self, SkipDBRecord *r)
{
	if (NULL == self || r == self)
		return;
	self->olderRecord = r;
}

SkipDBRecord *SkipDBRecord_olderRecord(SkipDBRecord *self)
{
	return self ? self->olderRecord : NULL;
}

void SkipDBRecord_setYoungerRecord_(SkipDBRecord *self, SkipDBRecord *r)
{
	if (NULL == self || r == self)
		return;
	self->youngerRecord = r;
}

SkipDBRecord *SkipDBRecord_youngerRecord(SkipDBRecord *self)
{
	return self ? self->youngerRecord : NULL;
}

void SkipDBRecord_removeFromAgeList(SkipDBRecord *self)
{
	SkipDBRecord *older, *younger;
	if (NULL == self)
		return;
	older = self->olderRecord;
	younger = self->youngerRecord;
	if (older)
		SkipDBRecord_setYoungerRecord_(older, younger);
	if (younger)
		SkipDBRecord_setOlderRecord_(younger, older);
	self->olderRecord = NULL;
	self->youngerRecord = NULL;
}

void SkipDBRecord_showAgeList(SkipDBRecord *self)
{
	SkipDBRecord *r = self;

	printf("age list:\n");

	while (r)
	{
		printf("  record %p '%s'\n",
			  (void *)r, (char *)UArray_asCString(r->key));
		r = r->olderRecord;
	}
}

//-----------------------------------------

int SkipDBRecord_pointersAreEmpty(SkipDBRecord *self)
{
	SkipDBPointer *pointers;
	if (NULL == self)
		return 1;
	if (self->level < 0 || self->level > SKIPDB_MAX_LEVEL ||
		(self->level > 0 && NULL == self->pointers))
		return 0;
	pointers = self->pointers;

	if (pointers)
	{
		int i, max = self->level;

		for (i = 0; i < max; i ++)
		{
			SkipDBRecord *r = pointers[i].record;
			PID_TYPE pid = pointers[i].pid;

			if (r || pid) return 0;
		}
	}

	return 1;
}


void SkipDBRecord_removeReferencesToUnmarked(SkipDBRecord *self)
{
	SkipDBPointer *pointers;
	if (NULL == self || self->level < 0 ||
		self->level > SKIPDB_MAX_LEVEL ||
		(self->level > 0 && NULL == self->pointers))
		return;
	pointers = self->pointers;

	if (pointers)
	{
		int i, max = self->level;

		for (i = 0; i < max; i ++)
		{
			SkipDBRecord *r = pointers[i].record;

			if (r && r->mark == 0)
			{
				pointers[i].pid = SkipDBRecord_pid(r);
				pointers[i].record = NULL;
			}
		}
	}

	if (self->previousRecord && self->previousRecord->mark == 0)
	{
		self->previousPid = SkipDBRecord_pid(self->previousRecord);
		self->previousRecord = NULL;
	}
}

//#define SKIPDB_DEBUG 1

#ifdef SKIPDB_DEBUG
static List *deallocedRecords = NULL;
#endif

void SkipDBRecord_dealloc(SkipDBRecord *self)
{
	if (NULL == self)
		return;
#ifdef SKIPDB_DEBUG
	if (!deallocedRecords) deallocedRecords = List_new();
	printf("SkipDBRecord_dealloc(%p)\n", (void *)self);
	List_append_(deallocedRecords, self);
#endif

	if (self->sdb)
		SkipDB_noteWillFreeRecord_((SkipDB *)(self->sdb), self);

	if (self->pointers)
	{
		if (self->level < 0 || self->level > SKIPDB_MAX_LEVEL) {
			/* A corrupt level must not be used to index this allocation. */
			self->level = 0;
		}

// Commented out since we should not touch records linked
//  from here, because they may have already been freed.
//  (e.g. see SkipDB_freeAllCachedRecords or SkipDB_freeExcessCachedRecords).
//		SkipDBPointer *next = self->pointers + 0;
//
//		if (next && next->record)
//		{
//			next->record->previousRecord = NULL;
//		}

		free(self->pointers);
	}

	if (self->ownsKey && self->key)
		UArray_free(self->key);
	if (self->value)
		UArray_free(self->value);


#ifdef SKIPDB_DEBUG
	return;
#endif
	free(self);
}

void SkipDBRecord_db_(SkipDBRecord *self, void *sdb)
{
	if (self)
		self->sdb = sdb;
}

void SkipDBRecord_pid_(SkipDBRecord *self, PID_TYPE pid)
{
	if (NULL == self)
		return;
	self->pid = pid;
	if (self->sdb && pid)
		SkipDB_noteAssignedPidToRecord_((SkipDB *)self->sdb, self);
}

PID_TYPE SkipDBRecord_pid(SkipDBRecord *self)
{
	return self ? self->pid : 0;
}

PID_TYPE SkipDBRecord_pidAllocIfNeeded(SkipDBRecord *self)
{
	PID_TYPE pid;
	if (NULL == self)
		return 0;
	if (!self->pid) {
		if (NULL == self->sdb)
			return 0;
		pid = UDB_allocPid(SkipDB_udb((SkipDB *)self->sdb));
		if (pid == 0)
			return 0;
		SkipDBRecord_pid_(self, pid);
	}
	return self->pid;
}

void SkipDBRecord_mark(SkipDBRecord *self)
{
	if (self)
		self->mark = 1;
}

void SkipDBRecord_markAsDirty(SkipDBRecord *self)
{
	if (self && !self->isDirty) {
		self->isDirty = 1;
		if (self->sdb)
			SkipDB_noteDirtyRecord_((SkipDB *)self->sdb, self);
	}
}

void SkipDBRecord_markAsClean(SkipDBRecord *self)
{
	if (self)
		self->isDirty = 0;
}

uint8_t SkipDBRecord_isDirty(SkipDBRecord *self)
{
	return self ? self->isDirty : 0;
}

// key ------------------------------------

void SkipDBRecord_keyDatum_(SkipDBRecord *self, Datum k)
{
	if (self && self->key && (k.size == 0 || k.data != NULL))
		UArray_setData_type_size_copy_(self->key, k.data, CTYPE_uint8_t, k.size, 1);
}

Datum SkipDBRecord_keyDatum(SkipDBRecord *self)
{
	return self && self->key ? Datum_FromUArray_(self->key) : Datum_Empty();
}

UArray *SkipDBRecord_key(SkipDBRecord *self)
{
	return self ? self->key : NULL;
}

// value ------------------------------------

void SkipDBRecord_valueDatum_(SkipDBRecord *self, Datum v)
{
	if (self && self->value && (v.size == 0 || v.data != NULL))
		UArray_setData_type_size_copy_(self->value, v.data, CTYPE_uint8_t, v.size, 1);
}

Datum SkipDBRecord_valueDatum(SkipDBRecord *self)
{
	return self && self->value ? Datum_FromUArray_(self->value) : Datum_Empty();
}

UArray *SkipDBRecord_value(SkipDBRecord *self)
{
	return self ? self->value : NULL;
}

// pointers ------------------------------------

int SkipDBRecord_level(SkipDBRecord *self)
{
	return self ? self->level : 0;
}

void SkipDBRecord_level_(SkipDBRecord *self, int level)
{
	SkipDBPointer *new_pointers;
	int old_level;
	if (NULL == self || level < 0 || level > SKIPDB_MAX_LEVEL)
		return;
	old_level = self->level;
	if (level == old_level)
		return;
	if (level == 0) {
		free(self->pointers);
		self->pointers = NULL;
		self->level = 0;
		return;
	}
	new_pointers = (SkipDBPointer *)realloc(self->pointers,
		(size_t)level * sizeof(SkipDBPointer));
	if (NULL == new_pointers)
		return;
	if (level > old_level)
		memset(new_pointers + old_level, 0,
			(size_t)(level - old_level) * sizeof(SkipDBPointer));
	self->pointers = new_pointers;
	self->level = level;
}

static int SkipDBRecord_checkLevel_(SkipDBRecord *self, int level)
{
	return self != NULL && level >= 0 && level < self->level &&
		self->pointers != NULL;
}

static SkipDBPointer *SkipDBRecord_pointerAtLevel_(SkipDBRecord *self, int level)
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

//直接下降到0层级，获取下个元素
SkipDBRecord *SkipDBRecord_recordAtLevel_(SkipDBRecord *self, int level)
{
	SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
	SkipDBRecord *r;
	if (NULL == p)
		return NULL;
	r = p->record;
	if (NULL == r && p->pid && self->sdb) {
		r = SkipDB_recordAtPid_((SkipDB *)self->sdb, p->pid);
		p->record = r;
	}
	return r;
}

// serialization ------------------------------------

int SkipDBRecord_toStream_(SkipDBRecord *self, BStream *s)
{
	int level, max;
	if (NULL == self || NULL == s || NULL == self->key ||
		NULL == self->value || NULL == s->ba || NULL == s->errorBa)
		return 0;
	max = self->level;
	if (max < 0 || max > SKIPDB_MAX_LEVEL ||
		(max > 0 && self->pointers == NULL))
		return 0;

	BStream_writeTaggedUArray_(s, self->key);
	BStream_writeTaggedUArray_(s, self->value);
	BStream_writeTaggedInt32_(s, max);
	for (level = 0; level < max; ++level) {
		SkipDBPointer *p = SkipDBRecord_pointerAtLevel_(self, level);
		PID_TYPE pid;
		if (NULL == p)
			return 0;
		pid = SkipDBRecord_allocedPidAtLevel_(self, level);
		if (p->record != NULL && pid == 0)
			return 0;
		if (pid > INT32_MAX)
			return 0;
		BStream_writeTaggedInt32_(s, (int32_t)pid);
		BStream_writeTaggedUint8_(s, p->matchingPrefixSize);
	}
	return UArray_size(s->errorBa) == 0;
}

int SkipDBRecord_fromStream_(SkipDBRecord *self, BStream *s)
{
	int level, max;
	size_t stream_size;

	if (NULL == self || NULL == s || NULL == s->ba ||
		NULL == self->key || NULL == self->value || NULL == s->errorBa)
		return 0;
	UArray_setSize_(s->errorBa, 0);
	stream_size = UArray_size(s->ba);
	if (s->index > stream_size)
		return 0;

	BStream_readTaggedUArray_(s, self->key);
	if (UArray_size(s->errorBa) != 0)
		return 0;
	BStream_readTaggedUArray_(s, self->value);
	if (UArray_size(s->errorBa) != 0)
		return 0;
	max = BStream_readTaggedInt32(s);
	if (UArray_size(s->errorBa) != 0 || max < 0 ||
		max > SKIPDB_MAX_LEVEL)
		return 0;

	SkipDBRecord_level_(self, max);
	if (self->level != max)
		return 0;
	for (level = 0; level < max; ++level) {
		int32_t encoded_pid = BStream_readTaggedInt32(s);
		uint8_t suffix;
		if (UArray_size(s->errorBa) != 0 || encoded_pid < 0)
			return 0;
		suffix = BStream_readTaggedUint8(s);
		if (UArray_size(s->errorBa) != 0)
			return 0;
		SkipDBRecord_atLevel_setPid_setSuffix_(self, level,
			(PID_TYPE)encoded_pid, suffix);
	}
	if (UArray_size(s->errorBa) != 0 || s->index != stream_size)
		return 0;
	return 1;
}

void SkipDBRecord_save(SkipDBRecord *self)
{
	SkipDB *sdb;
	BStream *stream;
	Datum data;
	PID_TYPE pid;

	if (NULL == self || !self->isDirty || NULL == self->sdb)
		return;
	sdb = (SkipDB *)self->sdb;
	stream = SkipDB_tmpStream(sdb);
	if (NULL == stream || NULL == UDB_udb(sdb) ||
		!UDB_isOpen(UDB_udb(sdb)))
		return;
	BStream_empty(stream);
	if (!SkipDBRecord_toStream_(self, stream))
		return;
	data = Datum_FromUArray_(BStream_byteArray(stream));
	pid = SkipDBRecord_pid(self);
	if (pid) {
		UDB_at_put_(UDB_udb(sdb), pid, data);
		if (!UDB_isOpen(UDB_udb(sdb)))
			return;
	} else {
		pid = UDB_put_(UDB_udb(sdb), data);
		if (pid == 0)
			return;
		SkipDBRecord_pid_(self, pid);
	}
	SkipDBRecord_markAsClean(self);
}

// search ------------------------------------

static int SkipDBRecord_compareKey_(SkipDBRecord *self, Datum key)
{
	Datum d = Datum_FromUArray_(self->key);
	return Datum_compare_(&d, &key);
}

//Added by jannson
//#define USE_SUFFIX_MATCHING 1

SkipDBRecord *SkipDBRecord_find_quick_(SkipDBRecord *self, Datum key, int quick)
{
	SkipDB *db;
	SkipDBRecord *current;
	int level;
	size_t steps = 0;
	long max_steps;

	if (NULL == self || NULL == self->key ||
		(key.size > 0 && NULL == key.data))
		return NULL;
	db = (SkipDB *)self->sdb;
	if (NULL == db || NULL == db->udb || NULL == db->udb->index)
		return NULL;
	max_steps = UDBIndex_pidCount(db->udb->index);
	if (max_steps <= 0)
		return NULL;
	current = self;

#ifdef USE_SUFFIX_MATCHING
	{
		Datum kDatum = Datum_FromUArray_(self->key);
		unsigned char skMatchSize = Datum_matchingPrefixSizeWith_(&kDatum, &key);
#endif
		level = current->level - 1;
		while (current && level >= 0) {
			SkipDBRecord *r;
			int c;

			if (++steps > (size_t)max_steps || level >= SKIPDB_MAX_LEVEL) {
				SkipDB_clearUpdate(db);
				return NULL;
			}
			r = SkipDBRecord_recordAtLevel_(current, level);
			SkipDB_updateAt_put_(db, level, current);
			if (NULL == r) {
				level--;
				continue;
			}
			if (NULL == r->key) {
				SkipDB_clearUpdate(db);
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

	if (NULL == db || NULL == db->udb || NULL == db->udb->index)
		return NULL;
	max_steps = UDBIndex_pidCount(db->udb->index);
	if (max_steps <= 0)
		return NULL;

	while (current)
	{
		if (++steps > (size_t)max_steps ||
			current->level < 0 || current->level > SKIPDB_MAX_LEVEL ||
			(current->level > 0 && NULL == current->pointers))
			return NULL;

		next = NULL;
		for (level = current->level - 1; level >= 0; --level)
		{
			next = SkipDBRecord_recordAtLevel_(current, level);
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
	PID_TYPE otherPid = SkipDBRecord_pid(other);
	int level = self->level;

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

// object ------------------------------------

void SkipDBRecord_object_(SkipDBRecord *self, void *object)
{
	if (self)
		self->object = object;
}

void *SkipDBRecord_object(SkipDBRecord *self)
{
	return self ? self->object : NULL;
}

// next ------------------------------------

SkipDBRecord *SkipDBRecord_nextRecord(SkipDBRecord *self)
{
	return self ? SkipDBRecord_recordAtLevel_(self, 0) : NULL;
}

// previous ------------------------------------

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
	if (r != self->previousRecord)
	{
		self->previousRecord = r;
		if (r)
			self->previousPid = SkipDBRecord_pid(r);
		SkipDBRecord_markAsDirty(self);
	}
}

#ifdef SKIPDB_DEBUG
void textValid(void *record)
{
	if (!deallocedRecords) deallocedRecords = List_new();
	if (List_contains_(deallocedRecords, record))
	{
		printf("attempt to access freed record %p\n", (void *)record);
		exit(-1);
	}
}
#endif

SkipDBRecord *SkipDBRecord_previousRecord(SkipDBRecord *self)
{
	if (NULL == self)
		return NULL;
	if (!self->previousRecord && self->previousPid && self->sdb)
	{
		self->previousRecord = SkipDB_recordAtPid_((SkipDB *)(self->sdb), self->previousPid);
	}

#ifdef SKIPDB_DEBUG
	textValid(self);
	textValid(self->previousRecord);
#endif

	return self->previousRecord;
}

