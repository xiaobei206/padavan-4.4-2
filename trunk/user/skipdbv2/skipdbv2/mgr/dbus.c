#define _GNU_SOURCE
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
#include <errno.h>  // errno
#include <unistd.h>
#include "skipd.h"

//"list "
#define LIST_LEN 5

/* Validate one space-delimited protocol token before constructing a frame. */
static int valid_protocol_field(const char *text, int allow_empty)
{
    const unsigned char *p = (const unsigned char *)text;
    size_t length;

    if (NULL == text)
        return 0;
    length = strlen(text);
    if ((!allow_empty && length == 0) ||
        length > (size_t)READ_MAX - HEADER_PREFIX - 16)
        return 0;
    for (; *p; ++p) {
        if (*p <= 0x20 || *p == 0x7f)
            return 0;
    }
    return 1;
}

typedef struct _dbclient {
    int remote_fd;
    char command[DELAY_KEY_LEN+1];
    char key[DELAY_KEY_LEN+1];
    //unsigned int timeout;

    char* buf;
    int buf_max;
    int buf_len;
    int buf_pos;
} dbclient;

#if 0
static void print_time(char* prefix)
{
    struct timeval tv;
    unsigned int ms;

    gettimeofday (&tv, NULL);
    ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    fprintf(stderr, "%s: %03u\n", prefix, ms);
}
#endif

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

