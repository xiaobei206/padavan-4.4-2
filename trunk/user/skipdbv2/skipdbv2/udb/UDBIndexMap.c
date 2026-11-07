/* Fixed-width PID-to-record offset mapping operations. */
#include "UDBIndexInternal.h"
#include <stdio.h>
#include <limits.h>

/* Compute a seek-safe byte offset for one fixed-width PID slot. */
int UDBIndex_offsetForPid_(PID_TYPE pid, size_t *offset)
{
	if (NULL == offset ||
		pid > (PID_TYPE)-1 / sizeof(UDBIndexEntry))
		return 0;
	*offset = (size_t)pid * sizeof(UDBIndexEntry);
	if (*offset > (size_t)LONG_MAX)
		return 0;
	/* The complete fixed-width slot must fit within signed file offsets. */
	return sizeof(UDBIndexEntry) <= (size_t)LONG_MAX - *offset;
}

PID_TYPE UDBIndex_posForPid_(UDBIndex *self, PID_TYPE pid)
{
	UDBIndexEntry entry;
	size_t file_size, offset;

	if (NULL == self || NULL == self->file || NULL == self->file->file ||
		self->file->maxPos < 0 ||
		!UDBIndex_offsetForPid_(pid, &offset))
		return 0;

	file_size = (size_t)self->file->maxPos;
	if (offset > file_size ||
		file_size - offset < sizeof(UDBIndexEntry))
		return 0;

	entry.pos = 0;
	JFile_setPosition_(self->file, (long)offset);
	if (JFile_fread(self->file, (unsigned char *)&entry, sizeof(UDBIndexEntry), 1) != 1)
		return 0;

	return entry.pos;
}


int UDBIndex_setPos_forPid_(UDBIndex *self, PID_TYPE pos, PID_TYPE pid)
{
	UDBIndexEntry entry;
	PID_TYPE old_pos = 0;
	long previous_file_pos, previous_max_pos;
	size_t offset;
	int was_reserved = 0;

	if (!UDBIndex_hasStreams_(self) ||
		!UDBIndex_offsetForPid_(pid, &offset) ||
		pos > (PID_TYPE)LONG_MAX || (pid == 0 && pos != 0))
		return 0;

	old_pos = UDBIndex_posForPid_(self, pid);
	if (ferror(self->file->file))
		return 0;
	was_reserved = UDBIndex_hasReservation_(self, pid);
	/* Never manufacture mappings or free PIDs from an arbitrary unmapped slot. */
	if (pid != 0 && old_pos == 0 && !was_reserved)
		return 0;

	previous_file_pos = self->file->pos;
	previous_max_pos = self->file->maxPos;
	entry.pos = pos;
	JFile_setPosition_(self->file, (long)offset);
	if (JFile_fwrite(self->file, (unsigned char *)&entry,
			 sizeof(UDBIndexEntry), 1) != 1) {
		self->file->pos = previous_file_pos;
		self->file->maxPos = previous_max_pos;
		return 0;
	}

	if (pos == 0) {
		if (pid != 0 && (old_pos != 0 || was_reserved))
			return UDBIndex_releasePid_(self, pid);
		UDBIndex_unreservePid_(self, pid);
	} else {
		UDBIndex_forgetPid_(self, pid);
	}
	return 1;
}

long UDBIndex_pidCount(UDBIndex *self)
{
	long end_pos;

	if (NULL == self || NULL == self->file || NULL == self->file->file)
		return -1;
	end_pos = JFile_setPositionToEnd(self->file);
	if (end_pos < 0 ||
		end_pos % (long)sizeof(UDBIndexEntry) != 0)
		return -1;
	return end_pos / (long)sizeof(UDBIndexEntry);
}


void UDBIndex_show(UDBIndex *self)
{
	long pid_count = UDBIndex_pidCount(self);
	PID_TYPE pid;

	printf("UDBIndex: (pid/pos)\n\n");
	if (pid_count < 0) {
		printf("  unable to read PID index length\n\n");
		return;
	}

	for (pid = 0; pid < (PID_TYPE)pid_count; pid++) {
		PID_TYPE pos = UDBIndex_posForPid_(self, pid);
		printf("  %i -> %i\n", (int)pid, (int)pos);
	}
	printf("\n");
}


