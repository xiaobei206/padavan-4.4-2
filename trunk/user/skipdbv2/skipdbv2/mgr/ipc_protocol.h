#ifndef SKIPDB_IPC_PROTOCOL_H
#define SKIPDB_IPC_PROTOCOL_H

#include <stddef.h>

typedef struct _skipdb_ipc_frame_view {
    char *command;
    size_t command_len;
    char *key;
    size_t key_len;
    char *payload;
    size_t payload_len;
} skipdb_ipc_frame_view;

/* Split command/key/raw payload without interpreting binary payload bytes. */
int skipdb_ipc_parse_envelope(char *body, size_t body_len,
    skipdb_ipc_frame_view *frame);

#endif
