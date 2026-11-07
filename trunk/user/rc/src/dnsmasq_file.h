#ifndef PADAVAN_DNSMASQ_FILE_H
#define PADAVAN_DNSMASQ_FILE_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Build a complete replacement beside its destination; never truncate a
 * predictable shared-/tmp pathname or follow a pre-existing destination link. */
static FILE *
dnsmasq_file_open(char *path)
{
	int fd;
	FILE *fp;
	int saved_errno;

	if (!path || !path[0]) {
		errno = EINVAL;
		return NULL;
	}

	fd = mkstemp(path);
	if (fd < 0) {
		path[0] = '\0';
		return NULL;
	}

	fp = fdopen(fd, "w");
	if (!fp) {
		saved_errno = errno;
		close(fd);
		unlink(path);
		path[0] = '\0';
		errno = saved_errno;
		return NULL;
	}

	return fp;
}

static void
dnsmasq_file_discard(char *path)
{
	if (path && path[0]) {
		unlink(path);
		path[0] = '\0';
	}
}

/* Flush/close the staged file before atomically replacing the published path.
 * rename() replaces a destination symlink itself; it never follows its target. */
static int
dnsmasq_file_publish(FILE *fp, char *path, const char *destination, mode_t mode)
{
	int failed = 0;
	int saved_errno = 0;

	if (!fp || !path || !path[0] || !destination || !destination[0]) {
		if (fp)
			fclose(fp);
		dnsmasq_file_discard(path);
		errno = EINVAL;
		return -1;
	}

	errno = 0;
	if (fflush(fp) != 0) {
		failed = 1;
		saved_errno = errno ? errno : EIO;
	}
	if (ferror(fp) && !failed) {
		failed = 1;
		saved_errno = errno ? errno : EIO;
	}
	if (!failed && fchmod(fileno(fp), mode) != 0) {
		failed = 1;
		saved_errno = errno ? errno : EIO;
	}
	if (fclose(fp) != 0) {
		if (!saved_errno)
			saved_errno = errno ? errno : EIO;
		failed = 1;
	}

	if (failed) {
		dnsmasq_file_discard(path);
		errno = saved_errno ? saved_errno : EIO;
		return -1;
	}

	if (rename(path, destination) != 0) {
		saved_errno = errno ? errno : EIO;
		dnsmasq_file_discard(path);
		errno = saved_errno;
		return -1;
	}

	path[0] = '\0';
	return 0;
}

/* Keep DHCP-derived domain names inside the single dnsmasq server= value.
 * The DHCP option is network-controlled; control bytes, slashes and whitespace
 * could otherwise terminate this directive and inject another server rule. */
static int
dnsmasq_valid_server_domain(const char *domain)
{
	size_t length, label_len, i;
	unsigned char c;

	if (!domain)
		return 0;
	length = strlen(domain);
	if (length == 0 || length > 254)
		return 0;
	/* A single final dot is the conventional absolute DNS-name form. */
	if (domain[length - 1] == '.')
		length--;
	if (length == 0 || length > 253)
		return 0;

	label_len = 0;
	for (i = 0; i < length; i++) {
		c = (unsigned char)domain[i];
		if (c == '.') {
			if (label_len == 0 || domain[i - 1] == '-')
				return 0;
			label_len = 0;
			continue;
		}
		if (!((c >= 'a' && c <= 'z') ||
		      (c >= 'A' && c <= 'Z') ||
		      (c >= '0' && c <= '9') || c == '-'))
			return 0;
		if (label_len == 0 && c == '-')
			return 0;
		label_len++;
		if (label_len > 63)
			return 0;
	}

	if (label_len == 0 || domain[length - 1] == '-')
		return 0;

	return 1;
}

/* Validate a last-known-good server file without following the final path. */
static int
dnsmasq_file_is_safe_regular(const char *path)
{
	struct stat st;

	if (!path || !path[0]) {
		errno = EINVAL;
		return -1;
	}
	if (lstat(path, &st) != 0)
		return -1;
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
	if (st.st_mode & (S_IWGRP | S_IWOTH)) {
		errno = EPERM;
		return -1;
	}

	return 0;
}

/* dnsmasq opens this predictable lease pathname as part of service startup.
 * Refuse links, non-regular files and multiply-linked files before changing
 * permissions; opening never truncates existing lease records. */
static int
dnsmasq_lease_file_ensure(const char *path)
{
	int fd;
	int saved_errno;
	struct stat st;
	mode_t lease_mode;

	if (!path || !path[0]) {
		errno = EINVAL;
		return -1;
	}

	fd = open(path, O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK,
		   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) != 0) {
		saved_errno = errno ? errno : EIO;
		close(fd);
		errno = saved_errno;
		return -1;
	}
	if (!S_ISREG(st.st_mode) || st.st_uid != geteuid() || st.st_nlink != 1) {
		close(fd);
		errno = !S_ISREG(st.st_mode) ? EINVAL :
			(st.st_uid != geteuid() ? EPERM : EMLINK);
		return -1;
	}
	/* Preserve a restrictive umask/mode while ensuring the owner can maintain leases. */
	lease_mode = (st.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) | S_IRUSR | S_IWUSR;
	if (fchmod(fd, lease_mode) != 0) {
		saved_errno = errno ? errno : EIO;
		close(fd);
		errno = saved_errno;
		return -1;
	}
	if (close(fd) != 0)
		return -1;

	return 0;
}

#endif /* PADAVAN_DNSMASQ_FILE_H */
