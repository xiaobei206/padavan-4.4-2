#define _GNU_SOURCE
#include "../mgr/ipc_transport.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int test_validation_and_buffer(void)
{
    char *buffer = NULL;
    int capacity = 0;
    int ok = 1;

    ok = ok && skipdb_ipc_valid_protocol_field("alpha", 0, 16);
    ok = ok && skipdb_ipc_valid_protocol_field("", 1, 0);
    ok = ok && !skipdb_ipc_valid_protocol_field("", 0, 16);
    ok = ok && !skipdb_ipc_valid_protocol_field("bad key", 0, 16);
    ok = ok && !skipdb_ipc_valid_protocol_field("bad\tkey", 0, 16);
    ok = ok && !skipdb_ipc_valid_protocol_field("abcd", 0, 3);
    ok = ok && !skipdb_ipc_valid_protocol_field(NULL, 1, 16);
    if (!ok)
        return -1;

    if (skipdb_ipc_ensure_buffer(&buffer, &capacity, 3, 4, 8) != 0 ||
        capacity != 4 || NULL == buffer)
        goto fail;
    if (skipdb_ipc_ensure_buffer(&buffer, &capacity, 4, 4, 8) != 0 ||
        capacity != 4)
        goto fail;
    if (skipdb_ipc_ensure_buffer(&buffer, &capacity, 6, 4, 8) != 0 ||
        capacity != 6)
        goto fail;
    if (skipdb_ipc_ensure_buffer(&buffer, &capacity, 9, 4, 8) == 0)
        goto fail;
    free(buffer);
    return 0;

fail:
    free(buffer);
    fprintf(stderr, "IPC transport validation/buffer test failed\n");
    return -1;
}

static int test_unix_connect(void)
{
    char path[] = "/tmp/skipdb-ipc-transport-XXXXXX";
    char long_path[sizeof(((struct sockaddr_un *)0)->sun_path) + 1];
    struct sockaddr_un address;
    int seed = -1, listener = -1, client = -1, accepted = -1;
    socklen_t address_len;

    errno = 0;
    if (skipdb_ipc_connect_unix(NULL) != -1 || errno != EINVAL)
        goto fail;
    errno = 0;
    if (skipdb_ipc_connect_unix("") != -1 || errno != EINVAL)
        goto fail;
    memset(long_path, 'x', sizeof(long_path));
    long_path[sizeof(long_path) - 1] = '\0';
    errno = 0;
    if (skipdb_ipc_connect_unix(long_path) != -1 || errno != ENAMETOOLONG)
        goto fail;

    seed = mkstemp(path);
    if (seed < 0)
        goto fail;
    close(seed);
    seed = -1;
    if (unlink(path) != 0)
        goto fail;

    listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0)
        goto fail;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path, strlen(path) + 1);
    address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
        strlen(path) + 1);
    if (bind(listener, (struct sockaddr *)&address, address_len) != 0 ||
        listen(listener, 1) != 0)
        goto fail;

    client = skipdb_ipc_connect_unix(path);
    if (client < 0)
        goto fail;
    {
        int flags = fcntl(client, F_GETFD);
        if (flags < 0 || (flags & FD_CLOEXEC) == 0) {
            fprintf(stderr, "Unix socket descriptor is not close-on-exec\\n");
            goto fail;
        }
    }
    accepted = accept(listener, NULL, NULL);
    if (accepted < 0)
        goto fail;

    close(accepted);
    close(client);
    close(listener);
    unlink(path);
    return 0;

fail:
    if (seed >= 0)
        close(seed);
    if (accepted >= 0)
        close(accepted);
    if (client >= 0)
        close(client);
    if (listener >= 0)
        close(listener);
    unlink(path);
    fprintf(stderr, "IPC transport Unix-socket test failed\n");
    return -1;
}

static int test_nonblocking_and_write(void)
{
    int sockets[2] = {-1, -1};
    int flags;
    char received[8];
    static const char payload[] = "payload";
    ssize_t n;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;
    if (skipdb_ipc_set_nonblock(sockets[0]) != 0)
        goto fail;
    flags = fcntl(sockets[0], F_GETFL);
    if (flags < 0 || !(flags & O_NONBLOCK))
        goto fail;
    if (skipdb_ipc_write_exact(sockets[0], NULL, 0, 50) != 0)
        goto fail;
    if (skipdb_ipc_write_exact(sockets[0], payload,
        (int)(sizeof(payload) - 1), 50) != 0)
        goto fail;
    n = recv(sockets[1], received, sizeof(payload) - 1, 0);
    if (n != (ssize_t)(sizeof(payload) - 1) ||
        memcmp(received, payload, sizeof(payload) - 1) != 0)
        goto fail;

    close(sockets[0]);
    close(sockets[1]);
    return 0;

fail:
    if (sockets[0] >= 0)
        close(sockets[0]);
    if (sockets[1] >= 0)
        close(sockets[1]);
    fprintf(stderr, "IPC transport nonblocking/write test failed\n");
    return -1;
}

static int test_read_success_eof_and_timeout(void)
{
    int sockets[2] = {-1, -1};
    int received = -1, result;
    char buffer[8];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;
    if (skipdb_ipc_set_nonblock(sockets[0]) != 0 ||
        write(sockets[1], "hello", 5) != 5)
        goto fail;
    result = skipdb_ipc_read_exact(sockets[0], buffer, 5, 50, &received);
    if (result != 0 || received != 5 || memcmp(buffer, "hello", 5) != 0)
        goto fail;
    close(sockets[0]);
    close(sockets[1]);
    sockets[0] = sockets[1] = -1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;
    if (skipdb_ipc_set_nonblock(sockets[0]) != 0 ||
        write(sockets[1], "ab", 2) != 2 ||
        shutdown(sockets[1], SHUT_WR) != 0)
        goto fail;
    received = -1;
    result = skipdb_ipc_read_exact(sockets[0], buffer, 3, 50, &received);
    if (result != -1 || received != 2 || memcmp(buffer, "ab", 2) != 0)
        goto fail;
    close(sockets[0]);
    close(sockets[1]);
    sockets[0] = sockets[1] = -1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;
    if (skipdb_ipc_set_nonblock(sockets[0]) != 0)
        goto fail;
    received = -1;
    result = skipdb_ipc_read_exact(sockets[0], buffer, 1, 10, &received);
    if (result != -3 || received != 0) {
        fprintf(stderr, "IPC transport timeout test failed: result=%d received=%d\n",
            result, received);
        goto fail;
    }
    close(sockets[0]);
    close(sockets[1]);
    return 0;

fail:
    if (sockets[0] >= 0)
        close(sockets[0]);
    if (sockets[1] >= 0)
        close(sockets[1]);
    fprintf(stderr, "IPC transport exact-read/partial-EOF test failed\n");
    return -1;
}

int main(void)
{
    if (test_validation_and_buffer() != 0 ||
        test_unix_connect() != 0 ||
        test_nonblocking_and_write() != 0 ||
        test_read_success_eof_and_timeout() != 0)
        return EXIT_FAILURE;
    puts("SkipDB IPC transport tests passed.");
    return EXIT_SUCCESS;
}
