#ifndef DBAPI_INTERNAL_H
#define DBAPI_INTERNAL_H

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "skipd.h"

typedef struct _dbclient {
    int remote_fd;

    char *buf;
    int buf_max;
    int buf_len;
    int buf_pos;
} dbclient;

typedef int (*fn_db_parse)(dbclient *client, void *o, char *prefix,
    char *key, char *value);

int dbapi_check_buf(dbclient *client, int len);
int dbapi_ignore_result(dbclient *client, const char *command,
    const char *key);
int dbapi_parse_list_result(dbclient *client, char *prefix, void *o,
    fn_db_parse fn);

#endif
