#ifndef SKIPDB_IPC_TRANSPORT_H
#define SKIPDB_IPC_TRANSPORT_H

#include <stddef.h>

/* Shared Unix-socket and buffered-I/O primitives for the SkipDB CLI clients. */
int skipdb_ipc_connect_unix(const char *sock_path);
int skipdb_ipc_set_nonblock(int fd);
int skipdb_ipc_ensure_buffer(char **buffer, int *capacity, int requested,
    int minimum_capacity, int maximum_capacity);
int skipdb_ipc_valid_protocol_field(const char *text, int allow_empty,
    size_t max_length);
int skipdb_ipc_valid_protocol_field_n(const char *text, size_t length,
    int allow_empty, size_t max_length);

/*
 * Exact reads preserve the legacy client error codes:
 *  0 success, -1 peer closed, -2 I/O/clock failure, -3 timeout, -4 invalid input.
 * The connected descriptor must be nonblocking; received is updated on partial I/O.
 */
int skipdb_ipc_read_exact(int fd, char *buffer, int length,
    unsigned int timeout_ms, int *received);
int skipdb_ipc_write_exact(int fd, const char *buffer, int length,
    unsigned int timeout_ms);

#endif
