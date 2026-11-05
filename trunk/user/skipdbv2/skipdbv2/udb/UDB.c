/*#io
UDB ioDoc(
		docCopyright("Steve Dekorte", 2004)
		docLicense("BSD revised")
		docObject("UDB")    
		docDescription("")
		*/

#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

// ops --------------------------------------------------- 

PID_TYPE UDB_nextPid(UDB *self)
{
	return self && self->isOpen && self->index
		? UDBIndex_nextPid(self->index) : 0;
}

PID_TYPE UDB_allocPid(UDB *self)
{
	return self && self->isOpen && self->withinTransaction && self->index
		? UDBIndex_allocPid(self->index) : 0;
}

static int UDB_rollbackJournalAppend(JFile *file, long log_start,
                                      long old_pos, long old_max_pos)
{
	if (NULL == file)
		return 0;
	if (file->journalingOn) {
		if (file->log && log_start >= 0 &&
			fflush(file->log) == 0 &&
			ftruncate(fileno(file->log), (off_t)log_start) == 0 &&
			fseek(file->log, log_start, SEEK_SET) == 0) {
			clearerr(file->log);
			file->pos = old_pos;
			file->maxPos = old_max_pos;
			return 1;
		}
		if (file->log) {
			fclose(file->log);
			file->log = NULL;
		}
		return 0;
	}
	if (file->file && old_pos == old_max_pos &&
		ftruncate(fileno(file->file), (off_t)old_max_pos) == 0 &&
		fseek(file->file, old_pos, SEEK_SET) == 0) {
		clearerr(file->file);
		file->pos = old_pos;
		file->maxPos = old_max_pos;
		return 1;
	}
	if (file->file) {
		fclose(file->file);
		file->file = NULL;
	}
	return 0;
}

static int UDB_append_withPid_(UDB *self, Datum d, PID_TYPE pid)
{
	UDBRecord *record;
	JFile *record_file;
	long old_max_pos, log_start = -1;

	if (NULL == self || !self->isOpen || !self->withinTransaction ||
		NULL == self->records || NULL == self->index || pid == 0 ||
		(d.size > 0 && NULL == d.data))
		return 0;
	record_file = self->records->file;
	if (NULL == record_file || NULL == record_file->file ||
		NULL == record_file->log || record_file->maxPos < 0)
		return 0;
	old_max_pos = record_file->maxPos;
	if (record_file->journalingOn) {
		if (fseek(record_file->log, 0, SEEK_END) != 0 ||
			(log_start = ftell(record_file->log)) < 0)
			return 0;
	}
	record = UDBRecords_newRecord(self->records);
	if (NULL == record)
		return 0;
	UDBRecord_pid_(record, pid);
	if (!UDBRecord_saveWithDatum_(record, d))
		return 0;
	if (!UDBIndex_setPos_forPid_(self->index, UDBRecord_pos(record), pid)) {
		UDB_rollbackJournalAppend(record_file, log_start, old_max_pos,
			old_max_pos);
		return 0;
	}
	return 1;
}

PID_TYPE UDB_put_(UDB *self, Datum d) // returns pid
{
	PID_TYPE pid;
	if (NULL == self || (d.size > 0 && NULL == d.data) ||
		!UDB_isInTransaction(self))
		return 0;

	pid = UDBIndex_allocPid(self->index);
	if (pid == 0)
		return 0;
	if (!UDB_append_withPid_(self, d, pid)) {
		/* The PID remains transaction-reserved; abort so it cannot be reused
		 * while a failed append may have left partial journal state behind. */
		UDB_close(self);
		return 0;
	}
	return pid;
}

