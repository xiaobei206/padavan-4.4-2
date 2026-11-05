#define _GNU_SOURCE
#include <stdlib.h>
#include <stddef.h>
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
#include "skipd.h"
#include "ipc_transport.h"
#include "dbus_internal.h"

//"list "
#define LIST_LEN 5

/* Validate one space-delimited protocol token before constructing a frame. */
static int valid_protocol_field(const char *text, int allow_empty)
{
    return skipdb_ipc_valid_protocol_field(text, allow_empty,
        (size_t)READ_MAX - HEADER_PREFIX - 16);
}



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
    return skipdb_ipc_connect_unix(sock_path);
}

int setnonblock(int fd) {
    return skipdb_ipc_set_nonblock(fd);
}

static int check_buf(dbclient* client, int len) {
    if (NULL == client)
        return -1;
    return skipdb_ipc_ensure_buffer(&client->buf, &client->buf_max,
        len, BUF_MAX, READ_MAX);
}

int dbus_read_util(dbclient* client, int len, unsigned int delay) {
    if (NULL == client || client->remote_fd < 0 || len < 0 ||
        len > READ_MAX || check_buf(client, len) != 0)
        return -4;
    return skipdb_ipc_read_exact(client->remote_fd, client->buf, len,
        delay, &client->buf_pos);
}

static int write_util(dbclient* client, int len) {
    if (NULL == client || NULL == client->buf ||
        client->remote_fd < 0 || len < 0 || len > client->buf_max)
        return -1;
    return skipdb_ipc_write_exact(client->remote_fd, client->buf, len, 20);
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

