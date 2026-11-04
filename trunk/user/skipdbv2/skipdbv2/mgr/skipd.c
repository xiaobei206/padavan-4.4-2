#define _XOPEN_SOURCE

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
#include <unistd.h>
#include <syslog.h>
#include <ev.h>

#include "coroutine.h"
#include "SkipDB.h"
#include "skipd.h"

typedef struct _skipd_server {
    ev_io io;
    int fd;
    struct sockaddr_un socket;
    int socket_len;

    ev_timer watcher;
    int to_commit;

    SkipDB *db;
    int curr_db;
    int switch_mark;
    int in_doing;

    int daemon;
    char db_path[SK_PATH_MAX];
    char sock_path[SK_PATH_MAX];
    char pid_path[SK_PATH_MAX];
} skipd_server;

/* enum skipd_client_state {
    client_state_read = 0,
    client_state_write,
    client_state_closing
}; */

enum ccr_break_state {
    ccr_break_continue = 0,
    ccr_break_curr,
    ccr_break_all,
    ccr_break_killed
};

enum ccr_error_code {
    ccr_error_err1 = -1,
    ccr_error_err2 = -2,
    ccr_error_err3 = -3,
    ccr_error_ok = 0, /* 此命令暂时中断，需等待再次执行 */
    ccr_error_ok1 = 1, /* 此命令成功结束执行 */
    ccr_error_ok2 = 2
};

typedef struct _skipd_client {
    ev_io io_read;
    ev_io io_write;

    struct ccrContextTag ccr_read;
    struct ccrContextTag ccr_write;
    struct ccrContextTag ccr_process;
    struct ccrContextTag ccr_runcmd;
    int break_level;

    int fd;

    char* command;
    char* key;
    int data_len;

    char* origin;
    int origin_len;
    int read_len;
    int read_pos;

    char* send;
    int send_max;
    int send_len;
    int send_pos;

    SkipDBCursor *cursor;
    char *owned_key;
    int in_doing;
    skipd_server* server;
} skipd_client;

typedef struct _delay_cmd {
    ev_timer watcher;
    char key[DELAY_KEY_LEN + DELAY_PREFIX_LEN];
    int tick;

    skipd_server* server;
} delay_cmd;

typedef struct _time_cmd {
    ev_timer watcher;
    char key[DELAY_KEY_LEN + DELAY_PREFIX_LEN];

    skipd_server* server;
} time_cmd;

/* declare */
extern int SkipDB_maxPos(SkipDB* self);
extern SkipDBRecord* SkipDB_list_first(SkipDB* self, Datum k, SkipDBCursor** pcur);
extern SkipDBRecord* SkipDB_list_next(SkipDB* self, Datum k, SkipDBCursor* cursor);
static void server_sync(EV_P_ skipd_server* server);
static void server_switch(skipd_server* server);

void skipd_daemonize(char * path);
static int setnonblock(int fd);
static int client_ccr_process(EV_P_ skipd_client* client);
static int client_ccr_write(EV_P_ skipd_client* client);

char* global_magic = MAGIC;
skipd_server* global_server;
ev_signal signal_watcher;
ev_signal signal_watcher2;

#define STATIC_BUF_LEN 511
char static_buffer[STATIC_BUF_LEN+1];
static int script_log_fd = -1;

