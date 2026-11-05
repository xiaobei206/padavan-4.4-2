#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "skipd.h"
#include "ipc_protocol.h"
#include "dbus_internal.h"

/*
 * skipd frames every response body as:
 *     <command> <key> <raw-value>
 * The value is length-delimited and may contain embedded NUL bytes.
 */
static int read_response_payload(dbclient *client, unsigned int delay,
    size_t *payload_offset, int *body_len,
    size_t *key_offset, size_t *key_len, int *error_response)
{
    int n1, n2;
    size_t command_len;
    int is_error = 0;
    skipdb_ipc_frame_view frame;
    const char *magic = MAGIC;

    if (NULL == client || NULL == payload_offset || NULL == body_len)
        return -4;
    n1 = dbus_read_util(client, HEADER_PREFIX, delay);
    if (n1 < 0)
        return n1;
    if (0 != memcmp(client->buf, magic, MAGIC_LEN))
        return -3;
    client->buf[HEADER_PREFIX - 1] = '\0';
    if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
        n2 <= 0 || n2 > READ_MAX)
        return -4;
    n1 = dbus_read_util(client, n2, delay);
    if (n1 < 0)
        return n1;
    if (skipdb_ipc_parse_envelope(client->buf, (size_t)n2, &frame) != 0)
        return -4;
    command_len = strlen(client->command);
    if (command_len == 0)
        return -4;
    if (frame.command_len == 9 &&
        memcmp(frame.command, "__error__", 9) == 0)
        is_error = 1;
    else if (frame.command_len != command_len ||
        memcmp(frame.command, client->command, command_len) != 0)
        return -4;
    if (is_error &&
        (frame.key_len != command_len ||
         memcmp(frame.key, client->command, command_len) != 0))
        return -4;
    if (NULL != key_offset)
        *key_offset = (size_t)(frame.key - client->buf);
    if (NULL != key_len)
        *key_len = frame.key_len;
    if (NULL != error_response)
        *error_response = is_error;
    *payload_offset = (size_t)(frame.payload - client->buf);
    *body_len = n2;
    return 0;
}


int parse_common_result(dbclient *client) {
    int n1, body_len;
    size_t payload_offset, payload_len, key_offset, key_len;
    int success, error_response = 0;
    const char *payload;

    /* Script-backed commands can take longer than ordinary DB operations. */
    n1 = read_response_payload(client, 60000, &payload_offset, &body_len,
        &key_offset, &key_len, &error_response);
    if (n1 < 0)
        return n1;

    payload = client->buf + payload_offset;
    payload_len = (size_t)body_len - payload_offset;
    if (error_response) {
        if (payload_len > 0 &&
            fwrite(payload, 1, payload_len, stdout) != payload_len)
            return -5;
        return -5;
    }

    success = (payload_len == 3 &&
        memcmp(payload, "ok\n", 3) == 0) ||
        (payload_len == 9 &&
        memcmp(payload, "delay_ok\n", 9) == 0);

    /* Preserve the historical full command/key/response output. */
    if (body_len > 0 &&
        fwrite(client->buf, 1, (size_t)body_len, stdout) != (size_t)body_len)
        return -5;
    if (body_len == 0 || client->buf[body_len - 1] != '\n')
        putchar('\n');
    return success ? 0 : -5;
}

int parse_get_result(dbclient *client) {
    int n1, body_len;
    size_t payload_offset, payload_len, key_offset, key_len;
    int error_response = 0;
    const char *payload;

    n1 = read_response_payload(client, 510, &payload_offset, &body_len,
        &key_offset, &key_len, &error_response);
    if (n1 < 0)
        return n1;

    payload = client->buf + payload_offset;
    payload_len = (size_t)body_len - payload_offset;

    /* A dedicated response command reports a single-frame server error. */
    if (error_response) {
        if (payload_len > 0 &&
            fwrite(payload, 1, payload_len, stdout) != payload_len)
            return -5;
        return -5;
    }

    /*
     * Text Datums are stored with one trailing NUL. Preserve embedded NUL
     * bytes, but do not leak the storage terminator into terminal output.
     */
    if (payload_len > 0 && payload[payload_len - 1] == '\0')
        --payload_len;

    /* Print only the raw Datum value, not skipd's command/key envelope. */
    if (payload_len > 0 &&
        fwrite(payload, 1, payload_len, stdout) != payload_len)
        return -5;
    if (payload_len == 0 || payload[payload_len - 1] != '\n')
        putchar('\n');

    if ((!strcmp(client->command, "inc") ||
         !strcmp(client->command, "desc")) &&
        ((payload_len == 6 && memcmp(payload, "error\n", 6) == 0) ||
         (payload_len == 15 && memcmp(payload, "database error\n", 15) == 0) ||
         (payload_len == 17 && memcmp(payload, "integer overflow\n", 17) == 0)))
        return -5;

    if (!strcmp(client->command, "replace") &&
        payload_len == 15 && memcmp(payload, "database error\n", 15) == 0)
        return -5;

    return 0;
}

