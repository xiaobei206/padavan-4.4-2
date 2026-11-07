#ifndef PADAVAN_FIREWALL_RULE_FILE_H
#define PADAVAN_FIREWALL_RULE_FILE_H
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* Atomically claim a private /tmp rule file; never follow a pre-existing path. */
static FILE *
firewall_rule_file_open(char *path)
{
	int fd;
	FILE *fp;
	if (!path || !*path) {
		errno = EINVAL;
		return NULL;
	}
	fd = mkstemp(path);
	if (fd < 0) {
		path[0] = '\0';
		return NULL;
	}
	if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
		int saved_errno = errno;
		close(fd);
		unlink(path);
		path[0] = '\0';
		errno = saved_errno;
		return NULL;
	}
	fp = fdopen(fd, "w");
	if (!fp) {
		int saved_errno = errno;
		close(fd);
		unlink(path);
		path[0] = '\0';
		errno = saved_errno;
		return NULL;
	}
	return fp;
}

/* A rule set must be fully flushed and closed before it is applied. */
static int
firewall_rule_file_finish(FILE *fp, char *path)
{
	int failed = 0;
	int saved_errno = 0;

	if (!fp) {
		if (path && path[0]) {
			unlink(path);
			path[0] = '\0';
		}
		errno = EINVAL;
		return -1;
	}
	if (fflush(fp) != 0) {
		failed = 1;
		saved_errno = errno;
	}
	if (ferror(fp)) {
		failed = 1;
		if (!saved_errno)
			saved_errno = errno ? errno : EIO;
	}
	if (fclose(fp) != 0) {
		if (!saved_errno)
			saved_errno = errno;
		failed = 1;
	}
	if (failed) {
		if (path && path[0]) {
			unlink(path);
			path[0] = '\0';
		}
		errno = saved_errno ? saved_errno : EIO;
		return -1;
	}
	return 0;
}

static void
firewall_rule_file_remove(char *path)
{
	if (path && path[0]) {
		unlink(path);
		path[0] = '\0';
	}
}
#endif /* PADAVAN_FIREWALL_RULE_FILE_H */
