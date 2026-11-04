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

int parse_common_result(dbclient *client) {
    int n1, n2;
    const char *magic = MAGIC;
    int success;

    n1 = read_util(client, HEADER_PREFIX, 110);
    if (n1 < 0)
        return n1;
    if (0 != memcmp(client->buf, magic, MAGIC_LEN))
        return -3;

    client->buf[HEADER_PREFIX - 1] = '\0';
    if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
        n2 <= 0 || n2 > READ_MAX)
        return -4;

    n1 = read_util(client, n2, 110);
    if (n1 < 0)
        return n1;

    success = (n2 == 3 && memcmp(client->buf, "ok\n", 3) == 0) ||
        (n2 == 9 && memcmp(client->buf, "delay_ok\n", 9) == 0);
    if (n2 > 0 && fwrite(client->buf, 1, (size_t)n2, stdout) != (size_t)n2)
        return -5;
    if (n2 == 0 || client->buf[n2 - 1] != '\n')
        putchar('\n');
    return success ? 0 : -5;
}

int parse_get_result(dbclient *client) {
    int n1, n2;
    const char *magic = MAGIC;

    n1 = read_util(client, HEADER_PREFIX, 110);
    if (n1 < 0)
        return n1;
    if (0 != memcmp(client->buf, magic, MAGIC_LEN))
        return -3;

    client->buf[HEADER_PREFIX - 1] = '\0';
    if (S2ISUCCESS != str2int(&n2, client->buf + MAGIC_LEN, 10) ||
        n2 < 0 || n2 > READ_MAX)
        return -4;

    n1 = read_util(client, n2, 510);
    if (n1 < 0)
        return n1;

    /* get/inc/desc/replace responses are the raw Datum payload, not a
     * key/value record. Preserve the exact length, including embedded NULs. */
    if (n2 > 0 && fwrite(client->buf, 1, (size_t)n2, stdout) != (size_t)n2)
        return -5;
    if (n2 == 0 || client->buf[n2 - 1] != '\n')
        putchar('\n');

    if ((n2 == 6 && memcmp(client->buf, "error\n", 6) == 0) ||
        (n2 == 15 && memcmp(client->buf, "database error\n", 15) == 0) ||
        (n2 == 17 && memcmp(client->buf, "integer overflow\n", 17) == 0))
        return -5;
    return 0;
}

int parse_list_result(dbclient *client) {
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
            n2 <= 0 || n2 > READ_MAX) {
            //message error or excessive response size
            return -4;
        }

        n1 = read_util(client, n2, 510);
        if(n1 < 0) {
            return n1;
        }

        client->buf[n2] = '\0';

        if (n2 == 7 && memcmp(client->buf, "__end__\n", 7) == 0) {
            break;
        }

        p2 = client->buf;
        p1 = strstr(p2, " ");
        if (NULL == p1)
            return -4;
        p2 = p1 + 1;
        p1 = strstr(p2, " ");
        if (NULL == p1)
            return -4;
        *p1 = '=';

        if (client->buf[n2 - 1] == '\n') {
            printf("%s", p2);
        } else {
            printf("%s\n", p2);
        }
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
    int n1, n2;
    char *p1, *p2;
    size_t remaining;
    char* magic = MAGIC;

    for(;;) {
        n1 = read_util(client, HEADER_PREFIX, 110);
        if(n1 < 0) {
            return n1;
        }

        if(0 != memcmp(client->buf, magic, MAGIC_LEN)) {
            return -3;
        }

        client->buf[HEADER_PREFIX-1] = '\0';
        if(S2ISUCCESS != str2int(&n2, client->buf+MAGIC_LEN, 10) ||
            n2 <= LIST_LEN + 1 || n2 > READ_MAX) {
            return -4;
        }

        n1 = read_util(client, n2, 510);
        if(n1 < 0) {
            return n1;
        }

        client->buf[n2] = '\0';
        if (n2 == 7 && memcmp(client->buf, "__end__\n", 7) == 0) {
            break;
        }

        p1 = client->buf + LIST_LEN;
        p2 = (char *)memchr(p1, ' ', (size_t)(n2 - LIST_LEN));
        if (NULL == p2 || p2 == p1)
            return -4;
        *p2++ = '\0';

        remaining = (size_t)(client->buf + n2 - p2);
        if (remaining == 0)
            return -4;
        if (client->buf[n2 - 1] == '\0' || client->buf[n2 - 1] == '\n')
            --remaining;
        else
            return -4;
        if (memchr(p2, '\0', remaining) != NULL ||
            !valid_shell_identifier(p1))
            return -4;

        fputs("export ", stdout);
        fputs(p1, stdout);
        putchar('=');
        print_shell_quoted(p2, remaining);
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
    equals = strstr(client->buf, "=");
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
            if (bulk_key(gclient, "replace", argv[2], envp) != 0)
                err = -23;
        } else if(!strcmp("save", argv[1])) {
            if(argc < 3) {
                err = -13;
                break;
            }
            if (bulk_key(gclient, "set", argv[2], envp) != 0)
                err = -23;
        } else if(!strcmp("get", argv[1])) {
            if(argc < 3) {
                err = -13;
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
            if(argc < 4) {
                err = -18;
                break;
            }
            strcpy(client->command, "set");
            n1 = strlen(client->command) + strlen(argv[2]) + strlen(argv[3]) + 3 + 9;
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

            //setnonblock(remote_fd);
            //n1 = parse_common_result(client);
        } else if(!strcmp("time", argv[1])) {
            if(argc < 5) {
                err = -21;
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

            //setnonblock(remote_fd);
            //n1 = parse_common_result(client);
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

