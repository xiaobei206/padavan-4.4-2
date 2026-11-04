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
#include <limits.h> // [LONG|INT][MIN|MAX]
#include <stddef.h>
#include <errno.h>  // errno
#include <unistd.h>
#include "skipd.h"

//"list "
#define LIST_LEN 5

typedef struct _dbclient {
    int remote_fd;

    char* buf;
    int buf_max;
    int buf_len;
    int buf_pos;
} dbclient;

typedef int (*fn_db_parse)(dbclient* client, void* o, char* prefix, char* key, char* value);
static int ignore_result(dbclient *client);

static int write_util(dbclient* client, int len, unsigned int delay) {
    int n, written = 0;
    unsigned long long now, timeout;
    struct timeval tv, tv2;

    if (NULL == client || NULL == client->buf || len < 0 ||
        client->remote_fd < 0)
        return -1;
    gettimeofday(&tv2, NULL);
    timeout = (unsigned long long)tv2.tv_sec * 1000ULL +
        (unsigned long long)tv2.tv_usec / 1000ULL + delay;
    while (written < len) {
#ifdef MSG_NOSIGNAL
        n = (int)send(client->remote_fd, client->buf + written,
            (size_t)(len - written), MSG_NOSIGNAL);
#else
        n = (int)send(client->remote_fd, client->buf + written,
            (size_t)(len - written), 0);
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

        gettimeofday(&tv, NULL);
        now = (unsigned long long)tv.tv_sec * 1000ULL +
            (unsigned long long)tv.tv_usec / 1000ULL;
        if (now > timeout)
            break;
        usleep(50);
    }
    return written == len ? 0 : -1;
}

//TODO better for code reused
static int create_client_fd(char* sock_path) {
    int remote_fd;
    size_t path_len;
    socklen_t address_len;
    struct sockaddr_un remote;

    if (NULL == sock_path)
        return -1;
    path_len = strlen(sock_path);
    if (path_len == 0 || path_len >= sizeof(remote.sun_path))
        return -1;
    if ((remote_fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0)
        return -1;

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    memcpy(remote.sun_path, sock_path, path_len + 1);
    address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
        path_len + 1);
    if (connect(remote_fd, (struct sockaddr *)&remote, address_len) < 0) {
        close(remote_fd);
        return -1;
    }
    return remote_fd;
}

static int check_buf(dbclient* client, int len) {
    int capacity;
    char *new_buf;

    if (NULL == client || len < 0 ||
        len > READ_MAX + HEADER_PREFIX)
        return -1;
    capacity = _max(len, BUF_MAX);
    if (capacity > READ_MAX + HEADER_PREFIX || capacity == INT_MAX)
        return -1;
    if (client->buf && client->buf_max >= capacity)
        return 0;
    new_buf = (char *)realloc(client->buf, (size_t)capacity + 1);
    if (NULL == new_buf)
        return -1;
    client->buf = new_buf;
    client->buf_max = capacity;
    return 0;
}

static int setnonblock(int fd) {
    int flags;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
        client->remote_fd < 0)
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
        check_buf(client, (int)body_len + HEADER_PREFIX) != 0)
        return -1;

    written = snprintf(client->buf, (size_t)client->buf_max + 1,
        "%s%07lu %s %s %s\n", MAGIC, (unsigned long)body_len,
        command, key, value);
    if (written < 0 || (size_t)written != body_len + HEADER_PREFIX)
        return -1;
    if (write_util(client, written, 200) != 0)
        return -1;
    return ignore_result(client);
}

