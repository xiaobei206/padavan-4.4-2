#include "ipc_protocol.h"

#include <string.h>

int skipdb_ipc_parse_envelope(char *body, size_t body_len,
    skipdb_ipc_frame_view *frame)
{
    char *command_end;
    char *key;
    char *key_end;
    size_t command_len;
    size_t key_len;
    size_t key_offset;

    if (NULL == body || NULL == frame || body_len == 0)
        return -4;

    memset(frame, 0, sizeof(*frame));
    command_end = (char *)memchr(body, ' ', body_len);
    if (NULL == command_end || command_end == body)
        return -4;

    command_len = (size_t)(command_end - body);
    if (memchr(body, '\0', command_len) != NULL)
        return -4;

    key_offset = command_len + 1;
    if (key_offset >= body_len)
        return -4;
    key = body + key_offset;
    key_end = (char *)memchr(key, ' ', body_len - key_offset);
    if (NULL == key_end || key_end == key)
        return -4;

    key_len = (size_t)(key_end - key);
    if (memchr(key, '\0', key_len) != NULL)
        return -4;

    frame->command = body;
    frame->command_len = command_len;
    frame->key = key;
    frame->key_len = key_len;
    frame->payload = key_end + 1;
    frame->payload_len = body_len - (size_t)(frame->payload - body);
    return 0;
}
