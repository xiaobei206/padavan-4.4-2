/* Transaction journal parsing, replay, validation, and commit orchestration. */
#include "JFileInternal.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

void JFile_writeLogToFileIfNeeded(JFile *self)
{
	if (self && self->log && ftell(self->log) > 0)
		JFile_writeLogToFile(self);
}

static int JFile_applyLogToFile_(JFile *self, int clear_log)
{
	int logNotEmpty = 0;
	int lastTerminator = 0;
	long log_end, file_end;

	if (NULL == self || NULL == self->log || NULL == self->file ||
		fseek(self->log, 0, SEEK_END) != 0 ||
		(log_end = ftell(self->log)) < 0 ||
		fseek(self->log, 0, SEEK_SET) != 0)
		goto fatalError;

	while (1) {
		long pos, data_pos, record_start, read_pos;
		size_t total;
		unsigned char *new_buf;

		record_start = ftell(self->log);
		if (record_start < 0)
			goto fatalError;
		if (fread(&pos, sizeof(pos), 1, self->log) != 1) {
			read_pos = ftell(self->log);
			/* Only a zero-byte read exactly at EOF is a clean end. */
			if (!ferror(self->log) && read_pos == record_start &&
				record_start == log_end)
				break;
			printf("JFile: incomplete/error reading log position\n");
			goto fatalError;
		}

		if (fread(&total, sizeof(total), 1, self->log) != 1) {
			printf("JFile: incomplete/error reading log length\n");
			goto fatalError;
		}

		/* Older versions could leave zero-length payload records in the log. */
		if (total == 0) {
			lastTerminator = fgetc(self->log);
			if (lastTerminator != JFILE_END_TRANSACTION_ITEM &&
				lastTerminator != JFILE_END_TRANSACTION) {
				printf("JFile: invalid empty log record terminator\n");
				goto fatalError;
			}
			logNotEmpty = 1;
			continue;
		}

		data_pos = ftell(self->log);
		if (pos < 0 || data_pos < 0 || data_pos >= log_end ||
			total > (size_t)(log_end - data_pos - 1) ||
			total > (size_t)LONG_MAX ||
			pos > LONG_MAX - (long)total) {
			printf("JFile: invalid log record bounds\n");
			goto fatalError;
		}

		new_buf = (unsigned char *)realloc(self->buf, total);
		if (NULL == new_buf) {
			printf("JFile: out of memory reading log\n");
			goto fatalError;
		}
		self->buf = new_buf;

		if (fread(self->buf, total, 1, self->log) != 1) {
			printf("JFile: error reading buf %lu\n", (unsigned long)total);
			goto fatalError;
		}

		lastTerminator = fgetc(self->log);
		if (lastTerminator != JFILE_END_TRANSACTION_ITEM &&
			lastTerminator != JFILE_END_TRANSACTION) {
			printf("JFile: invalid log terminator\n");
			goto fatalError;
		}

		if (fseek(self->file, pos, SEEK_SET) != 0 ||
			fwrite(self->buf, total, 1, self->file) != 1) {
			printf("JFile: error applying log record\n");
			goto fatalError;
		}
		logNotEmpty = 1;
	}

	if (ftell(self->log) != log_end ||
		(logNotEmpty && lastTerminator != JFILE_END_TRANSACTION))
		goto fatalError;

	if (logNotEmpty) {
		JFile_syncFileWritesToDisk(self);
		if (NULL == self->file)
			goto fatalError;
		if (clear_log && JFile_clipLog(self) != 0)
			goto fatalError;
	}

	if (fseek(self->file, 0, SEEK_END) != 0 ||
		(file_end = ftell(self->file)) < 0 ||
		fseek(self->log, 0, SEEK_END) != 0)
		goto fatalError;
	self->maxPos = file_end;
	if (self->pos > self->maxPos)
		self->pos = self->maxPos;
	return 1;

fatalError:
	fprintf(stderr, "JFile: FATAL ERROR: invalid log file '%s' - may result in inconsistency\n",
		(self && self->logPath) ? self->logPath : "(unknown)");
	if (self && self->log) {
		fclose(self->log);
		self->log = NULL;
	}
	return 0;
}

void JFile_writeLogToFile(JFile *self)
{
	(void)JFile_applyLogToFile_(self, 1);
}

int JFile_applyLogToFile(JFile *self)
{
	return JFile_applyLogToFile_(self, 0);
}

