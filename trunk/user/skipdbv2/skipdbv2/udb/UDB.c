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

UDB *UDB_new(void)
{
	UDB *self = (UDB *)calloc(1, sizeof(UDB));

	if (NULL == self)
		return NULL;

	self->index = UDBIndex_new();
	self->records = UDBRecords_new();
	if (NULL == self->index || NULL == self->records) {
		UDBIndex_free(self->index);
		UDBRecords_free(self->records);
		free(self);
		return NULL;
	}

	UDB_setPath_(self, "default");
	if (NULL == self->path || NULL == self->committedPath ||
		NULL == self->index->file->path || NULL == self->index->file->logPath ||
		NULL == self->records->file->path || NULL == self->records->file->logPath) {
		UDBIndex_free(self->index);
		UDBRecords_free(self->records);
		free(self->path);
		free(self->committedPath);
		free(self);
		return NULL;
	}
	return self;
}

void UDB_free(UDB *self)
{
	if (NULL == self)
		return;
	UDB_close(self);
	free(self->path);    
	free(self->committedPath);    
	
	UDBIndex_free(self->index);   
	UDBRecords_free(self->records);   
	free(self);
}

void UDB_delete(UDB *self)
{
	if (NULL == self)
		return;
	UDB_close(self);
	UDBIndex_delete(self->index);
	UDBRecords_delete(self->records);
	remove(self->committedPath);
}

void UDB_setPath_(UDB *self, const char *path)
{
	size_t path_len;
	char *new_path, *new_committed_path;
	char *index_path, *index_log_path, *records_path, *records_log_path;

	if (NULL == self || NULL == path || self->isOpen ||
		NULL == self->index || NULL == self->records ||
		NULL == self->index->file || NULL == self->records->file)
		return;

	path_len = strlen(path);
	if (path_len > (size_t)-1 - sizeof(".udbIndex.log") ||
		path_len > (size_t)-1 - sizeof(".udbData.log") ||
		path_len > (size_t)-1 - sizeof(".committed"))
		return;

	new_path = (char *)malloc(path_len + 1);
	new_committed_path = (char *)malloc(path_len + sizeof(".committed"));
	index_path = (char *)malloc(path_len + sizeof(".udbIndex"));
	index_log_path = (char *)malloc(path_len + sizeof(".udbIndex.log"));
	records_path = (char *)malloc(path_len + sizeof(".udbData"));
	records_log_path = (char *)malloc(path_len + sizeof(".udbData.log"));
	if (NULL == new_path || NULL == new_committed_path ||
		NULL == index_path || NULL == index_log_path ||
		NULL == records_path || NULL == records_log_path) {
		free(new_path);
		free(new_committed_path);
		free(index_path);
		free(index_log_path);
		free(records_path);
		free(records_log_path);
		return;
	}

	memcpy(new_path, path, path_len + 1);
	memcpy(new_committed_path, path, path_len);
	memcpy(new_committed_path + path_len, ".committed", sizeof(".committed"));
	memcpy(index_path, path, path_len);
	memcpy(index_path + path_len, ".udbIndex", sizeof(".udbIndex"));
	memcpy(index_log_path, path, path_len);
	memcpy(index_log_path + path_len, ".udbIndex.log", sizeof(".udbIndex.log"));
	memcpy(records_path, path, path_len);
	memcpy(records_path + path_len, ".udbData", sizeof(".udbData"));
	memcpy(records_log_path, path, path_len);
	memcpy(records_log_path + path_len, ".udbData.log", sizeof(".udbData.log"));

	free(self->path);
	free(self->committedPath);
	free(self->index->file->path);
	free(self->index->file->logPath);
	free(self->records->file->path);
	free(self->records->file->logPath);
	self->path = new_path;
	self->committedPath = new_committed_path;
	self->index->file->path = index_path;
	self->index->file->logPath = index_log_path;
	self->records->file->path = records_path;
	self->records->file->logPath = records_log_path;
}

/*
void UDB_setSecondaryPath_(UDB *self, const char *secondaryPath)
{
	UDBIndex_setPath_(self->index, self->path);
	UDBIndex_setLogPath_(self->index, secondaryPath);
	
	UDBRecords_setPath_(self->records, secondaryPath);
	UDBRecords_setLogPath_(self->records, self->path);
}
*/


void UDB_setLogPath_(UDB *self, const char *path)
{
	if (NULL == self || NULL == path || self->isOpen ||
		NULL == self->index || NULL == self->records)
		return;

	/* JFile derives each log filename from the database file basename. */
	UDBIndex_setLogPath_(self->index, path);
	UDBRecords_setLogPath_(self->records, path);
}

