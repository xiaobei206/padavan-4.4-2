#include "skipd_internal.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* Shared key parsing and persistence for the delay/time command pair. */
enum scheduled_key_result {
    SCHEDULED_KEY_OK = 0,
    SCHEDULED_KEY_MALFORMED,
    SCHEDULED_KEY_INVALID,
    SCHEDULED_KEY_TOO_LONG
};

static int parse_scheduled_key_(skipd_client *client, char *arguments,
    char **value_start)
{
    char *key_end;
    size_t key_length;

    if (NULL == client || NULL == client->origin || NULL == arguments ||
        NULL == value_start || client->data_len <= 0)
        return SCHEDULED_KEY_MALFORMED;

    key_end = strchr(arguments, ' ');
    if (NULL == key_end)
        return SCHEDULED_KEY_MALFORMED;

    key_length = (size_t)(key_end - arguments);
    *key_end = '\0';
    client->key = arguments;
    if (!valid_protocol_token(client->key, 0))
        return SCHEDULED_KEY_INVALID;
    if (key_length >= DELAY_KEY_LEN)
        return SCHEDULED_KEY_TOO_LONG;

    *value_start = key_end + 1;
    return SCHEDULED_KEY_OK;
}

static int format_scheduled_key_(char *buffer, size_t capacity,
    const char *prefix, const char *key)
{
    int length;

    if (NULL == buffer || capacity == 0 || NULL == prefix || NULL == key)
        return -1;
    length = snprintf(buffer, capacity, "%s%s", prefix, key);
    return length < 0 || (size_t)length >= capacity ? -1 : 0;
}

/* Return 1 for a duplicate task, 0 for a durable write, and -1 on DB error. */
static int save_scheduled_value_(EV_P_ skipd_server *server,
    const char *key, Datum value)
{
    Datum dkey;
    int write_ok;

    if (NULL == server || NULL == server->db || NULL == key)
        return -1;

    dkey = Datum_FromCString_((char *)key);
    if (SkipDB_exists(server->db, dkey))
        return 1;

    SkipDB_beginTransaction(server->db);
    write_ok = SkipDB_at_put_(server->db, dkey, value);
    server_sync(EV_A_ server);
    return write_ok && SkipDB_isOpen(server->db) ? 0 : -1;
}

