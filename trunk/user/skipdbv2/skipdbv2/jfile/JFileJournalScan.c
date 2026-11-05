/* Bounded parser for the native JFile transaction journal format. */
#include "JFileInternal.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

int JFileJournal_scan_(JFile *self, JFileJournalScan *scan)
{
	long end, cursor = 0;
	const size_t header_size = sizeof(long) + sizeof(size_t);

	if (NULL == self || NULL == self->log || NULL == scan)
		return -1;

	memset(scan, 0, sizeof(*scan));
	if (fseek(self->log, 0, SEEK_END) != 0 ||
		(end = ftell(self->log)) < 0)
		return -1;
	scan->logEnd = end;

	while (cursor < end) {
		long remaining = end - cursor;
		long pos, data_pos, payload_end;
		size_t total;
		int terminator;

		/* A complete record needs both header fields and one terminator. */
		if (remaining < (long)(header_size + 1))
			break;
		if (fseek(self->log, cursor, SEEK_SET) != 0 ||
			fread(&pos, sizeof(pos), 1, self->log) != 1 ||
			fread(&total, sizeof(total), 1, self->log) != 1)
			return -1;

		data_pos = cursor + (long)header_size;
		/* JFile_fwrite never emits an empty payload record. */
		if (pos < 0 || total == 0 || total > (size_t)LONG_MAX ||
			total > (size_t)(end - data_pos - 1) ||
			pos > LONG_MAX - (long)total)
			break;

		payload_end = data_pos + (long)total;
		if (fseek(self->log, payload_end, SEEK_SET) != 0)
			return -1;
		terminator = fgetc(self->log);
		if (terminator == EOF && ferror(self->log))
			return -1;
		if (terminator != JFILE_END_TRANSACTION_ITEM &&
			terminator != JFILE_END_TRANSACTION)
			break;

		cursor = payload_end + 1;
		++scan->recordCount;
		if (terminator == JFILE_END_TRANSACTION) {
			scan->committedEnd = cursor;
			scan->committedRecordCount = scan->recordCount;
		}
	}

	scan->validEnd = cursor;
	/* A valid ITEM-only suffix is incomplete until a transaction terminator. */
	if (fseek(self->log, 0, SEEK_END) != 0 || ftell(self->log) != end)
		return -1;
	return 0;
}