void JFile_begin(JFile *self)
{
	// begin is implicit unless we need to mark a rollback point
}

void JFile_commitToFile(JFile *self)
{
	if (NULL == self || NULL == self->file || NULL == self->log)
		return;
	JFile_commitToLog(self);
	if (NULL == self->file || NULL == self->log)
		return;
	JFile_writeLogToFile(self);
}

void JFile_commitToLog(JFile *self)
{
	long log_pos;

	if (NULL == self)
		return;
	if (self->journalingOn) {
		if (NULL == self->log)
			return;
		log_pos = ftell(self->log);
		if (log_pos < 0) {
			fclose(self->log);
			self->log = NULL;
			return;
		}
		if (log_pos > 0)
			JFile_syncLogWritesToDisk(self);
	} else {
		JFile_syncFileWritesToDisk(self);
	}
}

int JFile_needsSync(JFile *self)
{
	return JFile_logSize(self) != 0;
}

size_t JFile_logSize(JFile *self)
{
	long pos;
	if (NULL == self || NULL == self->log)
		return 0;
	pos = ftell(self->log);
	return pos < 0 ? 0 : (size_t)pos;
}

long JFile_lastCommitPosition(JFile *self)
{
	long lastCommitPosition = 0;
	long end;

	/* Negative means the log could not be inspected; callers must not trim it. */
	if (NULL == self || NULL == self->log ||
		fseek(self->log, 0, SEEK_END) != 0)
		return -1;
	end = ftell(self->log);
	if (end < 0 || fseek(self->log, 0, SEEK_SET) != 0)
		return -1;

	while (1) {
		long pos, data_pos;
		size_t total;
		int r, terminator;

		r = fread(&pos, sizeof(pos), 1, self->log);
		if (r != 1) {
			if (ferror(self->log))
				return -1;
			break;
		}

		r = fread(&total, sizeof(total), 1, self->log);
		if (r != 1) {
			if (ferror(self->log))
				return -1;
			break;
		}

		data_pos = ftell(self->log);
		if (data_pos < 0)
			return -1;
		if (data_pos >= end ||
			total > (size_t)(end - data_pos - 1) ||
			pos < 0 || total > (size_t)LONG_MAX ||
			pos > LONG_MAX - (long)total)
			break;

		if (fseek(self->log, (long)total, SEEK_CUR) != 0)
			return -1;
		terminator = fgetc(self->log);
		if (terminator == EOF && ferror(self->log))
			return -1;
		if (terminator == JFILE_END_TRANSACTION_ITEM)
			continue;
		if (terminator == JFILE_END_TRANSACTION) {
			lastCommitPosition = ftell(self->log);
			if (lastCommitPosition < 0)
				return -1;
			continue;
		}
		break;
	}

	if (fseek(self->log, 0, SEEK_END) != 0)
		return -1;
	return lastCommitPosition;
}

int JFile_verifyLog(JFile *self)
{
	long lastCommitPosition, log_end;
	if (NULL == self || NULL == self->log)
		return 0;
	lastCommitPosition = JFile_lastCommitPosition(self);
	if (lastCommitPosition < 0 ||
		fseek(self->log, 0, SEEK_END) != 0 ||
		(log_end = ftell(self->log)) < 0)
		return 0;
	return lastCommitPosition == log_end;
}

void JFile_clipLogToLastCompletedTransaction(JFile *self)
{
	long lastCommitPosition, log_end;

	if (NULL == self || NULL == self->log)
		return;
	lastCommitPosition = JFile_lastCommitPosition(self);
	if (fseek(self->log, 0, SEEK_END) != 0 ||
		(log_end = ftell(self->log)) < 0)
		goto fail;
	if (lastCommitPosition < 0 || lastCommitPosition > log_end)
		goto fail;
	if (lastCommitPosition != log_end) {
		printf("JFile: incomplete transaction found in '%s' - discarding\n", self->logPath);
		if (ftruncate(fileno(self->log), (off_t)lastCommitPosition) != 0 ||
			fseek(self->log, 0, SEEK_END) != 0)
			goto fail;
	}
	return;

fail:
	fprintf(stderr, "JFile: unable to trim journal '%s' safely\n",
		self->logPath ? self->logPath : "(unknown)");
	fclose(self->log);
	self->log = NULL;
}
