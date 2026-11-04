/*#io
UDBIndex ioDoc(
			docCopyright("Steve Dekorte", 2004)
			docLicense("BSD revised")
			docObject("UDBIndex")    
			docDescription("")
			*/

#include "UDBIndex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

UDBIndex *UDBIndex_new(void)
{
	UDBIndex *self = (UDBIndex *)calloc(1, sizeof(UDBIndex));
	if (NULL == self)
		return NULL;
	self->file = JFile_new();
	self->holes = List_new();
	if (NULL == self->file || NULL == self->holes) {
		JFile_free(self->file);
		List_free(self->holes);
		free(self);
		return NULL;
	}
	UDBIndex_setPath_(self, "default");
	if (NULL == self->file->path || NULL == self->file->logPath) {
		JFile_free(self->file);
		List_free(self->holes);
		free(self);
		return NULL;
	}
	return self;
}

void UDBIndex_free(UDBIndex *self)
{
	if (NULL == self)
		return;
	UDBIndex_close(self);
	JFile_free(self->file);
	List_free(self->holes);
	free(self);
}

JFile *UDBIndex_jfile(UDBIndex *self)
{
	return self ? self->file : NULL;
}

void UDBIndex_delete(UDBIndex *self)
{
	if (NULL == self)
		return;
	JFile_close(self->file);
	JFile_delete(self->file);
}

void UDBIndex_setPath_(UDBIndex *self, const char *path)
{
	if (self && self->file && path)
		JFile_setPath_withExtension_(self->file, path, "udbIndex");
}

void UDBIndex_setLogPath_(UDBIndex *self, const char *s)
{
	if (self && self->file && s)
		JFile_setLogPath_(self->file, s);
}

char *UDBIndex_path(UDBIndex *self)
{
	return self ? JFile_path(self->file) : NULL;
}

int UDBIndex_open(UDBIndex *self)
{
	if (NULL == self || NULL == self->file)
		return 0;
	JFile_open(self->file);
	if (NULL == self->file->file || NULL == self->file->log)
		return 0;
	if (!UDBIndex_finishOpening(self))
		return 0;
	{
		long pid_count = UDBIndex_pidCount(self);
		if (pid_count <= 0)
			return 0;
		/* Existing index slots are already assigned; holes remain reusable. */
		self->maxPid = (PID_TYPE)(pid_count - 1);
	}
	return 1;
}

int UDBIndex_finishOpening(UDBIndex *self)
{
	if (NULL == self || NULL == self->file ||
		NULL == self->file->file || NULL == self->file->log ||
		self->file->maxPos < 0 ||
		self->file->maxPos % (long)sizeof(UDBIndexEntry) != 0) {
		fprintf(stderr, "UDBIndex: truncated PID index entry\n");
		return 0;
	}
	JFile_begin(self->file);
	if (!UDBIndex_setPos_forPid_(self, 0, 0))
		return 0;
	UDBIndex_preCommit(self);
	if (NULL == self->file->file || NULL == self->file->log)
		return 0;
	if (!UDBIndex_findHoles(self))
		return 0;
	return 1;
}

void UDBIndex_close(UDBIndex *self)
{
	if (NULL != self)
		JFile_close(self->file);
}

void UDBIndex_begin(UDBIndex *self)
{
	if (self && self->file && self->file->file && self->file->log)
		JFile_begin(self->file);
}

void UDBIndex_preCommit(UDBIndex *self)
{
	if (self && self->file)
		JFile_commitToLog(self->file);
}

void UDBIndex_commit(UDBIndex *self)
{
	if (self && self->file)
		JFile_commitToFile(self->file);
}

// holes ------------------------------------------- 

int UDBIndex_findHoles(UDBIndex *self)
{
	PID_TYPE pid;

	if (NULL == self || NULL == self->holes || NULL == self->file ||
		NULL == self->file->file)
		return 0;
	pid = (PID_TYPE)UDBIndex_pidCount(self);
	List_removeAll(self->holes);

	/* PID 0 is reserved as the null/sentinel record, not a reusable hole. */
	while (pid > 1) {
		size_t old_size;
		--pid;
		if (!UDBIndex_posForPid_(self, pid)) {
			old_size = self->holes->size;
			List_push_(self->holes, (void *)pid);
			if (self->holes->size != old_size + 1)
				return 0;
		}
	}
	return 1;
}

// pid ops ------------------------------------------- 

PID_TYPE UDBIndex_nextPid(UDBIndex *self)
{
	PID_TYPE hole;
	if (NULL == self || NULL == self->holes || NULL == self->file ||
		NULL == self->file->file)
		return 0;
	hole = (PID_TYPE)List_top(self->holes);
	if (hole)
		return hole;
	return (PID_TYPE)JFile_setPositionToEnd(self->file) /
		sizeof(UDBIndexEntry);
}