/* Command parsing and coroutine-driven request dispatch. */
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
        if (!valid_protocol_token(client->key, 0)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
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
        if (!valid_protocol_token(client->key, 0)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
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
        if (!valid_protocol_token_span(client->key,
                (size_t)((client->origin + client->data_len - 1) - client->key), 0)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);
        dvalue = SkipDB_at_(client->server->db, dkey);
        if (!SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            if (client_send_key(EV_A_ client, "__error__",
                    client->command, p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
        }
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
        if (!valid_protocol_token(client->key, 0)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
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
        if (!valid_protocol_token_span(client->key,
                (size_t)((client->origin + client->data_len - 1) - client->key), 1)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
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

        if (!SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            if (client_send_key(EV_A_ client, "__error__", "list",
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
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
        if (!valid_protocol_token_span(client->key,
                (size_t)((client->origin + client->data_len - 1) - client->key), 0)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
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
        int parse_result, save_result;
        char *value_start, *value_separator;

        p1 = p2+1;
        parse_result = parse_scheduled_key_(client, p1, &value_start);
        if (parse_result == SCHEDULED_KEY_MALFORMED)
            ccrStop(ctx, ccr_error_err2);
        if (parse_result == SCHEDULED_KEY_INVALID) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
        if (parse_result == SCHEDULED_KEY_TOO_LONG) {
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        p1 = value_start;
        dvalue = Datum_FromData_length_((unsigned char *)p1,
            client->data_len - (p1 - client->origin));
        value_separator = find_terminated_value_separator(dvalue);
        if (NULL == value_separator) {
            p1 = "value param error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        if (strlen((char *)dvalue.data) >= 300) {
            p1 = "value too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        tmpi = (int)(value_separator - p1);
        memcpy(static_buffer, p1, (size_t)tmpi);
        static_buffer[tmpi] = '\0';
        if (S2ISUCCESS != str2int(&tmpi, static_buffer, 10) || tmpi < 0) {
            p1 = "delay tick error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        delay_obj = calloc(1, sizeof(delay_cmd));
        if (NULL == delay_obj) {
            p1 = "memory error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        if (format_scheduled_key_(delay_obj->key, sizeof(delay_obj->key),
                "__delay__", client->key) != 0) {
            free(delay_obj);
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        client->key = delay_obj->key;
        delay_obj->tick = tmpi;
        delay_obj->server = client->server;

        save_result = save_scheduled_value_(EV_A_ client->server,
            client->key, dvalue);
        if (save_result == 1) {
            p1 = "exists\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(delay_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }
        if (save_result < 0) {
            p1 = "database error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(delay_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }

        ev_timer_init(&delay_obj->watcher, delay_cmd_cb,
            delay_obj->tick, delay_obj->tick);
        ev_timer_start(EV_A_ &delay_obj->watcher);
        p1 = "delay_ok\n";
        client_send(EV_A_ client, p1, strlen(p1));
        ccrReturn(ctx, ccr_error_ok1);

    } else if(!strcmp(client->command, "time")) {
        int parse_result, save_result;
        char *value_start;

        p1 = p2+1;
        parse_result = parse_scheduled_key_(client, p1, &value_start);
        if (parse_result == SCHEDULED_KEY_MALFORMED)
            ccrStop(ctx, ccr_error_err2);
        if (parse_result == SCHEDULED_KEY_INVALID) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
        if (parse_result == SCHEDULED_KEY_TOO_LONG) {
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        p1 = value_start;
        p2 = strstr(p1, " ");
        if (NULL == p2) {
            p1 = "value error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        tmpi = (int)(p2 - p1);
        if (tmpi <= 0 || (size_t)tmpi >= sizeof(static_buffer)) {
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

        dvalue = Datum_FromData_length_((unsigned char *)p1,
            client->data_len - (p1 - client->origin));
        if (NULL == find_terminated_value_separator(dvalue)) {
            p1 = "value param error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        if (strlen((char *)dvalue.data) >= 300) {
            p1 = "value too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }

        time_obj = calloc(1, sizeof(time_cmd));
        if (NULL == time_obj) {
            p1 = "memory error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        if (format_scheduled_key_(time_obj->key, sizeof(time_obj->key),
                "__time__", client->key) != 0) {
            free(time_obj);
            p1 = "key too big\n";
            client_send(EV_A_ client, p1, strlen(p1));
            ccrReturn(ctx, ccr_error_ok1);
        }
        client->key = time_obj->key;

        save_result = save_scheduled_value_(EV_A_ client->server,
            client->key, dvalue);
        if (save_result == 1) {
            p1 = "exists\n";
            client_send(EV_A_ client, p1, strlen(p1));
            free(time_obj);
            ccrReturn(ctx, ccr_error_ok1);
        }
        if (save_result < 0) {
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
        if(!is_valid_script_name(p1,
                (size_t)((client->origin + client->data_len - 1) - p1))) {
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

        if (!SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            if (client_send_key(EV_A_ client, "__error__", "fire",
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
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
            if (client->owned_key) {
                free(client->owned_key);
                client->owned_key = NULL;
            }
            client->key = NULL;
            ccrReturn(ctx, ccr_error_ok);
            ccrStop(ctx, ccr_error_ok1);
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
        if (!valid_protocol_token(client->key, 0)) {
            p1 = "invalid key\n";
            if (client_send_key(EV_A_ client, "__error__", client->command,
                    p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
            ccrStop(ctx, ccr_error_ok1);
        }
        p1 = p2+1;

        dkey = Datum_FromCString_(client->key);

        if(S2ISUCCESS != str2int(&tmpi, p1, 10)) {
            p1 = "error\n";
            client_send(EV_A_ client, p1, strlen(p1));
            client->break_level = ccr_break_killed;
            ccrStop(ctx, ccr_error_err2);
        }

        dvalue = SkipDB_at_(client->server->db, dkey);
        if (!SkipDB_isOpen(client->server->db)) {
            p1 = "database error\n";
            if (client_send(EV_A_ client, p1, strlen(p1)) != 0) {
                client->break_level = ccr_break_killed;
                ccrStop(ctx, ccr_error_err2);
            }
            ccrReturn(ctx, ccr_error_ok1);
        }
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

int client_ccr_process(EV_P_ skipd_client* client)
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
