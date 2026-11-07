/* UDB record-space compaction operations. */
#include "UDB.h"
#include <sys/types.h>
#include <time.h>
#include <limits.h>

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

	do {
		compactions = UDB_compactStepFor_(self, 0.1);
		if (compactions > INT_MAX - count)
			return INT_MAX;
		count += compactions;
		if (count == INT_MAX)
			return count;
	} while (compactions > 0);

	return count;
}

//#define DEBUG 1

int UDB_compactStep(UDB *self)
{
	return UDB_compactStepFor_(self, 0.0);
}

/* Require both compaction data and journal streams to remain open. */
static int UDB_compactionStreamsAreOpen_(UDB *self)
{
	return self != NULL && self->records != NULL &&
		self->records->file != NULL &&
		self->records->file->file != NULL &&
		self->records->file->log != NULL;
}

static int UDB_compactStepOnce_(UDB *self)
{
	UDBRecord *firstEmptyRecord, *record;
	PID_TYPE oldEmptyPos, newEmptyPos, newSize;
	PID_TYPE emptyTotalSize = 0, recordTotalSize = 0;

	if (NULL == self || !self->isOpen || self->withinTransaction ||
		NULL == self->records || NULL == self->index)
		return 0;

	firstEmptyRecord = UDBRecords_firstEmptyRecord(self->records);
	/* A corrupt header makes UDBRecords close its streams so that a failed
	 * scan cannot be mistaken for an empty free list or a truncation target. */
	if (!UDB_compactionStreamsAreOpen_(self)) {
		UDB_close(self);
		return 0;
	}
	if (NULL == firstEmptyRecord || !UDBRecord_isEmpty(firstEmptyRecord))
		return 0;

	record = UDBRecords_recordAfter_(self->records, firstEmptyRecord);
	/*
	 * A larger hole must leave room for a complete residual header. The
	 * destination can otherwise fit the row, but the remainder would be too
	 * short to represent as a physical record and would corrupt the scan.
	 */
	if (record && !UDBRecord_isEmpty(record)) {
		emptyTotalSize = UDBRecord_totalSize(firstEmptyRecord);
		recordTotalSize = UDBRecord_totalSize(record);
		if (emptyTotalSize == 0 || recordTotalSize == 0 ||
			recordTotalSize > emptyTotalSize ||
			(emptyTotalSize > recordTotalSize &&
			 emptyTotalSize - recordTotalSize < sizeof(UDBRecordHeader)))
			return 0;
	}

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
			!UDB_compactionStreamsAreOpen_(self)) {
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
		if (recordTotalSize > (PID_TYPE)-1 - oldEmptyPos) {
			UDB_close(self);
			return 0;
		}
		newEmptyPos = oldEmptyPos + recordTotalSize;
		if (!UDBRecords_firstEmptyRecordPos_(self->records, newEmptyPos) ||
			!UDBRecord_moveToPos_(record, oldEmptyPos)) {
			UDB_close(self);
			return 0;
		}

		UDBRecord_setPos_(firstEmptyRecord, newEmptyPos);
		if (emptyTotalSize > recordTotalSize) {
			PID_TYPE remainderTotalSize = emptyTotalSize - recordTotalSize;

			/* Split the original hole and tombstone its former live row. */
			UDBRecord_size_(firstEmptyRecord,
				remainderTotalSize - sizeof(UDBRecordHeader));
			if (!UDBRecord_saveHeader(firstEmptyRecord)) {
				UDB_close(self);
				return 0;
			}
			record->header.tag = RECORD_TAG_EMPTY;
			if (!UDBRecord_saveHeader(record)) {
				UDB_close(self);
				return 0;
			}
		} else if (!UDBRecord_saveHeader(firstEmptyRecord)) {
			/* An exact-fit hole naturally becomes the old row's tombstone. */
			UDB_close(self);
			return 0;
		}
		if (!UDBIndex_setPos_forPid_(self->index, oldEmptyPos,
				UDBRecord_pid(record))) {
			UDB_close(self);
			return 0;
		}
		/* The empty extent's former PID stays free; never map it to a row. */
	}

	UDB_commitTransaction(self);
	return self->isOpen ? 1 : 0;
}

/*
 * A nonpositive budget preserves the single-step API. Positive budgets perform
 * whole compaction operations until the requested time slice expires or no
 * further progress is possible.
 */
int UDB_compactStepFor_(UDB *self, double maxSeconds)
{
	struct timespec started, now;
	int count = 0;
	int has_time_budget = maxSeconds > 0.0 &&
		clock_gettime(CLOCK_MONOTONIC, &started) == 0;

	for (;;) {
		double elapsed;
		if (!UDB_compactStepOnce_(self))
			return count;
		if (count < INT_MAX)
			++count;
		if (count == INT_MAX || !has_time_budget)
			return count;
		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
			return count;
		elapsed = (double)now.tv_sec - (double)started.tv_sec +
			((double)now.tv_nsec - (double)started.tv_nsec) / 1000000000.0;
		if (elapsed < 0.0 || elapsed >= maxSeconds)
			return count;
	}
}