char *UDB_path(UDB *self)
{
	return self ? self->path : NULL;
}

int UDB_readCommitted(UDB *self);
int UDB_setCommittedFlag_(UDB *self, int flag);

static int UDB_streamsOpen(UDB *self)
{
	return self != NULL && self->index != NULL && self->records != NULL &&
		self->index->file != NULL && self->records->file != NULL &&
		self->index->file->file != NULL && self->index->file->log != NULL &&
		self->records->file->file != NULL && self->records->file->log != NULL;
}

void UDB_open(UDB *self)
{
	int marker, recovering;

	if (NULL == self || NULL == self->committedPath || self->isOpen ||
		NULL == self->index || NULL == self->records ||
		NULL == self->index->file || NULL == self->records->file)
		return;

	errno = 0;
	self->committedFile = fopen(self->committedPath, "r+");
	if (NULL == self->committedFile) {
		FILE *created;
		if (errno != ENOENT)
			return;
		created = fopen(self->committedPath, "w+");
		if (NULL == created)
			return;
		if (fclose(created) != 0)
			return;
		self->committedFile = fopen(self->committedPath, "r+");
		if (NULL == self->committedFile)
			return;
	}

	marker = UDB_readCommitted(self);
	if (marker == -2) {
		UDB_close(self);
		return;
	}
	recovering = (marker == 1);

	/* With no durable transaction marker, discard both journals before open. */
	if (!recovering) {
		if (JFile_clipLog(self->index->file) != 0 ||
			JFile_clipLog(self->records->file) != 0) {
			UDB_close(self);
			return;
		}
	}

	if (!UDBIndex_open(self->index)) {
		UDB_close(self);
		return;
	}
	UDBRecords_open(self->records);
	if (!UDB_streamsOpen(self)) {
		UDB_close(self);
		return;
	}

	/* A recovered transaction must not leave the global marker armed. */
	if (recovering && UDB_setCommittedFlag_(self, 0) != 0) {
		UDB_close(self);
		return;
	}
	self->isOpen = 1;
}

int UDB_isOpen(UDB *self)
{
	return self ? self->isOpen : 0;
}

void UDB_close(UDB *self)
{
	if (NULL == self)
		return;
	UDBIndex_close(self->index);
	UDBRecords_close(self->records);
	if (self->committedFile) {
		fclose(self->committedFile);
		self->committedFile = NULL;
	}
	self->isOpen = 0;
	self->withinTransaction = 0;
}

// transactions --------------------------------------------------- 

int UDB_isInTransaction(UDB *self)
{
	if (NULL == self || !self->isOpen || !self->withinTransaction)
		return 0;
	return 1;
}

void UDB_beginTransaction(UDB *self)
{
	if (NULL == self || !self->isOpen || self->withinTransaction)
		return;
	UDBIndex_begin(self->index);
	UDBRecords_begin(self->records);
	self->withinTransaction = 1;
}

int UDB_readCommitted(UDB *self)
{
	int marker;
	if (NULL == self || NULL == self->committedFile ||
		fseek(self->committedFile, 0, SEEK_SET) != 0)
		return -2;
	marker = fgetc(self->committedFile);
	if (marker == EOF && ferror(self->committedFile))
		return -2;
	return marker;
}

int UDB_setCommittedFlag_(UDB *self, int flag)
{
	int fd;
	if (NULL == self || NULL == self->committedFile ||
		(flag != 0 && flag != 1) ||
		fseek(self->committedFile, 0, SEEK_SET) != 0 ||
		fputc(flag, self->committedFile) == EOF ||
		fflush(self->committedFile) != 0)
		return -1;

	fd = fileno(self->committedFile);
	if (fd < 0)
		return -1;
#ifdef F_FULLFSYNC
	if (fcntl(fd, F_FULLFSYNC, 0) != 0)
		return -1;
#else
	if (fsync(fd) != 0)
		return -1;
#endif
	return 0;
}

