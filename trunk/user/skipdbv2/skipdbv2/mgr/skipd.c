#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h> // [LONG|INT][MIN|MAX]
#include <errno.h>  // errno
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <time.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <stddef.h>
#include <unistd.h>
#include <syslog.h>
#include <ev.h>

#include "coroutine.h"
#include "SkipDB.h"
#include "skipd.h"


#include "skipd_internal.h"

void skipd_daemonize(char *path);
static int client_ccr_write(EV_P_ skipd_client *client);

char *global_magic = MAGIC;
skipd_server *global_server;
ev_signal signal_watcher;
ev_signal signal_watcher2;
char static_buffer[STATIC_BUF_LEN + 1];
int script_log_fd = -1;

static int open_script_log(void)
{
#ifdef O_NOFOLLOW
    struct stat st;
    int fd = open("/tmp/skipd.log",
        O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0)
        return -1;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 ||
        st.st_uid != geteuid() || fchmod(fd, 0600) != 0) {
        int saved_errno = errno ? errno : EPERM;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
#else
    /* Never follow a predictable /tmp path without O_NOFOLLOW support. */
    errno = ENOTSUP;
    return -1;
#endif
}

char *find_terminated_value_separator(Datum value)
{
    if (NULL == value.data || value.size < 2 ||
        ((char*)value.data)[value.size - 1] != '\0' ||
        memchr(value.data, '\0', value.size - 1) != NULL)
        return NULL;
    return (char*)memchr(value.data, ' ', value.size - 1);
}

/* Check the command/key/value envelope before persisting a value. */
static int response_frame_fits(const char *command,
    const char *key, int value_len)
{
    size_t command_len, key_len;

    if (NULL == command || NULL == key || value_len < 0)
        return 0;
    command_len = strlen(command);
    key_len = strlen(key);
    if (command_len > (size_t)READ_MAX ||
        key_len > (size_t)READ_MAX ||
        command_len > (size_t)READ_MAX - key_len ||
        command_len + key_len > (size_t)READ_MAX - 2)
        return 0;
    return (size_t)value_len <=
        (size_t)READ_MAX - command_len - key_len - 2;
}

 int value_fits_list_frame(const char *key, size_t value_len)
{
    size_t key_len;

    if (NULL == key || READ_MAX < 6)
        return 0;
    key_len = strlen(key);
    if (key_len > (size_t)READ_MAX - 6)
        return 0;
    return value_len <= (size_t)READ_MAX - key_len - 6;
}

/*
 * Compute the next occurrence of a local wall-clock time. A fixed 86400
 * second repeat drifts by one hour when the local timezone enters/leaves DST.
 */
 int next_daily_delay(const char *time_text, time_t now, double *delay)
{
    struct tm target, *local_now;
    char *end;
    time_t next;

    if (NULL == time_text || NULL == delay || now == (time_t)-1)
        return 0;

    memset(&target, 0, sizeof(target));
    target.tm_mday = 1;
    end = strptime(time_text, "%H:%M:%S", &target);
    if (NULL == end || *end != '\0' ||
        target.tm_hour < 0 || target.tm_hour > 23 ||
        target.tm_min < 0 || target.tm_min > 59 ||
        target.tm_sec < 0 || target.tm_sec > 59)
        return 0;

    local_now = localtime(&now);
    if (NULL == local_now)
        return 0;

    target.tm_year = local_now->tm_year;
    target.tm_mon = local_now->tm_mon;
    target.tm_mday = local_now->tm_mday;
    target.tm_isdst = -1;
    next = mktime(&target);
    if (next == (time_t)-1)
        return 0;

    if (next <= now) {
        /* Re-resolve tomorrow's local time using that date's DST rules. */
        target.tm_mday += 1;
        target.tm_isdst = -1;
        next = mktime(&target);
        if (next == (time_t)-1)
            return 0;
    }

    *delay = difftime(next, now);
    return *delay > 0.0;
}

