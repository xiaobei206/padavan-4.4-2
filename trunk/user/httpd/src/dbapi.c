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
#include "dbapi.h"

//"list "
#define LIST_LEN 5

typedef enum {
    S2ISUCCESS = 0,
    S2IOVERFLOW,
    S2IUNDERFLOW,
    S2IINCONVERTIBLE
} STR2INT_ERROR;

static int ignore_result(dbclient *client);

static int write_util(dbclient *client, int len, unsigned int delay) {
    int n, written = 0;
    unsigned long long now, timeout;
    struct timeval tv, tv2;

    if (!client || !client->buf || client->remote_fd < 0 || len < 0)
        return -1;

    if (gettimeofday(&tv2, NULL) != 0)
        return -1;
    timeout = (unsigned long long)tv2.tv_sec * 1000ULL +
              (unsigned long long)tv2.tv_usec / 1000ULL + delay;
    while (written < len) {
        n = send(client->remote_fd, client->buf + written, (size_t)(len - written), MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (gettimeofday(&tv, NULL) != 0)
                    return -1;
                now = (unsigned long long)tv.tv_sec * 1000ULL +
                      (unsigned long long)tv.tv_usec / 1000ULL;
                if (now >= timeout)
                    break;
                usleep(50);
                continue;
            }
            return -1;
        }
        if (n == 0)
            return -1;
        written += n;
    }

    return written == len ? 0 : -1;
}

//TODO better for code reused
static int create_client_fd(const char *sock_path) {
    int len, remote_fd;
    size_t path_len;
    struct sockaddr_un remote;

    if (!sock_path)
        return -1;
    path_len = strlen(sock_path);
    if (path_len == 0 || path_len >= sizeof(remote.sun_path))
        return -1;

    remote_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (remote_fd < 0)
        return -1;

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    memcpy(remote.sun_path, sock_path, path_len + 1);
    len = (int)(offsetof(struct sockaddr_un, sun_path) + path_len + 1);
    if (connect(remote_fd, (struct sockaddr *)&remote, (socklen_t)len) < 0) {
        close(remote_fd);
        return -1;
    }

    return remote_fd;
}

static int check_buf(dbclient *client, int len) {
    int capacity;
    char *new_buf;

    if (!client || len < 0 || len > READ_MAX)
        return -1;

    capacity = _max(len, BUF_MAX);
    if (client->buf && client->buf_max < capacity) {
        free(client->buf);
        client->buf = NULL;
        client->buf_max = 0;
    }

    if (!client->buf) {
        new_buf = (char *)malloc((size_t)capacity + 1);
        if (!new_buf)
            return -1;
        client->buf = new_buf;
        client->buf_max = capacity;
    }

    return 0;
}

