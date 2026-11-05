#define _GNU_SOURCE

#include "skipd_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int script_log_fd = -1;

static int open_script_log(void)
{
#ifdef O_NOFOLLOW
    struct stat st;
    int fd = open("/tmp/skipd.log",
        O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (fd < 0)
        return -1;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 ||
        st.st_uid != geteuid() || fchmod(fd, 0600) != 0) {
        int saved_errno = errno ? errno : EPERM;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    return fd;
#else
    /* Never follow a predictable /tmp path without O_NOFOLLOW support. */
    errno = ENOTSUP;
    return -1;
#endif
}

int skipd_script_init(void)
{
    if (script_log_fd >= 0)
        return 0;
    script_log_fd = open_script_log();
    return script_log_fd >= 0 ? 0 : -1;
}

void skipd_script_cleanup(void)
{
    if (script_log_fd >= 0) {
        close(script_log_fd);
        script_log_fd = -1;
    }
}

int is_valid_script_name(const char *name, size_t len)
{
    size_t i;

    if (NULL == name || len == 0 || len >= DELAY_KEY_LEN ||
        (len == 1 && name[0] == '.') ||
        (len == 2 && name[0] == '.' && name[1] == '.'))
        return 0;

    for (i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.'))
            return 0;
    }
    return 1;
}

int is_safe_script_command(const char *cmd, size_t length)
{
    size_t i, component_start = 0;

    if (NULL == cmd || length == 0)
        return 0;
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)cmd[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.' || c == '/'))
            return 0;
        if (c == '/') {
            if (i == component_start && i != 0)
                return 0;
            if (i - component_start == 2 &&
                cmd[component_start] == '.' && cmd[component_start + 1] == '.')
                return 0;
            component_start = i + 1;
        }
    }
    if (length == component_start ||
        (length - component_start == 2 &&
         cmd[component_start] == '.' && cmd[component_start + 1] == '.'))
        return 0;
    return 1;
}

void sys_script(const char *cmd, size_t cmd_size)
{
    char command[STATIC_BUF_LEN + 1];
    int written;
    size_t cmd_len;

    if (NULL == cmd || cmd_size == 0 || cmd[cmd_size - 1] != '\0')
        return;
    cmd_len = strnlen(cmd, cmd_size);
    if (cmd_len != cmd_size - 1 || cmd_len == 0 ||
        !is_safe_script_command(cmd, cmd_len)) {
        skipd_log(SKIPD_DEBUG, "invalid script path; not executed\n");
        return;
    }

    if (script_log_fd >= 0) {
        written = snprintf(command, sizeof(command),
            "%s >> /proc/self/fd/%d 2>&1 &\n", cmd, script_log_fd);
    } else {
        /* Fail closed on logging setup errors rather than redirecting through
         * a symlinkable pathname in the world-writable /tmp directory. */
        written = snprintf(command, sizeof(command),
            "%s > /dev/null 2>&1 &\n", cmd);
    }
    if (written < 0 || (size_t)written >= sizeof(command)) {
        command[0] = '\0';
        skipd_log(SKIPD_DEBUG, "script command too long; not executed\n");
        return;
    }
    (void)system(command);
    command[0] = '\0';
}
