/*#io
JFile ioDoc(
		  docCopyright("Steve Dekorte", 2004)
		  docLicense("BSD revised")
		  docObject("JFile")
		  docDescription("A journaled file.")
		  log file is currently endian dependent
*/

#include "JFile.h"
#include "Common.h"	// for Win32 snprintf
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include "PortableTruncate.h"

// See: http://lists.apple.com/archives/darwin-dev/2005/Feb/msg00072.html

#include <fcntl.h>

#define JFILE_END_TRANSACTION_ITEM 1
#define JFILE_END_TRANSACTION 2

int JFile_clipLog(JFile *self);
void JFile_writeLogToFile(JFile *self);
void JFile_flushLog(JFile *self);
void JFile_flushFile(JFile *self);

JFile *JFile_new(void)
{
	JFile *self = (JFile *)calloc(1, sizeof(JFile));
	if (NULL == self)
		return NULL;
	//JFile_setPath_(self, "default");
	JFile_setJournalingOn_(self, 1);
	self->logHighWaterMark = 1024*1024*8;
	self->fullSync = 1;
	return self;
}

void JFile_setJournalingOn_(JFile *self, int aBool)
{
	if (NULL != self)
		self->journalingOn = aBool;
}

void JFile_free(JFile *self)
{
	if (NULL == self)
		return;
	JFile_close(self);
	if (self->path) free(self->path);
	if (self->logPath) free(self->logPath);
	if (self->buf) free(self->buf);
	free(self);
}

void JFile_setPath_(JFile *self, const char *path)
{
	size_t path_len;
	char *new_path, *new_log_path;

	if (NULL == self || NULL == path)
		return;
	path_len = strlen(path);
	if (path_len > (size_t)-1 - 5)
		return;

	new_path = (char *)malloc(path_len + 1);
	new_log_path = (char *)malloc(path_len + 5);
	if (NULL == new_path || NULL == new_log_path) {
		free(new_path);
		free(new_log_path);
		return;
	}
	memcpy(new_path, path, path_len + 1);
	memcpy(new_log_path, path, path_len);
	memcpy(new_log_path + path_len, ".log", 5);

	free(self->path);
	free(self->logPath);
	self->path = new_path;
	self->logPath = new_log_path;
}

char *JFile_fileName(JFile *self)
{
#ifdef __WIN32__
	char *fileName = strrchr(self->path, '\\');
#else
	char *fileName = strrchr(self->path, '/');
#endif
	return  fileName ? fileName : self->path;
}

void JFile_setLogPath_(JFile *self, const char *path)
{
	const char *file_name;
	size_t path_len, name_len, need_slash, total_len;
	char *new_log_path;

	if (NULL == self || NULL == path || NULL == self->path)
		return;
	file_name = JFile_fileName(self);
	if (file_name && (*file_name == '/' || *file_name == '\\'))
		++file_name;
	if (NULL == file_name || '\0' == *file_name)
		return;

	path_len = strlen(path);
	name_len = strlen(file_name);
	need_slash = path_len > 0 && path[path_len - 1] != '/';
	if (path_len > (size_t)-1 - name_len - 6 - need_slash)
		return;
	total_len = path_len + need_slash + name_len + 5;
	new_log_path = (char *)malloc(total_len);
	if (NULL == new_log_path)
		return;

	memcpy(new_log_path, path, path_len);
	if (need_slash)
		new_log_path[path_len] = '/';
	memcpy(new_log_path + path_len + need_slash, file_name, name_len);
	memcpy(new_log_path + path_len + need_slash + name_len, ".log", 5);
	free(self->logPath);
	self->logPath = new_log_path;
}

void JFile_setPath_withExtension_(JFile *self, const char *path, const char *ext)
{
	size_t path_len, ext_len, length;
	char *s;

	if (NULL == self || NULL == path || NULL == ext)
		return;
	path_len = strlen(path);
	ext_len = strlen(ext);
	if (path_len > (size_t)-1 - ext_len - 2)
		return;
	length = path_len + ext_len + 2;
	s = (char *)malloc(length);
	if (NULL == s)
		return;
	memcpy(s, path, path_len);
	s[path_len] = '.';
	memcpy(s + path_len + 1, ext, ext_len + 1);
	JFile_setPath_(self, s);
	free(s);
}