static int read_util(dbclient* client, int len, unsigned int delay) {
    int clen, n;
    unsigned int now, timeout;
    struct timeval tv, tv2;

    if (NULL == client || client->remote_fd < 0 ||
        len <= 0 || len > READ_MAX ||
        check_buf(client, len) != 0)
        return -4;
    gettimeofday(&tv2, NULL);
    timeout = (tv2.tv_sec * 1000) + (tv2.tv_usec / 1000) + delay;
    client->buf_pos = 0;

    for(;;) {
        clen = len - client->buf_pos;
        n = recv(client->remote_fd, client->buf + client->buf_pos, clen, 0);
        if(n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                gettimeofday(&tv, NULL);
                now = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
                if(now > timeout) {
                    break;
                }

                usleep(50);
                continue;
            }
            //timeout
            return -2;
        } else if(n == 0) {
            //socket closed
            return -1;
        } else {
            client->buf_pos += n;
            if(client->buf_pos == len) {
                //read ok
                return 0;
            }
        }
    }

    //unkown error
    return -3;
}

static int ignore_result(dbclient *client) {
    int n1, n2;
    char* magic = MAGIC;

    do {
        n1 = read_util(client, HEADER_PREFIX, 110);
        if(n1 < 0) {
            return n1;
        }

        if(0 != memcmp(client->buf, magic, MAGIC_LEN)) {
            //message error
            return -3;
        }

        client->buf[HEADER_PREFIX-1] = '\0';
        if(S2ISUCCESS != str2int(&n2, client->buf+MAGIC_LEN, 10) ||
            n2 <= 0 || n2 > READ_MAX) {
            //message error or excessive response length
            return -4;
        }

        n1 = read_util(client, n2, 110);
        if(n1 < 0) {
            return n1;
        }
    } while(0);

    return 0;
}

static int parse_list_result(dbclient *client, char* prefix, void* o, fn_db_parse fn) {
    int n1, n2;
    char *p1, *p2, *magic = MAGIC;

    for(;;) {
        n1 = read_util(client, HEADER_PREFIX, 110);
        if(n1 < 0) {
            return n1;
        }

        if(0 != memcmp(client->buf, magic, MAGIC_LEN)) {
            //message error
            return -3;
        }

        client->buf[HEADER_PREFIX-1] = '\0';
        if(S2ISUCCESS != str2int(&n2, client->buf+MAGIC_LEN, 10) ||
            n2 <= LIST_LEN + 1 || n2 > READ_MAX) {
            //message error or malformed list result
            return -4;
        }

        n1 = read_util(client, n2, 510);
        if(n1 < 0) {
            return n1;
        }

        client->buf[n2] = '\0';

        if (n2 == 7 && memcmp(client->buf, "__end__\n", 7) == 0)
            break;

        p1 = client->buf + LIST_LEN;
        p2 = memchr(p1, ' ', (size_t)(n2 - LIST_LEN));
        if (NULL == p2)
            return -4;
        *p2 = '\0';
        p2++;
        if(client->buf[n2-1] == '\n') {
            client->buf[n2-1] = '\0';
        }
        if(0 != (*fn)(client, o, prefix, p1, p2)) {
            break;
        }
    }

    return 0;
}

int dbclient_list(dbclient* client, char* prefix, void* o, fn_db_parse fn) {
    size_t body_len;
    int written;

    if (NULL == client || NULL == prefix || NULL == fn ||
        client->remote_fd < 0)
        return -1;
    body_len = strlen(prefix) + 6; /* "list " + newline */
    if (body_len > READ_MAX ||
        check_buf(client, (int)body_len + HEADER_PREFIX) != 0)
        return -1;
    written = snprintf(client->buf, (size_t)client->buf_max + 1,
        "%s%07lu list %s\n", MAGIC, (unsigned long)body_len, prefix);
    if (written < 0 || (size_t)written != body_len + HEADER_PREFIX)
        return -1;
    if (write_util(client, written, 100) != 0)
        return -1;
    return parse_list_result(client, prefix, o, fn);
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
    if (NULL == client || NULL == prefix || !((*prefix >= 'A' && *prefix <= 'Z') ||
        (*prefix >= 'a' && *prefix <= 'z') || *prefix == '_' || *prefix == '
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
))
        return -1;
    for (p = (const unsigned char *)prefix + 1; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') || *p == '_' || *p == '
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
))
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
