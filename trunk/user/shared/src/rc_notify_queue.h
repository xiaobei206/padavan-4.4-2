#ifndef RC_NOTIFY_QUEUE_H
#define RC_NOTIFY_QUEUE_H
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* The PID 1 notification queue is a privileged command queue. */
static inline int
rc_notify_event_name_is_safe(const char *name)
{
	const unsigned char *p;
	size_t len = 0;
	if (!name || !*name || !strcmp(name, ".") || !strcmp(name, ".."))
		return 0;
	for (p = (const unsigned char *)name; *p; ++p) {
		if (++len > 128)
			return 0;
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.'))
			return 0;
	}
	return 1;
}

static inline int
rc_notify_queue_open_dir(const char *path, uid_t owner)
{
	struct stat st;
	int fd;
	if (!path || !*path) {
		errno = EINVAL;
		return -1;
	}
	fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) != 0) {
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return -1;
	}
	if (!S_ISDIR(st.st_mode) || st.st_uid != owner ||
	    (st.st_mode & 0777) != 0700) {
		close(fd);
		errno = EPERM;
		return -1;
	}
	return fd;
}

static inline int
rc_notify_queue_marker_is_safe_at(int dir_fd, const char *name, uid_t owner)
{
	struct stat st;
	int fd;
	if (!rc_notify_event_name_is_safe(name)) {
		errno = EINVAL;
		return -1;
	}
	fd = openat(dir_fd, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st) != 0) {
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return -1;
	}
	if (!S_ISREG(st.st_mode) || st.st_uid != owner || st.st_nlink != 1 ||
	    (st.st_mode & 0777) != 0600) {
		close(fd);
		errno = EPERM;
		return -1;
	}
	if (close(fd) != 0)
		return -1;
	return 0;
}

/* Return 1 when created, 0 for an existing safe marker, or -1 on failure. */
static inline int
rc_notify_queue_touch_at(int dir_fd, const char *name, uid_t owner)
{
	struct stat st;
	int fd, saved_errno;
	if (!rc_notify_event_name_is_safe(name)) {
		errno = EINVAL;
		return -1;
	}
	fd = openat(dir_fd, name,
		    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_NONBLOCK, 0600);
	if (fd < 0) {
		if (errno == EEXIST &&
		    rc_notify_queue_marker_is_safe_at(dir_fd, name, owner) == 0)
			return 0;
		return -1;
	}
	if (fchmod(fd, 0600) != 0) {
		saved_errno = errno;
		close(fd);
		unlinkat(dir_fd, name, 0);
		errno = saved_errno;
		return -1;
	}
	if (fstat(fd, &st) != 0) {
		saved_errno = errno;
		close(fd);
		unlinkat(dir_fd, name, 0);
		errno = saved_errno;
		return -1;
	}
	if (!S_ISREG(st.st_mode) || st.st_uid != owner || st.st_nlink != 1 ||
	    (st.st_mode & 0777) != 0600) {
		close(fd);
		unlinkat(dir_fd, name, 0);
		errno = EPERM;
		return -1;
	}
	if (close(fd) != 0) {
		saved_errno = errno;
		unlinkat(dir_fd, name, 0);
		errno = saved_errno;
		return -1;
	}
	return 1;
}
#endif /* RC_NOTIFY_QUEUE_H */
