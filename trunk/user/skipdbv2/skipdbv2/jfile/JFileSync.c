/* Durability barriers and physical journal/file maintenance. */
#include "JFileInternal.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

/* Close a failed persistence stream without masking its primary error. */
static void JFile_closeFailedStream_(FILE **stream)
{
	int saved_errno = errno;

	if (saved_errno == 0)
		saved_errno = EIO;
	if (NULL != stream && NULL != *stream) {
		(void)fclose(*stream);
		*stream = NULL;
	}
	errno = saved_errno;
}

int JFile_hardSyncFileDescriptor_(JFile *self, int fd)
{
	int result;
	(void)self;
	if (fd < 0) {
		errno = EBADF;
		return -1;
	}
/* A signal interruption must not be mistaken for a completed durability barrier. */
#ifdef F_FULLFSYNC
	do {
		result = fcntl(fd, F_FULLFSYNC, 0);
	} while (result == -1 && errno == EINTR);
#else
	/* Linux fsync provides the strongest portable durability request here. */
	do {
		result = fsync(fd);
	} while (result == -1 && errno == EINTR);
#endif
	return result;
}

void JFile_syncFileWritesToDisk(JFile *self)
{
	int fd;

	if (NULL == self || NULL == self->file)
		return;
	errno = 0;
	if (fflush(self->file) != 0)
		goto fail;
	if (self->fullSync) {
		errno = 0;
		fd = fileno(self->file);
		if (fd < 0) {
			if (errno == 0)
				errno = EBADF;
			goto fail;
		}
		errno = 0;
		if (JFile_hardSyncFileDescriptor_(self, fd) != 0)
			goto fail;
	}
	return;

fail:
	JFile_closeFailedStream_(&self->file);
}

void JFile_syncLogWritesToDisk(JFile *self)
{
	long end;
	int last, fd;

	if (NULL == self || NULL == self->log)
		return;
	errno = 0;
	if (fflush(self->log) != 0)
		goto fail;
	if (self->fullSync) {
		errno = 0;
		fd = fileno(self->log);
		if (fd < 0) {
			if (errno == 0)
				errno = EBADF;
			goto fail;
		}
		errno = 0;
		if (JFile_hardSyncFileDescriptor_(self, fd) != 0)
			goto fail;
	}

	errno = 0;
	if (fseek(self->log, 0, SEEK_END) != 0)
		goto fail;
	errno = 0;
	end = ftell(self->log);
	if (end < 0) {
		if (errno == 0)
			errno = EIO;
		goto fail;
	}
	if (end == 0) {
		errno = EINVAL;
		goto fail;
	}
	errno = 0;
	if (fseek(self->log, -1, SEEK_END) != 0)
		goto fail;
	errno = 0;
	last = fgetc(self->log);
	if (last == EOF) {
		if (errno == 0)
			errno = EIO;
		goto fail;
	}
	if (last != JFILE_END_TRANSACTION_ITEM) {
		errno = EINVAL;
		goto fail;
	}
	errno = 0;
	if (fseek(self->log, -1, SEEK_END) != 0)
		goto fail;
	errno = 0;
	if (fputc(JFILE_END_TRANSACTION, self->log) == EOF) {
		if (errno == 0)
			errno = EIO;
		goto fail;
	}
	errno = 0;
	if (fflush(self->log) != 0)
		goto fail;
	if (self->fullSync) {
		errno = 0;
		fd = fileno(self->log);
		if (fd < 0) {
			if (errno == 0)
				errno = EBADF;
			goto fail;
		}
		errno = 0;
		if (JFile_hardSyncFileDescriptor_(self, fd) != 0)
			goto fail;
	}
	return;

fail:
	JFile_closeFailedStream_(&self->log);
}

/* Truncate and reposition one already validated, managed journal stream. */
int JFile_truncateLogTo_(JFile *self, long size)
{
	off_t target;
	int fd;

	if (NULL == self || NULL == self->log || size < 0) {
		errno = EINVAL;
		return -1;
	}
	target = (off_t)size;
	if (target < 0 || (long)target != size) {
		errno = EOVERFLOW;
		return -1;
	}
	errno = 0;
	if (fflush(self->log) != 0) {
		if (errno == 0)
			errno = EIO;
		return -1;
	}
	errno = 0;
	fd = fileno(self->log);
	if (fd < 0) {
		if (errno == 0)
			errno = EBADF;
		return -1;
	}
	errno = 0;
	if (ftruncate(fd, target) != 0) {
		if (errno == 0)
			errno = EIO;
		return -1;
	}
	if (self->fullSync) {
		errno = 0;
		if (JFile_hardSyncFileDescriptor_(self, fd) != 0) {
			if (errno == 0)
				errno = EIO;
			return -1;
		}
	}
	clearerr(self->log);
	errno = 0;
	if (fseek(self->log, size, SEEK_SET) != 0) {
		if (errno == 0)
			errno = EIO;
		return -1;
	}
	return 0;
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
	if (JFile_truncateLogTo_(self, 0) != 0) {
		JFile_closeFailedStream_(&self->log);
		return -1;
	}
	return 0;
}

void JFile_truncate_(JFile *self, off_t size)
{
	off_t target;
	int fd;

	if (NULL == self || NULL == self->file || NULL == self->log ||
		size < 0 || size > (off_t)LONG_MAX)
		return;
	/* JFile_commitToFile commits the pending journal exactly once before replay. */
	JFile_commitToFile(self);
	if (NULL == self->file || NULL == self->log)
		return;
	target = size;
	errno = 0;
	if (fflush(self->file) != 0)
		goto fail;
	errno = 0;
	fd = fileno(self->file);
	if (fd < 0) {
		if (errno == 0)
			errno = EBADF;
		goto fail;
	}
	errno = 0;
	if (ftruncate(fd, target) != 0)
		goto fail;
	if (self->fullSync) {
		errno = 0;
		if (JFile_hardSyncFileDescriptor_(self, fd) != 0)
			goto fail;
	}
	self->maxPos = (long)size;
	if (self->pos > self->maxPos)
		self->pos = self->maxPos;
	return;

fail:
	/* This API is void; close the stream so UDB's checks abort the transaction. */
	JFile_closeFailedStream_(&self->file);
}