static int setnonblock(int fd) {
    int flags = fcntl(fd, F_GETFL);

    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static STR2INT_ERROR str2int(int *i, char *s, int base) {
  char *end;
  long  l;
  errno = 0;
  l = strtol(s, &end, base);

  if ((errno == ERANGE && l == LONG_MAX) || l > INT_MAX) {
    return S2IOVERFLOW;
  }
  if ((errno == ERANGE && l == LONG_MIN) || l < INT_MIN) {
    return S2IUNDERFLOW;
  }
  if (*s == '\0' || *end != '\0') {
    return S2IINCONVERTIBLE;
  }
  *i = l;
  return S2ISUCCESS;
}

int dbclient_start(dbclient *client) {
    const char *socket_path = "/tmp/.skipd_server_sock";

    if (!client)
        return -1;
    memset(client, 0, sizeof(dbclient));
    client->remote_fd = -1;

    client->remote_fd = create_client_fd(socket_path);
    if(-1 == client->remote_fd) {
        system("/usr/sbin/skipd -d /etc/storage/db >/dev/null 2>&1 &");
        sleep(1);

        client->remote_fd = create_client_fd(socket_path);
        if(-1 == client->remote_fd) {
            return -1;
        }
    }

    if (setnonblock(client->remote_fd) != 0) {
        close(client->remote_fd);
        client->remote_fd = -1;
        return -1;
    }
    return 0;
}

int dbclient_bulk(dbclient *client, const char *command, const char *key,
                  int nk, const char *value, int nv) {
    size_t command_len;
    int n1, n2, total_len, response;

    if (!client || !command || nk < 0 || nv < 0 ||
        (nk > 0 && !key) || (nv > 0 && !value))
        return -1;

    command_len = strlen(command);
    if (command_len == 0 || command_len > READ_MAX ||
        (size_t)nk > READ_MAX || (size_t)nv > READ_MAX ||
        command_len + (size_t)nk + (size_t)nv + 3 > READ_MAX - HEADER_PREFIX)
        return -1;

    n1 = (int)(command_len + (size_t)nk + (size_t)nv + 3);
    total_len = n1 + HEADER_PREFIX;
    if (check_buf(client, total_len) != 0)
        return -1;

    n2 = snprintf(client->buf, (size_t)client->buf_max + 1,
                  "%s%07d %s ", MAGIC, n1, command);
    if (n2 < 0 || n2 + nk + nv + 2 != total_len)
        return -1;

    if (nk > 0)
        memcpy(client->buf + n2, key, (size_t)nk);
    client->buf[n2 + nk] = ' ';
    n2 += nk + 1;

    if (nv > 0)
        memcpy(client->buf + n2, value, (size_t)nv);
    client->buf[n2 + nv] = '\n';
    n2 += nv + 1;
    client->buf[n2] = '\0';

    if (write_util(client, total_len, 200) != 0)
        return -1;
    response = ignore_result(client);
    return response == 0 ? 0 : -1;
}

static int read_util(dbclient *client, int len, unsigned int delay) {
    int clen, n;
    unsigned long long now, timeout;
    struct timeval tv, tv2;

    if (!client || client->remote_fd < 0 || len <= 0 || len > READ_MAX)
        return -4;
    if (check_buf(client, len) != 0)
        return -4;
    if (gettimeofday(&tv2, NULL) != 0)
        return -1;
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
                    return -3;
                now = (unsigned long long)tv.tv_sec * 1000ULL +
                      (unsigned long long)tv.tv_usec / 1000ULL;
                if (now >= timeout) {
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
            //message error
            return -4;
        }

        n1 = read_util(client, n2, 110);
        if(n1 < 0) {
            return n1;
        }
    } while(0);

    return 0;
}

static int parse_list_result(dbclient *client, char* prefix, webs_t wp, fn_db_parse fn) {
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
           n2 <= LIST_LEN || n2 > READ_MAX) {
            //message error
            return -4;
        }

        n1 = read_util(client, n2, 510);
        if(n1 < 0) {
            return n1;
        }

        client->buf[n2] = '\0';

        if(NULL != strstr(client->buf, "__end__")) {
            break;
        }

        p1 = client->buf + LIST_LEN;
        p2 = strchr(p1, ' ');
        if (!p2)
            return -4;
        *p2 = '\0';
        p2++;
        if(client->buf[n2-1] == '\n') {
            client->buf[n2-1] = '\0';
        }
        if(0 != (*fn)(client, wp, prefix, p1, p2)) {
            break;
        }
    }

    return 0;
}

int dbclient_list(dbclient *client, char *prefix, webs_t wp, fn_db_parse fn) {
    size_t prefix_len;
    const unsigned char *p;
    int n1, total_len, n2;

    if (!client || !prefix || !fn)
        return -1;
    prefix_len = strlen(prefix);
    if (prefix_len == 0 || prefix_len > READ_MAX - HEADER_PREFIX - 16)
        return -1;
    for (p = (const unsigned char *)prefix; *p; ++p) {
        if (*p < 32 || *p == 127 || *p == ' ')
            return -1;
    }

    n1 = (int)(strlen("list") + prefix_len + 2);
    total_len = n1 + HEADER_PREFIX;
    if (check_buf(client, total_len) != 0)
        return -1;
    n2 = snprintf(client->buf, (size_t)client->buf_max + 1,
                  "%s%07d list %s\n", MAGIC, n1, prefix);
    if (n2 < 0 || n2 != total_len)
        return -1;
    if (write_util(client, total_len, 100) != 0)
        return -1;

    return parse_list_result(client, prefix, wp, fn);
}

int dbclient_end(dbclient *client) {
    if (!client)
        return -1;
    free(client->buf);
    client->buf = NULL;
    client->buf_max = 0;
    if (client->remote_fd >= 0)
        close(client->remote_fd);
    client->remote_fd = -1;
    return 0;
}