static int valid_protocol_token_span(const char *text, size_t length,
    int allow_empty)
{
    size_t i;

    if (NULL == text || (!allow_empty && length == 0))
        return 0;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\0' || c <= 0x20 || c == 0x7f)
            return 0;
    }
    return 1;
}

 int valid_protocol_token(const char *text, int allow_empty)
{
    return text != NULL &&
        valid_protocol_token_span(text, strlen(text), allow_empty);
}

 int is_valid_script_name(const char *name, size_t len)
{
    size_t i;

    if (NULL == name || len == 0 || len >= DELAY_KEY_LEN ||
        (len == 1 && name[0] == '.') ||
        (len == 2 && name[0] == '.' && name[1] == '.'))
        return 0;

    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.'))
            return 0;
    }
    return 1;
}

#if 0
 void print_time(char* prefix)
{
    struct timeval tv;
    unsigned int ms;

    gettimeofday (&tv, NULL);
    ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
    printf("%s: %03u\n", prefix, ms);
}
#endif

static void sigint_cb (EV_P_ ev_signal *w, int revents) {
    ev_signal_stop (EV_A_ w);
    ev_break(EV_A_ EVBREAK_ALL);
}

 int is_safe_script_command(const char *cmd, size_t length)
{
    size_t i, component_start = 0;

    if (NULL == cmd || length == 0)
        return 0;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)cmd[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.' || c == '/'))
            return 0;
        if (c == '/') {
            if (i == component_start && i != 0)
                return 0;
            if (i - component_start == 2 &&
                cmd[component_start] == '.' && cmd[component_start + 1] == '.')
                return 0;
            component_start = i + 1;
        }
    }
    if (length == component_start ||
        (length - component_start == 2 &&
         cmd[component_start] == '.' && cmd[component_start + 1] == '.'))
        return 0;
    return 1;
}

 void sys_script(const char *cmd, size_t cmd_size)
{
    int written;
    size_t cmd_len;

    if (NULL == cmd || cmd_size == 0 || cmd[cmd_size - 1] != '\0')
        return;
    cmd_len = strnlen(cmd, cmd_size);
    if (cmd_len != cmd_size - 1 || cmd_len == 0 ||
        !is_safe_script_command(cmd, cmd_len)) {
        skipd_log(SKIPD_DEBUG, "invalid script path; not executed\n");
        return;
    }

    if (script_log_fd >= 0) {
        written = snprintf(static_buffer, sizeof(static_buffer),
            "%s >> /proc/self/fd/%d 2>&1 &\n", cmd, script_log_fd);
    } else {
        /* Fail closed on logging setup errors rather than redirecting through
         * a symlinkable pathname in the world-writable /tmp directory. */
        written = snprintf(static_buffer, sizeof(static_buffer),
            "%s > /dev/null 2>&1 &\n", cmd);
    }
    if (written < 0 || (size_t)written >= sizeof(static_buffer)) {
        static_buffer[0] = '\0';
        skipd_log(SKIPD_DEBUG, "script command too long; not executed\n");
        return;
    }
    (void)system(static_buffer);
    static_buffer[0] = '\0';
}

int log_level = LOG_ERR;
void emit_log(int level, char* line) {
    //TODO for level
    int syslog_level = LOG_ERR;
    syslog(syslog_level, "%s", line);
    //printf("%s", line);
}

void _skipd_logv(int filter, const char *format, va_list vl)
{
	char buf[256];

        //TODO for log_evel
	/* if (!(log_level & filter)) {
	    return;
        } */

	vsnprintf(buf, sizeof(buf), format, vl);
	buf[sizeof(buf) - 1] = '\0';

	emit_log(filter, buf);
}

void skipd_log(int filter, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	_skipd_logv(filter, format, ap);
	va_end(ap);
}

static void client_release(EV_P_ skipd_client* client) {
    //skipd_log(SKIPD_DEBUG, "closing client\n");

    ev_io_stop(EV_A_ &client->io_read);
    ev_io_stop(EV_A_ &client->io_write);
    close(client->fd);

    if(NULL != client->send) {
        free(client->send);
    }

    if(NULL != client->origin) {
        free(client->origin);
    }
    if (client->cursor) {
        SkipDBCursor_release(client->cursor);
        client->cursor = NULL;
    }
    if (client->in_doing && client->server) {
        if (client->server->in_doing > 0)
            client->server->in_doing--;
        client->in_doing = 0;
    }
    if (client->owned_key) {
        free(client->owned_key);
        client->owned_key = NULL;
        client->key = NULL;
    }

    free(client);
}

