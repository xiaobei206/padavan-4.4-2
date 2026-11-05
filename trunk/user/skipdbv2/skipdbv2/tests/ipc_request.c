#include "../mgr/ipc_request.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_formatted_request(void)
{
    char buffer[64];
    int length = skipdb_ipc_format_request(buffer, sizeof(buffer),
        "magicv1 ", 8, 16, "%s %s %s\n", "set", "alpha", "value");
    return length == 32 &&
        strcmp(buffer, "magicv1 0000016 set alpha value\n") == 0 ? 0 : -1;
}

static int test_empty_prefix_request(void)
{
    char buffer[64];
    int length = skipdb_ipc_format_request(buffer, sizeof(buffer),
        "magicv1 ", 8, 6, "list %s\n", "");
    return length == 22 &&
        strcmp(buffer, "magicv1 0000006 list \n") == 0 ? 0 : -1;
}

static int test_prebuilt_body(void)
{
    char buffer[64] = "get alpha\n";
    int length = skipdb_ipc_finalize_request(buffer, sizeof(buffer),
        "magicv1 ", 8, 10);
    return length == 26 &&
        strcmp(buffer, "magicv1 0000010 get alpha\n") == 0 ? 0 : -1;
}

static int test_reject_invalid_frames(void)
{
    char short_buffer[20] = "set\n";
    char no_newline[16] = "set";
    char oversized[8] = "x\n";

    if (skipdb_ipc_finalize_request(NULL, sizeof(short_buffer),
        "magicv1 ", 8, 4) != -1 ||
        skipdb_ipc_finalize_request(short_buffer, sizeof(short_buffer),
        NULL, 8, 4) != -1 ||
        skipdb_ipc_finalize_request(short_buffer, sizeof(short_buffer),
        "magicv1", 8, 4) != -1 ||
        skipdb_ipc_finalize_request(short_buffer, sizeof(short_buffer),
        "magicv1 ", 8, 4) != -1 ||
        skipdb_ipc_finalize_request(no_newline, sizeof(no_newline),
        "magicv1 ", 8, 3) != -1 ||
        skipdb_ipc_finalize_request(oversized, sizeof(oversized),
        "magicv1 ", 8, 10000000) != -1 ||
        skipdb_ipc_format_request(short_buffer, sizeof(short_buffer),
        "magicv1 ", 8, 9, "%s\n", "set") != -1)
        return -1;
    return 0;
}

int main(void)
{
    if (test_formatted_request() != 0 ||
        test_empty_prefix_request() != 0 ||
        test_prebuilt_body() != 0 ||
        test_reject_invalid_frames() != 0) {
        fputs("SkipDB IPC request-frame tests failed.\n", stderr);
        return EXIT_FAILURE;
    }
    puts("SkipDB IPC request-frame tests passed.");
    return EXIT_SUCCESS;
}
