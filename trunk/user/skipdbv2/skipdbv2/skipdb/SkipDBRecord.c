/* SkipDB record identity, ownership, and scalar metadata. */
#include "SkipDB.h"
#include "SkipDBRecordInternal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef SKIPDB_DEBUG
static List *deallocedRecords = NULL;
#endif

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

/* Accept empty datums or non-empty byte spans with a valid pointer. */
static int SkipDBRecord_datumIsValid_(Datum value)
{
	return value.size == 0 || value.data != NULL;
}

void SkipDBRecord_keyDatum_(SkipDBRecord *self, Datum k)
{
	if (self && self->key && SkipDBRecord_datumIsValid_(k))
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

void SkipDBRecord_valueDatum_(SkipDBRecord *self, Datum v)
{
	if (self && self->value && SkipDBRecord_datumIsValid_(v))
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

void SkipDBRecord_object_(SkipDBRecord *self, void *object)
{
	if (self)
		self->object = object;
}

void *SkipDBRecord_object(SkipDBRecord *self)
{
	return self ? self->object : NULL;
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
#endif /* SKIPDB_DEBUG */