static void client_read_inner(EV_P_ skipd_client* client, int revents) {
    int rt;

    //process command
    assert(client->break_level == 0);

    rt = client_ccr_process(EV_A_ client);
    //skipd_log(SKIPD_DEBUG, "process:%d\n", rt);
    if(ccr_error_err1 == rt) {
        //Close it directly
        client_release(EV_A_ client);
    } else if(ccr_error_ok == rt) {
        //Just continue
        client->break_level = 0;
    } else if(rt < 0) {
        if(ccr_break_killed == client->break_level) {
            //wait for waiting completely
            if(0 == client->send_len) {
                //close it
                client_release(EV_A_ client);
            } else {
                memset(&client->ccr_process, 0, sizeof(struct ccrContextTag));
            }
        }
    } else if(rt > 0) {
        //OK and than reset
        memset(&client->ccr_process, 0, sizeof(struct ccrContextTag));
        client->break_level = 0;
    }
}

// This callback is called when client data is available
static void client_read(EV_P_ ev_io *w, int revents) {
    skipd_client* client = container_of(w, skipd_client, io_read);
    client_read_inner(EV_A_ client, revents);
}

static void client_write(EV_P_ ev_io *w, int revents) {
    skipd_client* client = container_of(w, skipd_client, io_write);
    int rt;

    //skipd_log(SKIPD_DEBUG, "begin for writing\n");

    rt = client_ccr_write(EV_A_ client);
    if(rt == ccr_error_ok)
        return; /* EAGAIN: keep coroutine state and wait for writability. */
    if(rt < 0 || ccr_break_killed == client->break_level) {
        client_release(EV_A_ client);
    } else {
        memset(&client->ccr_write, 0, sizeof(struct ccrContextTag));
        client->break_level = 0;

        /* Resume any request coroutine that yielded while preparing output. */
        client_read_inner(EV_A_ client, 0);
    }

    //TODO void event_active (struct event *ev, int res, short ncalls)
}


 int client_send_key(EV_P_ skipd_client* client, char* cmd, char* key, char* buf, int len) {
    char pref_buf[HEADER_PREFIX+1];
    size_t cmd_len, key_len, body_len;
    int n, resp_len;
    char *new_send;

    if(NULL == client || NULL == cmd || NULL == key || len < 0 ||
       (len > 0 && NULL == buf))
        return -1;

    cmd_len = strlen(cmd);
    key_len = strlen(key);
    if(cmd_len > READ_MAX || key_len > READ_MAX ||
       cmd_len > (size_t)READ_MAX - key_len ||
       cmd_len + key_len > (size_t)READ_MAX - 2 ||
       (size_t)len > (size_t)READ_MAX - cmd_len - key_len - 2)
        return -1;

    body_len = cmd_len + key_len + (size_t)len + 2;
    if(body_len == 0 || body_len > READ_MAX ||
       body_len > (size_t)INT_MAX - HEADER_PREFIX)
        return -1;
    resp_len = (int)(body_len + HEADER_PREFIX);

    memcpy(pref_buf, global_magic, MAGIC_LEN);
    snprintf(pref_buf + MAGIC_LEN, sizeof(pref_buf) - MAGIC_LEN, "%07d ", (int)body_len);

    if(NULL == client->send) {
        client->send = (char*)malloc(BUF_MAX + 1);
        if(NULL == client->send)
            return -1;
        client->send_max = BUF_MAX;
        client->send_len = 0;
    }

    if(resp_len > client->send_max) {
        new_send = (char*)malloc((size_t)resp_len + 1);
        if(NULL == new_send)
            return -1;
        free(client->send);
        client->send = new_send;
        client->send_max = resp_len;
        client->send_len = 0;
    }

    memcpy(client->send, pref_buf, HEADER_PREFIX);
    n = snprintf(client->send + HEADER_PREFIX,
                 (size_t)resp_len - HEADER_PREFIX + 1, "%s %s ", cmd, key);
    if(n < 0 || (size_t)n != cmd_len + key_len + 2)
        return -1;
    if(len > 0)
        memcpy(client->send + HEADER_PREFIX + n, buf, (size_t)len);
    client->send[resp_len] = '\0';
    client->send_len = resp_len;
    client->send_pos = 0;

    //switch to read state
    memset(&client->ccr_write, 0, sizeof(struct ccrContextTag));
    ev_io_stop(EV_A_ &client->io_read);
    ev_io_start(EV_A_ &client->io_write);

    return 0;
}

 int client_send(EV_P_ skipd_client* client, char* buf, int len) {
    char *command = NULL == client->command ? "errcmd" : client->command;
    char *key = NULL == client->key ? "errkey" : client->key;
    char *error = "response too large\n";

    if (!response_frame_fits(command, key, len))
        return client_send_key(EV_A_ client, "__error__", command,
            error, (int)strlen(error));
    return client_send_key(EV_A_ client, command, key, buf, len);
}

