#define main dbus_cli_main
#include "../mgr/dbus.c"
#undef main

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static int write_all(int fd, const void *data, size_t length)
{
    const char *p = (const char *)data;
    size_t written = 0;

    while (written < length) {
        ssize_t n = write(fd, p + written, length - written);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        written += (size_t)n;
    }
    return 0;
}

static int write_frame(int fd, const void *body, size_t body_len)
{
    char header[HEADER_PREFIX];
    int header_len;

    if (body_len == 0 || body_len > READ_MAX)
        return -1;
    header_len = snprintf(header, sizeof(header), "%s%07lu ",
        MAGIC, (unsigned long)body_len);
    if (header_len != HEADER_PREFIX ||
        write_all(fd, header, HEADER_PREFIX) != 0 ||
        write_all(fd, body, body_len) != 0)
        return -1;
    return 0;
}

static int run_list_stream_case(int script_mode,
    const char *const *frames, const size_t *frame_lengths, size_t frame_count,
    const char *expected_output, size_t expected_output_len)
{
    int sockets[2] = {-1, -1};
    int saved_stdout = -1;
    int actual_result = -1;
    size_t i, actual_output_len = 0;
    unsigned char actual_output[READ_MAX + 1];
    FILE *capture = NULL;
    dbclient client;

    memset(&client, 0, sizeof(client));
    client.remote_fd = -1;
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;
    client.remote_fd = sockets[0];
    if (setnonblock(client.remote_fd) != 0)
        goto fail;

    for (i = 0; i < frame_count; ++i) {
        if (write_frame(sockets[1], frames[i], frame_lengths[i]) != 0)
            goto fail;
    }

    capture = tmpfile();
    if (NULL == capture || fflush(stdout) != 0)
        goto fail;
    saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0 || dup2(fileno(capture), STDOUT_FILENO) < 0)
        goto fail;

    actual_result = script_mode
        ? parse_script_result(&client)
        : parse_list_result(&client);

    if (fflush(stdout) != 0 || fseek(capture, 0, SEEK_SET) != 0)
        goto restore;
    actual_output_len = fread(actual_output, 1, sizeof(actual_output), capture);

restore:
    fflush(stdout);
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        saved_stdout = -1;
    }
    if (actual_result != 0 ||
        actual_output_len != expected_output_len ||
        (actual_output_len > 0 &&
         memcmp(actual_output, expected_output, actual_output_len) != 0)) {
        fprintf(stderr,
            "dbus %s list parser failed: result=%d, output length=%lu expected=%lu\n",
            script_mode ? "script" : "plain",
            actual_result, (unsigned long)actual_output_len,
            (unsigned long)expected_output_len);
        goto fail;
    }

    fclose(capture);
    close(sockets[1]);
    free(client.buf);
    close(client.remote_fd);
    return 0;

fail:
    if (saved_stdout >= 0) {
        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (capture != NULL)
        fclose(capture);
    if (sockets[1] >= 0)
        close(sockets[1]);
    free(client.buf);
    if (client.remote_fd >= 0)
        close(client.remote_fd);
    return -1;
}

static int run_case(int common, const char *command,
    const void *body, size_t body_len, int expected_result,
    const void *expected_output, size_t expected_output_len)
{
    int sockets[2] = {-1, -1};
    int saved_stdout = -1;
    int actual_result;
    char header[HEADER_PREFIX + 1];
    unsigned char actual_output[READ_MAX + 1];
    FILE *capture = NULL;
    size_t actual_output_len = 0;
    int header_len;
    dbclient client;

    memset(&client, 0, sizeof(client));
    client.remote_fd = -1;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
        return -1;
    client.remote_fd = sockets[0];
    if (setnonblock(client.remote_fd) != 0)
        goto fail;

    header_len = snprintf(header, sizeof(header), "%s%07lu ",
        MAGIC, (unsigned long)body_len);
    if (header_len != HEADER_PREFIX ||
        write_all(sockets[1], header, HEADER_PREFIX) != 0 ||
        write_all(sockets[1], body, body_len) != 0)
        goto fail;

    if (strlen(command) >= sizeof(client.command))
        goto fail;
    memcpy(client.command, command, strlen(command) + 1);

    capture = tmpfile();
    if (NULL == capture)
        goto fail;
    if (fflush(stdout) != 0)
        goto fail;
    saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0 || dup2(fileno(capture), STDOUT_FILENO) < 0)
        goto fail;

    actual_result = common
        ? parse_common_result(&client)
        : parse_get_result(&client);

    if (fflush(stdout) != 0 ||
        fseek(capture, 0, SEEK_SET) != 0)
        goto restore;
    actual_output_len = fread(actual_output, 1, sizeof(actual_output), capture);

