#ifndef PADAVAN_PPP_OPTIONS_FILE_H
#define PADAVAN_PPP_OPTIONS_FILE_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* The directory must never be a symlink, a non-directory or writable by
 * another uid. Repair permissive mode on a root-owned directory in /tmp. */
static int
ppp_options_directory_ready(const char *directory)
{
	struct stat st;
	mode_t safe_mode;

	if (!directory || !directory[0]) {
		errno = EINVAL;
		return -1;
	}
	if (mkdir(directory, 0700) != 0 && errno != EEXIST)
		return -1;
	if (lstat(directory, &st) != 0)
		return -1;
	if (!S_ISDIR(st.st_mode) || st.st_uid != geteuid()) {
		errno = EPERM;
		return -1;
	}

	if (st.st_mode & (S_IWGRP | S_IWOTH)) {
		safe_mode = (st.st_mode & 0777) | S_IRWXU;
		safe_mode &= ~(S_IWGRP | S_IWOTH);
		if (chmod(directory, safe_mode) != 0)
			return -1;
		if (lstat(directory, &st) != 0)
			return -1;
		if (!S_ISDIR(st.st_mode) || st.st_uid != geteuid() ||
		    (st.st_mode & (S_IWGRP | S_IWOTH))) {
			errno = EPERM;
			return -1;
		}
	}

	return 0;
}

/* Tighten/validate an old file before opening it for truncation. */
static int
ppp_options_path_ready(const char *path)
{
	struct stat st;

	if (!path || !path[0]) {
		errno = EINVAL;
		return -1;
	}
	if (lstat(path, &st) != 0)
		return errno == ENOENT ? 0 : -1;
	if (!S_ISREG(st.st_mode)) {
		errno = EINVAL;
		return -1;
	}
	if (st.st_uid != geteuid()) {
		errno = EPERM;
		return -1;
	}
	if (st.st_nlink != 1) {
		errno = EMLINK;
		return -1;
	}
	/* Protect leftovers from an interrupted older, post-hoc chmod workflow. */
	if ((st.st_mode & 0777) != (S_IRUSR | S_IWUSR)) {
		if (chmod(path, S_IRUSR | S_IWUSR) != 0)
			return -1;
	}

	return 0;
}

/* Make credential files private before truncation or writing any secret. */
static FILE *
ppp_options_file_open(const char *path)
{
	int fd, saved_errno;
	FILE *fp;
	struct stat st;
	const mode_t private_mode = S_IRUSR | S_IWUSR;

	if (ppp_options_path_ready(path) != 0)
		return NULL;

	fd = open(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK, private_mode);
	if (fd < 0)
		return NULL;

	if (fstat(fd, &st) != 0) {
		saved_errno = errno ? errno : EIO;
		close(fd);
		errno = saved_errno;
		return NULL;
	}
	if (!S_ISREG(st.st_mode) || st.st_uid != geteuid() || st.st_nlink != 1) {
		saved_errno = !S_ISREG(st.st_mode) ? EINVAL :
			(st.st_uid != geteuid() ? EPERM : EMLINK);
		close(fd);
		errno = saved_errno;
		return NULL;
	}
	if (fchmod(fd, private_mode) != 0) {
		saved_errno = errno ? errno : EIO;
		close(fd);
		errno = saved_errno;
		return NULL;
	}
	if (ftruncate(fd, 0) != 0) {
		saved_errno = errno ? errno : EIO;
		close(fd);
		errno = saved_errno;
		return NULL;
	}

	fp = fdopen(fd, "w");
	if (!fp) {
		saved_errno = errno ? errno : EIO;
		close(fd);
		errno = saved_errno;
		return NULL;
	}

	return fp;
}

static int
ppp_options_file_finish(FILE *fp, const char *path)
{
	int failed = 0, saved_errno = 0;

	if (!fp) {
		errno = EINVAL;
		return -1;
	}
	if (fflush(fp) != 0) {
		failed = 1;
		saved_errno = errno ? errno : EIO;
	}
	if (ferror(fp) && !failed) {
		failed = 1;
		saved_errno = errno ? errno : EIO;
	}
	if (fclose(fp) != 0) {
		if (!saved_errno)
			saved_errno = errno ? errno : EIO;
		failed = 1;
	}
	if (failed) {
		if (path && path[0])
			unlink(path);
		errno = saved_errno ? saved_errno : EIO;
		return -1;
	}

	return 0;
}

#endif /* PADAVAN_PPP_OPTIONS_FILE_H */
