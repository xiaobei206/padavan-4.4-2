/* Atomic two-file transaction prepare, apply, and disarm sequence. */
#include "UDBInternal.h"

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
		!UDB_streamsOpen_(self))
		return;
	UDBIndex_begin(self->index);
	UDBRecords_begin(self->records);
	if (!UDB_streamsOpen_(self)) {
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
	if (!UDB_streamsOpen_(self) ||
		!UDBIndex_prepareCommit(self->index) ||
		!UDB_streamsOpen_(self) ||
		!UDBCommitMarker_verifyJournal(self->index->file->logPath) ||
		!UDBCommitMarker_verifyJournal(self->records->file->logPath) ||
		UDBCommitMarker_write(&self->commitMarker, 1) != 0) {
		/* Keep marker 0 until both prepared journals have passed validation. */
		UDB_close(self);
		return;
	}

	/* Apply both journals while retaining both recovery logs. */
	UDBIndex_commit(self->index);
	if (!UDB_streamsOpen_(self)) {
		UDB_close(self);
		return;
	}
	UDBRecords_commit(self->records);
	if (!UDB_streamsOpen_(self)) {
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
