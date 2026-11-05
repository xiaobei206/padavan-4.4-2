#include "JFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static int file_contains(JFile *file, long offset, const char *expected)
{
	char actual[128];
	size_t size = strlen(expected) + 1;
	if (NULL == file || NULL == file->file || size > sizeof(actual) ||
		fseek(file->file, offset, SEEK_SET) != 0 ||
		fread(actual, 1, size, file->file) != size)
		return 0;
	return memcmp(actual, expected, size) == 0;
}

static int append_crash_tail(const char *path)
{
	const unsigned char tail = 0x5a;
	int fd = open(path, O_WRONLY | O_APPEND | O_CLOEXEC);
	int result = 0;

	if (fd < 0)
		return -1;
	if (write(fd, &tail, sizeof(tail)) != (ssize_t)sizeof(tail) ||
		fsync(fd) != 0)
		result = -1;
	if (close(fd) != 0)
		result = -1;
	return result;
}

int main(void)
{
	JFile *file = NULL;
	const char *first = "first-committed-value";
	const char *second = "second-committed-value";
	const char *pending = "uncommitted-tail";
	size_t first_size = strlen(first) + 1;
	size_t second_size = strlen(second) + 1;
	char path[128];
	long committed_log_end;
	int path_len;
	int result = EXIT_FAILURE;

	file = JFile_new();
	if (NULL == file) {
		fprintf(stderr, "JFile_new failed\n");
		return EXIT_FAILURE;
	}
	path_len = snprintf(path, sizeof(path), "jfile_journal_test_%ld", (long)getpid());
	if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
		fprintf(stderr, "unable to construct journal test path\n");
		goto done;
	}
	JFile_setPath_(file, path);
	JFile_remove(file);
	JFile_open(file);
	if (NULL == file->file || NULL == file->log) {
		fprintf(stderr, "journal fixture failed to open\n");
		goto done;
	}
	if (JFile_fwrite(file, (void *)first, first_size, 1) != 1) {
		fprintf(stderr, "failed to append the first journal record\n");
		goto done;
	}
	JFile_commitToLog(file);
	if (!JFile_verifyLog(file)) {
		fprintf(stderr, "committed journal failed structural verification\n");
		goto done;
	}
	if (fseek(file->file, 0, SEEK_END) != 0 || ftell(file->file) != 0) {
		fprintf(stderr, "journal commit modified the data file before replay\n");
		goto done;
	}
	if (!JFile_applyLogToFile(file) || !file_contains(file, 0, first) ||
		!JFile_verifyLog(file)) {
		fprintf(stderr, "committed journal did not replay while retaining evidence\n");
		goto done;
	}
	if (!JFile_applyLogToFile(file) || !file_contains(file, 0, first)) {
		fprintf(stderr, "replaying the same committed journal was not idempotent\n");
		goto done;
	}
	if (JFile_clipLog(file) != 0 || JFile_logSize(file) != 0) {
		fprintf(stderr, "applied journal could not be safely clipped\n");
		goto done;
	}

	JFile_setPosition_(file, (long)first_size);
	if (JFile_fwrite(file, (void *)second, second_size, 1) != 1) {
		fprintf(stderr, "failed to append the second journal record\n");
		goto done;
	}
	JFile_commitToLog(file);
	if (!JFile_verifyLog(file)) {
		fprintf(stderr, "second committed journal failed verification\n");
		goto done;
	}
	committed_log_end = (long)JFile_logSize(file);
	if (committed_log_end <= 0) {
		fprintf(stderr, "committed journal has an invalid physical length\n");
		goto done;
	}

	/* Keep a complete uncommitted item after the last durable transaction. */
	JFile_setPosition_(file, (long)(first_size + second_size));
	if (JFile_fwrite(file, (void *)pending, strlen(pending) + 1, 1) != 1 ||
		JFile_lastCommitPosition(file) != committed_log_end) {
		fprintf(stderr, "scanner did not preserve the prior committed boundary\n");
		goto done;
	}
	if (JFile_verifyLog(file)) {
		fprintf(stderr, "journal with an uncommitted item was accepted as committed\n");
		goto done;
	}

	/* Reproduce a torn header after closing stdio, like a power-loss tail. */
	JFile_close(file);
	if (append_crash_tail(file->logPath) != 0) {
		fprintf(stderr, "failed to append and sync the interrupted journal byte\n");
		goto done;
	}
	JFile_open(file);
	if (NULL == file->file || NULL == file->log ||
		!file_contains(file, 0, first) ||
		!file_contains(file, (long)first_size, second)) {
		fprintf(stderr, "startup recovery did not replay complete transactions\n");
		goto done;
	}
	if (fseek(file->file, 0, SEEK_END) != 0 ||
		ftell(file->file) != (long)(first_size + second_size) ||
		JFile_logSize(file) != 0) {
		fprintf(stderr, "startup recovery retained an interrupted journal tail\n");
		goto done;
	}
	result = EXIT_SUCCESS;

done:
	JFile_remove(file);
	JFile_free(file);
	return result;
}