static int client_ccr_write(EV_P_ skipd_client* client) {
    struct ccrContextTag* ctx = &client->ccr_write;

    //stack
    ccrBeginContext
    int n;
    ccrEndContext(ctx);

    ccrBegin(ctx);
    while(client->send_pos < client->send_len) {
#ifdef MSG_NOSIGNAL
        CS->n = send(client->fd, client->send + client->send_pos,
                      (size_t)(client->send_len - client->send_pos),
                      MSG_NOSIGNAL);
#else
        CS->n = write(client->fd, client->send + client->send_pos,
                      client->send_len - client->send_pos);
#endif
        if(CS->n < 0) {
            if(errno == EINTR)
                continue;
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                ccrReturn(ctx, ccr_error_ok);
                continue;
            }

            client->send_len = 0;
            client->send_pos = 0;
            ccrStop(ctx, ccr_error_err1);
        }
        if(CS->n == 0) {
            client->send_len = 0;
            client->send_pos = 0;
            ccrStop(ctx, ccr_error_err1);
        }

        /* Keep send_len as the total frame length; send_pos is the offset. */
        client->send_pos += CS->n;
    }

    client->send_len = 0;
    client->send_pos = 0;
    ev_io_stop(EV_A_ &client->io_write);
    ev_io_start(EV_A_ &client->io_read);
    ccrFinish(ctx, ccr_error_ok1);
}

/* static void client_read_fix(skipd_client* client) {
    if(client->read_len > client->read_pos) {
        memmove(client->origin, client->origin + client->read_pos + 1, client->read_len - client->read_pos - 1);
    }
    client->read_len -= client->read_pos + 1;
    client->read_pos = 0;
} */

 int client_ccr_read_util(skipd_client* client, int step_len) {
    int buf_len;
    struct ccrContextTag* ctx = &client->ccr_read;

    //stack
    ccrBeginContext
    int n;
    int left;
    ccrEndContext(ctx);

    ccrBegin(ctx);

    /* A negative payload length becomes a huge size_t in recv(). */
    if(step_len <= 0 || step_len > READ_MAX) {
        ccrReturn(ctx, ccr_error_err2);
    }

    buf_len = _max(step_len, BUF_MAX);
    if(buf_len > client->origin_len) {
        if(NULL != client->origin) {
            free(client->origin);
        }
        client->origin = (char*)malloc((size_t)buf_len + 1);
        if(NULL == client->origin) {
            client->origin_len = 0;
            ccrReturn(ctx, ccr_error_err2);
        }
        client->origin_len = buf_len;
        client->origin[client->origin_len] = '\0';
    }
    client->read_pos = 0;
    client->read_len = 0;

    for(;;) {
        CS->left = step_len - client->read_len;
        assert(0 != CS->left);

        CS->n = recv(client->fd, client->origin + client->read_len, CS->left, 0);
        if (0 == CS->n) {
            //client closed
            ccrReturn(ctx, ccr_error_err1);
        } else if(CS->n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                client->break_level = ccr_break_all;
                ccrReturn(ctx, ccr_error_ok);
            } else {
                ccrReturn(ctx, ccr_error_err1);
            }
        } else {
            client->read_len += CS->n;
            if(client->read_len == step_len) {
                break;
            }
        }
    }
    /* Bound all subsequent string parsing to this exact protocol frame. */
    client->origin[step_len] = '\0';
    ccrFinish(ctx, ccr_error_ok1);
}