void UDB_commitTransaction(UDB *self)
{
	if (NULL == self || !self->isOpen || !self->withinTransaction)
		return;

	UDBIndex_preCommit(self->index);
	UDBRecords_preCommit(self->records);
	if (!UDB_streamsOpen(self) || UDB_setCommittedFlag_(self, 1) != 0) {
		/* Leave journals untouched; the old marker decides recovery on reopen. */
		UDB_close(self);
		return;
	}

	UDBIndex_commit(self->index);
	UDBRecords_commit(self->records);
	if (!UDB_streamsOpen(self)) {
		/* Marker 1 lets a later open replay the durable transaction again. */
		UDB_close(self);
		return;
	}

	if (UDB_setCommittedFlag_(self, 0) != 0) {
		/* Do not permit another transaction while the marker may still be 1. */
		UDB_close(self);
		return;
	}

	self->withinTransaction = 0;
}

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
		if (self->index->holes) {
			while (List_removeFirst_(self->index->holes, (void *)pid)) {
				/* Collapse duplicate free-slot entries before restoring this PID. */
			}
			List_push_(self->index->holes, (void *)pid);
		}
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
		/* Updating an unmapped PID would create an orphaned duplicate row. */
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
	if (!pos)
		return NULL;
	record = UDBRecords_recordAtPos_(self->records, pos);
	if (NULL == record || UDBRecord_isEmpty(record))
		return NULL;
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
	
	return UDBRecord_readDatum(record);
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

// compact --------------------------------------------------- 

UDBRecord *UDB_firstEmptyRecord(UDB *self)
{
	UDBRecord *record = UDBRecords_firstRecord(self->records);
	
	while (record)
	{	
		if (UDBRecord_isEmpty(record)) return record;
		record = UDBRecords_nextRecord(self->records);
	}
	return NULL;
}

int UDB_compact(UDB *self)
{
	int count = 0;
	int compactions;
	
	do 
	{
		compactions = UDB_compactStepFor_(self, 0.1);
		count += compactions;
	} while (compactions);
	
	return count;
}

//#define DEBUG 1

int UDB_compactStep(UDB *self)
{
	return UDB_compactStepFor_(self, 0.0);
}

int UDB_compactStepFor_(UDB *self, double maxSeconds)
{
	UDBRecord *firstEmptyRecord, *record;
	PID_TYPE oldEmptyPos, newEmptyPos, newSize;

	(void)maxSeconds;
	if (NULL == self || !self->isOpen || self->withinTransaction ||
		NULL == self->records || NULL == self->index)
		return 0;

	firstEmptyRecord = UDBRecords_firstEmptyRecord(self->records);
	if (NULL == firstEmptyRecord || !UDBRecord_isEmpty(firstEmptyRecord))
		return 0;

	record = UDBRecords_recordAfter_(self->records, firstEmptyRecord);
	if (NULL == record) {
		/* Remove a trailing free block; no live records follow it. */
		if (!UDBRecords_truncate_(self->records,
				(off_t)UDBRecord_pos(firstEmptyRecord)) ||
			self->records->file->file == NULL ||
			self->records->file->log == NULL) {
			UDB_close(self);
			return 0;
		}
		return 0;
	}

	UDB_beginTransaction(self);
	if (!self->withinTransaction ||
		!UDBRecords_firstEmptyRecordPos_(self->records,
			UDBRecord_pos(firstEmptyRecord))) {
		UDB_close(self);
		return 0;
	}

	if (UDBRecord_isEmpty(record)) {
		/* Merge adjacent empty extents. */
		newSize = UDBRecord_totalSize(firstEmptyRecord) -
			sizeof(UDBRecordHeader) + UDBRecord_totalSize(record);
		UDBRecord_size_(firstEmptyRecord, newSize);
		if (!UDBRecord_saveHeader(firstEmptyRecord) ||
			!UDBIndex_setPos_forPid_(self->index, 0,
				UDBRecord_pid(record))) {
			UDB_close(self);
			return 0;
		}
	} else {
		oldEmptyPos = UDBRecord_pos(firstEmptyRecord);
		if (UDBRecord_totalSize(record) > (PID_TYPE)-1 - oldEmptyPos) {
			UDB_close(self);
			return 0;
		}
		newEmptyPos = oldEmptyPos + UDBRecord_totalSize(record);
		if (!UDBRecords_firstEmptyRecordPos_(self->records, newEmptyPos) ||
			!UDBRecord_moveToPos_(record, oldEmptyPos)) {
			UDB_close(self);
			return 0;
		}
		UDBRecord_setPos_(firstEmptyRecord, newEmptyPos);
		if (!UDBRecord_saveHeader(firstEmptyRecord) ||
			!UDBIndex_setPos_forPid_(self->index, oldEmptyPos,
				UDBRecord_pid(record))) {
			UDB_close(self);
			return 0;
		}
		/* The empty extent's former PID stays free; never map it to a row. */
	}

	UDB_commitTransaction(self);
	return self->isOpen ? 1 : 0;
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

