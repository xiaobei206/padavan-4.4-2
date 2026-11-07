#ifndef PADAVAN_MODEM_NODE_FILE_H
#define PADAVAN_MODEM_NODE_FILE_H

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * USB hotplug handlers run as root. Their metadata directory must never be
 * a symlink or a directory owned/writable by an untrusted local account.
 */
static int
modem_node_dir_prepare(const char *directory)
{
	struct stat st;

	if (!directory || directory[0] != '/')
		return -1;

	if (mkdir(directory, 0755) != 0 && errno != EEXIST)
		return -1;

	if (lstat(directory, &st) != 0 ||
	    !S_ISDIR(st.st_mode) || st.st_uid != geteuid())
		return -1;

	if ((st.st_mode & 0777) != 0755 && chmod(directory, 0755) != 0)
		return -1;

	if (lstat(directory, &st) != 0 ||
	    !S_ISDIR(st.st_mode) || st.st_uid != geteuid() ||
	    (st.st_mode & 0777) != 0755)
		return -1;

	return 0;
}

/* Kernel device names become leaf filenames, never relative paths. */
static int
modem_node_name_is_safe(const char *name)
{
	const unsigned char *p;
	size_t length = 0;

	if (!name || !*name ||
	    !((*name >= 'a' && *name <= 'z') ||
	      (*name >= 'A' && *name <= 'Z')))
		return 0;

	for (p = (const unsigned char *)name; *p; ++p) {
		if (++length > 63)
			return 0;
		if (!((*p >= 'a' && *p <= 'z') ||
		      (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') ||
		      *p == '_' || *p == '-' || *p == '.'))
			return 0;
	}

	return 1;
}

/*
 * Stage in the verified directory and rename atomically. A pre-existing
 * destination symlink is replaced as a directory entry; its target is never
 * opened or truncated.
 */
static int
modem_node_file_write(const char *directory, const char *name,
		      const char *contents)
{
	char path[PATH_MAX], staging[PATH_MAX];
	size_t length, offset;
	ssize_t written;
	int path_len, staging_len, fd;

	if (!modem_node_name_is_safe(name) || !contents ||
	    modem_node_dir_prepare(directory) != 0)
		return -1;

	path_len = snprintf(path, sizeof(path), "%s/%s", directory, name);
	staging_len = snprintf(staging, sizeof(staging),
			       "%s/.node.XXXXXX", directory);
	if (path_len < 0 || path_len >= (int)sizeof(path) ||
	    staging_len < 0 || staging_len >= (int)sizeof(staging))
		return -1;

	fd = mkstemp(staging);
	if (fd < 0)
		return -1;

	length = strlen(contents);
	offset = 0;
	while (offset < length) {
		written = write(fd, contents + offset, length - offset);
		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			goto fail;
		offset += (size_t)written;
	}

	if (fchmod(fd, 0644) != 0 || fsync(fd) != 0)
		goto fail;
	if (close(fd) != 0) {
		fd = -1;
		goto fail;
	}
	fd = -1;

	if (rename(staging, path) != 0)
		goto fail;

	return 0;

fail:
	if (fd >= 0)
		close(fd);
	unlink(staging);
	return -1;
}

static int
modem_node_file_remove(const char *directory, const char *name)
{
	char path[PATH_MAX];
	int path_len;

	if (!modem_node_name_is_safe(name) ||
	    modem_node_dir_prepare(directory) != 0)
		return -1;

	path_len = snprintf(path, sizeof(path), "%s/%s", directory, name);
	if (path_len < 0 || path_len >= (int)sizeof(path))
		return -1;

	if (unlink(path) == 0 || errno == ENOENT)
		return 0;

	return -1;
}

#endif /* PADAVAN_MODEM_NODE_FILE_H */