static skipd_client* client_new(int fd) {
    //int opt = 0;
    skipd_client* client = (skipd_client*)calloc(1, sizeof(skipd_client));
    if(NULL == client) {
        close(fd);
        return NULL;
    }
    client->fd = fd;
    client->send = NULL;
    client->origin = NULL;

    //setsockopt(client->fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    if(setnonblock(client->fd) < 0) {
        close(fd);
        free(client);
        return NULL;
    }
    ev_io_init(&client->io_read, client_read, client->fd, EV_READ);
    ev_io_init(&client->io_write, client_write, client->fd, EV_WRITE);

    return client;
}

// This callback is called when data is readable on the unix socket.
static void server_cb(EV_P_ ev_io *w, int revents) {
    int client_fd;
    skipd_client* client;

    skipd_server* server = container_of(w, skipd_server, io);

    while (1) {
        client_fd = accept(server->fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR || errno == ECONNABORTED)
                continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                skipd_log(SKIPD_DEBUG, "accept() failed errno=%i (%s)",
                    errno, strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;
        }

        //print_time("newclient");
        client = client_new(client_fd);
        if(NULL == client)
            continue;
        client->server = server;
        ev_io_start(EV_A_ &client->io_read);
    }
}


static struct option options[] = {
    { "help",	no_argument,		NULL, 'h' },
    { "db_path", required_argument,	NULL, 'd' },
    { "sock_path", required_argument,	NULL, 's' },
    { "daemon", 	required_argument,	NULL, 'D' },
    { NULL, 0, 0, 0 }
};

static int check_dbpath(skipd_server* server)
{
    struct stat s;
    size_t n;

    if (NULL == server)
        return -1;
    n = strnlen(server->db_path, sizeof(server->db_path));
    if (n == 0 || n >= sizeof(server->db_path))
        return -1;
    if (n > 1 && server->db_path[n - 1] == '/') {
        server->db_path[--n] = '\0';
    }

    /* The longest generated name is "<path>/switch.tmp", including NUL. */
    if (n > sizeof(server->db_path) - sizeof("/switch.tmp"))
        return -1;

    if (lstat(server->db_path, &s) != 0) {
        if (errno != ENOENT)
            return -1;
        if (mkdir(server->db_path, 0700) != 0 && errno != EEXIST)
            return -1;
        /* Re-check after mkdir (including EEXIST races) before using it. */
        if (lstat(server->db_path, &s) != 0)
            return -1;
    }

    /* skipd may write the active-database marker as root. Do not follow a
     * directory symlink or accept a directory another user can modify. */
    if (!S_ISDIR(s.st_mode) || s.st_uid != geteuid() ||
        (s.st_mode & (S_IWGRP | S_IWOTH)) != 0)
        return -1;

    return 0;
}

