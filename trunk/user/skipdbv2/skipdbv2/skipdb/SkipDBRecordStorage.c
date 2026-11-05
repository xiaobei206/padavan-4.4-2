/* Stable tagged serialization and UDB persistence for SkipDB records. */
#include "SkipDB.h"
#include "SkipDBRecordInternal.h"

/* Shared readiness contract for record serialization and deserialization. */
static int SkipDBRecord_streamIsReady_(SkipDBRecord *self, BStream *s)
{
	return self != NULL && s != NULL && self->key != NULL &&
		self->value != NULL && s->ba != NULL && s->errorBa != NULL;
}

/* Both directions must honor the same supported skip-list level range. */
static int SkipDBRecord_levelIsSupported_(int level)
{
	return level >= 0 && level <= SKIPDB_MAX_LEVEL;
}

int SkipDBRecord_toStream_(SkipDBRecord *self, BStream *s)
{
	int level, max;
	if (!SkipDBRecord_streamIsReady_(self, s))
		return 0;
	max = self->level;
	if (!SkipDBRecord_levelIsSupported_(max) ||
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

	if (!SkipDBRecord_streamIsReady_(self, s))
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
	if (UArray_size(s->errorBa) != 0 ||
		!SkipDBRecord_levelIsSupported_(max))
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
	if (NULL == stream || NULL == SkipDB_udb(sdb) ||
		!UDB_isOpen(SkipDB_udb(sdb)))
		return;
	BStream_empty(stream);
	if (!SkipDBRecord_toStream_(self, stream))
		return;
	data = Datum_FromUArray_(BStream_byteArray(stream));
	pid = SkipDBRecord_pid(self);
	if (pid) {
		UDB_at_put_(SkipDB_udb(sdb), pid, data);
		if (!UDB_isOpen(SkipDB_udb(sdb)))
			return;
	} else {
		pid = UDB_put_(SkipDB_udb(sdb), data);
		if (pid == 0)
			return;
		SkipDBRecord_pid_(self, pid);
	}
	SkipDBRecord_markAsClean(self);
}
