#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ppp_options_file.h"

static void
read_exact(const char *path, const char *expected)
{
	char buf[128];
	FILE *fp = fopen(path, "r");
	assert(fp != NULL);
	assert(fgets(buf, sizeof(buf), fp) != NULL);
	assert(strcmp(buf, expected) == 0);
	assert(fclose(fp) == 0);
}

int
main(void)
{
	char directory[] = "/tmp/ppp-options-test.XXXXXX";
	char unsafe_directory[256], options[256], victim[256];
	char symlink_path[256], hardlink_path[256], fifo_path[256], fail_path[256];
	FILE *fp;
	struct stat st;
	int saved_errno;

	assert(mkdtemp(directory) != NULL);
	assert(ppp_options_directory_ready(directory) == 0);
	assert(snprintf(unsafe_directory, sizeof(unsafe_directory), "%s/unsafe", directory) > 0);
	assert(mkdir(unsafe_directory, 0700) == 0);
	assert(chmod(unsafe_directory, 0777) == 0);
	assert(ppp_options_directory_ready(unsafe_directory) == 0);
	assert(stat(unsafe_directory, &st) == 0);
	assert((st.st_mode & (S_IWGRP | S_IWOTH)) == 0);
	assert(rmdir(unsafe_directory) == 0);

	assert(snprintf(options, sizeof(options), "%s/options.wan0", directory) > 0);
	assert(snprintf(victim, sizeof(victim), "%s/victim", directory) > 0);
	assert(snprintf(symlink_path, sizeof(symlink_path), "%s/options.symlink", directory) > 0);
	assert(snprintf(hardlink_path, sizeof(hardlink_path), "%s/options.hardlink", directory) > 0);
	assert(snprintf(fifo_path, sizeof(fifo_path), "%s/options.fifo", directory) > 0);
	assert(snprintf(fail_path, sizeof(fail_path), "%s/options.fail", directory) > 0);

	/* A leftover readable credentials file is tightened before truncation. */
	fp = fopen(options, "w");
	assert(fp != NULL);
	assert(fputs("old-password\n", fp) >= 0);
	assert(fclose(fp) == 0);
	assert(chmod(options, 0644) == 0);
	fp = ppp_options_file_open(options);
	assert(fp != NULL);
	assert(stat(options, &st) == 0 && (st.st_mode & 0777) == 0600);
	assert(fputs("password 'new-secret'\n", fp) >= 0);
	assert(ppp_options_file_finish(fp, options) == 0);
	assert(stat(options, &st) == 0 && (st.st_mode & 0777) == 0600);
	read_exact(options, "password 'new-secret'\n");

	fp = fopen(victim, "w");
	assert(fp != NULL);
	assert(fputs("preserve-me\n", fp) >= 0);
	assert(fclose(fp) == 0);
	assert(chmod(victim, 0644) == 0);

	/* Refuse a symlink and leave the victim untouched. */
	assert(symlink(victim, symlink_path) == 0);
	assert(ppp_options_file_open(symlink_path) == NULL);
	read_exact(victim, "preserve-me\n");

	/* Refuse hardlinks before truncating, protecting their target. */
	assert(link(victim, hardlink_path) == 0);
	assert(ppp_options_file_open(hardlink_path) == NULL);
	read_exact(victim, "preserve-me\n");

	/* Reject a FIFO before open so no writer can block on a reader. */
	assert(mkfifo(fifo_path, 0600) == 0);
	assert(ppp_options_file_open(fifo_path) == NULL);
	assert(lstat(fifo_path, &st) == 0 && S_ISFIFO(st.st_mode));

	/* Failed flush/close must not leave a partial credentials file. */
	fp = ppp_options_file_open(fail_path);
	assert(fp != NULL);
	assert(fputs("partial-secret\n", fp) >= 0);
	assert(close(fileno(fp)) == 0);
	errno = 0;
	assert(ppp_options_file_finish(fp, fail_path) == -1);
	saved_errno = errno;
	assert(saved_errno != 0);
	assert(access(fail_path, F_OK) != 0 && errno == ENOENT);

	assert(unlink(fifo_path) == 0);
	assert(unlink(hardlink_path) == 0);
	assert(unlink(symlink_path) == 0);
	assert(unlink(victim) == 0);
	assert(unlink(options) == 0);
	assert(rmdir(directory) == 0);
	return 0;
}