PID_TYPE UDBIndex_allocPid(UDBIndex *self)
{
	PID_TYPE hole;
	PID_TYPE max;
	if (NULL == self || NULL == self->holes || NULL == self->file ||
		NULL == self->file->file)
		return 0;
	hole = (PID_TYPE)List_pop(self->holes);
	if (hole) {
		/* Normalize legacy duplicate free-slot entries before reserving the PID. */
		while (List_removeFirst_(self->holes, (void *)hole)) {
			/* Remove every stale duplicate for this allocation. */
		}
		return hole;
	}

	max = (PID_TYPE)JFile_setPositionToEnd(self->file) /
		sizeof(UDBIndexEntry);
	if (max > self->maxPid)
		self->maxPid = max;
	else {
		if (self->maxPid == (PID_TYPE)-1)
			return 0;
		self->maxPid++;
	}
	return self->maxPid;
}

PID_TYPE UDBIndex_posForPid_(UDBIndex *self, PID_TYPE pid)
{
	UDBIndexEntry entry;
	size_t file_size, offset;

	if (NULL == self || NULL == self->file || self->file->maxPos < 0)
		return 0;

	file_size = (size_t)self->file->maxPos;
	if (pid > (PID_TYPE)-1 / sizeof(UDBIndexEntry))
		return 0;
	offset = (size_t)pid * sizeof(UDBIndexEntry);
	if (offset > (size_t)LONG_MAX || offset > file_size ||
		file_size - offset < sizeof(UDBIndexEntry))
		return 0;

	entry.pos = 0;
	JFile_setPosition_(self->file, (long)offset);
	if (JFile_fread(self->file, (unsigned char *)&entry, sizeof(UDBIndexEntry), 1) != 1)
		return 0;

	return entry.pos;
}

/* True only for a PID reserved by allocPid and not currently free. */
int UDBIndex_isReservedPid_(UDBIndex *self, PID_TYPE pid)
{
	size_t i;

	if (NULL == self || NULL == self->holes || NULL == self->file ||
		NULL == self->file->file || pid == 0 || pid > self->maxPid ||
		UDBIndex_posForPid_(self, pid) != 0 || ferror(self->file->file))
		return 0;

	for (i = 0; i < self->holes->size; ++i) {
		if (self->holes->items[i] == (void *)pid)
			return 0;
	}
	return 1;
}

int UDBIndex_setPos_forPid_(UDBIndex *self, PID_TYPE pos, PID_TYPE pid)
{
	UDBIndexEntry entry;
	PID_TYPE old_pos = 0;
	long previous_file_pos, previous_max_pos;
	size_t offset;

	if (NULL == self || NULL == self->file ||
		pid > (PID_TYPE)-1 / sizeof(UDBIndexEntry) ||
		pos > (PID_TYPE)LONG_MAX || (pid == 0 && pos != 0) ||
		NULL == self->holes || NULL == self->file->file ||
		NULL == self->file->log)
		return 0;
	offset = (size_t)pid * sizeof(UDBIndexEntry);
	if (offset > (size_t)LONG_MAX)
		return 0;

	if (pos == 0)
		old_pos = UDBIndex_posForPid_(self, pid);
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

	/* Keep the free-PID stack unique when records are removed/reused/compacted. */
	if (pos == 0) {
		if (pid != 0 && old_pos != 0) {
			size_t old_hole_count;
			while (List_removeFirst_(self->holes, (void *)pid)) {
				/* Normalize stale duplicates before adding the freed PID. */
			}
			old_hole_count = self->holes->size;
			List_push_(self->holes, (void *)pid);
			if (self->holes->size != old_hole_count + 1)
				return 0;
		}
	} else {
		while (List_removeFirst_(self->holes, (void *)pid)) {
			/* Remove any stale duplicate free-slot entries. */
		}
	}
	return 1;
}

long UDBIndex_pidCount(UDBIndex *self)
{
	if (NULL == self || NULL == self->file || NULL == self->file->file)
		return 0;
	return JFile_setPositionToEnd(self->file) / sizeof(UDBIndexEntry);
}

void UDBIndex_show(UDBIndex *self)
{
	PID_TYPE pid, maxPid = UDBIndex_pidCount(self);
	
	printf("UDBIndex: (pid/pos)\n\n");
	
	for (pid = 0; pid < maxPid; pid ++)
	{
		PID_TYPE pos = UDBIndex_posForPid_(self, pid);
		printf("  %i -> %i\n", (int)pid, (int)pos);
	}
	
	printf("\n");
}

