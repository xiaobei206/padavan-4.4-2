#include "skipd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Persistent delay/time jobs and periodic database synchronization. */
 void delay_cmd_cb(EV_P_ ev_timer* watcher, int revents) {
    char* p1;
    int tmpi;
    size_t prefix_len;
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

        prefix_len = (size_t)(p1 - (char*)dvalue.data);
        if(prefix_len > STATIC_BUF_LEN) {
            skipd_log(SKIPD_DEBUG, "delay cmd:%s, dvalue.data error\n", dkey.data);
            break;
        }
        memcpy(static_buffer, dvalue.data, prefix_len);
        static_buffer[prefix_len] = '\0';
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

 void time_cmd_cb(EV_P_ ev_timer* watcher, int revents) {
    char *p1;
    char time_text[16];
    size_t time_len;
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

    time_len = (size_t)(p1 - (char*)dvalue.data);
    if (time_len == 0 || time_len >= sizeof(time_text)) {
        skipd_log(SKIPD_DEBUG, "time %s schedule value error\n", dkey.data);
        ev_timer_stop(EV_A_ watcher);
        free(time_obj);
        return;
    }
    memcpy(time_text, dvalue.data, time_len);
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


 void server_init_delay(EV_P_ skipd_server *server) {
    int n1;
    size_t delay_len;
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
            delay_len = (size_t)(p1 - (char*)dvalue.data);
            if(delay_len > STATIC_BUF_LEN || NULL == dkey.data ||
               dkey.size == 0 || dkey.size > sizeof(delay_obj->key) ||
               ((char*)dkey.data)[dkey.size - 1] != '\0' ||
               memchr(dkey.data, '\0', dkey.size - 1) != NULL) {
                /* Persisted keys are length-delimited; never log them as %s
                 * until their complete NUL-terminated representation is known. */
                skipd_log(SKIPD_DEBUG, "delay read key/value error\n");
                record = SkipDB_list_next(server->db, skey, cursor);
                continue;
            }
            memcpy(static_buffer, dvalue.data, delay_len);
            static_buffer[delay_len] = '\0';
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

 void server_init_time(EV_P_ skipd_server *server) {
    size_t time_len;
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
            time_len = (size_t)(p1 - (char*)dvalue.data);
            if (time_len == 0 || time_len >= sizeof(static_buffer) ||
                NULL == dkey.data || dkey.size == 0 ||
                dkey.size > sizeof(time_obj->key) ||
                ((char*)dkey.data)[dkey.size - 1] != '\0' ||
                memchr(dkey.data, '\0', dkey.size - 1) != NULL) {
                skipd_log(SKIPD_DEBUG, "time read key/value error\n");
                record = SkipDB_list_next(server->db, skey, cursor);
                continue;
            }

            memcpy(static_buffer, dvalue.data, time_len);
            static_buffer[time_len] = '\0';
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

 void server_sync(EV_P_ skipd_server* server) {
    ev_timer_again(EV_A_ &server->watcher);

    server->to_commit = 1;
    SkipDB_sync(server->db);
}

 void server_sync_tick(EV_P_ ev_timer *w, int revents) {
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