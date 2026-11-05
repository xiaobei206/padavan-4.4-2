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
	if (NULL == self->path || NULL == self->commitMarker.path ||
		NULL == self->index->file->path || NULL == self->index->file->logPath ||
		NULL == self->records->file->path || NULL == self->records->file->logPath) {
		UDBIndex_free(self->index);
		UDBRecords_free(self->records);
		free(self->path);
		UDBCommitMarker_clear(&self->commitMarker);
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
	free(self->commitMarker.path);    
	
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
	if (self->commitMarker.path)
		remove(self->commitMarker.path);
}

void UDB_setPath_(UDB *self, const char *path)
{
	size_t n;
	char *main_path,*index_path,*index_log,*data_path,*data_log;
	if(!self||!path||self->isOpen||!self->index||!self->records||
		!self->index->file||!self->records->file)return;
	n=strlen(path);
	if(n>(size_t)-1-sizeof(".udbIndex.log")||n>(size_t)-1-sizeof(".udbData.log"))return;
	main_path=(char *)malloc(n+1);
	index_path=(char *)malloc(n+sizeof(".udbIndex"));
	index_log=(char *)malloc(n+sizeof(".udbIndex.log"));
	data_path=(char *)malloc(n+sizeof(".udbData"));
	data_log=(char *)malloc(n+sizeof(".udbData.log"));
	if(!main_path||!index_path||!index_log||!data_path||!data_log){
		free(main_path);free(index_path);free(index_log);free(data_path);free(data_log);return;}
	memcpy(main_path,path,n+1);
	memcpy(index_path,path,n);memcpy(index_path+n,".udbIndex",sizeof(".udbIndex"));
	memcpy(index_log,path,n);memcpy(index_log+n,".udbIndex.log",sizeof(".udbIndex.log"));
	memcpy(data_path,path,n);memcpy(data_path+n,".udbData",sizeof(".udbData"));
	memcpy(data_log,path,n);memcpy(data_log+n,".udbData.log",sizeof(".udbData.log"));
	if(UDBCommitMarker_setPath_(&self->commitMarker,path)!=0){
		free(main_path);free(index_path);free(index_log);free(data_path);free(data_log);return;}
	free(self->path);free(self->index->file->path);free(self->index->file->logPath);
	free(self->records->file->path);free(self->records->file->logPath);
	self->path=main_path;self->index->file->path=index_path;self->index->file->logPath=index_log;
	self->records->file->path=data_path;self->records->file->logPath=data_log;
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


static int UDB_streamsOpen(UDB *self)
{
	return self != NULL && self->index != NULL && self->records != NULL &&
		self->index->file != NULL && self->records->file != NULL &&
		self->index->file->file != NULL && self->index->file->log != NULL &&
		self->records->file->file != NULL && self->records->file->log != NULL;
}

void UDB_open(UDB *self)
{
	const char *artifact_paths[4];
	int marker, recovering, marker_created = 0;

	if (NULL == self || NULL == self->commitMarker.path || self->isOpen ||
		NULL == self->index || NULL == self->records ||
		NULL == self->index->file || NULL == self->records->file)
		return;

	artifact_paths[0]=self->index->file->path;
	artifact_paths[1]=self->index->file->logPath;
	artifact_paths[2]=self->records->file->path;
	artifact_paths[3]=self->records->file->logPath;
	errno=0;
	if(UDBCommitMarker_open(&self->commitMarker,0)!=0){
		int artifacts;
		if(errno!=ENOENT)return;
		artifacts=UDBCommitMarker_artifactsExist(artifact_paths,4);
		if(artifacts!=0){
			fprintf(stderr,"UDB: commit marker is missing while database artifacts exist\n");
			return;
		}
		if(UDBCommitMarker_open(&self->commitMarker,1)!=0)return;
		marker_created=1;
	}
	/* Establish a durable clean marker before creating any database artifacts. */
	if (marker_created && UDBCommitMarker_write(&self->commitMarker, 0) != 0) {
		UDB_close(self);
		return;
	}
	marker = UDBCommitMarker_read(&self->commitMarker);
	if (marker == -1 && UDBCommitMarker_artifactsAreEmpty(artifact_paths,4)) {
		/* Recover a crash during first-time marker initialization only. */
		if (UDBCommitMarker_write(&self->commitMarker, 0) == 0)
			marker = UDBCommitMarker_read(&self->commitMarker);
	}
	if (marker < 0) {
		fprintf(stderr, "UDB: invalid or empty commit marker; refusing startup\n");
		UDB_close(self);
		return;
	}
	recovering = (marker == 1);
	self->index->file->deferLogReplay = recovering;
	self->records->file->deferLogReplay = recovering;

	/* Validate both durable journals before opening either database file. */
	if (recovering &&
		(!UDBCommitMarker_verifyJournal(self->index->file->logPath) ||
		 !UDBCommitMarker_verifyJournal(self->records->file->logPath))) {
		fprintf(stderr, "UDB: committed transaction journal is incomplete or corrupt\n");
		UDB_close(self);
		return;
	}

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

	if (recovering) {
		/* Replay both files without deleting either recovery journal. */
		if (!JFile_applyLogToFile(self->index->file) ||
			!UDB_streamsOpen(self) ||
			!JFile_applyLogToFile(self->records->file) ||
			!UDB_streamsOpen(self)) {
			UDB_close(self);
			return;
		}

		/* Both files are now durable; disarm recovery before journal cleanup. */
		if (UDBCommitMarker_write(&self->commitMarker, 0) != 0) {
			UDB_close(self);
			return;
		}
		if (JFile_clipLog(self->index->file) != 0 ||
			JFile_clipLog(self->records->file) != 0) {
			UDB_close(self);
			return;
		}

		self->index->file->deferLogReplay = 0;
		self->records->file->deferLogReplay = 0;
		if (!UDBIndex_finishOpening(self->index) ||
			!UDBRecords_finishOpening(self->records) ||
			!UDB_streamsOpen(self)) {
			UDB_close(self);
			return;
		}
	}

	/* Persist directories containing data, commit marker, and both journals. */
	if (UDBCommitMarker_syncParentDirectory(self->path) != 0 ||
		UDBCommitMarker_syncParentDirectory(self->commitMarker.path) != 0 ||
		UDBCommitMarker_syncParentDirectory(self->index->file->logPath) != 0 ||
		UDBCommitMarker_syncParentDirectory(self->records->file->logPath) != 0) {
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
	if (self->index && self->index->file)
		self->index->file->deferLogReplay = 0;
	if (self->records && self->records->file)
		self->records->file->deferLogReplay = 0;
	UDBCommitMarker_close(&self->commitMarker);
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
	if (NULL == self || !self->isOpen || self->withinTransaction ||
		!UDB_streamsOpen(self))
		return;
	UDBIndex_begin(self->index);
	UDBRecords_begin(self->records);
	if (!UDB_streamsOpen(self)) {
		UDB_close(self);
		return;
	}
	self->withinTransaction = 1;
}

void UDB_commitTransaction(UDB *self)
{
	if (NULL == self || !self->isOpen || !self->withinTransaction)
		return;

	UDBIndex_preCommit(self->index);
	UDBRecords_preCommit(self->records);
	if (!UDB_streamsOpen(self) ||
		!UDBIndex_prepareCommit(self->index) ||
		!UDB_streamsOpen(self) || UDBCommitMarker_write(&self->commitMarker, 1) != 0) {
		/* Marker 0 discards both complete journals if preparation fails. */
		UDB_close(self);
		return;
	}

	/* Apply both journals while retaining both recovery logs. */
	UDBIndex_commit(self->index);
	if (!UDB_streamsOpen(self)) {
		UDB_close(self);
		return;
	}
	UDBRecords_commit(self->records);
	if (!UDB_streamsOpen(self)) {
		UDB_close(self);
		return;
	}

	/* The data files are durable; only now may recovery be disarmed. */
	if (UDBCommitMarker_write(&self->commitMarker, 0) != 0) {
		UDB_close(self);
		return;
	}

	/* With marker 0, leftover journals are harmless and can be truncated. */
	if (JFile_clipLog(self->index->file) != 0 ||
		JFile_clipLog(self->records->file) != 0) {
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
	/* A corrupt header makes UDBRecords close its streams so that a failed
	 * scan cannot be mistaken for an empty free list or a truncation target. */
	if (!self->records->file || !self->records->file->file ||
		!self->records->file->log) {
		UDB_close(self);
		return 0;
	}
	if (NULL == firstEmptyRecord || !UDBRecord_isEmpty(firstEmptyRecord))
		return 0;

	record = UDBRecords_recordAfter_(self->records, firstEmptyRecord);
	/* Sliding the hole over the next live record is safe only when the hole
	 * can contain the whole record; otherwise the destination write overlaps
	 * the record header/data that follows it. Leave this hole untouched. */
	if (record && !UDBRecord_isEmpty(record) &&
		UDBRecord_totalSize(record) > UDBRecord_totalSize(firstEmptyRecord))
		return 0;

	if (NULL == record) {
		PID_TYPE tail_pos = UDBRecord_pos(firstEmptyRecord);
		/* Clear the persisted free-list pointer before truncating its target. */
		UDB_beginTransaction(self);
		if (!self->withinTransaction ||
			!UDBRecords_firstEmptyRecordPos_(self->records, 0)) {
			UDB_close(self);
			return 0;
		}
		UDB_commitTransaction(self);
		if (!self->isOpen)
			return 0;
		/* The trailing hole contains no live rows, so truncation is now safe. */
		if (!UDBRecords_truncate_(self->records, (off_t)tail_pos) ||
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
		/* An empty extent may retain a PID that now maps to a newer live row.
		 * Merging physical extents must not alter either PID mapping. */
		if (!UDBRecord_saveHeader(firstEmptyRecord)) {
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

