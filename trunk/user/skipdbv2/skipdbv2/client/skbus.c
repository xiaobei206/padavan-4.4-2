#include <stdlib.h>
#include <stdio.h>
#include <sys/un.h>
#include <errno.h>
#include <string.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <ev.h>

#define MAGIC "magicv1 "
#define MAGIC_LEN 8
#define HEADER_LEN 8
#define HEADER_PREFIX (MAGIC_LEN + HEADER_LEN)
#define SKBUS_MAX_PAYLOAD 65536

EV_P;
ev_io stdin_watcher;
ev_io remote_w;
ev_io send_w;
int remote_fd = -1;
char *line = NULL;
size_t line_capacity = 0;
char *out_buf = NULL;
size_t out_len = 0;
size_t out_pos = 0;

static void send_cb(EV_P_ ev_io *w, int revents)
{
    if (revents & EV_WRITE) {
        while (out_pos < out_len) {
            ssize_t n;
#ifdef MSG_NOSIGNAL
            n = send(remote_fd, out_buf + out_pos, out_len - out_pos,
                MSG_NOSIGNAL);
#else
            n = send(remote_fd, out_buf + out_pos, out_len - out_pos, 0);
#endif
            if (n > 0) {
                out_pos += (size_t)n;
                continue;
            }
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                return;
            if (n == 0)
                errno = EPIPE;
            perror("skbus send");
            ev_break(EV_A_ EVBREAK_ALL);
            return;
        }

        free(out_buf);
        out_buf = NULL;
        out_len = 0;
        out_pos = 0;
        ev_io_stop(EV_A_ &send_w);
        ev_io_set(&send_w, remote_fd, EV_READ);
        ev_io_start(EV_A_ &send_w);
    }

    if (revents & EV_READ) {
        unsigned char buffer[101];
        ssize_t n;

        do {
            n = recv(remote_fd, buffer, sizeof(buffer) - 1, 0);
        } while (n < 0 && errno == EINTR);

        if (n > 0) {
            /* Responses are framed/binary data; write exactly the received bytes. */
            if (fwrite(buffer, 1, (size_t)n, stdout) != (size_t)n ||
                fflush(stdout) != 0) {
                perror("skbus stdout");
                ev_break(EV_A_ EVBREAK_ALL);
            }
        } else if (n == 0) {
            puts("orderly disconnect");
            ev_break(EV_A_ EVBREAK_ALL);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("skbus recv");
            ev_break(EV_A_ EVBREAK_ALL);
        }
    }
}

static void stdin_cb(EV_P_ ev_io *w, int revents)
{
    ssize_t line_len;
    int header_len;

    (void)w;
    (void)revents;
    line_len = getline(&line, &line_capacity, stdin);
    if (line_len <= 0) {
        if (line_len < 0 && !feof(stdin))
            perror("skbus getline");
        ev_break(EV_A_ EVBREAK_ALL);
        return;
    }
    if ((size_t)line_len > SKBUS_MAX_PAYLOAD || line_len > 9999999) {
        fprintf(stderr, "input exceeds the %d-byte protocol limit\n",
            SKBUS_MAX_PAYLOAD);
        return;
    }
    if (out_buf != NULL) {
        fprintf(stderr, "previous request is still being sent\n");
        return;
    }

    out_buf = (char *)malloc(HEADER_PREFIX + (size_t)line_len + 1);
    if (NULL == out_buf) {
        perror("skbus request allocation");
        ev_break(EV_A_ EVBREAK_ALL);
        return;
    }
    header_len = snprintf(out_buf, HEADER_PREFIX + 1, "%s%07d ",
        MAGIC, (int)line_len);
    if (header_len != HEADER_PREFIX) {
        free(out_buf);
        out_buf = NULL;
        fprintf(stderr, "unable to construct protocol header\n");
        ev_break(EV_A_ EVBREAK_ALL);
        return;
    }
    memcpy(out_buf + HEADER_PREFIX, line, (size_t)line_len);
    out_len = HEADER_PREFIX + (size_t)line_len;
    out_pos = 0;

    ev_io_stop(EV_A_ &send_w);
    ev_io_set(&send_w, remote_fd, EV_READ | EV_WRITE);
    ev_io_start(EV_A_ &send_w);
}

static void remote_cb(EV_P_ ev_io *w, int revents)
{
    int socket_error = 0;
    socklen_t error_len = sizeof(socket_error);

    (void)revents;
    if (getsockopt(remote_fd, SOL_SOCKET, SO_ERROR,
            &socket_error, &error_len) != 0 || socket_error != 0) {
        if (socket_error != 0)
            errno = socket_error;
        perror("skbus connect");
        ev_break(EV_A_ EVBREAK_ALL);
        return;
    }

    puts("connected, now watching stdin");
    ev_io_start(EV_A_ &stdin_watcher);
    ev_io_start(EV_A_ &send_w);
    ev_io_stop(EV_A_ w);
}

static int setnonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void connection_new(EV_P_ const char *sock_path)
{
    socklen_t address_len;
    size_t path_len;
    struct sockaddr_un remote;

    if (NULL == sock_path) {
        errno = EINVAL;
        perror("skbus socket path");
        exit(EXIT_FAILURE);
    }
    path_len = strnlen(sock_path, sizeof(remote.sun_path));
    if (path_len == 0 || path_len >= sizeof(remote.sun_path)) {
        errno = ENAMETOOLONG;
        perror("skbus socket path");
        exit(EXIT_FAILURE);
    }

    remote_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (remote_fd < 0) {
        perror("skbus socket");
        exit(EXIT_FAILURE);
    }
    if (setnonblock(remote_fd) < 0) {
        perror("skbus socket nonblock");
        close(remote_fd);
        remote_fd = -1;
        exit(EXIT_FAILURE);
    }

    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    memcpy(remote.sun_path, sock_path, path_len + 1);
    address_len = (socklen_t)(offsetof(struct sockaddr_un, sun_path) +
        path_len + 1);

    ev_io_init(&remote_w, remote_cb, remote_fd, EV_WRITE);
    ev_io_start(EV_A_ &remote_w);
    ev_io_init(&send_w, send_cb, remote_fd, EV_READ);
    ev_io_init(&stdin_watcher, stdin_cb, STDIN_FILENO, EV_READ);

    if (connect(remote_fd, (struct sockaddr *)&remote, address_len) < 0 &&
        errno != EINPROGRESS && errno != EAGAIN) {
        perror("skbus connect");
        close(remote_fd);
        remote_fd = -1;
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    loop = EV_DEFAULT;
    connection_new(EV_A_ "/tmp/.skipd_server_sock");
    ev_loop(EV_A_ 0);

    if (remote_fd >= 0)
        close(remote_fd);
    free(out_buf);
    free(line);
    return 0;
}