char *JFile_path(JFile *self)
{
	return self ? self->path : NULL;
}

void JFile_remove(JFile *self)
{
	if (NULL == self)
		return;
	JFile_close(self);
	if (self->path)
		remove(self->path);
	if (self->logPath)
		remove(self->logPath);
}

void JFile_open(JFile *self)
{
	long file_end;
	int created_log = 0;

	if (NULL == self || NULL == self->path || NULL == self->logPath)
		return;

	JFile_close(self);
	self->pos = 0;
	self->maxPos = 0;

	self->file = fopen(self->path, "r+");
	if (NULL == self->file) {
		/* Do not truncate an existing database after an unrelated open error. */
		if (errno != ENOENT)
			goto fail;
		self->file = fopen(self->path, "w+");
		if (NULL == self->file)
			goto fail;
	}

	if (fseek(self->file, 0, SEEK_END) != 0 ||
		(file_end = ftell(self->file)) < 0)
		goto fail;
	self->maxPos = file_end;
	self->pos = 0;

	self->log = fopen(self->logPath, "r+");
	if (NULL == self->log) {
		if (errno != ENOENT)
			goto fail;
		self->log = fopen(self->logPath, "w+");
		if (NULL == self->log)
			goto fail;
		created_log = 1;
	}

	if (created_log) {
#ifdef F_PREALLOCATE
		fcntl(fileno(self->log), F_PREALLOCATE, self->logHighWaterMark);
#else
		/* Linux journal preallocation intentionally remains disabled. */
#endif
	} else {
		JFile_clipLogToLastCompletedTransaction(self);
		if (NULL == self->log)
			goto fail;
		JFile_writeLogToFile(self);
		if (NULL == self->log || fseek(self->file, 0, SEEK_END) != 0 ||
			(file_end = ftell(self->file)) < 0)
			goto fail;
		self->maxPos = file_end;
	}
	return;

fail:
	fprintf(stderr, "JFile: unable to open database '%s' and journal '%s'\n",
		self->path, self->logPath);
	JFile_close(self);
	self->pos = 0;
	self->maxPos = 0;
}

void JFile_close(JFile *self)
{
	if (NULL == self)
		return;
	if (self->file)
	{
		fclose(self->file);
		self->file = NULL;
	}

	if (self->log)
	{
		fclose(self->log);
		self->log = NULL;
	}
}

void JFile_delete(JFile *self)
{
	if (NULL == self)
		return;
	JFile_close(self);
	if (self->path)
		remove(self->path);
	if (self->logPath)
		remove(self->logPath);
}

size_t JFile_fwrite(JFile *self, void *buf, size_t size, size_t nobjs)
{
	size_t total, nobjsWritten;
	long old_pos, log_start;
	FILE *log;

	if (NULL == self || (size > 0 && NULL == buf))
		return 0;
	if (size != 0 && nobjs > (size_t)-1 / size)
		return 0;
	total = size * nobjs;
	/* A zero-byte write must not create a journal record with no payload. */
	if (total == 0)
		return 0;
	if (self->pos < 0 || total > (size_t)LONG_MAX ||
		self->pos > LONG_MAX - (long)total)
		return 0;

	old_pos = self->pos;
	if (self->journalingOn) {
		log = self->log;
		if (NULL == log)
			return 0;
		log_start = ftell(log);
		if (log_start < 0)
			return 0;
		if (fwrite(&old_pos, sizeof(old_pos), 1, log) != 1 ||
			fwrite(&total, sizeof(total), 1, log) != 1 ||
			fwrite(buf, size, nobjs, log) != nobjs ||
			fputc(JFILE_END_TRANSACTION_ITEM, log) == EOF) {
			/* Remove a partial record so later writes cannot hide behind it. */
			(void)fflush(log);
			clearerr(log);
			if (ftruncate(fileno(log), (off_t)log_start) == 0) {
				clearerr(log);
				if (fseek(log, log_start, SEEK_SET) != 0) {
					fclose(log);
					self->log = NULL;
				}
			} else {
				fclose(log);
				self->log = NULL;
			}
			return 0;
		}
		nobjsWritten = nobjs;
		self->pos = old_pos + (long)total;
	} else {
		if (NULL == self->file || fseek(self->file, old_pos, SEEK_SET) != 0)
			return 0;
		nobjsWritten = fwrite(buf, size, nobjs, self->file);
		{
			long next_pos = ftell(self->file);
			if (next_pos >= 0)
				self->pos = next_pos;
		}
	}

	if (self->pos > self->maxPos)
		self->maxPos = self->pos;
	return nobjsWritten;
}

