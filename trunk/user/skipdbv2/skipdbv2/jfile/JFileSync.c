/* Durability barriers and physical journal/file maintenance. */
#include "JFileInternal.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int JFile_hardSyncFileDescriptor_(JFile *self, int fd)
{
	(void)self;
	if (fd < 0)
		return -1;
#ifdef F_FULLFSYNC
	return fcntl(fd, F_FULLFSYNC, 0);
#else
	/* Linux fsync provides the strongest portable durability request here. */
	return fsync(fd);
#endif
}

void JFile_syncFileWritesToDisk(JFile *self)
{
	if (NULL == self || NULL == self->file)
		return;
	if (fflush(self->file) != 0 ||
		(self->fullSync &&
		 (fileno(self->file) < 0 ||
		  JFile_hardSyncFileDescriptor_(self, fileno(self->file)) != 0))) {
		fclose(self->file);
		self->file = NULL;
	}
}

void JFile_syncLogWritesToDisk(JFile *self)
{
	long end;
	int last, fd;

	if (NULL == self || NULL == self->log)
		return;
	if (fflush(self->log) != 0)
		goto fail;
	fd = fileno(self->log);
	if (self->fullSync &&
		(fd < 0 || JFile_hardSyncFileDescriptor_(self, fd) != 0))
		goto fail;

	if (fseek(self->log, 0, SEEK_END) != 0 ||
		(end = ftell(self->log)) <= 0 ||
		fseek(self->log, -1, SEEK_END) != 0)
		goto fail;
	last = fgetc(self->log);
	if (last != JFILE_END_TRANSACTION_ITEM)
		goto fail;
	if (fseek(self->log, -1, SEEK_END) != 0 ||
		fputc(JFILE_END_TRANSACTION, self->log) == EOF ||
		fflush(self->log) != 0)
		goto fail;
	fd = fileno(self->log);
	if (self->fullSync &&
		(fd < 0 || JFile_hardSyncFileDescriptor_(self, fd) != 0))
		goto fail;
	return;

fail:
	if (self && self->log) {
		fclose(self->log);
		self->log = NULL;
	}
}

int JFile_clipLog(JFile *self)
{
	if (NULL == self || NULL == self->logPath)
		return -1;

	if (NULL == self->log) {
		/* Reopen through the same no-symlink, regular-file checks as startup. */
		self->log = JFile_openManagedFile_(self->logPath, NULL);
		if (NULL == self->log)
			return errno == ENOENT ? 0 : -1;
	}
	if (fflush(self->log) != 0 ||
		ftruncate(fileno(self->log), 0) != 0 ||
		fseek(self->log, 0, SEEK_SET) != 0) {
		fclose(self->log);
		self->log = NULL;
		return -1;
	}
	return 0;
}

void JFile_truncate_(JFile *self, off_t size)
{
	if (NULL == self || NULL == self->file || NULL == self->log ||
		size < 0 || size > (off_t)LONG_MAX)
		return;
	JFile_commitToLog(self);
	if (NULL == self->file || NULL == self->log)
		return;
	JFile_commitToFile(self);
	if (NULL == self->file || NULL == self->log)
		return;
	if (fflush(self->file) != 0 ||
		ftruncate(fileno(self->file), size) != 0) {
		/* This API is void; close the stream so UDB's stream checks force
		 * the transaction to abort instead of committing with stale length. */
		fclose(self->file);
		self->file = NULL;
		return;
	}
	self->maxPos = (long)size;
	if (self->pos > self->maxPos)
		self->pos = self->maxPos;
}
