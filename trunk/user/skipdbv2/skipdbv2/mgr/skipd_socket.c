#include "skipd_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* Secure UNIX socket setup and single-instance lock ownership. */
int setnonblock(int fd) {
    int flags, descriptor_flags;

    if (fd < 0)
        return -1;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;
    descriptor_flags = fcntl(fd, F_GETFD);
    if (descriptor_flags < 0)
        return -1;
    return fcntl(fd, F_SETFD, descriptor_flags | FD_CLOEXEC);
}

static int socket_instance_lock_fd = -1;

int unix_socket_init(struct sockaddr_un* socket_un, char* sock_path,
    const char *lock_dir, int max_queue) {
    int fd = -1, lock_fd = -1, probe_fd = -1, saved_errno;
    size_t path_len;
    char lock_path[SK_PATH_MAX];
    struct stat existing, current;
    struct sockaddr_un probe_addr;
    socklen_t address_len;
    int n;

    (void)max_queue;
    if (NULL == socket_un || NULL == sock_path ||
        NULL == lock_dir || '\0' == *lock_dir) {
        errno = EINVAL;
        return -1;
    }
    if (socket_instance_lock_fd >= 0) {
        errno = EALREADY;
        return -1;
    }
    path_len = strnlen(sock_path, sizeof(socket_un->sun_path));
    if (path_len == 0 || path_len >= sizeof(socket_un->sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    /* Keep the instance lock in the validated, non-writable DB directory.
     * A user must not be able to pre-create a predictable /tmp lock and
     * permanently prevent the root-owned service from starting. */
    n = snprintf(lock_path, sizeof(lock_path), "%s/.skipd.sock.lock", lock_dir);
    if (n < 0 || (size_t)n >= sizeof(lock_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
#ifdef O_NOFOLLOW
    lock_fd = open(lock_path, O_RDWR | O_CREAT | O_NOFOLLOW |
        O_NONBLOCK | O_CLOEXEC, 0600);
#else
    errno = ENOTSUP;
    return -1;
#endif
    if (lock_fd < 0)
        return -1;
    if (fstat(lock_fd, &current) != 0)
        goto fail;
    if (!S_ISREG(current.st_mode) || current.st_uid != geteuid() ||
        current.st_nlink != 1) {
        errno = EPERM;
        goto fail;
    }
    if (fchmod(lock_fd, 0600) != 0)
        goto fail;
    if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
        saved_errno = errno;
        close(lock_fd);
        errno = saved_errno;
        return -1;
    }

    /* A running instance keeps this advisory lock for its whole lifetime. */
    if (lstat(sock_path, &existing) == 0) {
        if (!S_ISSOCK(existing.st_mode) || existing.st_uid != geteuid()) {
            errno = EEXIST;
            goto fail;
        }

        /* Older instances may not hold the lock yet; probe before cleanup. */
        probe_fd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (probe_fd < 0)
            goto fail;
        if (setnonblock(probe_fd) < 0)
            goto fail;
        memset(&probe_addr, 0, sizeof(probe_addr));
        probe_addr.sun_family = AF_UNIX;
        memcpy(probe_addr.sun_path, sock_path, path_len + 1);
        address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
            path_len + 1);
        if (connect(probe_fd, (struct sockaddr *)&probe_addr, address_len) == 0) {
            errno = EADDRINUSE;
            goto fail;
        }
        saved_errno = errno;
        close(probe_fd);
        probe_fd = -1;
        if (saved_errno != ECONNREFUSED && saved_errno != ENOENT) {
            errno = saved_errno;
            goto fail;
        }

        /* Do not unlink a pathname replaced while the liveness probe ran. */
        if (lstat(sock_path, &current) != 0) {
            if (errno != ENOENT)
                goto fail;
        } else {
            if (current.st_dev != existing.st_dev ||
                current.st_ino != existing.st_ino ||
                !S_ISSOCK(current.st_mode) ||
                current.st_uid != geteuid()) {
                errno = EADDRINUSE;
                goto fail;
            }
            if (unlink(sock_path) != 0)
                goto fail;
        }
    } else if (errno != ENOENT) {
        goto fail;
    }

    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        goto fail;
    if (setnonblock(fd) < 0)
        goto fail;

    memset(socket_un, 0, sizeof(*socket_un));
    socket_un->sun_family = AF_UNIX;
    memcpy(socket_un->sun_path, sock_path, path_len + 1);
    socket_instance_lock_fd = lock_fd;
    return fd;

fail:
    saved_errno = errno;
    if (probe_fd >= 0)
        close(probe_fd);
    if (fd >= 0)
        close(fd);
    if (lock_fd >= 0)
        close(lock_fd);
    errno = saved_errno;
    return -1;
}

void unix_socket_release_instance_lock(void)
{
    if (socket_instance_lock_fd >= 0) {
        close(socket_instance_lock_fd);
        socket_instance_lock_fd = -1;
    }
}
