/*#io
UDBRecords ioDoc(
			  docCopyright("Steve Dekorte", 2004)
			  docLicense("BSD revised")
			  docObject("UDBRecords")    
			  docDescription("")
			  */

#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

UDBRecords *UDBRecords_new(void)
{
	UDBRecords *self = (UDBRecords *)calloc(1, sizeof(*self));
	if (NULL == self)
		return NULL;
	self->file = JFile_new();
	self->record = UDBRecord_new();
	self->firstEmptyRecord = UDBRecord_new();
	if (NULL == self->file || NULL == self->record ||
		NULL == self->firstEmptyRecord) {
		JFile_free(self->file);
		UDBRecord_free(self->record);
		UDBRecord_free(self->firstEmptyRecord);
		free(self);
		return NULL;
	}
	UDBRecords_setPath_(self, "default");
	if (NULL == self->file->path || NULL == self->file->logPath) {
		JFile_free(self->file);
		UDBRecord_free(self->record);
		UDBRecord_free(self->firstEmptyRecord);
		free(self);
		return NULL;
	}
	return self;
}

void UDBRecords_free(UDBRecords *self)
{
	if (NULL == self)
		return;
	UDBRecords_close(self);
	JFile_free(self->file);    
	UDBRecord_free(self->record);   
	UDBRecord_free(self->firstEmptyRecord);   
	free(self);
}

JFile *UDBRecords_jfile(UDBRecords *self)
{
	return self->file;
}

void UDBRecords_delete(UDBRecords *self)
{
	JFile_delete(self->file);
}

void UDBRecords_setPath_(UDBRecords *self, const char *path)
{
	JFile_setPath_withExtension_(self->file, path, "udbData");
}

void UDBRecords_setLogPath_(UDBRecords *self, const char *path)
{
	JFile_setLogPath_(self->file, path);
}

char *UDBRecords_path(UDBRecords *self) 
{ 
	return JFile_path(self->file); 
}

void UDBRecords_open(UDBRecords *self)
{
	if (NULL == self || NULL == self->file ||
		NULL == self->record || NULL == self->firstEmptyRecord)
		return;
	JFile_open(self->file);
	if (NULL == self->file->file || NULL == self->file->log)
		return;

	if (JFile_setPositionToEnd(self->file) == 0) {
		/* Position zero stores the first-empty-record offset. */
		JFile_begin(self->file);
		if (!UDBRecords_firstEmptyRecordPos_(self, 0)) {
			fclose(self->file->log);
			self->file->log = NULL;
			return;
		}
		JFile_commitToLog(self->file);
		if (NULL == self->file->log)
			return;
	}

	UDBRecord_setJFile_(self->record, self->file);
	UDBRecord_setJFile_(self->firstEmptyRecord, self->file);
	self->firstEmptyPos = UDBRecords_firstEmptyRecordPos(self);
}

void UDBRecords_close(UDBRecords *self)
{
	if (NULL != self)
		JFile_close(self->file);
}

void UDBRecords_begin(UDBRecords *self)
{
	JFile_begin(self->file);
}

void UDBRecords_preCommit(UDBRecords *self)
{
	JFile_commitToLog(self->file);
}

void UDBRecords_commit(UDBRecords *self)
{
	JFile_commitToFile(self->file);	
}

// ops -------------------------------------------------- 

UDBRecord *UDBRecords_firstRecord(UDBRecords *self)
{
	/* The data file begins with firstEmptyPos at offset zero. */
	return UDBRecords_recordAtPos_(self, sizeof(PID_TYPE));
}

UDBRecord *UDBRecords_newRecord(UDBRecords *self)
{
	UDBRecord_setPosToEnd(self->record);
	return self->record;
}

UDBRecord *UDBRecords_nextRecord(UDBRecords *self)
{
	PID_TYPE nextPos = self->record->pos + UDBRecord_totalSize(self->record);
	//printf("rec %i totalSize: %i\n", 
	//	self->record->header.pid, UDBRecord_totalSize(self->record));
	return UDBRecords_recordAtPos_(self, nextPos);
}

UDBRecord *UDBRecords_recordAfter_(UDBRecords *self, UDBRecord *record)
{
	PID_TYPE nextPos = record->pos + UDBRecord_totalSize(record);
	return UDBRecords_recordAtPos_(self, nextPos);
}


UDBRecord *UDBRecords_recordAtPos_(UDBRecords *self, PID_TYPE pos)
{
	UDBRecord_setPos_(self->record, pos);
	
	if (UDBRecord_readHeader(self->record))
	{
		return self->record;
	}
	
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

/*
 void UDBRecords_moveRecord_toPos_(UDBRecords *self, UDBRecord *record, PID_TYPE emptyPos)
 {
	 UDBRecord_moveToPos_(record, emptyPos);
 }
 */

void UDBRecords_show(UDBRecords *self)
{
	UDBRecord *record = UDBRecords_firstRecord(self);
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

// first empty record ------------------------------------ 

UDBRecord *UDBRecords_findFirstEmptyRecord(UDBRecords *self)
{
	UDBRecord *record = UDBRecords_firstRecord(self);
	
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

	if (NULL == self || NULL == self->file || self->file->maxPos < 0)
		return 0;
	file_size = (size_t)self->file->maxPos;
	if (file_size < sizeof(PID_TYPE)) {
		self->firstEmptyPos = 0;
		return 0;
	}

	JFile_setPosition_(self->file, 0);
	if (JFile_fread(self->file, (unsigned char *)&pos, sizeof(PID_TYPE), 1) != 1)
		pos = 0;
	if (pos != 0 && ((size_t)pos < sizeof(PID_TYPE) ||
		(size_t)pos > file_size ||
		file_size - (size_t)pos < sizeof(UDBRecordHeader)))
		pos = 0;

	self->firstEmptyPos = pos;
	return pos;
}

int UDBRecords_firstEmptyRecordPos_(UDBRecords *self, PID_TYPE pos)
{
	PID_TYPE old_empty;
	long old_file_pos, old_max_pos;

	if (NULL == self || NULL == self->file || self->file->file == NULL ||
		self->file->log == NULL || pos > (PID_TYPE)LONG_MAX)
		return 0;
	if (pos != 0 &&
		(pos < sizeof(PID_TYPE) || self->file->maxPos < 0 ||
		 pos > (PID_TYPE)self->file->maxPos ||
		 (PID_TYPE)self->file->maxPos - pos < sizeof(UDBRecordHeader)))
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

void UDBRecords_truncate_(UDBRecords *self, off_t size)
{
	if (self && self->file)
		JFile_truncate_(self->file, size);
}
