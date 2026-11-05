/* SkipDB prefix scans and prefix-bound cursor iteration. */
#include "SkipDBInternal.h"
#include <stdio.h>
#include <string.h>

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