size_t JFile_fread(JFile *self, void *buf, size_t size, size_t nobjs)
{
	size_t nobjsRead;
	long next_pos;

	if (NULL == self || NULL == self->file ||
		(size > 0 && nobjs > (size_t)-1 / size) ||
		(size > 0 && NULL == buf) || self->pos < 0)
		return 0;
	if (size == 0 || nobjs == 0)
		return 0;
	if (fseek(self->file, self->pos, SEEK_SET) != 0)
		return 0;
	nobjsRead = fread(buf, size, nobjs, self->file);
	next_pos = ftell(self->file);
	if (next_pos >= 0)
		self->pos = next_pos;
	return nobjsRead;
}

int JFile_fputc(JFile *self, int i)
{
	unsigned char c = (unsigned char)i;
	if (JFile_fwrite(self, &c, 1, 1) != 1)
		return EOF;
	return c;
}

int JFile_fgetc(JFile *self)
{
	unsigned char value;

	if (JFile_fread(self, &value, 1, 1) != 1)
		return EOF;
	return (int)value;
}

void JFile_writeInt_(JFile *self, int v)
{
	JFile_fwrite(self, &v, sizeof(int), 1);
}

int JFile_readInt(JFile *self)
{
	int v = 0;
	if (JFile_fread(self, &v, sizeof(int), 1) != 1)
		return 0;
	return v;
}

void JFile_setPosition_(JFile *self, long pos)
{
	if (NULL == self || pos < 0)
		return;
	self->pos = pos;
}

void JFile_fseek(JFile *self, long offset, int whence)
{
	long base, next;

	if (NULL == self)
		return;
	switch (whence) {
		case SEEK_SET:
			base = 0;
			break;
		case SEEK_CUR:
			base = self->pos;
			break;
		case SEEK_END:
			base = self->maxPos;
			break;
		default:
			return;
	}
	if (base < 0 ||
		(offset > 0 && base > LONG_MAX - offset) ||
		(offset < 0 && offset < -base))
		return;
	next = base + offset;
	if (next < 0)
		return;
	self->pos = next;
}

long JFile_position(JFile *self)
{
	return self ? self->pos : 0;
}

long JFile_setPositionToEnd(JFile *self)
{
	if (NULL == self || self->maxPos < 0)
		return 0;
	self->pos = self->maxPos;
	return self->pos;
}

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

void JFile_writeLogToFileIfNeeded(JFile *self)
{
	if (self && self->log && ftell(self->log) > 0)
		JFile_writeLogToFile(self);
}