/*
 * List replies share the normal command/key/value envelope. The terminal
 * frame uses a distinct command token so a real value equal to "__end__\n"
 * cannot prematurely terminate the stream.
 *
 * Returns 1 for the terminal frame, 0 for a list item, or a negative error.
 */
static int split_list_frame(dbclient *client, int body_len,
    char **key, size_t *key_len, char **value, size_t *value_len)
{
    skipdb_ipc_frame_view frame;

    if (NULL == client || NULL == client->buf || body_len <= 0 ||
        NULL == key || NULL == key_len || NULL == value || NULL == value_len ||
        skipdb_ipc_parse_envelope(client->buf, (size_t)body_len, &frame) != 0)
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
            return 1;
        return -4;
    }
    if (frame.command_len != 4 ||
        memcmp(frame.command, "list", 4) != 0)
        return -4;
    frame.key[frame.key_len] = '\0';
    *key = frame.key;
    *key_len = frame.key_len;
    *value = frame.payload;
    *value_len = frame.payload_len;
    return 0;
}


int parse_list_result(dbclient *client) {
    int n1, n2, parsed;
    char *key, *value;
    size_t key_len, value_len;
    char *magic = MAGIC;

    for (;;) {
        n1 = dbus_read_util(client, HEADER_PREFIX, 110);
        if (n1 < 0)
            return n1;

        if (0 != memcmp(client->buf, magic, MAGIC_LEN))
            return -3;

        client->buf[HEADER_PREFIX - 1] = '\0';
        if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
            n2 <= 0 || n2 > READ_MAX)
            return -4;

        n1 = dbus_read_util(client, n2, 510);
        if (n1 < 0)
            return n1;
        client->buf[n2] = '\0';

        parsed = split_list_frame(client, n2, &key, &key_len,
            &value, &value_len);
        if (parsed < 0)
            return parsed;
        if (parsed == 1)
            break;

        /* SkipDB string Datums include one trailing NUL byte. */
        if (value_len > 0 && value[value_len - 1] == '\0')
            --value_len;

        if (fwrite(key, 1, key_len, stdout) != key_len ||
            putchar('=') == EOF ||
            (value_len > 0 &&
             fwrite(value, 1, value_len, stdout) != value_len))
            return -5;
        if (value_len == 0 || value[value_len - 1] != '\n')
            putchar('\n');
    }

    return 0;
}

static int valid_shell_identifier(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;

    if (NULL == p ||
        !((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
        return 0;
    for (++p; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_'))
            return 0;
    }
    return 1;
}

static void print_shell_quoted(const char *value, size_t length)
{
    size_t i;
    putchar('\'');
    for (i = 0; i < length; ++i) {
        if (value[i] == '\'')
            fputs("'\\''", stdout);
        else
            putchar((unsigned char)value[i]);
    }
    putchar('\'');
}

int parse_script_result(dbclient *client) {
    int n1, n2, parsed;
    char *key, *value;
    size_t key_len, value_len;
    char *magic = MAGIC;

    for (;;) {
        n1 = dbus_read_util(client, HEADER_PREFIX, 110);
        if (n1 < 0)
            return n1;

        if (0 != memcmp(client->buf, magic, MAGIC_LEN))
            return -3;

        client->buf[HEADER_PREFIX - 1] = '\0';
        if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
            n2 <= 0 || n2 > READ_MAX)
            return -4;

        n1 = dbus_read_util(client, n2, 510);
        if (n1 < 0)
            return n1;
        client->buf[n2] = '\0';

        parsed = split_list_frame(client, n2, &key, &key_len,
            &value, &value_len);
        if (parsed < 0)
            return parsed;
        if (parsed == 1)
            break;

        /* Empty Datums are valid; non-empty text carries one terminator. */
        if (value_len > 0) {
            if (value[value_len - 1] != '\0' &&
                value[value_len - 1] != '\n')
                return -4;
            --value_len;
        }
        if (memchr(value, '\0', value_len) != NULL ||
            !valid_shell_identifier(key))
            return -4;

        fputs("export ", stdout);
        if (fwrite(key, 1, key_len, stdout) != key_len)
            return -5;
        putchar('=');
        print_shell_quoted(value, value_len);
        putchar(';');
    }

    return 0;
}