int setnonblock(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int check_buf(dbclient* client, int len) {
    int capacity;
    char *new_buf;

    if (NULL == client || len < 0 || len > READ_MAX)
        return -1;
    capacity = _max(len, BUF_MAX);
    if (capacity < 0 || capacity == INT_MAX)
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

static int read_util(dbclient* client, int len, unsigned int delay) {
    int clen, n;
    unsigned long long now, timeout;
    struct timeval tv, tv2;

    if (NULL == client || client->remote_fd < 0 ||
        len < 0 || len > READ_MAX || check_buf(client, len) != 0)
        return -4;
    if (len == 0)
        return 0;
    if (gettimeofday(&tv2, NULL) != 0)
        return -2;
    timeout = (unsigned long long)tv2.tv_sec * 1000ULL +
        (unsigned long long)tv2.tv_usec / 1000ULL + delay;
    client->buf_pos = 0;

    for(;;) {
        clen = len - client->buf_pos;
        n = recv(client->remote_fd, client->buf + client->buf_pos, clen, 0);
        if(n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (gettimeofday(&tv, NULL) != 0)
                    return -2;
                now = (unsigned long long)tv.tv_sec * 1000ULL +
                    (unsigned long long)tv.tv_usec / 1000ULL;
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
    size_t command_len, key_start, response_command_len;
    char *command_end, *key_end;
    int is_error = 0;
    const char *magic = MAGIC;

    if (NULL == client || NULL == client->command ||
        NULL == payload_offset || NULL == body_len)
        return -4;

    n1 = read_util(client, HEADER_PREFIX, delay);
    if (n1 < 0)
        return n1;
    if (0 != memcmp(client->buf, magic, MAGIC_LEN))
        return -3;

    client->buf[HEADER_PREFIX - 1] = '\0';
    if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
        n2 < 0 || n2 > READ_MAX)
        return -4;

    n1 = read_util(client, n2, delay);
    if (n1 < 0)
        return n1;

    command_len = strlen(client->command);
    command_end = (char *)memchr(client->buf, ' ', (size_t)n2);
    if (command_len == 0 || NULL == command_end || command_end == client->buf)
        return -4;

    response_command_len = (size_t)(command_end - client->buf);
    if (response_command_len == 9 &&
        memcmp(client->buf, "__error__", 9) == 0)
        is_error = 1;
    else if (response_command_len != command_len ||
        memcmp(client->buf, client->command, command_len) != 0)
        return -4;

    key_start = response_command_len + 1;
    key_end = (char *)memchr(client->buf + key_start, ' ',
        (size_t)n2 - key_start);
    if (NULL == key_end || key_end == client->buf + key_start ||
        memchr(client->buf + key_start, '\0',
            (size_t)(key_end - (client->buf + key_start))) != NULL)
        return -4;

    if (is_error &&
        ((size_t)(key_end - (client->buf + key_start)) != command_len ||
         memcmp(client->buf + key_start, client->command, command_len) != 0))
        return -4;

    if (NULL != key_offset)
        *key_offset = key_start;
    if (NULL != key_len)
        *key_len = (size_t)(key_end - (client->buf + key_start));
    if (NULL != error_response)
        *error_response = is_error;
    *payload_offset = (size_t)(key_end - client->buf) + 1;
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
    char *body, *command_end, *key_start, *key_end, *payload;
    size_t command_len, key_size, payload_size;

    if (NULL == client || NULL == client->buf || body_len <= 0 ||
        NULL == key || NULL == key_len || NULL == value || NULL == value_len)
        return -4;

    body = client->buf;
    command_end = (char *)memchr(body, ' ', (size_t)body_len);
    if (NULL == command_end || command_end == body)
        return -4;

    command_len = (size_t)(command_end - body);
    key_start = command_end + 1;
    key_end = (char *)memchr(key_start, ' ',
        (size_t)(body + body_len - key_start));
    if (NULL == key_end || key_end == key_start)
        return -4;

    key_size = (size_t)(key_end - key_start);
    payload = key_end + 1;
    payload_size = (size_t)(body + body_len - payload);
    if (memchr(key_start, '\0', key_size) != NULL)
        return -4;

    if (command_len == 9 && memcmp(body, "__error__", 9) == 0)
        return (key_size == 4 && memcmp(key_start, "list", 4) == 0)
            ? -5 : -4;

    if (command_len == 7 && memcmp(body, "__end__", 7) == 0) {
        if (key_size == 4 && memcmp(key_start, "list", 4) == 0 &&
            payload_size == 8 && memcmp(payload, "__end__\n", 8) == 0)
            return 1;
        return -4;
    }

    if (command_len != 4 || memcmp(body, "list", 4) != 0)
        return -4;

    *key_end = '\0';
    *key = key_start;
    *key_len = key_size;
    *value = payload;
    *value_len = payload_size;
    return 0;
}

int parse_list_result(dbclient *client) {
    int n1, n2, parsed;
    char *key, *value;
    size_t key_len, value_len;
    char *magic = MAGIC;

    for (;;) {
        n1 = read_util(client, HEADER_PREFIX, 110);
        if (n1 < 0)
            return n1;

        if (0 != memcmp(client->buf, magic, MAGIC_LEN))
            return -3;

        client->buf[HEADER_PREFIX - 1] = '\0';
        if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
            n2 <= 0 || n2 > READ_MAX)
            return -4;

        n1 = read_util(client, n2, 510);
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
        n1 = read_util(client, HEADER_PREFIX, 110);
        if (n1 < 0)
            return n1;

        if (0 != memcmp(client->buf, magic, MAGIC_LEN))
            return -3;

        client->buf[HEADER_PREFIX - 1] = '\0';
        if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
            n2 <= 0 || n2 > READ_MAX)
            return -4;

        n1 = read_util(client, n2, 510);
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

static int write_util(dbclient* client, int len) {
    int n, written = 0;
    unsigned long long now, timeout;
    struct timeval tv, tv2;

    if (NULL == client || NULL == client->buf ||
        client->remote_fd < 0 || len < 0 || len > client->buf_max)
        return -1;
    if (gettimeofday(&tv2, NULL) != 0)
        return -1;
    timeout = (unsigned long long)tv2.tv_sec * 1000ULL +
        (unsigned long long)tv2.tv_usec / 1000ULL + 20ULL;

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

        if (gettimeofday(&tv, NULL) != 0)
            return -1;
        now = (unsigned long long)tv.tv_sec * 1000ULL +
            (unsigned long long)tv.tv_usec / 1000ULL;
        if (now > timeout)
            break;
        usleep(5);
    }
    return written == len ? 0 : -1;
}

static int bulk_key(dbclient* client, char* command, char* prefix, char* envp[]) {
    char **env;
    char *p1, *p2;
    size_t nenv, nc, nkey, np, body_len;
    int n2;

    if (NULL == client || NULL == command || NULL == prefix || NULL == envp)
        return -1;
    nc = strlen(command);
    np = strlen(prefix);
    if (nc >= sizeof(client->command))
        return -1;
    memcpy(client->command, command, nc + 1);

    for (env = envp; *env != NULL; ++env) {
        p1 = *env;
        if (NULL == p1 || strncmp(p1, prefix, np) != 0)
            continue;
        nenv = strlen(p1);
        if (nc > (size_t)READ_MAX - HEADER_PREFIX - 2 ||
            nenv > (size_t)READ_MAX - HEADER_PREFIX - nc - 2)
            return -1;
        body_len = nc + nenv + 2;
        if (body_len > (size_t)READ_MAX - HEADER_PREFIX ||
            check_buf(client, (int)(body_len + HEADER_PREFIX)) != 0)
            return -1;

        p2 = strchr(p1, '=');
        if (NULL == p2)
            return -1;
        nkey = (size_t)(p2 - p1);
        n2 = snprintf(client->buf, (size_t)client->buf_max + 1,
            "%s%07lu %s ", MAGIC, (unsigned long)body_len, client->command);
        if (n2 < 0 || (size_t)n2 != HEADER_PREFIX + nc + 1)
            return -1;
        memcpy(client->buf + n2, p1, nkey);
        n2 += (int)nkey;
        client->buf[n2++] = ' ';
        memcpy(client->buf + n2, p2 + 1, nenv - nkey - 1);
        n2 += (int)(nenv - nkey - 1);
        client->buf[n2++] = '\n';
        if ((size_t)n2 != body_len + HEADER_PREFIX)
            return -1;
        client->buf[n2] = '\0';
        if (write_util(client, n2) != 0)
            return -1;
        if (setnonblock(client->remote_fd) < 0)
            return -1;
        n2 = !strcmp(command, "replace")
            ? parse_get_result(client) : parse_common_result(client);
        if (n2 < 0)
            return n2;
    }
    return 0;
}

static void help(void) {
    printf("help:\n");
    printf("dbus set key=value\n");
    printf("dbus ram key=value\n");
    printf("dbus replace key=value\n");
    printf("dbus get key\n");
    printf("dbus list key\n");
    printf("dbus remove key\n");
    printf("dbus export key\n");
    printf("dbus update key\n");
    printf("dbus delay key tick path_of_shell.sh\n");
    printf("dbus time key H:M:S path_of_shell.sh\n");
    printf("dbus fire key\n");
    printf("dbus event key path_of_shell.sh\n");
    printf("dbus inc key=value\n");
    printf("dbus desc key=value\n");
}

static int prefix_set_command(dbclient* client, int argc, char **argv)
{
    int i, n2;
    char *equals;
    size_t body_len, arg_len;

    if (NULL == client || NULL == client->command || NULL == argv || argc < 3)
        return -1;
    body_len = strlen(client->command) + 1;
    for (i = 2; i < argc; ++i) {
        if (NULL == argv[i])
            return -1;
        arg_len = strlen(argv[i]);
        if (arg_len > (size_t)READ_MAX - HEADER_PREFIX - body_len)
            return -1;
        body_len += arg_len;
        if (i != argc - 1) {
            if (body_len >= (size_t)READ_MAX - HEADER_PREFIX)
                return -1;
            body_len++;
        }
    }
    if (body_len >= (size_t)READ_MAX - HEADER_PREFIX)
        return -1;
    body_len++; /* trailing newline */
    if (body_len > 9999999 ||
        check_buf(client, (int)body_len + HEADER_PREFIX) != 0)
        return -1;

    n2 = snprintf(client->buf, (size_t)client->buf_max + 1,
        "%s%07lu %s ", MAGIC, (unsigned long)body_len, client->command);
    if (n2 < 0 || n2 >= client->buf_max)
        return -1;
    for (i = 2; i < argc; ++i) {
        arg_len = strlen(argv[i]);
        memcpy(client->buf + n2, argv[i], arg_len);
        n2 += (int)arg_len;
        client->buf[n2++] = (i == argc - 1) ? '\n' : ' ';
    }
    client->buf[n2] = '\0';
    /* Only the first argument is the key=value field. Validate its key
     * before rewriting '=' to a protocol separator; otherwise "bad key=x"
     * silently turns into a successful write to the unintended key "bad". */
    equals = strchr(argv[2], '=');
    if (NULL == equals || equals == argv[2])
        return -1;
    {
        const unsigned char *p;
        for (p = (const unsigned char *)argv[2]; p < (const unsigned char *)equals; ++p) {
            if (*p <= 0x20 || *p == 0x7f)
                return -1;
        }
    }
    equals = strchr(client->buf + HEADER_PREFIX + strlen(client->command) + 1, '=');
    if (NULL == equals)
        return -1;
    *equals = ' ';
    return n2;
}

dbclient* gclient;
int main(int argc, char **argv, char * envp[])
{
    int n1, n2, err = 0;
    struct tm tm1;
    char *time_end;
    dbclient* client;
    int remote_fd;

    //print_time("start");

    remote_fd = create_client_fd("/tmp/.skipd_server_sock");
    if(-1 == remote_fd) {
#if 0
        //Try to restart skipd
        system("service start_skipd >/dev/null 2>&1 &");
        sleep(1);
        remote_fd = create_client_fd("/tmp/.skipd_server_sock");
        if(-1 == remote_fd) {
            perror("connect to skipd error");
            return -1;
        }
#else
        perror("connect to skipd error");
        return -1;
#endif
    }
    /* Make the bounded write/read deadlines effective from the first frame. */
    if (setnonblock(remote_fd) < 0) {
        close(remote_fd);
        return EXIT_FAILURE;
    }
    gclient = (dbclient*)calloc(1, sizeof(dbclient));
    if (NULL == gclient) {
        close(remote_fd);
        return EXIT_FAILURE;
    }
    gclient->remote_fd = remote_fd;
    client = gclient;

    do {
        if(argc < 2) {
            err = -11;
            break;
        }
        if(!strcmp("list", argv[1])) {
            if(argc < 3) {
                err = -12;
                break;
            }
            if (!valid_protocol_field(argv[2], 1)) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n1 = strlen(argv[2]) + 2 + strlen(client->command);
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s\n", MAGIC, n1, client->command, argv[2]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_list_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("export", argv[1])) {
            if (argc < 3) {
                err = -12;
                break;
            }
            if (!valid_protocol_field(argv[2], 1)) {
                err = -16;
                break;
            }
            strcpy(client->command, "list");
            n1 = strlen(argv[2]) + 2 + strlen(client->command);
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s\n", MAGIC, n1, client->command, argv[2]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_script_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("update", argv[1])) {
            if(argc < 3) {
                err = -13;
                break;
            }
            if (!valid_protocol_field(argv[2], 0)) {
                err = -16;
                break;
            }
            if (bulk_key(gclient, "replace", argv[2], envp) != 0)
                err = -23;
        } else if(!strcmp("save", argv[1])) {
            if(argc < 3) {
                err = -13;
                break;
            }
            if (!valid_protocol_field(argv[2], 0)) {
                err = -16;
                break;
            }
            if (bulk_key(gclient, "set", argv[2], envp) != 0)
                err = -23;
        } else if(!strcmp("get", argv[1])) {
            if(argc < 3) {
                err = -13;
                break;
            }
            if (!valid_protocol_field(argv[2], 0)) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n1 = strlen(argv[2]) + 2 + strlen(client->command);
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s\n", MAGIC, n1, client->command, argv[2]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_get_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("remove", argv[1])) {
            if(argc < 3) {
                err = -14;
                break;
            }
            if (!valid_protocol_field(argv[2], 0)) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n1 = strlen(argv[2]) + 2 + strlen(client->command);
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s\n", MAGIC, n1, client->command, argv[2]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("fire", argv[1])) {
            if(argc < 3) {
                err = -15;
                break;
            }
            if (!valid_protocol_field(argv[2], 0)) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n1 = strlen(argv[2]) + 2 + strlen(client->command);
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s\n", MAGIC, n1, client->command, argv[2]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("set", argv[1])) {
            if((argc < 3) || (NULL == strstr(argv[2], "="))) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n2 = prefix_set_command(client, argc, argv);
            if (n2 < 0) {
                err = -23;
                break;
            }
            //print_time("started");
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }
            //print_time("end");

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(client);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("inc", argv[1])) {
            if((argc < 3) || (NULL == strstr(argv[2], "="))) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n2 = prefix_set_command(client, argc, argv);
            if (n2 < 0) {
                err = -23;
                break;
            }
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_get_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("desc", argv[1])) {
            if((argc < 3) || (NULL == strstr(argv[2], "="))) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n2 = prefix_set_command(client, argc, argv);
            if (n2 < 0) {
                err = -23;
                break;
            }
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_get_result(gclient);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("ram", argv[1])) {
            if((argc < 3) || (NULL == strstr(argv[2], "="))) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n2 = prefix_set_command(client, argc, argv);
            if (n2 < 0) {
                err = -23;
                break;
            }
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(client);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("replace", argv[1])) {
            if((argc < 3) || (NULL == strstr(argv[2], "="))) {
                err = -16;
                break;
            }
            strcpy(client->command, argv[1]);
            n2 = prefix_set_command(client, argc, argv);
            if (n2 < 0) {
                err = -23;
                break;
            }
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_get_result(client);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("event", argv[1])) {
            size_t key_len, value_len, body_len;
            if(argc < 4) {
                err = -18;
                break;
            }
            if (!valid_protocol_field(argv[2], 0)) {
                err = -16;
                break;
            }
            key_len = strlen(argv[2]);
            value_len = strlen(argv[3]);
            if (value_len > (size_t)READ_MAX - 15 ||
                key_len > (size_t)READ_MAX - 15 - value_len) {
                err = -16;
                break;
            }
            body_len = 15 + key_len + value_len;
            n1 = (int)body_len;
            strcpy(client->command, "set");
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s __event__%s %s\n", MAGIC, n1, client->command, argv[2], argv[3]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(client);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("delay", argv[1])) {
            if(argc < 5) {
                err = -19;
                break;
            }
            if (!valid_protocol_field(argv[2], 0) ||
                !valid_protocol_field(argv[4], 0)) {
                err = -16;
                break;
            }
            if(S2ISUCCESS != str2int(&n2, argv[3], 10) || n2 < 0) {
                err = -20;
                break;
            }

            strcpy(client->command, argv[1]);
            n1 = strlen(client->command) + strlen(argv[2]) + strlen(argv[3]) + strlen(argv[4]) + 4;
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s %s %s\n", MAGIC, n1, client->command, argv[2], argv[3], argv[4]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(client);
            if (n1 < 0)
                err = n1;
        } else if(!strcmp("time", argv[1])) {
            if(argc < 5) {
                err = -21;
                break;
            }
            if (!valid_protocol_field(argv[2], 0) ||
                !valid_protocol_field(argv[4], 0)) {
                err = -16;
                break;
            }

            memset(&tm1, 0, sizeof(tm1));
            tm1.tm_mday = 1;
            time_end = strptime(argv[3], "%H:%M:%S", &tm1);
            if(NULL == time_end || *time_end != '\0' ||
               tm1.tm_hour < 0 || tm1.tm_hour > 23 ||
               tm1.tm_min < 0 || tm1.tm_min > 59 ||
               tm1.tm_sec < 0 || tm1.tm_sec > 59) {
                err = -22;
                break;
            }

            strcpy(client->command, argv[1]);
            n1 = strlen(client->command) + strlen(argv[2]) + strlen(argv[3]) + strlen(argv[4]) + 4;
            if (check_buf(client, n1 + HEADER_PREFIX) != 0) {
                err = -23;
                break;
            }
            n2 = snprintf(client->buf, (size_t)client->buf_max + 1, "%s%07d %s %s %s %s\n", MAGIC, n1, client->command, argv[2], argv[3], argv[4]);
            if (n2 <= 0 || n2 > client->buf_max ||
                write_util(client, n2) != 0) {
                err = -23;
                break;
            }

            if (setnonblock(remote_fd) < 0) {
                err = -23;
                break;
            }
            n1 = parse_common_result(client);
            if (n1 < 0)
                err = n1;
        } else {
            err = -24; /* Unknown command. */
        }
    } while(0);
    if(err < 0) {
        help();
    }

    close(remote_fd);
    if(NULL != gclient->buf) {
        free(gclient->buf);
    }
    free(gclient);
    return err < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

