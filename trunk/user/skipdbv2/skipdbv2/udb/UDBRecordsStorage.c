/* UDBRecords lifecycle, startup validation, and journal delegation. */
#include "UDB.h"
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>

/* Shared readiness check for the records data stream and its journal. */
static int UDBRecords_hasReadyStreams_(UDBRecords *self)
{
	return self != NULL && self->file != NULL &&
		self->file->file != NULL && self->file->log != NULL;
}

void UDBRecords_open(UDBRecords *self)
{
	if (NULL == self || NULL == self->file ||
		NULL == self->record || NULL == self->firstEmptyRecord)
		return;
	JFile_open(self->file);
	if (!UDBRecords_hasReadyStreams_(self) || self->file->deferLogReplay)
		return;
	(void)UDBRecords_finishOpening(self);
}

int UDBRecords_finishOpening(UDBRecords *self)
{
	long data_size;
	if (NULL == self || !UDBRecords_hasReadyStreams_(self) ||
		NULL == self->record || NULL == self->firstEmptyRecord)
		return 0;

	data_size = JFile_setPositionToEnd(self->file);
	if (data_size < 0 ||
		(data_size > 0 && (size_t)data_size < sizeof(PID_TYPE))) {
		fprintf(stderr, "UDBRecords: truncated first-empty metadata header\n");
		JFile_close(self->file);
		return 0;
	}

	if (data_size == 0) {
		/* Position zero stores the first-empty-record offset. */
		JFile_begin(self->file);
		if (!UDBRecords_firstEmptyRecordPos_(self, 0)) {
			/* JFile_fwrite may already have closed the journal on rollback failure. */
			JFile_close(self->file);
			return 0;
		}
		/* Materialize the initial metadata header before it is read below;
		 * the normal journal replay only runs during the next open. */
		JFile_commitToFile(self->file);
		if (!UDBRecords_hasReadyStreams_(self))
			return 0;
	}

	UDBRecord_setJFile_(self->record, self->file);
	UDBRecord_setJFile_(self->firstEmptyRecord, self->file);

	/* Treat the persisted offset as a hint, never as proof of a record
	 * boundary. A corrupt offset can land inside payload bytes that happen
	 * to look like an empty header and cause compaction to overwrite data.
	 * Walk the validated physical chain to recover the first real hole. */
	(void)UDBRecords_firstEmptyRecordPos(self);
	{
		UDBRecord *first_empty = UDBRecords_findFirstEmptyRecord(self);
		if (!UDBRecords_hasReadyStreams_(self))
			return 0;
		if (first_empty) {
			self->firstEmptyPos = UDBRecord_pos(first_empty);
			self->firstEmptyRecord->header = first_empty->header;
			UDBRecord_setPos_(self->firstEmptyRecord, self->firstEmptyPos);
		} else {
			self->firstEmptyPos = 0;
		}
	}
	return UDBRecords_hasReadyStreams_(self);
}

int UDBRecords_isUncommitted(UDBRecords *self)
{
	if (NULL == self || NULL == self->file)
		return 0;
	return JFile_needsSync(self->file);
}

void UDBRecords_close(UDBRecords *self)
{
	if (NULL != self)
		JFile_close(self->file);
}

void UDBRecords_begin(UDBRecords *self)
{
	if (UDBRecords_hasReadyStreams_(self))
		JFile_begin(self->file);
}

void UDBRecords_preCommit(UDBRecords *self)
{
	if (self && self->file)
		JFile_commitToLog(self->file);
}

void UDBRecords_commit(UDBRecords *self)
{
	if (self && self->file)
		(void)JFile_applyLogToFile(self->file);
}

int UDBRecords_truncate_(UDBRecords *self, off_t size)
{
	if (NULL == self || NULL == self->file || size < 0 ||
		size > (off_t)LONG_MAX)
		return 0;
	JFile_truncate_(self->file, size);
	return UDBRecords_hasReadyStreams_(self) &&
		self->file->maxPos == (long)size;
}
