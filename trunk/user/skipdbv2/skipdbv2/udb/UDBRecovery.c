/* Coordinated two-file validation and crash recovery for UDB. */
#include "UDBInternal.h"
#include <stdio.h>
#include <errno.h>

int UDB_streamsOpen_(UDB *self)
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
	if (!UDB_streamsOpen_(self)) {
		UDB_close(self);
		return;
	}

	if (recovering) {
		/* Replay both files without deleting either recovery journal. */
		if (!JFile_applyLogToFile(self->index->file) ||
			!UDB_streamsOpen_(self) ||
			!JFile_applyLogToFile(self->records->file) ||
			!UDB_streamsOpen_(self)) {
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
			!UDB_streamsOpen_(self)) {
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
