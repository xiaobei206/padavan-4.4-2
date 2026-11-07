#ifndef HTTPD_UPLOAD_FILE_H
#define HTTPD_UPLOAD_FILE_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
/* O_EXCL with O_CREAT still rejects an existing symlink on supported POSIX targets. */
#define O_NOFOLLOW 0
#endif

/* Create, never follow or truncate, a predictable upload destination. */
static FILE *
httpd_upload_open_exclusive(const char *path)
{
    int fd, saved_errno;
    FILE *stream;

    if (!path || !*path) {
        errno = EINVAL;
        return NULL;
    }

    fd = open(path, O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW,
               S_IRUSR | S_IWUSR);
    if (fd < 0)
        return NULL;

    if (fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
        saved_errno = errno;
        close(fd);
        unlink(path);
        errno = saved_errno;
        return NULL;
    }

    stream = fdopen(fd, "w+");
    if (!stream) {
        saved_errno = errno;
        close(fd);
        unlink(path);
        errno = saved_errno;
        return NULL;
    }

    return stream;
}

#endif /* HTTPD_UPLOAD_FILE_H */
