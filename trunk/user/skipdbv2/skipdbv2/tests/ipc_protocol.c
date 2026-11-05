#include "ipc_protocol.h"

#include <stdio.h>
#include <string.h>

static int expect_invalid(char *body, size_t body_len, const char *label)
{
    skipdb_ipc_frame_view frame;
    if (skipdb_ipc_parse_envelope(body, body_len, &frame) == -4)
        return 0;
    fprintf(stderr, "ipc envelope accepted malformed case: %s\n", label);
    return -1;
}

int main(void)
{
    char binary_body[] = {
        'g', 'e', 't', ' ', 'a', 'l', 'p', 'h', 'a', ' ',
        'A', '\0', 'B'
    };
    char empty_payload[] = "list empty ";
    char end_frame[] = "__end__ list __end__\n";
    char no_separator[] = "get";
    char no_key[] = "get ";
    char empty_key[] = "get  value";
    char empty_command[] = " key value";
    char nul_command[] = {'g', 'e', '\0', 't', ' ', 'k', ' ', 'v'};
    char nul_key[] = {'g', 'e', 't', ' ', 'k', '\0', 'x', ' ', 'v'};
    skipdb_ipc_frame_view frame;

    if (skipdb_ipc_parse_envelope(binary_body, sizeof(binary_body), &frame) != 0 ||
        frame.command_len != 3 || memcmp(frame.command, "get", 3) != 0 ||
        frame.key_len != 5 || memcmp(frame.key, "alpha", 5) != 0 ||
        frame.payload_len != 3 ||
        memcmp(frame.payload, binary_body + 10, 3) != 0 ||
        frame.payload[1] != '\0') {
        fprintf(stderr, "ipc envelope binary payload was not preserved\n");
        return 1;
    }

    if (skipdb_ipc_parse_envelope(empty_payload, sizeof(empty_payload) - 1,
            &frame) != 0 ||
        frame.command_len != 4 || frame.key_len != 5 ||
        frame.payload_len != 0) {
        fprintf(stderr, "ipc envelope empty payload was rejected\n");
        return 1;
    }

    if (skipdb_ipc_parse_envelope(end_frame, sizeof(end_frame) - 1,
            &frame) != 0 ||
        frame.command_len != 7 || frame.key_len != 4 ||
        frame.payload_len != 8 ||
        memcmp(frame.payload, "__end__\n", 8) != 0) {
        fprintf(stderr, "ipc envelope terminal marker was not split\n");
        return 1;
    }

    if (expect_invalid(no_separator, sizeof(no_separator) - 1,
            "missing command separator") != 0 ||
        expect_invalid(no_key, sizeof(no_key) - 1,
            "missing key separator") != 0 ||
        expect_invalid(empty_key, sizeof(empty_key) - 1,
            "empty key") != 0 ||
        expect_invalid(empty_command, sizeof(empty_command) - 1,
            "empty command") != 0 ||
        expect_invalid(nul_command, sizeof(nul_command),
            "embedded NUL command") != 0 ||
        expect_invalid(nul_key, sizeof(nul_key),
            "embedded NUL key") != 0 ||
        expect_invalid(NULL, 0, "null body") != 0)
        return 1;

    puts("ipc protocol tests passed");
    return 0;
}
