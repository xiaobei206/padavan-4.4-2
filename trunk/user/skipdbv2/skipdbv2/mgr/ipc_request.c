#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ipc_request.h"

int skipdb_ipc_finalize_request(char *buffer, size_t capacity,
    const char *magic, size_t magic_length, size_t body_length)
{
    char length_field[7];
    size_t header_length, frame_length;
    unsigned long length_value;
    int i;

    if (NULL == buffer || NULL == magic || magic_length == 0 ||
        strlen(magic) != magic_length || body_length == 0 ||
        body_length > 9999999 || body_length >= capacity ||
        buffer[body_length - 1] != '\n' || buffer[body_length] != '\0')
        return -1;

    if (magic_length > (size_t)INT_MAX - 8)
        return -1;
    header_length = magic_length + 8;
    if (header_length >= capacity ||
        body_length > capacity - header_length - 1)
        return -1;
    frame_length = header_length + body_length;
    if (frame_length > (size_t)INT_MAX)
        return -1;

    length_value = (unsigned long)body_length;
    for (i = 6; i >= 0; --i) {
        length_field[i] = (char)('0' + (length_value % 10));
        length_value /= 10;
    }
    if (length_value != 0)
        return -1;

    memmove(buffer + header_length, buffer, body_length);
    memcpy(buffer, magic, magic_length);
    memcpy(buffer + magic_length, length_field, sizeof(length_field));
    buffer[magic_length + 7] = ' ';
    buffer[frame_length] = '\0';
    return (int)frame_length;
}

int skipdb_ipc_format_request(char *buffer, size_t capacity,
    const char *magic, size_t magic_length, size_t body_length,
    const char *format, ...)
{
    va_list arguments;
    int formatted_length;

    if (NULL == buffer || NULL == format || body_length >= capacity)
        return -1;

    va_start(arguments, format);
    formatted_length = vsnprintf(buffer, capacity, format, arguments);
    va_end(arguments);

    if (formatted_length < 0 || (size_t)formatted_length != body_length)
        return -1;
    return skipdb_ipc_finalize_request(buffer, capacity, magic,
        magic_length, body_length);
}
