#include "../mgr/ipc_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static int fail(const char *message)
{
    fprintf(stderr, "ipc_io_test: %s\n", message);
    return EXIT_FAILURE;
}

int main(void)
{
    unsigned long long now;
    int sockets[2] = {-1, -1};
    char sent = 'x';
    char received = '\0';

    if (skipdb_monotonic_milliseconds(NULL) != -1)
        return fail("NULL monotonic output was accepted");
    if (skipdb_monotonic_milliseconds(&now) != 0)
        return fail("monotonic clock read failed");
    if (skipdb_wait_socket_until(-1, POLLIN, now) != -1)
        return fail("negative descriptor was accepted");
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return fail("socketpair creation failed");

    if (skipdb_wait_socket_until(sockets[0], POLLIN, now) != 0) {
        close(sockets[0]);
        close(sockets[1]);
        return fail("expired deadline did not return timeout");
    }
    if (skipdb_monotonic_milliseconds(&now) != 0 ||
        skipdb_wait_socket_until(sockets[0], POLLIN, now + 10ULL) != 0) {
        close(sockets[0]);
        close(sockets[1]);
        return fail("idle socket did not time out");
    }
    if (write(sockets[1], &sent, sizeof(sent)) != (ssize_t)sizeof(sent)) {
        close(sockets[0]);
        close(sockets[1]);
        return fail("socket write failed");
    }
    if (skipdb_monotonic_milliseconds(&now) != 0 ||
        skipdb_wait_socket_until(sockets[0], POLLIN, now + 1000ULL) != 1) {
        close(sockets[0]);
        close(sockets[1]);
        return fail("readable socket was not reported");
    }
    if (read(sockets[0], &received, sizeof(received)) != (ssize_t)sizeof(received) ||
        received != sent) {
        close(sockets[0]);
        close(sockets[1]);
        return fail("readiness check consumed or corrupted socket data");
    }
    if (skipdb_monotonic_milliseconds(&now) != 0 ||
        skipdb_wait_socket_until(sockets[0], POLLOUT, now + 1000ULL) != 1) {
        close(sockets[0]);
        close(sockets[1]);
        return fail("writable socket was not reported");
    }
    close(sockets[0]);
    close(sockets[1]);

    puts("SkipDB shared IPC I/O tests passed");
    return EXIT_SUCCESS;
}
