/* UDBIndex lifecycle and transaction facade. */
#include "UDBIndexInternal.h"
#include <stdio.h>
#include <stdlib.h>

int UDBIndex_isValid_(UDBIndex *self)
{
	return self != NULL && self->file != NULL &&
		self->holes != NULL && self->reserved != NULL;
}

int UDBIndex_hasFile_(UDBIndex *self)
{
	return UDBIndex_isValid_(self) && self->file->file != NULL;
}

int UDBIndex_hasStreams_(UDBIndex *self)
{
	return UDBIndex_hasFile_(self) && self->file->log != NULL;
}

UDBIndex *UDBIndex_new(void)
{
	UDBIndex *self = (UDBIndex *)calloc(1, sizeof(UDBIndex));
	if (NULL == self)
		return NULL;
	self->file = JFile_new();
	self->holes = List_new();
	self->reserved = List_new();
	if (NULL == self->file || NULL == self->holes || NULL == self->reserved) {
		JFile_free(self->file);
		List_free(self->holes);
		List_free(self->reserved);
		free(self);
		return NULL;
	}
	UDBIndex_setPath_(self, "default");
	if (NULL == self->file->path || NULL == self->file->logPath) {
		JFile_free(self->file);
		List_free(self->holes);
		List_free(self->reserved);
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
	List_free(self->reserved);
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
	if (NULL == self || NULL == self->file ||
		NULL == self->holes || NULL == self->reserved)
		return 0;
	List_removeAll(self->reserved);
	JFile_open(self->file);
	if (NULL == self->file->file || NULL == self->file->log)
		return 0;
	/* UDB recovery must replay both files before inspecting index contents. */
	if (self->file->deferLogReplay)
		return 1;
	return UDBIndex_finishOpening(self);
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
	{
		long pid_count = UDBIndex_pidCount(self);
		if (pid_count <= 0)
			return 0;
		/* Existing index slots are already assigned; holes remain reusable. */
		self->maxPid = (PID_TYPE)(pid_count - 1);
	}
	return 1;
}


void UDBIndex_close(UDBIndex *self)
{
	if (NULL != self) {
		JFile_close(self->file);
		List_removeAll(self->reserved);
	}
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
	if (NULL == self || NULL == self->file ||
		NULL == self->holes || NULL == self->reserved)
		return;

	/* Reservations must be finalized before the global UDB commit marker. */
	if (self->reserved->size != 0) {
		fprintf(stderr, "UDBIndex: PID reservations were not finalized before commit\n");
		JFile_close(self->file);
		return;
	}
	(void)JFile_applyLogToFile(self->file);
}


