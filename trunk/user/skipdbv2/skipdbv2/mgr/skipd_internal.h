#ifndef SKIPD_INTERNAL_H
#define SKIPD_INTERNAL_H

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <ev.h>

#include "coroutine.h"
#include "SkipDB.h"
#include "skipd.h"

#define STATIC_BUF_LEN 511

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
    ccr_error_ok = 0,
    ccr_error_ok1 = 1,
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

    char *command;
    char *key;
    int data_len;

    char *origin;
    int origin_len;
    int read_len;
    int read_pos;

    char *send;
    int send_max;
    int send_len;
    int send_pos;

    SkipDBCursor *cursor;
    char *owned_key;
    int in_doing;
    skipd_server *server;
} skipd_client;

typedef struct _delay_cmd {
    ev_timer watcher;
    char key[DELAY_KEY_LEN + DELAY_PREFIX_LEN];
    int tick;
    skipd_server *server;
} delay_cmd;

typedef struct _time_cmd {
    ev_timer watcher;
    char key[DELAY_KEY_LEN + DELAY_PREFIX_LEN];
    skipd_server *server;
} time_cmd;

/* Shared process state owned by skipd.c. */
extern char *global_magic;
extern skipd_server *global_server;
extern char static_buffer[STATIC_BUF_LEN + 1];

/* Protocol validation, scheduling and script execution helpers. */
char *find_terminated_value_separator(Datum value);
int value_fits_list_frame(const char *key, size_t value_len);
int next_daily_delay(const char *time_text, time_t now, double *delay);
int valid_protocol_token_span(const char *text, size_t length,
    int allow_empty);
int valid_protocol_token(const char *text, int allow_empty);
int is_valid_script_name(const char *name, size_t len);
int is_safe_script_command(const char *cmd, size_t length);
void sys_script(const char *cmd, size_t cmd_size);
int skipd_script_init(void);
void skipd_script_cleanup(void);
void print_time(char *prefix);

/* Request/response engine shared with the event-loop owner. */
int client_send_key(EV_P_ skipd_client *client, char *cmd, char *key,
    char *buf, int len);
int client_send(EV_P_ skipd_client *client, char *buf, int len);
int client_ccr_read_util(skipd_client *client, int step_len);
void skipd_server_accept_cb(EV_P_ ev_io *w, int revents);
int client_ccr_process(EV_P_ skipd_client *client);

/* Timer and persistence orchestration. */
void delay_cmd_cb(EV_P_ ev_timer *watcher, int revents);
void time_cmd_cb(EV_P_ ev_timer *watcher, int revents);
void server_sync(EV_P_ skipd_server *server);
void server_sync_tick(EV_P_ ev_timer *watcher, int revents);
void server_init_delay(EV_P_ skipd_server *server);
void server_init_time(EV_P_ skipd_server *server);
void server_switch(skipd_server *server);
int server_init(skipd_server *server, int max_queue);

/* UNIX socket lifecycle. */
int setnonblock(int fd);
int unix_socket_init(struct sockaddr_un *socket_un, char *sock_path,
    const char *lock_dir, int max_queue);
void unix_socket_release_instance_lock(void);

void skipd_log(int filter, const char *format, ...);

#endif
