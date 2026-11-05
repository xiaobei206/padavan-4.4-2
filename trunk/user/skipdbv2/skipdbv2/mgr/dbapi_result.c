#include <stddef.h>
#include <string.h>

#include "dbapi_internal.h"
#include "ipc_transport.h"
#include "ipc_protocol.h"

static int read_util(dbclient* client, int len, unsigned int delay) {
    if (NULL == client || client->remote_fd < 0 || len <= 0 ||
        len > READ_MAX || dbapi_check_buf(client, len) != 0)
        return -4;
    return skipdb_ipc_read_exact(client->remote_fd, client->buf, len,
        delay, &client->buf_pos);
}

int dbapi_ignore_result(dbclient *client,
    const char *command, const char *key)
{
    int n1, n2;
    skipdb_ipc_frame_view frame;
    size_t command_len, key_len;

    if (NULL == client || NULL == command || NULL == key)
        return -1;
    n1 = read_util(client, HEADER_PREFIX, 110);
    if (n1 < 0)
        return n1;
    if (0 != memcmp(client->buf, MAGIC, MAGIC_LEN))
        return -3;
    client->buf[HEADER_PREFIX - 1] = '\0';
    if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
        n2 <= 0 || n2 > READ_MAX)
        return -4;
    n1 = read_util(client, n2, 110);
    if (n1 < 0)
        return n1;
    client->buf[n2] = '\0';
    if (skipdb_ipc_parse_envelope(client->buf, (size_t)n2, &frame) != 0)
        return -4;

    command_len = strlen(command);
    key_len = strlen(key);
    if (frame.command_len == 9 &&
        memcmp(frame.command, "__error__", 9) == 0) {
        if (frame.key_len != command_len ||
            memcmp(frame.key, command, command_len) != 0)
            return -4;
        return -5;
    }
    if (frame.command_len != command_len ||
        memcmp(frame.command, command, command_len) != 0 ||
        frame.key_len != key_len ||
        memcmp(frame.key, key, key_len) != 0)
        return -4;

    if (strcmp(command, "set") == 0 || strcmp(command, "ram") == 0) {
        if (frame.payload_len != 3 ||
            memcmp(frame.payload, "ok\n", 3) != 0)
            return -5;
    }
    return 0;
}


int dbapi_parse_list_result(dbclient *client, char* prefix, void* o, fn_db_parse fn) {
    int n1, n2;
    skipdb_ipc_frame_view frame;
    size_t value_len;

    for (;;) {
        n1 = read_util(client, HEADER_PREFIX, 110);
        if (n1 < 0)
            return n1;
        if (0 != memcmp(client->buf, MAGIC, MAGIC_LEN))
            return -3;
        client->buf[HEADER_PREFIX - 1] = '\0';
        if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
            n2 <= 0 || n2 > READ_MAX)
            return -4;
        n1 = read_util(client, n2, 510);
        if (n1 < 0)
            return n1;
        client->buf[n2] = '\0';
        if (skipdb_ipc_parse_envelope(client->buf, (size_t)n2, &frame) != 0)
            return -4;

        if (frame.command_len == 9 &&
            memcmp(frame.command, "__error__", 9) == 0)
            return (frame.key_len == 4 && memcmp(frame.key, "list", 4) == 0)
                ? -5 : -4;

        if (frame.command_len == 7 &&
            memcmp(frame.command, "__end__", 7) == 0) {
            if (frame.key_len == 4 && memcmp(frame.key, "list", 4) == 0 &&
                frame.payload_len == 8 &&
                memcmp(frame.payload, "__end__\n", 8) == 0)
                break;
            return -4;
        }
        if (frame.command_len != 4 ||
            memcmp(frame.command, "list", 4) != 0)
            return -4;

        frame.key[frame.key_len] = '\0';
        value_len = frame.payload_len;
        if (value_len > 0) {
            if (frame.payload[value_len - 1] == '\0' ||
                frame.payload[value_len - 1] == '\n')
                --value_len;
            else
                return -4;
        }
        if (memchr(frame.payload, '\0', value_len) != NULL)
            return -4;
        frame.payload[value_len] = '\0';
        if (0 != (*fn)(client, o, prefix, frame.key, frame.payload))
            return -5;
    }
    return 0;
}