void JFile_writeLogToFile(JFile *self)
{
	size_t writeCount = 0;
	int logNotEmpty = 0;
	int lastTerminator = 0;
	long log_end;

	if (NULL == self || NULL == self->log || NULL == self->file ||
		fseek(self->log, 0, SEEK_END) != 0 ||
		(log_end = ftell(self->log)) < 0 ||
		fseek(self->log, 0, SEEK_SET) != 0)
		goto fatalError;

	while (1)
	{
		long pos, data_pos;
		size_t total;
		unsigned char *new_buf;

		if (fread(&pos, sizeof(long), 1, self->log) != 1)
		{
			if (feof(self->log)) break;
			printf("JFile: error reading pos\n");
			goto fatalError;
		}

		if (fread(&total, sizeof(size_t), 1, self->log) != 1)
		{
			printf("JFile: error reading total\n");
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
			pos > LONG_MAX - (long)total)
		{
			printf("JFile: invalid log record bounds\n");
			goto fatalError;
		}

		new_buf = (unsigned char *)realloc(self->buf, total);
		if (NULL == new_buf)
		{
			printf("JFile: out of memory reading log\n");
			goto fatalError;
		}
		self->buf = new_buf;

		if (fread(self->buf, total, 1, self->log) != 1)
		{
			printf("JFile: error reading buf %lu\n", (unsigned long)total);
			goto fatalError;
		}

		lastTerminator = fgetc(self->log);

		if (lastTerminator != JFILE_END_TRANSACTION_ITEM &&
			lastTerminator != JFILE_END_TRANSACTION)
		{
			printf("JFile: invalid log terminator\n");
			goto fatalError;
		}

		if (fseek(self->file, pos, SEEK_SET) != 0 ||
			fwrite(self->buf, total, 1, self->file) != 1)
		{
			printf("JFile: error applying log record\n");
			goto fatalError;
		}
		writeCount ++;
		logNotEmpty = 1;
	}

	if (logNotEmpty)
	{
		//printf("writing log\n");
		JFile_syncFileWritesToDisk(self);
		if (NULL == self->file || JFile_clipLog(self) != 0)
			goto fatalError;
	}

	return;

	fatalError:
		fprintf(stderr, "JFile: FATAL ERROR: invalid log file '%s' - may result in inconsistency\n",
			(self && self->logPath) ? self->logPath : "(unknown)");
		if (self && self->log) {
			fclose(self->log);
			self->log = NULL;
		}

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

#include <unistd.h>

int JFile_clipLog(JFile *self)
{
	if (NULL == self || NULL == self->logPath)
		return -1;

	if (NULL == self->log) {
		self->log = fopen(self->logPath, "r+");
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
		ftruncate(fileno(self->file), size) != 0)
		return;
	self->maxPos = (long)size;
	if (self->pos > self->maxPos)
		self->pos = self->maxPos;
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

	if (NULL == self || NULL == self->log ||
		fseek(self->log, 0, SEEK_END) != 0)
		return 0;
	end = ftell(self->log);
	if (end < 0 || fseek(self->log, 0, SEEK_SET) != 0)
		return 0;

	while (1)
	{
		long pos, data_pos;
		size_t total;
		int r, terminator;

		r = fread(&pos, sizeof(long), 1, self->log);
		if (r != 1) {
			if (!feof(self->log))
				fprintf(stderr, "JFile: error reading log position\n");
			break;
		}

		r = fread(&total, sizeof(size_t), 1, self->log);
		if (r != 1)
			break;

		data_pos = ftell(self->log);
		if (data_pos < 0 || data_pos >= end ||
			total > (size_t)(end - data_pos - 1) ||
			pos < 0 || total > (size_t)LONG_MAX ||
			pos > LONG_MAX - (long)total) {
			/* Ignore corrupt/truncated records; retain only completed prefix. */
			break;
		}

		if (fseek(self->log, (long)total, SEEK_CUR) != 0)
			break;
		terminator = fgetc(self->log);
		if (terminator == JFILE_END_TRANSACTION_ITEM)
			continue;
		if (terminator == JFILE_END_TRANSACTION) {
			lastCommitPosition = ftell(self->log);
			if (lastCommitPosition < 0)
				lastCommitPosition = 0;
			continue;
		}
		break;
	}

	fseek(self->log, 0, SEEK_END);
	return lastCommitPosition;
}

int JFile_verifyLog(JFile *self)
{
	long lastCommitPosition = JFile_lastCommitPosition(self);

	fseek(self->log, 0, SEEK_END);

	return lastCommitPosition == ftell(self->log);
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