static int open_script_log(void)
{
#ifdef O_NOFOLLOW
    struct stat st;
    int fd = open("/tmp/skipd.log",
        O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0)
        return -1;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
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

static char *find_terminated_value_separator(Datum value)
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

static int value_fits_list_frame(const char *key, size_t value_len)
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
static int next_daily_delay(const char *time_text, time_t now, double *delay)
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
    target.tm_isdst = local_now->tm_isdst;
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

static int is_valid_script_name(const char *name)
{
    const unsigned char *p;
    size_t len;

    if(NULL == name || '\0' == *name)
        return 0;
    len = strlen(name);
    if(len >= DELAY_KEY_LEN || 0 == strcmp(name, ".") || 0 == strcmp(name, ".."))
        return 0;

    for(p = (const unsigned char*)name; *p; ++p) {
        if(!((*p >= 'a' && *p <= 'z') ||
             (*p >= 'A' && *p <= 'Z') ||
             (*p >= '0' && *p <= '9') ||
             *p == '_' || *p == '-' || *p == '.'))
            return 0;
    }
    return 1;
}

#if 0
static void print_time(char* prefix)
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

static void sys_script(const char *cmd, size_t cmd_size)
{
    int written;
    size_t cmd_len;

    if (NULL == cmd || cmd_size == 0 || cmd[cmd_size - 1] != '\0')
        return;
    cmd_len = strnlen(cmd, cmd_size);
    if (cmd_len != cmd_size - 1)
        return;

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

static void delay_cmd_cb(EV_P_ ev_timer* watcher, int revents) {
    char* p1;
    int tmpi;
    delay_cmd* delay_obj = container_of(watcher, delay_cmd, watcher);
    Datum dkey = Datum_FromCString_(delay_obj->key);
    Datum dvalue = SkipDB_at_(delay_obj->server->db, dkey);
    do {
        if(NULL == dvalue.data) {
            skipd_log(SKIPD_DEBUG, "delay cmd:%s, dvalue.data is null\n", dkey.data);
            break;
        }

        p1 = find_terminated_value_separator(dvalue);
        if(NULL == p1) {
            skipd_log(SKIPD_DEBUG, "delay cmd:%s, dvalue.data error\n", dkey.data);
            break;
        }

        tmpi = (unsigned char*)p1 - dvalue.data;
        if(tmpi > STATIC_BUF_LEN) {
            skipd_log(SKIPD_DEBUG, "delay cmd:%s, dvalue.data error\n", dkey.data);
            break;
        }
        memcpy(static_buffer, dvalue.data, tmpi);
        static_buffer[tmpi] = '\0';
        if(S2ISUCCESS != str2int(&tmpi, static_buffer, 10) || tmpi < 0) {
            skipd_log(SKIPD_DEBUG, "delay cmd:%s, str2int error\n", dkey.data);
            break;
        }

        p1++;
        sys_script(p1, dvalue.size - (size_t)(p1 - (char*)dvalue.data));
        if(0 == tmpi) {
            skipd_log(SKIPD_DEBUG, "delay cmd:%s, delay delete, tmpi = 0\n", dkey.data);
            SkipDB_beginTransaction(delay_obj->server->db);
            SkipDB_removeAt_(delay_obj->server->db, dkey);
            server_sync(EV_A_ delay_obj->server);
            if (!SkipDB_isOpen(delay_obj->server->db))
                skipd_log(SKIPD_DEBUG, "delay cmd:%s, failed to remove one-shot task\n", dkey.data);
            break;
        }

        delay_obj->tick = tmpi;
        watcher->repeat = tmpi;
        ev_timer_again(EV_A_ watcher);
        return;

    } while(0);

    ev_timer_stop(EV_A_ watcher);
    free(delay_obj);
}

static void time_cmd_cb(EV_P_ ev_timer* watcher, int revents) {
    char *p1;
    char time_text[16];
    int time_len;
    double timer_delay;
    time_t now;
    time_cmd* time_obj = container_of(watcher, time_cmd, watcher);
    Datum dkey = Datum_FromCString_(time_obj->key);
    Datum dvalue = SkipDB_at_(time_obj->server->db, dkey);

    if (NULL == dvalue.data) {
        skipd_log(SKIPD_DEBUG, "time %s not found\n", dkey.data);
        ev_timer_stop(EV_A_ watcher);
        free(time_obj);
        return;
    }

    p1 = find_terminated_value_separator(dvalue);
    if (NULL == p1) {
        skipd_log(SKIPD_DEBUG, "time %s value error\n", dkey.data);
        ev_timer_stop(EV_A_ watcher);
        free(time_obj);
        return;
    }

    time_len = (int)(p1 - (char*)dvalue.data);
    if (time_len <= 0 || (size_t)time_len >= sizeof(time_text)) {
        skipd_log(SKIPD_DEBUG, "time %s schedule value error\n", dkey.data);
        ev_timer_stop(EV_A_ watcher);
        free(time_obj);
        return;
    }
    memcpy(time_text, dvalue.data, (size_t)time_len);
    time_text[time_len] = '\0';

    sys_script(p1 + 1,
        dvalue.size - (size_t)(p1 + 1 - (char*)dvalue.data));

    now = time(NULL);
    if (!next_daily_delay(time_text, now, &timer_delay)) {
        skipd_log(SKIPD_DEBUG, "time %s next schedule calculation failed\n",
            dkey.data);
        ev_timer_stop(EV_A_ watcher);
        free(time_obj);
        return;
    }

    /* Re-arm as a one-shot timer using the next local calendar occurrence. */
    ev_timer_stop(EV_A_ watcher);
    ev_timer_init(watcher, time_cmd_cb, timer_delay, 0.0);
    ev_timer_start(EV_A_ watcher);
}

static int client_send_key(EV_P_ skipd_client* client, char* cmd, char* key, char* buf, int len) {
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

static int client_send(EV_P_ skipd_client* client, char* buf, int len) {
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
        CS->n = write(client->fd, client->send + client->send_pos,
                      client->send_len - client->send_pos);
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

static int client_ccr_read_util(skipd_client* client, int step_len) {
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

static int client_run_command(EV_P_ skipd_client* client)
{
    char *p1, *p2;
    int tmpi, tmp2, write_ok;
    long long arithmetic_result;
    double timer_delay;
    Datum dkey, dvalue;
    delay_cmd* delay_obj;
    time_cmd* time_obj;
    struct ccrContextTag* ctx = &client->ccr_read;

    //stack
    ccrBeginContext
    int n;
    SkipDBCursor* cursor;
    SkipDBRecord* record;
    Datum skey;
    ccrEndContext(ctx);

    ccrBegin(ctx);

    p1 = client->origin;
    p2 = strstr(p1, " ");
    if(NULL == p2) {
        ccrStop(ctx, ccr_error_err2);
    }
    *p2 = '\0';
    client->command = p1;

    if(!strcmp(client->command, "set")) {
        p1 = p2+1;
        p2 = strstr(p1, " ");
        if(NULL == p2) {
            ccrStop(ctx, ccr_error_err2);
        }
        *p2 = '\0';
        client->key = p1;
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);
        dvalue = Datum_FromData_length_((unsigned char*)p1, client->data_len - (p1 - client->origin));
        if (!value_fits_list_frame(client->key, dvalue.size)) {
            p1 = "value too big to list\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        //OK a little hack hear
        //print_time("beginset");
        SkipDB_beginTransaction(client->server->db);
        write_ok = SkipDB_at_put_(client->server->db, dkey, dvalue);
        server_sync(EV_A_ client->server);

        if (!write_ok || !SkipDB_isOpen(client->server->db))
            p1 = "database error\n";
        else
            p1 = "ok\n";
        client_send(EV_A_ client, p1, strlen(p1));
        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "ram")) {
        p1 = p2+1;
        p2 = strstr(p1, " ");
        if(NULL == p2) {
            ccrStop(ctx, ccr_error_err2);
        }
        *p2 = '\0';
        client->key = p1;
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);
        dvalue = Datum_FromData_length_((unsigned char*)p1, client->data_len - (p1 - client->origin));
        if (!value_fits_list_frame(client->key, dvalue.size)) {
            p1 = "value too big to list\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        write_ok = SkipDB_at_put_(client->server->db, dkey, dvalue);
        if (!write_ok || !SkipDB_isOpen(client->server->db))
            p1 = "database error\n";
        else
            p1 = "ok\n";
        client_send(EV_A_ client, p1, strlen(p1));
        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "get")) {
        p1 = p2+1;
        client->key = p1;
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);
        dvalue = SkipDB_at_(client->server->db, dkey);
        if(NULL == dvalue.data) {
            p1 = "none\n";
            if (client_send(EV_A_ client, p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
        } else if (!value_fits_list_frame(client->key, dvalue.size)) {
            p1 = "response too large\n";
            if (client_send_key(EV_A_ client, "__error__",
                    client->command, p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
        } else if (client_send(EV_A_ client,
                (char*)dvalue.data, (int)dvalue.size) != 0) {
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }
        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "replace")) {
        p1 = p2+1;
        p2 = strstr(p1, " ");
        if(NULL == p2) {
            ccrStop(ctx, ccr_error_err2);
        }
        *p2 = '\0';
        client->key = p1;
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);
        /* dvalue = SkipDB_at_(client->server->db, dkey);
        if(NULL == dvalue.data) {
            p1 = "none\n";
            client_send(EV_A_ client, p1, strlen(p1));
        } else {
            //exists replace it
            client_send(EV_A_ client, (char*)dvalue.data, dvalue.size);
            dvalue = Datum_FromData_length_((unsigned char*)p1, client->data_len - (p1 - client->origin));
            SkipDB_beginTransaction(client->server->db);
            SkipDB_at_put_(client->server->db, dkey, dvalue);
            //SkipDB_commitTransaction(client->server->db);
            server_sync(EV_A_ client->server);
        } */
        dvalue = Datum_FromData_length_((unsigned char*)p1, client->data_len - (p1 - client->origin));
        if (!value_fits_list_frame(client->key, dvalue.size)) {
            p1 = "value too big to list\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        SkipDB_beginTransaction(client->server->db);
        write_ok = SkipDB_replace_put_(client->server->db, dkey, dvalue);
        server_sync(EV_A_ client->server);
        if (!SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            client_send(EV_A_ client, p1, strlen(p1));
        } else if (write_ok) {
            client_send(EV_A_ client, (char*)dvalue.data, dvalue.size);
        } else {
            p1 = "none\n";
            client_send(EV_A_ client, p1, strlen(p1));
        }
        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "list")) {
        p1 = p2+1;
        client->key = p1;
        p1 = p2+1;

        client->server->in_doing++;
        client->in_doing = 1;
        CS->skey = Datum_FromCString_(client->key);
        CS->record = SkipDB_list_first(client->server->db, CS->skey, &CS->cursor);
        client->cursor = CS->cursor;
        while(NULL != CS->record) {
            dkey = SkipDBRecord_keyDatum(CS->record);
            dvalue = SkipDBRecord_valueDatum(CS->record);
            if (client_send_key(EV_A_ client, client->command,
                    (char*)dkey.data, (char*)dvalue.data, dvalue.size) != 0) {
                p1 = "list response error\n";
                if (client_send_key(EV_A_ client, "__error__", "list",
                        p1, strlen(p1)) == 0) {
                    /* The error frame is queued; release the suspended cursor
                     * before yielding, then terminate this list coroutine once
                     * the frame has been written. Do not continue the stream. */
                    if (CS->cursor) {
                        SkipDBCursor_release(CS->cursor);
                        CS->cursor = NULL;
                    }
                    client->cursor = NULL;
                    if (client->in_doing) {
                        if (client->server->in_doing > 0)
                            client->server->in_doing--;
                        client->in_doing = 0;
                    }
                    ccrReturn(ctx, ccr_error_ok);
                    ccrStop(ctx, ccr_error_ok1);
                }
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok);

            CS->record = SkipDB_list_next(client->server->db, CS->skey, CS->cursor);
        }

        /*
         * Use a distinct command token for the terminal frame. A stored
         * value may legitimately equal "__end__\n", so the payload alone
         * must never be treated as a list terminator.
         */
        p1 = "__end__\n";
        if (client_send_key(EV_A_ client, "__end__", "list",
                p1, strlen(p1)) != 0) {
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }
        ccrReturn(ctx, ccr_error_ok);

        if (CS->cursor) {
            SkipDBCursor_release(CS->cursor);
            CS->cursor = NULL;
        }
        client->cursor = NULL;
        if (client->in_doing) {
            client->server->in_doing--;
            client->in_doing = 0;
        }

        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "remove")) {
        p1 = p2+1;
        client->key = p1;
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);

        SkipDB_beginTransaction(client->server->db);
        SkipDB_removeAt_(client->server->db, dkey);
        //SkipDB_commitTransaction(client->server->db);
        server_sync(EV_A_ client->server);
        if (!SkipDB_isOpen(client->server->db))
            p1 = "database error\n";
        else
            p1 = "ok\n";
        client_send(EV_A_ client, p1, strlen(p1));
        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "delay")) {
        p1 = p2+1;
        p2 = strstr(p1, " ");
        if(NULL == p2) {
            ccrStop(ctx, ccr_error_err2);
        }
        *p2 = '\0';
        client->key = p1;
        p1 = p2+1;

        if(strlen(client->key) >= DELAY_KEY_LEN) {
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        dvalue = Datum_FromData_length_((unsigned char*)p1, client->data_len - (p1 - client->origin));
        if (NULL == find_terminated_value_separator(dvalue)) {
            p1 = "value param error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        //dvalue.data[dvalue.size-1] = '\0';
        //FIXEM hard code hear
        if(strlen((char*)dvalue.data) >= 300) {
            p1 = "value too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        p2  = strstr(p1, " ");
        if(NULL == p2) {
            p1 = "value param error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        tmpi = (int)(p2 - p1);
        memcpy(static_buffer, p1, tmpi);
        static_buffer[tmpi] = '\0';
        if(S2ISUCCESS != str2int(&tmpi, static_buffer, 10) || tmpi < 0) {
            p1 = "delay tick error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        //All ok, we create a delay object
        delay_obj = calloc(1, sizeof(delay_cmd));
        if(NULL == delay_obj) {
            p1 = "memory error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        tmp2 = snprintf(delay_obj->key, sizeof(delay_obj->key),
            "__delay__%s", client->key);
        if (tmp2 < 0 || (size_t)tmp2 >= sizeof(delay_obj->key)) {
            free(delay_obj);
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        client->key = delay_obj->key;
        delay_obj->tick = tmpi;
        delay_obj->server = client->server;

        dkey = Datum_FromCString_(client->key);
        /* Do not overwrite an existing scheduled command on duplicate add. */
        if (SkipDB_exists(client->server->db, dkey)) {
            p1 = "exists\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(delay_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }
        SkipDB_beginTransaction(client->server->db);
        write_ok = SkipDB_at_put_(client->server->db, dkey, dvalue);
        server_sync(EV_A_ client->server);
        if (!write_ok || !SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(delay_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }
        ev_timer_init(&delay_obj->watcher, delay_cmd_cb, delay_obj->tick, delay_obj->tick);
        ev_timer_start(EV_A_ &delay_obj->watcher);

        p1 = "delay_ok\n";
        client_send(EV_A_ client, p1, strlen(p1));

        ccrReturn(ctx, ccr_error_ok1);
    } else if(!strcmp(client->command, "time")) {
        p1 = p2+1;
        p2 = strstr(p1, " ");
        if(NULL == p2) {
            ccrStop(ctx, ccr_error_err2);
        }
        *p2 = '\0';
        client->key = p1;
        p1 = p2+1;

        if(strlen(client->key) >= DELAY_KEY_LEN) {
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        p2 = strstr(p1, " ");
        if(NULL == p2) {
            p1 = "value error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        tmpi = (int)(p2 - p1);
        if(tmpi <= 0 || (size_t)tmpi >= sizeof(static_buffer)) {
            p1 = "time value too long\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        memcpy(static_buffer, p1, (size_t)tmpi);
        static_buffer[tmpi] = '\0';

        if (!next_daily_delay(static_buffer, time(NULL), &timer_delay)) {
            p1 = "time schedule error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        dvalue = Datum_FromData_length_((unsigned char*)p1, client->data_len - (p1 - client->origin));
        if (NULL == find_terminated_value_separator(dvalue)) {
            p1 = "value param error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        //dvalue.data[dvalue.size-1] = '\0';
        //FIXEM hard code hear
        if(strlen((char*)dvalue.data) >= 300) {
            p1 = "value too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        time_obj = calloc(1, sizeof(time_cmd));
        if(NULL == time_obj) {
            p1 = "memory error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        tmp2 = snprintf(time_obj->key, sizeof(time_obj->key),
            "__time__%s", client->key);
        if (tmp2 < 0 || (size_t)tmp2 >= sizeof(time_obj->key)) {
            free(time_obj);
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        client->key = time_obj->key;
        dkey = Datum_FromCString_(client->key);
        /* Do not overwrite an existing scheduled command on duplicate add. */
        if (SkipDB_exists(client->server->db, dkey)) {
            p1 = "exists\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(time_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }
        /* The delay was validated above, before writing the task. */

        SkipDB_beginTransaction(client->server->db);
        write_ok = SkipDB_at_put_(client->server->db, dkey, dvalue);
        server_sync(EV_A_ client->server);
        if (!write_ok || !SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(time_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }
        time_obj->server = client->server;
        ev_timer_init(&time_obj->watcher, time_cmd_cb, timer_delay, 0.0);
        ev_timer_start(EV_A_ &time_obj->watcher);
        p1 = "ok\n";
        client_send(EV_A_ client, p1, strlen(p1));
        ccrReturn(ctx, ccr_error_ok1);

    } else if(!strcmp(client->command, "fire")) {
        int path_len;
        p1 = p2+1;
        /* This argument becomes both a filesystem path and a shell command. */
        if(!is_valid_script_name(p1)) {
            p1 = "invalid script name\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        client->key = (char*)malloc(DELAY_KEY_LEN*2);
        if(NULL == client->key) {
            p1 = "memory error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        client->owned_key = client->key;
        path_len = snprintf(client->key, DELAY_KEY_LEN*2, "/usr/bin/%s.sh", p1);
        if(path_len < 0 || path_len >= DELAY_KEY_LEN*2) {
            free(client->owned_key);
            client->owned_key = NULL;
            client->key = NULL;
            p1 = "script path too long\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        if(-1 != access(client->key, F_OK)) {
            //file exist, then run script
            sys_script(client->key, strlen(client->key) + 1);

            //send end
            p1 = "ok\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(client->owned_key);
            client->owned_key = NULL;
            client->key = NULL;
            ccrReturn(ctx, ccr_error_ok1);
        } else {
            path_len = snprintf(client->key, DELAY_KEY_LEN*2, "/jffs/scripts/%s.sh", p1);
            if(path_len < 0 || path_len >= DELAY_KEY_LEN*2) {
                free(client->owned_key);
                client->owned_key = NULL;
                client->key = NULL;
                p1 = "script path too long\n";
                client_send(EV_A_ client, p1, strlen(p1));
                ccrReturn(ctx, ccr_error_ok1);
            }
            if(-1 != access(client->key, F_OK)) {
                //file exist, then run script
                sys_script(client->key, strlen(client->key) + 1);

                //send end
                p1 = "ok\n";
                client_send(EV_A_ client, p1, strlen(p1));
                free(client->owned_key);
                client->owned_key = NULL;
                client->key = NULL;
                ccrReturn(ctx, ccr_error_ok1);
            }
        }

        snprintf(client->key, DELAY_KEY_LEN*2, "__event__%s", p1);
        p1 = p2+1;

        CS->n = 0;
        CS->skey = Datum_FromCString_(client->key);
        client->server->in_doing++;
        client->in_doing = 1;
        CS->record = SkipDB_list_first(client->server->db, CS->skey, &CS->cursor);
        client->cursor = CS->cursor;
        while(NULL != CS->record) {
            dkey = SkipDBRecord_keyDatum(CS->record);
            dvalue = SkipDBRecord_valueDatum(CS->record);
            sys_script((char*)dvalue.data, dvalue.size);

            CS->record = SkipDB_list_next(client->server->db, CS->skey, CS->cursor);
            CS->n++;
        }

        //send end
        p1 = "ok\n";
        client_send(EV_A_ client, p1, strlen(p1));
        ccrReturn(ctx, ccr_error_ok);

        if (CS->cursor) {
            SkipDBCursor_release(CS->cursor);
            CS->cursor = NULL;
        }
        client->cursor = NULL;
        if (client->in_doing) {
            client->server->in_doing--;
            client->in_doing = 0;
        }
        if (client->owned_key) {
            free(client->owned_key);
            client->owned_key = NULL;
        }
        client->key = NULL;

        ccrReturn(ctx, ccr_error_ok1);
    } else if((!strcmp(client->command, "inc")) || (!strcmp(client->command, "desc"))) {
        p1 = p2+1;
        p2 = strstr(p1, " ");
        if(NULL == p2) {
            ccrStop(ctx, ccr_error_err2);
        }
        *p2 = '\0';
        client->key = p1;
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);

        if(S2ISUCCESS != str2int(&tmpi, p1, 10)) {
            p1 = "error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }

        dvalue = SkipDB_at_(client->server->db, dkey);
        if(NULL == dvalue.data) {
            tmp2 = 0;
        } else {
            char numeric_value[32];
            size_t numeric_len = dvalue.size;

            /* A stored Datum is length-delimited and is not always NUL-terminated. */
            if (numeric_len > sizeof(numeric_value) - 1) {
                p1 = "error\n";
                client_send(EV_A_ client, p1, strlen(p1));
                ccrReturn(ctx, ccr_error_ok1);
            }
            if (numeric_len > 0 && dvalue.data[numeric_len - 1] == '\0')
                --numeric_len;
            if (numeric_len == 0 ||
                memchr(dvalue.data, '\0', numeric_len) != NULL) {
                p1 = "error\n";
                client_send(EV_A_ client, p1, strlen(p1));
                ccrReturn(ctx, ccr_error_ok1);
            }
            memcpy(numeric_value, dvalue.data, numeric_len);
            numeric_value[numeric_len] = '\0';
            if(S2ISUCCESS != str2int(&tmp2, numeric_value, 10)) {
                p1 = "error\n";
                client_send(EV_A_ client, p1, strlen(p1));
                ccrReturn(ctx, ccr_error_ok1);
            }
        }

        if(!strcmp(client->command, "inc"))
            arithmetic_result = (long long)tmp2 + (long long)tmpi;
        else
            arithmetic_result = (long long)tmp2 - (long long)tmpi;
        if(arithmetic_result > INT_MAX || arithmetic_result < INT_MIN) {
            p1 = "integer overflow\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        snprintf(static_buffer, sizeof(static_buffer), "%lld", arithmetic_result);
        dvalue = Datum_FromCString_(static_buffer);
        SkipDB_beginTransaction(client->server->db);
        write_ok = SkipDB_at_put_(client->server->db, dkey, dvalue);
        server_sync(EV_A_ client->server);
        if (!write_ok || !SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            client_send(EV_A_ client, p1, strlen(p1));
        } else {
            client_send(EV_A_ client, (char*)dvalue.data, dvalue.size);
        }
        ccrReturn(ctx, ccr_error_ok1);
    }

    ccrFinish(ctx, ccr_error_err2);
}

static int client_ccr_process(EV_P_ skipd_client* client)
{
    char* p;
    struct ccrContextTag* ctx = &client->ccr_process;

    //stack
    ccrBeginContext
    int rt;
    int n;
    int left;
    ccrEndContext(ctx);

    ccrBegin(ctx);

    for(;;) {

        /* Sub routine */
        memset(&client->ccr_read, 0, sizeof(struct ccrContextTag));
        for(;;) {
            CS->rt = client_ccr_read_util(client, HEADER_PREFIX);
            if((ccr_error_err1 != CS->rt) && (CS->rt < 0)) {
                /* socket not closed but has some other error*/
                p = "command no found\n";
                client_send(EV_A_ client, p, strlen(p));
                client->break_level = ccr_break_killed;
                ccrStop(ctx, CS->rt);
            }

            /* TODO if(client->break_level >= ccr_break_all) {
                ccrReturn(ctx, CS->rt);
            } else if(ccr_break_curr == client->break_level) {
                client->break_level = ccr_break_continue;
                ccrReturn(ctx, CS->rt);
            } */

            /* OK */
            if(CS->rt > 0) {
                break;
            }
            ccrReturn(ctx, CS->rt);
        }

        assert(CS->rt > 0);
        if(0 != memcmp(client->origin, global_magic, MAGIC_LEN)) {
            p = "magic not found\n";
            client_send(EV_A_ client, p, strlen(p));
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }

        if(client->origin[HEADER_PREFIX-1] != ' ') {
            p = "header prefix error\n";
            client_send(EV_A_ client, p, strlen(p));
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }
        client->origin[HEADER_PREFIX-1] = '\0';

        if(S2ISUCCESS != str2int(&client->data_len, client->origin+MAGIC_LEN, 10)) {
            p = "data len error\n";
            client_send(EV_A_ client, p, strlen(p));
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }

        /* Sub routine */
        memset(&client->ccr_read, 0, sizeof(struct ccrContextTag));
        for(;;) {
            CS->rt = client_ccr_read_util(client, client->data_len);
            if((ccr_error_err1 != CS->rt) && (CS->rt < 0)) {
                p = "read data error\n";
                client_send(EV_A_ client, p, strlen(p));
                client->break_level = ccr_break_killed;
                ccrStop(ctx, CS->rt);
            }

            if(CS->rt > 0) {
                break;
            }
            ccrReturn(ctx, CS->rt);
        }

        assert(CS->rt > 0);
        client->origin[client->data_len] = '\0';
        //TODO must end with \n ?
        if('\n' != client->origin[client->data_len-1]) {
            p = "value error\n";
            client_send(EV_A_ client, p, strlen(p));
            client->break_level = ccr_break_killed;
            ccrReturn(ctx, ccr_error_err2);
        }
        client->origin[client->data_len-1] = '\0';

        // Command subroutine
        memset(&client->ccr_runcmd, 0, sizeof(struct ccrContextTag));
        for(;;) {
            CS->rt = client_run_command(EV_A_ client);
            if(CS->rt < 0) {
                if (client->break_level != ccr_break_killed) {
                    p = "run command error\n";
                    client_send(EV_A_ client, p, strlen(p));
                }
                client->break_level = ccr_break_killed;
                ccrReturn(ctx, CS->rt);
            }
            if(CS->rt > 0) {
                break;
            }
            ccrReturn(ctx, CS->rt);
        }

        /* Reset the read_len */
        client->command = NULL;
        client->key = NULL;
        if(client->origin_len > BUF_MAX) {
            free(client->origin);
            client->origin = NULL;
            client->origin_len = 0;
        }
    }
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

// Simply adds O_NONBLOCK to the file descriptor of choice
int setnonblock(int fd) {
    int flags;

    if (fd < 0)
        return -1;
    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int unix_socket_init(struct sockaddr_un* socket_un, char* sock_path, int max_queue) {
    int fd;
    size_t path_len;

    if (NULL == socket_un || NULL == sock_path) {
        errno = EINVAL;
        return -1;
    }
    path_len = strnlen(sock_path, sizeof(socket_un->sun_path));
    if (path_len == 0 || path_len >= sizeof(socket_un->sun_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    unlink(sock_path);
    fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("echo server socket");
        exit(EXIT_FAILURE);
    }
    if (setnonblock(fd) < 0) {
        perror("echo server socket nonblock");
        close(fd);
        exit(EXIT_FAILURE);
    }

    memset(socket_un, 0, sizeof(*socket_un));
    socket_un->sun_family = AF_UNIX;
    memcpy(socket_un->sun_path, sock_path, path_len + 1);
    return fd;
}

static int read_db_switch_marker(const char *path, int *db_id)
{
    char buf[4];
    ssize_t n;
    int fd;

    if (NULL == path || NULL == db_id)
        return -1;
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) {
            *db_id = 0;
            return 0;
        }
        return -1;
    }

    do {
        n = read(fd, buf, sizeof(buf));
    } while (n < 0 && errno == EINTR);
    if (close(fd) != 0)
        return -1;

    if (n == 1 && (buf[0] == '0' || buf[0] == '1')) {
        *db_id = buf[0] - '0';
        return 0;
    }
    if (n == 2 && (buf[0] == '0' || buf[0] == '1') &&
        buf[1] == '\n') {
        *db_id = buf[0] - '0';
        return 0;
    }
    errno = EINVAL;
    return -1;
}

/* Return -2 only when rename succeeded but directory fsync failed. */
static int write_db_switch_marker(const char *db_path, int db_id)
{
    char marker_path[SK_PATH_MAX];
    char temp_path[SK_PATH_MAX];
    char value;
    int fd = -1, dir_fd = -1, renamed = 0;
    int n;
    ssize_t written;

    if (NULL == db_path || (db_id != 0 && db_id != 1)) {
        errno = EINVAL;
        return -1;
    }
    n = snprintf(marker_path, sizeof(marker_path), "%s/switch", db_path);
    if (n < 0 || (size_t)n >= sizeof(marker_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    n = snprintf(temp_path, sizeof(temp_path), "%s/switch.tmp", db_path);
    if (n < 0 || (size_t)n >= sizeof(temp_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    fd = open(temp_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return -1;
    value = (char)('0' + db_id);
    do {
        written = write(fd, &value, 1);
    } while (written < 0 && errno == EINTR);
    if (written != 1 || fsync(fd) != 0)
        goto fail;
    if (close(fd) != 0) {
        fd = -1;
        goto fail;
    }
    fd = -1;

    if (rename(temp_path, marker_path) != 0)
        goto fail;
    renamed = 1;

    dir_fd = open(db_path, O_RDONLY);
    if (dir_fd < 0 || fsync(dir_fd) != 0) {
        if (dir_fd >= 0)
            close(dir_fd);
        return -2;
    }
    if (close(dir_fd) != 0)
        return -2;
    return 0;

fail:
    if (fd >= 0)
        close(fd);
    if (!renamed)
        unlink(temp_path);
    return -1;
}

static int server_open_database(SkipDB *db)
{
    int header_pid;

    if (NULL == db)
        return 0;
    header_pid = SkipDB_open(db);
    if (!SkipDB_isOpen(db))
        return 0;

    /* A new empty DB has an in-memory root with PID 0 until its first commit. */
    if (header_pid <= 0) {
        SkipDB_beginTransaction(db);
        SkipDB_commitTransaction(db);
    }

    return SkipDB_isOpen(db) && SkipDB_headerPid(db) > 0;
}

static void server_open(skipd_server* server)
{
    char real_path[SK_PATH_MAX];
    char marker_path[SK_PATH_MAX];
    int n;

    if (NULL == server) {
        errno = EINVAL;
        exit(EXIT_FAILURE);
    }
    n = snprintf(marker_path, sizeof(marker_path), "%s/switch",
        server->db_path);
    if (n < 0 || (size_t)n >= sizeof(marker_path)) {
        skipd_log(SKIPD_DEBUG, "database switch marker path too long\n");
        exit(EXIT_FAILURE);
    }
    if (read_db_switch_marker(marker_path, &server->curr_db) != 0) {
        skipd_log(SKIPD_DEBUG, "invalid database switch marker: %s\n",
            marker_path);
        exit(EXIT_FAILURE);
    }

    n = snprintf(real_path, sizeof(real_path), "%s/%d",
        server->db_path, server->curr_db);
    if (n < 0 || (size_t)n >= sizeof(real_path)) {
        skipd_log(SKIPD_DEBUG, "database path too long\n");
        exit(EXIT_FAILURE);
    }
    server->db = SkipDB_new();
    if (NULL == server->db) {
        skipd_log(SKIPD_DEBUG, "database allocation failed\n");
        exit(EXIT_FAILURE);
    }
    SkipDB_setPath_(server->db, real_path);
    if (!server_open_database(server->db)) {
        skipd_log(SKIPD_DEBUG, "database open failed: %s\n", real_path);
        SkipDB_free(server->db);
        server->db = NULL;
        exit(EXIT_FAILURE);
    }
}

static void server_switch(skipd_server* server)
{
    char real_path[SK_PATH_MAX];
    int n1, n2, marker_status, source_count;
    SkipDB *tmp, *other;

    if (NULL == server || NULL == server->db ||
        (server->curr_db != 0 && server->curr_db != 1) ||
        !SkipDB_isOpen(server->db))
        return;

    source_count = SkipDB_count(server->db);
    if (source_count < 0) {
        skipd_log(SKIPD_DEBUG, "active database record chain is corrupt; switch aborted\n");
        return;
    }
    other = SkipDB_new();
    if (NULL == other) {
        skipd_log(SKIPD_DEBUG, "database switch allocation failed\n");
        return;
    }
    n2 = 1 - server->curr_db;
    n1 = snprintf(real_path, sizeof(real_path), "%s/%d",
        server->db_path, n2);
    if (n1 < 0 || (size_t)n1 >= sizeof(real_path)) {
        SkipDB_free(other);
        skipd_log(SKIPD_DEBUG, "database switch path too long\n");
        return;
    }

    SkipDB_setPath_(other, real_path);
    SkipDB_delete(other);
    if (!server_open_database(other)) {
        skipd_log(SKIPD_DEBUG, "unable to open replacement database: %s\n",
            real_path);
        SkipDB_free(other);
        return;
    }

    if (!SkipDB_mergeInto_(server->db, other) ||
        !SkipDB_isOpen(server->db) || !SkipDB_isOpen(other) ||
        SkipDB_count(other) != source_count) {
        skipd_log(SKIPD_DEBUG, "database switch copy failed; retaining active database\n");
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }

    SkipDB_beginTransaction(other);
    if (!SkipDB_isOpen(other)) {
        skipd_log(SKIPD_DEBUG, "replacement database transaction could not start\n");
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }
    SkipDB_commitTransaction(other);
    if (!SkipDB_isOpen(other) || SkipDB_count(other) != source_count) {
        skipd_log(SKIPD_DEBUG, "replacement database commit failed; retaining active database\n");
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }

    marker_status = write_db_switch_marker(server->db_path, n2);
    if (marker_status != 0) {
        skipd_log(SKIPD_DEBUG, "database switch marker write failed\n");
        if (marker_status == -2) {
            /* The marker was renamed: preserve both databases and stop. */
            SkipDB_free(other);
            exit(EXIT_FAILURE);
        }
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }

    tmp = server->db;
    server->db = other;
    server->curr_db = n2;
    SkipDB_delete(tmp);
    SkipDB_free(tmp);
}

int server_init(skipd_server* server, int max_queue) {
    int count;
    mode_t old_umask;

    server->switch_mark = 8*1024*1024;//default 8M
    server_open(server);

    count = SkipDB_count(server->db);
    if (count < 0) {
        skipd_log(SKIPD_DEBUG, "SkipDB record chain is corrupt; refusing startup\n");
        SkipDB_close(server->db);
        return -1;
    }
    skipd_log(SKIPD_DEBUG, "Load count=%d maxPos=%d\n", count, SkipDB_maxPos(server->db));

    server->fd = unix_socket_init(&server->socket, server->sock_path, max_queue);
    if (server->fd < 0) {
        perror("invalid server socket path");
        return -1;
    }
    server->socket_len = (int)(sizeof(server->socket.sun_family) +
        strlen(server->socket.sun_path) + 1);

    /* The socket grants access to a root-owned command/database service. */
    old_umask = umask(0177);
    if (-1 == bind(server->fd, (struct sockaddr*) &server->socket, server->socket_len)) {
        umask(old_umask);
        perror("echo server bind");
        exit(EXIT_FAILURE);
    }
    umask(old_umask);
    /*
     * bind() creates the socket with mode 0600 under the 0177 umask above.
     * Avoid chmod(path): following the pathname after bind can race with a
     * replacement symlink in a non-sticky or otherwise writable socket dir.
     */

    if (-1 == listen(server->fd, max_queue)) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return 0;
}

static struct option options[] = {
    { "help",	no_argument,		NULL, 'h' },
    { "db_path", required_argument,	NULL, 'd' },
    { "sock_path", required_argument,	NULL, 's' },
    { "daemon", 	required_argument,	NULL, 'D' },
    { NULL, 0, 0, 0 }
};

/* static void not_blocked(EV_P_ ev_periodic *w, int revents) {
    puts(".");
} */

static void server_init_delay(EV_P_ skipd_server *server) {
    int n1;
    char *p1;
    Datum skey, dkey, dvalue;
    SkipDBCursor* cursor = NULL;
    SkipDBRecord* record;
    delay_cmd* delay_obj;

    //Init hear
    skey = Datum_FromCString_("__delay__");
    record = SkipDB_list_first(server->db, skey, &cursor);
    while(NULL != record) {
        dkey = SkipDBRecord_keyDatum(record);
        dvalue = SkipDBRecord_valueDatum(record);

        p1 = find_terminated_value_separator(dvalue);
        if(NULL != p1) {
            n1 = (p1 - (char*)dvalue.data);
            if(n1 > STATIC_BUF_LEN || NULL == dkey.data ||
               dkey.size == 0 || dkey.size > sizeof(delay_obj->key) ||
               ((char*)dkey.data)[dkey.size - 1] != '\0' ||
               memchr(dkey.data, '\0', dkey.size - 1) != NULL) {
                /* Persisted keys are length-delimited; never log them as %s
                 * until their complete NUL-terminated representation is known. */
                skipd_log(SKIPD_DEBUG, "delay read key/value error\n");
                record = SkipDB_list_next(server->db, skey, cursor);
                continue;
            }
            memcpy(static_buffer, dvalue.data, n1);
            static_buffer[n1] = '\0';
            if(S2ISUCCESS == str2int(&n1, static_buffer, 10) && n1 >= 0) {
                delay_obj = (delay_cmd*)malloc(sizeof(delay_cmd));
                if(NULL == delay_obj) {
                    skipd_log(SKIPD_DEBUG, "delay out of memory\n");
                    break;
                }
                memcpy(delay_obj->key, dkey.data, dkey.size);
                delay_obj->tick = n1;
                delay_obj->server = server;

                ev_timer_init(&delay_obj->watcher, delay_cmd_cb, delay_obj->tick, delay_obj->tick);
                ev_timer_start(EV_A_ &delay_obj->watcher);
            }
        }

        record = SkipDB_list_next(server->db, skey, cursor);
    }

    if(NULL != cursor) {
        SkipDBCursor_release(cursor);
    }
}

static void server_init_time(EV_P_ skipd_server *server) {
    int n1;
    char *p1;
    double timer_delay;
    Datum skey, dkey, dvalue;
    SkipDBCursor* cursor = NULL;
    SkipDBRecord* record;
    time_cmd* time_obj;

    skey = Datum_FromCString_("__time__");
    record = SkipDB_list_first(server->db, skey, &cursor);
    while (NULL != record) {
        dkey = SkipDBRecord_keyDatum(record);
        dvalue = SkipDBRecord_valueDatum(record);

        p1 = find_terminated_value_separator(dvalue);
        if (NULL != p1) {
            n1 = (int)(p1 - (char*)dvalue.data);
            if (n1 <= 0 || (size_t)n1 >= sizeof(static_buffer) ||
                NULL == dkey.data || dkey.size == 0 ||
                dkey.size > sizeof(time_obj->key) ||
                ((char*)dkey.data)[dkey.size - 1] != '\0' ||
                memchr(dkey.data, '\0', dkey.size - 1) != NULL) {
                skipd_log(SKIPD_DEBUG, "time read key/value error\n");
                record = SkipDB_list_next(server->db, skey, cursor);
                continue;
            }

            memcpy(static_buffer, dvalue.data, (size_t)n1);
            static_buffer[n1] = '\0';
            if (!next_daily_delay(static_buffer, time(NULL), &timer_delay)) {
                skipd_log(SKIPD_DEBUG, "time schedule value error\n");
                record = SkipDB_list_next(server->db, skey, cursor);
                continue;
            }

            time_obj = (time_cmd*)calloc(1, sizeof(time_cmd));
            if (NULL == time_obj) {
                skipd_log(SKIPD_DEBUG, "out of memory\n");
                break;
            }
            memcpy(time_obj->key, dkey.data, dkey.size);
            time_obj->server = server;

            ev_timer_init(&time_obj->watcher, time_cmd_cb, timer_delay, 0.0);
            ev_timer_start(EV_A_ &time_obj->watcher);
        }

        record = SkipDB_list_next(server->db, skey, cursor);
    }

    if (NULL != cursor)
        SkipDBCursor_release(cursor);
}

static void server_sync(EV_P_ skipd_server* server) {
    ev_timer_again(EV_A_ &server->watcher);

    server->to_commit = 1;
    SkipDB_sync(server->db);
}

static void server_sync_tick(EV_P_ ev_timer *w, int revents) {
    skipd_server *server = global_server;

    if(server->to_commit) {
        server->to_commit = 0;
        ev_timer_stop(EV_A_ w);
        skipd_log(SKIPD_DEBUG, "sync tick\n");

        //Do transaction
        SkipDB_beginTransaction(server->db);
        SkipDB_commitTransaction(server->db);
    }

    if((0 == server->in_doing) && (SkipDB_maxPos(server->db) > server->switch_mark)) {
        server_switch(server);
    }
}

static int check_dbpath(skipd_server* server)
{
    struct stat s;
    size_t n;
    int err;

    if(NULL == server)
        return -1;
    n = strnlen(server->db_path, sizeof(server->db_path));
    if(n == 0 || n >= sizeof(server->db_path))
        return -1;
    if(n > 1 && server->db_path[n - 1] == '/') {
        server->db_path[--n] = '\0';
    }
    /* Both "<path>/0" and "<path>/switch" must fit real_path. */
    if(n > SK_PATH_MAX - 8)
        return -1;
    err = stat(server->db_path, &s);
    if(-1 == err) {
        if(ENOENT != errno)
            return -1;
        if (mkdir(server->db_path, 0700) != 0 && errno != EEXIST)
            return -1;
        /* Re-stat after mkdir (including EEXIST races) before using the path. */
        if (stat(server->db_path, &s) != 0 || !S_ISDIR(s.st_mode))
            return -1;
    } else if(!S_ISDIR(s.st_mode)) {
        return -1;
    }

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

    if(daemon) {
        skipd_daemonize(server->pid_path);
    }

    //if(!daemon) {
    //    skipd_savepid(server->pid_path);
    //}

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

    // Create unix socket in non-blocking fashion
    if (server_init(server, max_queue) != 0)
        return EXIT_FAILURE;

    /* Register persisted timers before accepting clients. Otherwise a delay/time
     * command created during a delayed startup scan would be scheduled twice. */
    server_init_delay(EV_A_ server);
    server_init_time(EV_A_ server);

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

