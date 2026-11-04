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

void UDBIndex_open(UDBIndex *self)
{
	if (NULL == self || NULL == self->file)
		return;
	JFile_open(self->file);
	if (NULL == self->file->file || NULL == self->file->log)
		return;
	/* need to also call UDBIndex_finishOpening */
	UDBIndex_finishOpening(self);
	self->maxPid = 0;
}

void UDBIndex_finishOpening(UDBIndex *self)
{
	JFile_begin(self->file);
	UDBIndex_setPos_forPid_(self, 0, 0);
	UDBIndex_preCommit(self);
	UDBIndex_findHoles(self);
}

void UDBIndex_close(UDBIndex *self)
{
	if (NULL != self)
		JFile_close(self->file);
}

void UDBIndex_begin(UDBIndex *self)
{
	JFile_begin(self->file);
}

void UDBIndex_preCommit(UDBIndex *self)
{
	JFile_commitToLog(self->file);
}

void UDBIndex_commit(UDBIndex *self)
{
	JFile_commitToFile(self->file);
}

// holes ------------------------------------------- 

void UDBIndex_findHoles(UDBIndex *self)
{
	PID_TYPE pid;

	if (NULL == self || NULL == self->holes || NULL == self->file)
		return;
	pid = (PID_TYPE)UDBIndex_pidCount(self);
	List_removeAll(self->holes);

	/* PID 0 is reserved as the null/sentinel record, not a reusable hole. */
	while (pid > 1) {
		--pid;
		if (!UDBIndex_posForPid_(self, pid))
			List_push_(self->holes, (void *)pid);
	}
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
	if (hole)
		return hole;

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

int UDBIndex_setPos_forPid_(UDBIndex *self, PID_TYPE pos, PID_TYPE pid)
{
	UDBIndexEntry entry;
	PID_TYPE old_pos = 0;
	long previous_file_pos, previous_max_pos;
	size_t offset;

	if (NULL == self || NULL == self->file ||
		pid > (PID_TYPE)-1 / sizeof(UDBIndexEntry) ||
		pos > (PID_TYPE)LONG_MAX || NULL == self->holes)
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
		if (old_pos != 0)
			List_push_(self->holes, (void *)pid);
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

