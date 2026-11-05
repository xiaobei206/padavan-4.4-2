/* Validated physical record cursor and mutation operations. */
#include "UDB.h"
#include <stdio.h>
#include <limits.h>

// ops -------------------------------------------------- 

UDBRecord *UDBRecords_firstRecord(UDBRecords *self)
{
	if (NULL == self || NULL == self->file || NULL == self->file->file ||
		NULL == self->record || self->file->maxPos < (long)sizeof(PID_TYPE))
		return NULL;
	/* The data file begins with firstEmptyPos at offset zero. */
	return UDBRecords_recordAtPos_(self, sizeof(PID_TYPE));
}

UDBRecord *UDBRecords_newRecord(UDBRecords *self)
{
	if (NULL == self || NULL == self->record || NULL == self->file ||
		NULL == self->file->file)
		return NULL;
	UDBRecord_setPosToEnd(self->record);
	return self->record;
}

UDBRecord *UDBRecords_nextRecord(UDBRecords *self)
{
	PID_TYPE size, nextPos;
	if (NULL == self || NULL == self->record)
		return NULL;
	size = UDBRecord_totalSize(self->record);
	if (size == 0 || size > (PID_TYPE)LONG_MAX ||
		self->record->pos > (PID_TYPE)LONG_MAX - size)
		return NULL;
	nextPos = self->record->pos + size;
	return UDBRecords_recordAtPos_(self, nextPos);
}

UDBRecord *UDBRecords_recordAfter_(UDBRecords *self, UDBRecord *record)
{
	PID_TYPE size, nextPos;
	if (NULL == self || NULL == record || NULL == self->record)
		return NULL;
	size = UDBRecord_totalSize(record);
	if (size == 0 || size > (PID_TYPE)LONG_MAX ||
		record->pos > (PID_TYPE)LONG_MAX - size)
		return NULL;
	nextPos = record->pos + size;
	return UDBRecords_recordAtPos_(self, nextPos);
}


UDBRecord *UDBRecords_recordAtPos_(UDBRecords *self, PID_TYPE pos)
{
	long file_size;

	if (NULL == self || NULL == self->record || NULL == self->file ||
		NULL == self->file->file || pos > (PID_TYPE)LONG_MAX)
		return NULL;
	file_size = self->file->maxPos;
	if (file_size < 0 || pos > (PID_TYPE)file_size) {
		fprintf(stderr, "UDBRecords: record position is beyond end of file\n");
		JFile_close(self->file);
		return NULL;
	}
	/* Exactly EOF is a normal end-of-records sentinel. Any other failed
	 * header read means the record chain is truncated or corrupt. */
	if (pos == (PID_TYPE)file_size)
		return NULL;
	if ((PID_TYPE)file_size - pos < sizeof(UDBRecordHeader)) {
		fprintf(stderr, "UDBRecords: truncated record header\n");
		JFile_close(self->file);
		return NULL;
	}

	UDBRecord_setJFile_(self->record, self->file);
	UDBRecord_setPos_(self->record, pos);

	if (UDBRecord_readHeader(self->record))
		return self->record;

	fprintf(stderr, "UDBRecords: invalid record header at position %lu\n",
		(unsigned long)pos);
	JFile_close(self->file);
	return NULL;
}

int UDBRecords_removeRecord_(UDBRecords *self, UDBRecord *record)
{
	PID_TYPE pos;
	if (NULL == self || NULL == record || NULL == self->file)
		return 0;
	pos = UDBRecord_pos(record);
	if (!UDBRecord_remove(record))
		return 0;

	if ((!self->firstEmptyPos) ||
		(pos && pos < self->firstEmptyPos)) {
		if (!UDBRecords_firstEmptyRecordPos_(self, pos))
			return 0;
	}
	return 1;
}

void UDBRecords_moveRecord_toPos_(UDBRecords *self, UDBRecord *record, PID_TYPE pos)
{
	if (NULL == self || NULL == self->file || NULL == record ||
		record->file != self->file)
		return;
	(void)UDBRecord_moveToPos_(record, pos);
}

void UDBRecords_show(UDBRecords *self)
{
	UDBRecord *record;
	if (NULL == self)
		return;
	record = UDBRecords_firstRecord(self);
	printf("UDB Records:\n");
	if (record)
	{	
		for (;;)
		{
			UDBRecord_show(record);
			if (!UDBRecords_nextRecord(self)) break;
		}
	}
}
