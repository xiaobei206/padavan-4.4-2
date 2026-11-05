#ifndef SKIPDB_IPC_REQUEST_H
#define SKIPDB_IPC_REQUEST_H

#include <stddef.h>

/* body_length includes its newline; capacity includes room for the final NUL. */
int skipdb_ipc_finalize_request(char *buffer, size_t capacity,
    const char *magic, size_t magic_length, size_t body_length);

/* Format a body and prepend the common length-delimited IPC request header. */
int skipdb_ipc_format_request(char *buffer, size_t capacity,
    const char *magic, size_t magic_length, size_t body_length,
    const char *format, ...);

#endif