restore:
    fflush(stdout);
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        saved_stdout = -1;
    }
    if (actual_result != expected_result ||
        actual_output_len != expected_output_len ||
        (actual_output_len > 0 &&
         memcmp(actual_output, expected_output, actual_output_len) != 0)) {
        fprintf(stderr,
            "dbus parser case '%s' failed: result=%d expected=%d, "
            "output length=%lu expected=%lu\n",
            command, actual_result, expected_result,
            (unsigned long)actual_output_len,
            (unsigned long)expected_output_len);
        goto fail;
    }

    fclose(capture);
    close(sockets[1]);
    free(client.buf);
    close(client.remote_fd);
    return 0;

fail:
    if (saved_stdout >= 0) {
        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (capture != NULL)
        fclose(capture);
    if (sockets[1] >= 0)
        close(sockets[1]);
    free(client.buf);
    if (client.remote_fd >= 0)
        close(client.remote_fd);
    return -1;
}

int main(void)
{
    static const char common_ok[] = "set alpha ok\n";
    static const char common_error[] = "set alpha database error\n";
    static const char delay_ok[] = "delay timer delay_ok\n";
    static const char get_value[] = "get alpha value\n";
    static const char get_output[] = "value\n";
    static const char inc_overflow[] = "inc counter integer overflow\n";
    static const char inc_output[] = "integer overflow\n";
    static const char desc_error[] = "desc counter error\n";
    static const char desc_output[] = "error\n";
    static const char replace_error[] = "replace alpha database error\n";
    static const char database_error[] = "database error\n";
    static const char wrong_command[] = "set alpha ok\n";
    static const char missing_key_separator[] = "get alpha\n";
    static const unsigned char binary_value[] = {
        'g','e','t',' ','b','i','n','a','r','y',' ','A',0,'B'
    };
    static const unsigned char binary_output[] = {'A',0,'B','\n'};

    static const char list_value_is_end[] = "list alpha __end__\n\0";
    static const char list_end_frame[] = "__end__ list __end__\n";
    static const char script_item[] = "list MY_KEY hi\0";
    static const char expected_list[] = "alpha=__end__\n";
    static const char expected_script[] = "export MY_KEY='hi';";
    static const char *const list_frames[] = {
        list_value_is_end, list_end_frame
    };
    static const size_t list_frame_lengths[] = {
        sizeof(list_value_is_end) - 1, sizeof(list_end_frame) - 1
    };
    static const char *const script_frames[] = {
        script_item, list_end_frame
    };
    static const size_t script_frame_lengths[] = {
        sizeof(script_item) - 1, sizeof(list_end_frame) - 1
    };

    if (run_case(1, "set", common_ok, sizeof(common_ok) - 1,
            0, common_ok, sizeof(common_ok) - 1) != 0 ||
        run_case(1, "set", common_error, sizeof(common_error) - 1,
            -5, common_error, sizeof(common_error) - 1) != 0 ||
        run_case(1, "delay", delay_ok, sizeof(delay_ok) - 1,
            0, delay_ok, sizeof(delay_ok) - 1) != 0 ||
        run_case(0, "get", get_value, sizeof(get_value) - 1,
            0, get_output, sizeof(get_output) - 1) != 0 ||
        run_case(0, "inc", inc_overflow, sizeof(inc_overflow) - 1,
            -5, inc_output, sizeof(inc_output) - 1) != 0 ||
        run_case(0, "desc", desc_error, sizeof(desc_error) - 1,
            -5, desc_output, sizeof(desc_output) - 1) != 0 ||
        run_case(0, "replace", replace_error, sizeof(replace_error) - 1,
            -5, database_error, sizeof(database_error) - 1) != 0 ||
        run_case(0, "get", binary_value, sizeof(binary_value),
            0, binary_output, sizeof(binary_output)) != 0 ||
        run_case(0, "get", wrong_command, sizeof(wrong_command) - 1,
            -4, NULL, 0) != 0 ||
        run_case(0, "get", missing_key_separator,
            sizeof(missing_key_separator) - 1, -4, NULL, 0) != 0 ||
        run_list_stream_case(0, list_frames, list_frame_lengths, 2,
            expected_list, sizeof(expected_list) - 1) != 0 ||
        run_list_stream_case(1, script_frames, script_frame_lengths, 2,
            expected_script, sizeof(expected_script) - 1) != 0)
        return EXIT_FAILURE;

    puts("dbus protocol tests passed");
    return EXIT_SUCCESS;
}
