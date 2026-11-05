#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h> // fcntl
#include <unistd.h> // close
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <time.h>
#include <poll.h>
#include <limits.h> // [LONG|INT][MIN|MAX]
#include <stddef.h>
#include "dbapi_internal.h"
#include "ipc_transport.h"

//"list "
#define LIST_LEN 5


static int write_util(dbclient* client, int len, unsigned int delay) {
    if (NULL == client || NULL == client->buf || len < 0 ||
        len > client->buf_max || client->remote_fd < 0)
        return -1;
    return skipdb_ipc_write_exact(client->remote_fd, client->buf, len, delay);
}

//TODO better for code reused
static int create_client_fd(char* sock_path) {
    return skipdb_ipc_connect_unix(sock_path);
}

int dbapi_check_buf(dbclient* client, int len) {
    if (NULL == client)
        return -1;
    return skipdb_ipc_ensure_buffer(&client->buf, &client->buf_max,
        len, BUF_MAX, READ_MAX + HEADER_PREFIX);
}

static int valid_protocol_field(const char *text, int allow_empty)
{
    return skipdb_ipc_valid_protocol_field(text, allow_empty, (size_t)-1);
}

static int setnonblock(int fd) {
    return skipdb_ipc_set_nonblock(fd);
}

int dbclient_start(dbclient* client) {
    char* socket_path = "/tmp/.skipd_server_sock";

    if (NULL == client)
        return -1;
    memset(client, 0, sizeof(dbclient));
    client->remote_fd = -1;

    client->remote_fd = create_client_fd(socket_path);
    if (client->remote_fd < 0) {
        system("service start_skipd >/dev/null 2>&1 &");
        sleep(1);
        client->remote_fd = create_client_fd(socket_path);
        if (client->remote_fd < 0)
            return -1;
    }

    if (setnonblock(client->remote_fd) < 0) {
        close(client->remote_fd);
        client->remote_fd = -1;
        return -1;
    }
    return 0;
}

int dbclient_bulk(dbclient* client, char* command, char* key, char* value) {
    size_t body_len;
    int written;

    if (NULL == client || NULL == command || NULL == key || NULL == value ||
        client->remote_fd < 0 ||
        !valid_protocol_field(command, 0) ||
        !valid_protocol_field(key, 0))
        return -1;
    body_len = strlen(command);
    if (body_len > (size_t)READ_MAX - 3)
        return -1;
    if (strlen(key) > (size_t)READ_MAX - body_len - 3)
        return -1;
    body_len += strlen(key);
    if (strlen(value) > (size_t)READ_MAX - body_len - 3)
        return -1;
    body_len += strlen(value) + 3; /* spaces and newline */
    if (body_len == 0 || body_len > READ_MAX ||
        dbapi_check_buf(client, (int)body_len + HEADER_PREFIX) != 0)
        return -1;

    written = snprintf(client->buf, (size_t)client->buf_max + 1,
        "%s%07lu %s %s %s\n", MAGIC, (unsigned long)body_len,
        command, key, value);
    if (written < 0 || (size_t)written != body_len + HEADER_PREFIX)
        return -1;
    if (write_util(client, written, 200) != 0)
        return -1;
    return dbapi_ignore_result(client, command, key);
}

int dbclient_list(dbclient* client, char* prefix, void* o, fn_db_parse fn) {
    size_t body_len;
    int written;

    if (NULL == client || NULL == prefix || NULL == fn ||
        client->remote_fd < 0 || !valid_protocol_field(prefix, 1))
        return -1;
    body_len = strlen(prefix) + 6; /* "list " + newline */
    if (body_len > READ_MAX ||
        dbapi_check_buf(client, (int)body_len + HEADER_PREFIX) != 0)
        return -1;
    written = snprintf(client->buf, (size_t)client->buf_max + 1,
        "%s%07lu list %s\n", MAGIC, (unsigned long)body_len, prefix);
    if (written < 0 || (size_t)written != body_len + HEADER_PREFIX)
        return -1;
    if (write_util(client, written, 100) != 0)
        return -1;
    return dbapi_parse_list_result(client, prefix, o, fn);
}

int dbclient_end(dbclient* client) {
    if (NULL == client)
        return 0;
    free(client->buf);
    client->buf = NULL;
    client->buf_max = 0;
    if (client->remote_fd >= 0)
        close(client->remote_fd);
    client->remote_fd = -1;
    return 0;
}

static int myprint(dbclient* client, void* o, char* prefix, char* key, char* value) {
    printf("%s=%s\n", key, value);
    return 0;
}

static void print_js_quoted(const char *value) {
    const unsigned char *p = (const unsigned char *)value;
    putchar('"');
    while (p && *p) {
        switch (*p) {
            case '"': putchar('\\'); putchar('"'); break;
            case '\\': putchar('\\'); putchar('\\'); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            case '\b': fputs("\\b", stdout); break;
            case '\f': fputs("\\f", stdout); break;
            default:
                if (*p < 0x20)
                    printf("\\u%04x", (unsigned int)*p);
                else
                    putchar(*p);
                break;
        }
        ++p;
    }
    putchar('"');
}

static int print_json(dbclient* client, void* o, char* prefix, char* key, char* value) {
    if (NULL == key || NULL == value)
        return -1;
    fputs("o[", stdout);
    print_js_quoted(key);
    fputs("]=", stdout);
    print_js_quoted(value);
    fputs(";\n", stdout);
    return 0;
}

int dbclient_json(dbclient* client, char* prefix) {
    const unsigned char *p;

    if (NULL == client || NULL == prefix || *prefix == '\0' ||
        !((*prefix >= 'A' && *prefix <= 'Z') ||
          (*prefix >= 'a' && *prefix <= 'z') || *prefix == '_'))
        return -1;

    for (p = (const unsigned char *)prefix + 1; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_'))
            return -1;
    }

    printf("var %s=(function() {\nvar o={};\n", prefix);
    if (dbclient_list(client, prefix, NULL, print_json) != 0)
        return -1;
    printf("return o;\n})();\n");
    return 0;
}

int main(int argc, char **argv)
{
    dbclient client;
    dbclient_start(&client);
    dbclient_bulk(&client, "set", "hello1", "value-new-value");
    dbclient_bulk(&client, "set", "hello11", "value-new-value");
    dbclient_bulk(&client, "set", "hello111", "value-new-value");
    dbclient_bulk(&client, "set", "hello1112", "value-new-value");
    dbclient_bulk(&client, "set", "hello13", "value-new-value");
    dbclient_bulk(&client, "set", "hello15", "value-new-value");

    //dbclient_list(&client, "hello1", NULL, myprint);
    dbclient_json(&client, "hello1");
    dbclient_end(&client);
    return 0;
}
