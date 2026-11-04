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

void JFile_clipLog(JFile *self);
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
	return self->path;
}

void JFile_remove(JFile *self)
{
	JFile_close(self);
	remove(self->path);
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
		JFile_writeLogToFile(self);
		if (fseek(self->file, 0, SEEK_END) != 0 ||
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
	JFile_close(self);
	remove(self->path);
	remove(self->logPath);
}

size_t JFile_fwrite(JFile *self, void *buf, size_t size, size_t nobjs)
{
	size_t total = size * nobjs;
	size_t nobjsWritten;

	if (self->journalingOn)
	{
		fwrite(&(self->pos), sizeof(long), 1, self->log);
		fwrite(&total, sizeof(size_t), 1, self->log);
		nobjsWritten = fwrite(buf, size, nobjs, self->log);
		fputc(JFILE_END_TRANSACTION_ITEM, self->log); // commit byte
		self->pos += total;
	}
	else
	{
		fseek(self->file, self->pos, SEEK_SET);
		nobjsWritten = fwrite(buf, size, nobjs, self->file);
		self->pos = ftell(self->file);
	}

	if (nobjs != nobjsWritten)
	{
		printf("Error: JFile_fwrite nobjs != nobjsWritten\n");
	}

	if (self->pos > self->maxPos) self->maxPos = self->pos;

	return nobjsWritten;
}

size_t JFile_fread(JFile *self, void *buf, size_t size, size_t nobjs)
{
	size_t nobjsRead;

	fseek(self->file, self->pos, SEEK_SET);
	nobjsRead = fread(buf, size, nobjs, self->file);
	self->pos = ftell(self->file);

	return nobjsRead;
}

int JFile_fputc(JFile *self, int i)
{
	unsigned char c = i;
	JFile_fwrite(self, &c, 1, 1);
	return i;
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
	int v;
	JFile_fread(self, &v, sizeof(int), 1);
	return v;
}

void JFile_setPosition_(JFile *self, long pos)
{
	self->pos = pos;
}

void JFile_fseek(JFile *self, long offset, int whence)
{
	switch (whence)
	{
		case SEEK_CUR:
			self->pos += offset;
			break;
		case SEEK_SET:
			self->pos = offset;
			break;
		case SEEK_END:
			self->pos = self->maxPos + offset;
			break;
	}
}

long JFile_position(JFile *self)
{
	return self->pos;
}

long JFile_setPositionToEnd(JFile *self)
{
	self->pos = self->maxPos;
	return self->pos;
}

void JFile_hardSyncFileDescriptor_(JFile *self, int fd)
{
	#ifdef F_FULLFSYNC
		fcntl(fd, F_FULLFSYNC, NULL);
	#else
		#warning Linux cant ensure data sync to physical media
		fsync(fd);
	#endif
}

void JFile_syncFileWritesToDisk(JFile *self)
{
	//printf("f"); fflush(stdout);
	fflush(self->file);

	if(self->fullSync)
	{
		JFile_hardSyncFileDescriptor_(self, fileno(self->file));
	}
}

void JFile_syncLogWritesToDisk(JFile *self)
{
	//printf("j"); fflush(stdout);
	fflush(self->log);
	if (self->fullSync)
	{
			JFile_hardSyncFileDescriptor_(self, fileno(self->log));
	}

	fseek(self->log, -1, SEEK_END);

	fputc(JFILE_END_TRANSACTION, self->log);

	fflush(self->log);
	if (self->fullSync)
	{
			JFile_hardSyncFileDescriptor_(self, fileno(self->log));
	}
}

void JFile_writeLogToFileIfNeeded(JFile *self)
{
	if (ftell(self->log))
	{
		JFile_writeLogToFile(self);
	}
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

                //by janson, TODO better
                if (0 == total) {
                    return;
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
		JFile_clipLog(self);
	}

	return;

	fatalError:
		fprintf(stderr, "JFile: FATAL ERROR: invalid log file '%s' - may result in inconsistency\n",
			(self && self->logPath) ? self->logPath : "(unknown)");

}


void JFile_begin(JFile *self)
{
	// begin is implicit unless we need to mark a rollback point
}

void JFile_commitToFile(JFile *self)
{
	JFile_commitToLog(self);
	/*
	if (!JFile_verifyLog(self))
	{
		printf("log '%s' not valid\n", self->logPath);
		exit(-1);
	}
	*/
	JFile_writeLogToFile(self);
}

void JFile_commitToLog(JFile *self)
{
	if (self->journalingOn)
	{
		if (ftell(self->log))
		{
			JFile_syncLogWritesToDisk(self);
		}
	}
	else
	{
		JFile_syncFileWritesToDisk(self);
	}
}

#include <unistd.h>

void JFile_clipLog(JFile *self)
{
	if (!self->log)
	{
		self->log = fopen(self->logPath, "r+");

		if (self->log)
		{
			ftruncate(fileno(self->log), 0);
			fseek(self->log, 0, SEEK_SET);
		}
	}
	else
	{
		ftruncate(fileno(self->log), 0);
		fseek(self->log, 0, SEEK_SET);
	}
}

void JFile_truncate_(JFile *self, off_t size)
{
	JFile_commitToLog(self);
	JFile_commitToFile(self);
	ftruncate(fileno(self->file), size);
}

int JFile_needsSync(JFile *self)
{
	return JFile_logSize(self) != 0;
}

size_t JFile_logSize(JFile *self)
{
	// this only valid when not in the middle of a commit
	return ftell(self->log);
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
	long lastCommitPosition = JFile_lastCommitPosition(self);

	fseek(self->log, 0, SEEK_END);

	if (lastCommitPosition != ftell(self->log))
	{
		printf("JFile: incomplete transaction found in '%s' - discarding\n", self->logPath);
		ftruncate(fileno(self->log), lastCommitPosition);
		fseek(self->log, 0, SEEK_END);
	}
}

