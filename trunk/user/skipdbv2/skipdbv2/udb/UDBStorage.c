/*#io
UDB ioDoc(
		docCopyright("Steve Dekorte", 2004)
		docLicense("BSD revised")
		docObject("UDB")    
		docDescription("")
		*/

#include "UDBInternal.h"
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

