#include "JFileInternal.h"
#include <errno.h>
#include <sys/stat.h>
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

static int file_is_private_owned_regular(const char *path)
{
	struct stat file_stat;

	return path != NULL && lstat(path, &file_stat) == 0 &&
		S_ISREG(file_stat.st_mode) && file_stat.st_uid == geteuid() &&
		(file_stat.st_mode & 0777) == 0600;
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
	JFile *linked_file = NULL;
	const char *first = "first-committed-value";
	const char *second = "second-committed-value";
	const char *pending = "uncommitted-tail";
	size_t first_size = strlen(first) + 1;
	size_t second_size = strlen(second) + 1;
	char path[128];
	char link_path[128];
	long committed_log_end;
	int path_len;
	int result = EXIT_FAILURE;

	file = JFile_new();
	if (NULL == file) {
		fprintf(stderr, "JFile_new failed\n");
		return EXIT_FAILURE;
	}
	errno = 0;
	if (JFile_hardSyncFileDescriptor_(file, -1) != -1 || errno != EBADF) {
		fprintf(stderr, "invalid sync descriptor did not report EBADF\n");
		goto done;
	}
	path_len = snprintf(path, sizeof(path), "jfile_journal_test_%ld", (long)getpid());
	if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
		fprintf(stderr, "unable to construct journal test path\n");
		goto done;
	}
	path_len = snprintf(link_path, sizeof(link_path), "%s.link", path);
	if (path_len < 0 || (size_t)path_len >= sizeof(link_path)) {
		fprintf(stderr, "unable to construct symlink test path\n");
		goto done;
	}
	JFile_setPath_(file, path);
	JFile_remove(file);
	JFile_open(file);
	if (NULL == file->file || NULL == file->log) {
		fprintf(stderr, "journal fixture failed to open\n");
		goto done;
	}
	if (!file_is_private_owned_regular(file->path) ||
		!file_is_private_owned_regular(file->logPath)) {
		fprintf(stderr, "managed data or journal file is not private and owned\n");
		goto done;
	}

	/* Both managed data files and journals must be single-link files. */
	if (link(path, link_path) != 0) {
		fprintf(stderr, "unable to create hard-linked data-file fixture\n");
		goto done;
	}
	linked_file = JFile_new();
	if (NULL == linked_file) {
		fprintf(stderr, "unable to allocate hard-linked data-file fixture\n");
		goto done;
	}
	JFile_setPath_(linked_file, link_path);
	JFile_open(linked_file);
	if (linked_file->file != NULL || linked_file->log != NULL) {
		fprintf(stderr, "managed file open accepted a hard-linked data file\n");
		goto done;
	}
	JFile_close(linked_file);
	JFile_free(linked_file);
	linked_file = NULL;
	if (unlink(link_path) != 0) {
		fprintf(stderr, "unable to remove hard-linked data-file fixture\n");
		goto done;
	}

	if (link(file->logPath, link_path) != 0) {
		fprintf(stderr, "unable to create hard-linked journal fixture\n");
		goto done;
	}
	linked_file = JFile_new();
	if (NULL == linked_file) {
		fprintf(stderr, "unable to allocate hard-linked journal fixture\n");
		goto done;
	}
	JFile_setPath_(linked_file, path);
	JFile_open(linked_file);
	if (linked_file->file != NULL || linked_file->log != NULL) {
		fprintf(stderr, "managed file open accepted a hard-linked journal\n");
		goto done;
	}
	JFile_close(linked_file);
	JFile_free(linked_file);
	linked_file = NULL;
	if (unlink(link_path) != 0) {
		fprintf(stderr, "unable to remove hard-linked journal fixture\n");
		goto done;
	}

	/* A pre-existing symlink must never be followed as a managed data file. */
	if (symlink(path, link_path) != 0) {
		fprintf(stderr, "unable to create managed-file symlink fixture\n");
		goto done;
	}
	linked_file = JFile_new();
	if (NULL == linked_file) {
		fprintf(stderr, "unable to allocate symlink rejection fixture\n");
		goto done;
	}
	JFile_setPath_(linked_file, link_path);
	JFile_open(linked_file);
	if (linked_file->file != NULL || linked_file->log != NULL) {
		fprintf(stderr, "managed file open followed a symlink\n");
		goto done;
	}
	JFile_remove(linked_file);
	JFile_free(linked_file);
	linked_file = NULL;

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
	/* Zero-length records are not emitted by JFile_fwrite and must not verify. */
	JFile_close(file);
	{
		long zero_position = 0;
		size_t zero_payload = 0;
		unsigned char terminator = JFILE_END_TRANSACTION;
		int fd = open(file->logPath, O_WRONLY | O_TRUNC | O_CLOEXEC);
		int write_failed = 0;

		if (fd < 0 ||
			write(fd, &zero_position, sizeof(zero_position)) !=
				(ssize_t)sizeof(zero_position) ||
			write(fd, &zero_payload, sizeof(zero_payload)) !=
				(ssize_t)sizeof(zero_payload) ||
			write(fd, &terminator, sizeof(terminator)) !=
				(ssize_t)sizeof(terminator) ||
			fsync(fd) != 0)
			write_failed = 1;
		if (fd >= 0 && close(fd) != 0)
			write_failed = 1;
		if (write_failed) {
			fprintf(stderr, "unable to create zero-length journal fixture\n");
			goto done;
		}
	}
	file->log = JFile_openManagedFile_(file->logPath, NULL);
	if (NULL == file->log || JFile_verifyLog(file)) {
		fprintf(stderr, "zero-length journal record was accepted\n");
		goto done;
	}

	result = EXIT_SUCCESS;

done:
	if (linked_file) {
		JFile_close(linked_file);
		JFile_free(linked_file);
	}
	if (link_path[0])
		unlink(link_path);
	JFile_remove(file);
	JFile_free(file);
	return result;
}
