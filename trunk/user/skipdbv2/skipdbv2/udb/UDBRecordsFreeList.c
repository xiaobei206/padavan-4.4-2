/* Persisted first-empty offset and recoverable free-list metadata. */
#include "UDB.h"
#include <stdio.h>
#include <limits.h>

// first empty record ------------------------------------ 

/* Validate nonzero free-list pointers as complete physical record headers. */
static int UDBRecords_isValidEmptyPosition_(PID_TYPE pos, size_t file_size)
{
	return pos >= sizeof(PID_TYPE) && (size_t)pos <= file_size &&
		file_size - (size_t)pos >= sizeof(UDBRecordHeader);
}

UDBRecord *UDBRecords_findFirstEmptyRecord(UDBRecords *self)
{
	UDBRecord *record;
	if (NULL == self)
		return NULL;
	record = UDBRecords_firstRecord(self);
	
	while (record)
	{	
		if (UDBRecord_isEmpty(record)) return record;
		record = UDBRecords_nextRecord(self);
	}
	
	return NULL;
}

/*
 UDBRecord *UDBRecords_nextEmptyRecordAfter_(UDBRecords *self, UDBRecord *record)
 {
	 UDBRecord *record;
	 
	 while (record = UDBRecords_nextRecord(self))
	 {	
		 if (UDBRecord_isEmpty(record)) return record;
	 }
	 
	 return NULL;
 }
 */

PID_TYPE UDBRecords_firstEmptyRecordPos(UDBRecords *self)
{
	PID_TYPE pos = 0;
	size_t file_size;
	long old_file_pos, old_max_pos;

	if (NULL == self || NULL == self->file || NULL == self->file->file || self->file->maxPos < 0)
		return 0;
	file_size = (size_t)self->file->maxPos;
	if (file_size < sizeof(PID_TYPE)) {
		self->firstEmptyPos = 0;
		return 0;
	}

	old_file_pos = self->file->pos;
	old_max_pos = self->file->maxPos;
	JFile_setPosition_(self->file, 0);
	if (JFile_fread(self->file, (unsigned char *)&pos, sizeof(PID_TYPE), 1) != 1) {
		/* The metadata block must exist once finishOpening accepted the file.
		 * Never downgrade a failed read to the valid 'no free record' value. */
		fprintf(stderr, "UDBRecords: failed reading first-empty metadata\n");
		JFile_close(self->file);
		self->firstEmptyPos = 0;
		return 0;
	}
	if (pos != 0 && !UDBRecords_isValidEmptyPosition_(pos, file_size))
		pos = 0;

	self->firstEmptyPos = pos;
	/* A metadata query must not disturb a caller's physical record cursor. */
	self->file->pos = old_file_pos;
	self->file->maxPos = old_max_pos;
	return pos;
}

int UDBRecords_firstEmptyRecordPos_(UDBRecords *self, PID_TYPE pos)
{
	PID_TYPE old_empty;
	long old_file_pos, old_max_pos;

	if (NULL == self || NULL == self->file || self->file->file == NULL ||
		self->file->log == NULL || self->file->maxPos < 0 ||
		(self->file->maxPos > 0 &&
		 (size_t)self->file->maxPos < sizeof(PID_TYPE)) ||
		pos > (PID_TYPE)LONG_MAX)
		return 0;
	if (pos != 0 &&
		!UDBRecords_isValidEmptyPosition_(pos, (size_t)self->file->maxPos))
		return 0;

	old_empty = self->firstEmptyPos;
	old_file_pos = self->file->pos;
	old_max_pos = self->file->maxPos;
	self->firstEmptyPos = pos;
	JFile_setPosition_(self->file, 0);
	if (JFile_fwrite(self->file, (unsigned char *)&self->firstEmptyPos,
			 sizeof(PID_TYPE), 1) != 1) {
		self->firstEmptyPos = old_empty;
		self->file->pos = old_file_pos;
		self->file->maxPos = old_max_pos;
		return 0;
	}
	/* Writing offset-zero metadata must preserve the active record cursor. */
	self->file->pos = old_file_pos;
	self->file->maxPos = old_max_pos;
	return 1;
}

UDBRecord *UDBRecords_firstEmptyRecord(UDBRecords *self)
{
	UDBRecord *found;
	if (NULL == self || NULL == self->file ||
		NULL == self->record || NULL == self->firstEmptyRecord)
		return NULL;

	if (self->firstEmptyPos) {
		UDBRecord_setPos_(self->firstEmptyRecord, self->firstEmptyPos);
		if (UDBRecord_readHeader(self->firstEmptyRecord) &&
			UDBRecord_isEmpty(self->firstEmptyRecord))
			return self->firstEmptyRecord;
	}

	/* Recover a stale/corrupt first-empty pointer by scanning validated records. */
	found = UDBRecords_findFirstEmptyRecord(self);
	if (found) {
		UDBRecord_setPos_(self->firstEmptyRecord, UDBRecord_pos(found));
		self->firstEmptyRecord->header = found->header;
		self->firstEmptyPos = UDBRecord_pos(found);
		return self->firstEmptyRecord;
	}
	self->firstEmptyPos = 0;
	return NULL;
}
