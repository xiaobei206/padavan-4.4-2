#ifndef DBUS_INTERNAL_H
#define DBUS_INTERNAL_H

#include "skipd.h"

typedef struct _dbclient {
    int remote_fd;
    char command[DELAY_KEY_LEN+1];
    char key[DELAY_KEY_LEN+1];
    //unsigned int timeout;

    char* buf;
    int buf_max;
    int buf_len;
    int buf_pos;
} dbclient;

int dbus_read_util(dbclient *client, int len, unsigned int delay);

int parse_common_result(dbclient *client);
int parse_get_result(dbclient *client);
int parse_list_result(dbclient *client);
int parse_script_result(dbclient *client);

#endif
