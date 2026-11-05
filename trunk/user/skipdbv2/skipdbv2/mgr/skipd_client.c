#include "skipd_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* The write coroutine is resumed by the writable libev callback below. */
static int client_ccr_write(EV_P_ skipd_client *client);

/* Client response framing belongs with the connection's write path. */
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
        ccrStop(ctx, ccr_error_err2);
    }

    buf_len = _max(step_len, BUF_MAX);
    if(buf_len > client->origin_len) {
        char *new_origin = (char *)realloc(client->origin,
            (size_t)buf_len + 1);
        if (NULL == new_origin) {
            ccrStop(ctx, ccr_error_err2);
        }
        client->origin = new_origin;
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
void skipd_server_accept_cb(EV_P_ ev_io *w, int revents) {
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
