#ifndef SKIPDB_IPC_IO_H
#define SKIPDB_IPC_IO_H

#include <errno.h>
#include <stddef.h>
#include <limits.h>
#include <poll.h>
#include <time.h>

/*
 * Shared monotonic-deadline primitives for the SkipDB Unix-socket clients.
 * Keep timeout accounting independent of wall-clock adjustments and retry
 * interrupted poll calls without extending the caller's absolute deadline.
 */
static int skipdb_monotonic_milliseconds(unsigned long long *milliseconds)
{
    struct timespec ts;

    if (NULL == milliseconds ||
        clock_gettime(CLOCK_MONOTONIC, &ts) != 0 ||
        ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000L)
        return -1;
    if ((unsigned long long)ts.tv_sec > (ULLONG_MAX - 999ULL) / 1000ULL)
        return -1;

    *milliseconds = (unsigned long long)ts.tv_sec * 1000ULL +
        (unsigned long long)ts.tv_nsec / 1000000ULL;
    return 0;
}

static int skipdb_wait_socket_until(int fd, short events,
    unsigned long long deadline)
{
    struct pollfd pfd;
    unsigned long long now, remaining;
    int poll_timeout, result;

    if (fd < 0)
        return -1;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;

    for (;;) {
        if (skipdb_monotonic_milliseconds(&now) != 0)
            return -1;
        if (now >= deadline)
            return 0;
        remaining = deadline - now;
        poll_timeout = remaining > INT_MAX ? INT_MAX : (int)remaining;
        pfd.revents = 0;
        result = poll(&pfd, 1, poll_timeout);
        if (result < 0 && errno == EINTR)
            continue;
        if (result < 0)
            return -1;
        if (result == 0)
            continue; /* INT_MAX poll slices may end before the deadline. */
        return 1;
    }
}

#endif
