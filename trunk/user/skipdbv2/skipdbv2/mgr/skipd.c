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

char *global_magic = MAGIC;
skipd_server *global_server;
ev_signal signal_watcher;
ev_signal signal_watcher2;
char static_buffer[STATIC_BUF_LEN + 1];

char *find_terminated_value_separator(Datum value)
{
    if (NULL == value.data || value.size < 2 ||
        ((char*)value.data)[value.size - 1] != '\0' ||
        memchr(value.data, '\0', value.size - 1) != NULL)
        return NULL;
    return (char*)memchr(value.data, ' ', value.size - 1);
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

int valid_protocol_token_span(const char *text, size_t length,
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
    if (skipd_script_init() != 0)
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
    if (server_init(server, max_queue) != 0) {
        skipd_script_cleanup();
        return EXIT_FAILURE;
    }

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
        skipd_script_cleanup();
        return EXIT_FAILURE;
    }

    // Get notified whenever the socket is ready to read
    ev_io_init(&server->io, skipd_server_accept_cb, server->fd, EV_READ);
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
    skipd_script_cleanup();
    return EXIT_SUCCESS;
}

