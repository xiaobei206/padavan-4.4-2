#include "../mgr/dbapi.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static int write_all(int fd, const void *data, size_t length)
{
    const char *p = (const char *)data;
    size_t written = 0;

    while (written < length) {
        ssize_t n = write(fd, p + written, length - written);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        written += (size_t)n;
    }
    return 0;
}

static int write_frame(int fd, const void *body, size_t body_len)
{
    char header[HEADER_PREFIX];
    int header_len;

    if (body_len == 0 || body_len > READ_MAX)
        return -1;
    header_len = snprintf(header, sizeof(header), "%s%07lu ",
        MAGIC, (unsigned long)body_len);
    if (header_len != HEADER_PREFIX ||
        write_all(fd, header, HEADER_PREFIX) != 0 ||
        write_all(fd, body, body_len) != 0)
        return -1;
    return 0;
}

typedef struct {
    int calls;
    int failed;
} capture_state;

static int capture_pair(dbclient *client, void *opaque, char *prefix,
    char *key, char *value)
{
    capture_state *state = (capture_state *)opaque;
    (void)client;

    if (NULL == state || NULL == prefix || strcmp(prefix, "alpha") != 0 ||
        NULL == key || strcmp(key, "alpha") != 0 ||
        NULL == value || strcmp(value, "__end__\n") != 0) {
        if (state)
            state->failed = 1;
        return -1;
    }
    ++state->calls;
    return 0;
}

int main(void)
{
    static const char list_value_is_end[] = "list alpha __end__\n\0";
    static const char list_end_frame[] = "__end__ list __end__\n";
    int sockets[2] = {-1, -1};
    int result = -1;
    dbclient client;
    capture_state state;

    memset(&client, 0, sizeof(client));
    memset(&state, 0, sizeof(state));
    client.remote_fd = -1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        goto done;
    client.remote_fd = sockets[0];
    if (setnonblock(client.remote_fd) != 0 ||
        write_frame(sockets[1], list_value_is_end,
            sizeof(list_value_is_end) - 1) != 0 ||
        write_frame(sockets[1], list_end_frame,
            sizeof(list_end_frame) - 1) != 0)
        goto done;

    result = parse_list_result(&client, "alpha", &state, capture_pair);
    if (result != 0 || state.failed || state.calls != 1) {
        fprintf(stderr,
            "dbapi list parser failed: result=%d, failed=%d, calls=%d\n",
            result, state.failed, state.calls);
        result = -1;
        goto done;
    }

    puts("dbapi protocol tests passed");
    result = 0;

done:
    free(client.buf);
    if (client.remote_fd >= 0)
        close(client.remote_fd);
    if (sockets[1] >= 0)
        close(sockets[1]);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