void UDB_at_put_(UDB *self, PID_TYPE pid, Datum d)
{
	PID_TYPE old_pos;
	UDBRecord *old_record;

	if (!UDB_isInTransaction(self))
		return;
	if (pid == 0 || (d.size > 0 && NULL == d.data)) {
		UDB_close(self);
		return;
	}

	/* Retain the old location before UDBRecords_newRecord reuses its cursor. */
	old_pos = UDBIndex_posForPid_(self->index, pid);
	if (old_pos == 0) {
		/* Newly allocated PIDs have no index mapping until their first write.
		 * Only accept IDs actually reserved by the allocator, never arbitrary gaps. */
		if (!UDBIndex_isReservedPid_(self->index, pid)) {
			UDB_close(self);
			return;
		}
		if (!UDB_append_withPid_(self, d, pid))
			UDB_close(self);
		return;
	}
	old_record = UDBRecords_recordAtPos_(self->records, old_pos);
	if (NULL == old_record || UDBRecord_isEmpty(old_record) ||
		UDBRecord_pid(old_record) != pid) {
		UDB_close(self);
		return;
	}

	if (!UDB_append_withPid_(self, d, pid)) {
		/* This API has no status return; abort rather than silently partial-commit. */
		UDB_close(self);
		return;
	}

	/* The index now points at the new row; retire the superseded row as a hole. */
	old_record = UDBRecords_recordAtPos_(self->records, old_pos);
	if (NULL == old_record || UDBRecord_isEmpty(old_record) ||
		UDBRecord_pid(old_record) != pid ||
		!UDBRecords_removeRecord_(self->records, old_record)) {
		UDB_close(self);
	}
}

UDBRecord *UDB_recordAtPid_(UDB *self, PID_TYPE pid)
{
	PID_TYPE pos;
	if (NULL == self || !self->isOpen || NULL == self->index ||
		NULL == self->records)
		return NULL;
	pos = UDBIndex_posForPid_(self->index, pid);
	UDBRecord *record;
	if (!pos) {
		if (self->index->file && self->index->file->file &&
			ferror(self->index->file->file)) {
			fprintf(stderr, "UDB: PID index read error; closing database\n");
			UDB_close(self);
		}
		return NULL;
	}
	record = UDBRecords_recordAtPos_(self->records, pos);
	if (NULL == record || UDBRecord_isEmpty(record) ||
		UDBRecord_pid(record) != pid) {
		fprintf(stderr, "UDB: PID %lu maps to an invalid record; closing database\n",
			(unsigned long)pid);
		UDB_close(self);
		return NULL;
	}
	return record;
}

Datum UDB_at_(UDB *self, PID_TYPE pid)
{
	Datum d;
	UDBRecord *record = UDB_recordAtPid_(self, pid);
	
	if (!record)
	{
		d.size = 0;
		d.data = NULL;
		//printf("missing record with pid %i\n", pid);
		return d;
	}
	
	{
		JFile *file = self->records->file;
		d = UDBRecord_readDatum(record);
		/* A failed payload read must not masquerade as a missing/empty value. */
		if ((file && file->file && ferror(file->file)) ||
			(record->header.size > 0 && NULL == d.data)) {
			fprintf(stderr, "UDB: failed reading payload for PID %lu; closing database\n",
				(unsigned long)pid);
			UDB_close(self);
			d.size = 0;
			d.data = NULL;
		}
		return d;
	}
}

void UDB_removeAt_(UDB *self, PID_TYPE pid)
{
	UDBRecord *record;
	if (!UDB_isInTransaction(self))
		return;
	record = UDB_recordAtPid_(self, pid);
	if (NULL == record)
		return;
	if (!UDBRecords_removeRecord_(self->records, record) ||
		!UDBIndex_setPos_forPid_(self->index, 0, pid)) {
		/* Leave the global marker uncommitted; reopening will discard this tx. */
		UDB_close(self);
	}
}

void UDB_show(UDB *self)
{
	UDBRecord *record = UDBRecords_firstRecord(self->records);
	printf("UDB Records:\n");
	
	while (record)
	{	
		UDBRecord_show(record);
		record = UDBRecords_nextRecord(self->records);
	}
}

void UDB_showIndex(UDB *self)
{
	UDBIndex_show(self->index);
}