int main(int argc, char **argv)
{
    int n = 0, daemon = 0, max_queue = 64;
    int syslog_options = LOG_PID | LOG_PERROR | LOG_DEBUG;
    skipd_server *server;
    //struct ev_periodic every_few_seconds;
    EV_P  = ev_default_loop(0);

    global_server = (skipd_server*)calloc(1, sizeof(skipd_server));
    if(NULL == global_server) {
        perror("skipd: calloc");
        return EXIT_FAILURE;
    }
    server = global_server;

    strcpy(server->sock_path, "/tmp/.skipd_server_sock");

#if 1
    strcpy(server->pid_path, "/tmp/.skipd_pid");
    strcpy(server->db_path, "/etc/storage/db");
    daemon = 1;
#endif

    while (n >= 0) {
            n = getopt_long(argc, argv, "hd:D:s:", options, NULL);
            if (n < 0)
                    continue;
            switch (n) {
            case 'D':
                //TODO fix me if optarg is bigger than PATH_MAX
                //strcpy(server->pid_path, optarg);
                daemon = 0;
                break;
            case 'd': {
                size_t path_len;
                if(NULL == optarg ||
                   (path_len = strlen(optarg)) >= sizeof(server->db_path)) {
                    fprintf(stderr, "Database path too long\n");
                    return EXIT_FAILURE;
                }
                memcpy(server->db_path, optarg, path_len + 1);
                break;
            }
            case 's': {
                size_t path_len;
                if(NULL == optarg ||
                   (path_len = strlen(optarg)) >= sizeof(server->sock_path) ||
                   path_len >= sizeof(server->socket.sun_path)) {
                    fprintf(stderr, "Socket path too long\n");
                    return EXIT_FAILURE;
                }
                memcpy(server->sock_path, optarg, path_len + 1);
                break;
            }
            case 'h':
                fprintf(stderr, "Usage: skipd [-h] [-d DB_PATH] [-s SOCKET_PATH] [-D ARG]\n");
                return EXIT_SUCCESS;
            case '?':
            default:
                fprintf(stderr, "skipd: invalid command-line option\n");
                return EXIT_FAILURE;
            }
    }

    if (optind < argc) {
        fprintf(stderr, "skipd: unexpected positional argument\n");
        return EXIT_FAILURE;
    }

    if(0 == strlen(server->db_path) || (0 != check_dbpath(server))) {
        //syslog not initialize hear
        fprintf(stderr, "Database path error\n");
        return 1;
    }

    setlogmask(LOG_UPTO (LOG_DEBUG));
    openlog("skipd", syslog_options, LOG_DAEMON);
    script_log_fd = open_script_log();
    if (script_log_fd < 0)
        skipd_log(SKIPD_DEBUG, "secure script log unavailable; command output disabled\n");

    //kill -SIGUSR2 22459
    signal(SIGPIPE, SIG_IGN);
    signal(SIGABRT, SIG_IGN);
    ev_signal_init (&signal_watcher, sigint_cb, SIGINT);
    ev_signal_start (EV_A_ &signal_watcher);
    ev_signal_init (&signal_watcher2, sigint_cb, SIGTERM);
    ev_signal_start (EV_A_ &signal_watcher2);

    // Create and bind the service before daemonizing. A second invocation
    // must fail the socket-instance lock check before it can overwrite the PID
    // file of an already running daemon.
    if (server_init(server, max_queue) != 0)
        return EXIT_FAILURE;

    if (daemon)
        skipd_daemonize(server->pid_path);

    /* Register persisted timers before accepting clients. Otherwise a delay/time
     * command created during a delayed startup scan would be scheduled twice. */
    server_init_delay(EV_A_ server);
    server_init_time(EV_A_ server);
    if (!SkipDB_isOpen(server->db)) {
        skipd_log(SKIPD_DEBUG,
            "database became invalid while restoring scheduled tasks; refusing startup\n");
        close(server->fd);
        server->fd = -1;
        SkipDB_close(server->db);
        return EXIT_FAILURE;
    }

    // Get notified whenever the socket is ready to read
    ev_io_init(&server->io, server_cb, server->fd, EV_READ);
    ev_io_start(EV_A_ &server->io);

    //ev_periodic_init(&every_few_seconds, not_blocked, 0, 5, 0);
    //ev_periodic_start(EV_A_ &every_few_seconds);
    //update at 2s after write
    ev_timer_init(EV_A_ &server->watcher, server_sync_tick, 0.0, 2.0);
    //ev_timer_start(EV_A_ &server->watcher);

    // Run our loop, ostensibly forever
    ev_loop(EV_A_ 0);

    //sync at exit
    SkipDB_beginTransaction(server->db);
    SkipDB_commitTransaction(server->db);
	
    SkipDB_close(server->db);
    skipd_log(SKIPD_DEBUG, "exit..\n");
    // This point is only ever reached if the loop is manually exited
    close(server->fd);
    if (script_log_fd >= 0)
        close(script_log_fd);
    return EXIT_SUCCESS;
}

