#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc_io.h"
#include "ipc_transport.h"

/* Set close-on-exec even when SOCK_CLOEXEC is unavailable at runtime. */
static int skipdb_ipc_set_cloexec_(int fd)
{
    int flags = fcntl(fd, F_GETFD);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

int skipdb_ipc_connect_unix(const char *sock_path)
{
    int fd, saved_errno;
    size_t path_len;
    socklen_t address_len;
    struct sockaddr_un remote;

    if (NULL == sock_path || '\0' == *sock_path) {
        errno = EINVAL;
        return -1;
    }
    path_len = strlen(sock_path);
    if (path_len >= sizeof(remote.sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
#ifdef SOCK_CLOEXEC
    fd = socket(PF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0 && errno == EINVAL)
        fd = socket(PF_UNIX, SOCK_STREAM, 0);
#else
    fd = socket(PF_UNIX, SOCK_STREAM, 0);
#endif
    if (fd < 0)
        return -1;
    if (skipdb_ipc_set_cloexec_(fd) < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    memcpy(remote.sun_path, sock_path, path_len + 1);
    address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
        path_len + 1);
    if (connect(fd, (struct sockaddr *)&remote, address_len) < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
}

int skipdb_ipc_set_nonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int skipdb_ipc_ensure_buffer(char **buffer, int *capacity, int requested,
    int minimum_capacity, int maximum_capacity)
{
    int required;
    char *new_buffer;

    if (NULL == buffer || NULL == capacity || requested < 0 ||
        minimum_capacity < 0 || maximum_capacity < 0 ||
        requested > maximum_capacity)
        return -1;

    required = requested > minimum_capacity ? requested : minimum_capacity;
    if (required < 0 || required > maximum_capacity || required == INT_MAX)
        return -1;
    if (*buffer != NULL && *capacity >= required)
        return 0;

    new_buffer = (char *)realloc(*buffer, (size_t)required + 1);
    if (NULL == new_buffer)
        return -1;
    *buffer = new_buffer;
    *capacity = required;
    return 0;
}

int skipdb_ipc_valid_protocol_field(const char *text, int allow_empty,
    size_t max_length)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t length;

    if (NULL == text)
        return 0;
    length = strlen(text);
    if ((!allow_empty && length == 0) || length > max_length)
        return 0;
    for (; *p; ++p) {
        if (*p <= 0x20 || *p == 0x7f)
            return 0;
    }
    return 1;
}

int skipdb_ipc_read_exact(int fd, char *buffer, int length,
    unsigned int timeout_ms, int *received)
{
    int n, ready;
    unsigned long long now, timeout;

    if (received != NULL)
        *received = 0;
    if (fd < 0 || NULL == buffer || NULL == received || length < 0)
        return -4;
    if (length == 0)
        return 0;
    if (skipdb_monotonic_milliseconds(&now) != 0 ||
        ULLONG_MAX - now < timeout_ms)
        return -2;
    timeout = now + timeout_ms;

    for (;;) {
        n = (int)recv(fd, buffer + *received,
            (size_t)(length - *received), 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                ready = skipdb_wait_socket_until(fd, POLLIN, timeout);
                if (ready < 0)
                    return -2;
                if (ready == 0)
                    break;
                continue;
            }
            return -2;
        }
        if (n == 0)
            return -1;
        *received += n;
        if (*received == length)
            return 0;
    }
    return -3;
}

int skipdb_ipc_write_exact(int fd, const char *buffer, int length,
    unsigned int timeout_ms)
{
    int n, written = 0, ready;
    unsigned long long now, timeout;

    if (fd < 0 || length < 0 || (length > 0 && NULL == buffer))
        return -1;
    if (length == 0)
        return 0;
    if (skipdb_monotonic_milliseconds(&now) != 0 ||
        ULLONG_MAX - now < timeout_ms)
        return -1;
    timeout = now + timeout_ms;

    while (written < length) {
#ifdef MSG_NOSIGNAL
        n = (int)send(fd, buffer + written, (size_t)(length - written),
            MSG_NOSIGNAL);
#else
        n = (int)send(fd, buffer + written, (size_t)(length - written), 0);
#endif
        if (n > 0) {
            written += n;
            continue;
        }
        if (n == 0)
            return -1;
        if (errno == EINTR)
            continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            return -1;

        ready = skipdb_wait_socket_until(fd, POLLOUT, timeout);
        if (ready <= 0)
            break;
    }
    return written == length ? 0 : -1;
}
