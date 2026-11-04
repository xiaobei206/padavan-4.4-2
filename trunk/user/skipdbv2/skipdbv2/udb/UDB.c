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

	UDBIndex_open(self->index);
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
	{
		printf("UDB error - mutation operations must be within a transaction\n");
		exit(1);
		return 0;
	}
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

void UDB_append_withPid_(UDB *self, Datum d, PID_TYPE pid)
{
	UDBRecord *record;
	if (NULL == self || !self->isOpen || !self->withinTransaction ||
		NULL == self->records || (d.size > 0 && NULL == d.data))
		return;
	record = UDBRecords_newRecord(self->records);
	UDBRecord_pid_(record, pid);
	UDBRecord_saveWithDatum_(record, d);
	UDBIndex_setPos_forPid_(self->index, UDBRecord_pos(record), pid);
}

PID_TYPE UDB_put_(UDB *self, Datum d) // returns pid 
{
	if (UDB_isInTransaction(self))
	{
		PID_TYPE pid = UDBIndex_allocPid(self->index);
		UDB_append_withPid_(self, d, pid);
		return pid;
	}
	
	return 0;
}

void UDB_at_put_(UDB *self, PID_TYPE pid, Datum d)
{
	if (UDB_isInTransaction(self))
	{
		UDB_append_withPid_(self, d, pid);
	}
}

UDBRecord *UDB_recordAtPid_(UDB *self, PID_TYPE pid)
{
	PID_TYPE pos;
	if (NULL == self || !self->isOpen || NULL == self->index ||
		NULL == self->records)
		return NULL;
	pos = UDBIndex_posForPid_(self->index, pid);
	if (!pos) return NULL;
	return UDBRecords_recordAtPos_(self->records, pos);
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
	if (UDB_isInTransaction(self))
	{
		UDBRecord *record = UDB_recordAtPid_(self, pid);
		
		if (!record)
		{
                        //TODO jannson
			//printf("UDB error: missing record with pid %" PID_FORMAT " for remove\n", pid);
			record = UDB_recordAtPid_(self, pid);
			return;
		}
		
		UDBRecords_removeRecord_(self->records, record);
		UDBIndex_setPos_forPid_(self->index, 0, pid);
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
	UDBRecord *firstEmptyRecord = UDBRecords_firstEmptyRecord(self->records);
	
	if (!firstEmptyRecord)
	{
		return 0;
	}
	
	if (!UDBRecord_isEmpty(firstEmptyRecord))
	{
		printf("firstEmptyRecord not empty!\n");
		firstEmptyRecord = UDBRecords_firstEmptyRecord(self->records);
		UDBRecord_isEmpty(firstEmptyRecord);
		exit(1);
	}
	
#ifdef DEBUG
	printf("empty record pid: %i pos: %i size: %i isEmpty: %i\n", 
		  UDBRecord_pid(firstEmptyRecord), 
		  UDBRecord_pos(firstEmptyRecord), 
		  UDBRecord_totalSize(firstEmptyRecord), 
		  UDBRecord_isEmpty(firstEmptyRecord));
#endif
	    
	if (firstEmptyRecord)
	{
		UDBRecord *record = UDBRecords_recordAfter_(self->records, firstEmptyRecord);
		
		if (!record)
		{
			// reached end of file 
			PID_TYPE size = UDBRecord_pos(firstEmptyRecord);
			UDBRecords_truncate_(self->records, size);
			return 0;
		}
		
#ifdef DEBUG
		printf("next  record pid: %i pos: %i size: %i isEmpty: %i\n\n", 
			  UDBRecord_pid(record), 
			  UDBRecord_pos(record), 
			  UDBRecord_totalSize(record), 
			  UDBRecord_isEmpty(record));
#endif
		
		UDB_beginTransaction(self);
		
		if (UDBRecord_isEmpty(record))
		{
			// coalese this empty record into the first empty record 
			PID_TYPE newSize = UDBRecord_totalSize(firstEmptyRecord) 
			- sizeof(UDBRecordHeader) + UDBRecord_totalSize(record);
			UDBRecord_size_(firstEmptyRecord, newSize);
			UDBRecord_saveHeader(firstEmptyRecord);
#ifdef DEBUG
			printf("merge\n");
#endif
			UDBIndex_setPos_forPid_(self->index, 0, UDBRecord_pid(record));
		}
		else
		{
			// swap places with the first empty record 
			PID_TYPE oldEmptyPos = UDBRecord_pos(firstEmptyRecord);
			PID_TYPE newEmptyPos = oldEmptyPos + UDBRecord_totalSize(record);
			UDBRecords_firstEmptyRecordPos_(self->records, newEmptyPos);
			UDBRecord_moveToPos_(record, UDBRecord_pos(firstEmptyRecord));
			UDBRecord_setPos_(firstEmptyRecord, newEmptyPos);
			UDBRecord_saveHeader(firstEmptyRecord);
#ifdef DEBUG
			printf("move\n");
#endif
			UDBIndex_setPos_forPid_(self->index, newEmptyPos, UDBRecord_pid(firstEmptyRecord));
			UDBIndex_setPos_forPid_(self->index, oldEmptyPos, UDBRecord_pid(record));
		}
		
		UDB_commitTransaction(self);
		
		return 1;
	}
	return 0;
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

