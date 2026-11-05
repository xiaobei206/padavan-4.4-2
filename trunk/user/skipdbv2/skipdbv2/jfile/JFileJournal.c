/* Transaction journal replay, validation, and commit orchestration. */
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
	JFileJournalScan scan;
	long cursor = 0;
	int lastTerminator = 0;
	int logNotEmpty;

	if (NULL == self || NULL == self->log || NULL == self->file ||
		JFileJournal_scan_(self, &scan) != 0 ||
		scan.validEnd != scan.logEnd ||
		scan.committedEnd != scan.logEnd)
		goto fatalError;

	logNotEmpty = scan.committedRecordCount != 0;
	if (fseek(self->log, 0, SEEK_SET) != 0)
		goto fatalError;

	while (cursor < scan.committedEnd) {
		long pos, data_pos;
		size_t total;
		unsigned char *new_buf;

		if (fseek(self->log, cursor, SEEK_SET) != 0 ||
			fread(&pos, sizeof(pos), 1, self->log) != 1 ||
			fread(&total, sizeof(total), 1, self->log) != 1)
			goto fatalError;
		data_pos = cursor + (long)(sizeof(long) + sizeof(size_t));

		if (total > 0) {
			new_buf = (unsigned char *)realloc(self->buf, total);
			if (NULL == new_buf) {
				printf("JFile: out of memory reading log\n");
				goto fatalError;
			}
			self->buf = new_buf;
			if (fread(self->buf, 1, total, self->log) != total) {
				printf("JFile: error reading buf %lu\n", (unsigned long)total);
				goto fatalError;
			}
			if (fseek(self->file, pos, SEEK_SET) != 0 ||
				fwrite(self->buf, 1, total, self->file) != total) {
				printf("JFile: error applying log record\n");
				goto fatalError;
			}
		}

		lastTerminator = fgetc(self->log);
		if (lastTerminator != JFILE_END_TRANSACTION_ITEM &&
			lastTerminator != JFILE_END_TRANSACTION) {
			printf("JFile: invalid log terminator\n");
			goto fatalError;
		}
		cursor = data_pos + (long)total + 1;
	}

	if (cursor != scan.committedEnd ||
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
		(self->maxPos = ftell(self->file)) < 0 ||
		fseek(self->log, 0, SEEK_END) != 0)
		goto fatalError;
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
	(void)self;
	/* begin is implicit unless we need to mark a rollback point */
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
	JFileJournalScan scan;
	if (NULL == self || NULL == self->log ||
		JFileJournal_scan_(self, &scan) != 0)
		return -1;
	return scan.committedEnd;
}

int JFile_verifyLog(JFile *self)
{
	JFileJournalScan scan;
	if (NULL == self || NULL == self->log ||
		JFileJournal_scan_(self, &scan) != 0)
		return 0;
	return scan.validEnd == scan.logEnd &&
		scan.committedEnd == scan.logEnd;
}

void JFile_clipLogToLastCompletedTransaction(JFile *self)
{
	JFileJournalScan scan;

	if (NULL == self || NULL == self->log)
		return;
	if (JFileJournal_scan_(self, &scan) != 0 ||
		scan.committedEnd < 0 || scan.committedEnd > scan.logEnd)
		goto fail;
	if (scan.committedEnd != scan.logEnd) {
		printf("JFile: incomplete transaction found in '%s' - discarding\n",
			self->logPath);
		if (JFile_truncateLogTo_(self, scan.committedEnd) != 0)
			goto fail;
	}
	return;

fail:
	fprintf(stderr, "JFile: unable to trim journal '%s' safely\n",
		self->logPath ? self->logPath : "(unknown)");
	fclose(self->log);
	self->log = NULL;
}
