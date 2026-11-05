/* SkipDB skip-list traversal, search path, and read operations. */
#include "SkipDBInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

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
		if (!SkipDBRecord_fromStream_(record, self->stream) ||
			(record->level == 0 && pid != self->headerPid)) {
			fprintf(stderr, "SkipDB: invalid serialized record for PID %" PID_FORMAT "\n", pid);
			SkipDBRecord_dealloc(record);
			/* A mapped PID whose payload cannot be decoded, or a non-root
			 * row without a level-0 link slot, is corruption rather than EOF. */
			UDB_close(self->udb);
			return NULL;
		}
		SkipDBRecord_pid_(record, pid);
		return record;
	}

	if (pid != 1)
		fprintf(stderr, "SkipDB: missing record for PID %" PID_FORMAT "\n", pid);
	return NULL;
}

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

SkipDBRecord *SkipDB_recordAt_(SkipDB *self, Datum k)
{
	if (NULL == self || NULL == self->header || !SkipDB_isOpen(self) ||
		(k.size > 0 && NULL == k.data))
		return NULL;
	SkipDB_clearUpdate(self);
	/* Use the full walk so every level's predecessor is populated. */
	return SkipDBRecord_find_quick_(self->header, k, 0);
}

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
