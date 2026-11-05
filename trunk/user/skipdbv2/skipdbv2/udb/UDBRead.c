/* UDB record lookup and payload-read operations. */
#include "UDBInternal.h"
#include <stdio.h>

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
