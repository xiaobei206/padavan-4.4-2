#ifndef PADAVAN_FIREWALL_HOOK_FILE_H
#define PADAVAN_FIREWALL_HOOK_FILE_H
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define FIREWALL_HOOK_FILE_MAX_BYTES (1024U * 1024U)

static int
firewall_hook_file_prefix_is_safe(const char *prefix)
{
    const unsigned char *p;
    if (!prefix || !*prefix || strlen(prefix) > 48)
        return 0;
    for (p = (const unsigned char *)prefix; *p; ++p) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '.' || *p == '_' || *p == '-'))
            return 0;
    }
    return 1;
}

/* Validate the opened inode, then copy it into a private directory before
 * execution. Never execute a pathname that was only checked with stat(). */
static int
firewall_hook_file_stage(const char *source_path, const char *private_dir,
                         const char *prefix, char *staged_path,
                         size_t staged_path_size)
{
    char template_path[512], buffer[4096];
    struct stat dir_stat, source_stat;
    int source_fd = -1, staged_fd = -1, created = 0;
    int saved_errno = EINVAL;
    size_t needed, total = 0;
    ssize_t amount;

    if (staged_path && staged_path_size)
        staged_path[0] = '\0';
    if (!source_path || !*source_path || !private_dir || !*private_dir ||
        !staged_path || staged_path_size == 0 ||
        !firewall_hook_file_prefix_is_safe(prefix)) {
        errno = EINVAL;
        return -1;
    }
    if (lstat(private_dir, &dir_stat) != 0 || !S_ISDIR(dir_stat.st_mode) ||
        dir_stat.st_uid != geteuid() || (dir_stat.st_mode & 077) != 0 ||
        (dir_stat.st_mode & (S_IWUSR | S_IXUSR)) != (S_IWUSR | S_IXUSR)) {
        errno = EPERM;
        return -1;
    }
    needed = strlen(private_dir) + 1 + strlen(prefix) + 6 + 1;
    if (needed > sizeof(template_path) || needed > staged_path_size) {
        errno = ENAMETOOLONG;
        return -1;
    }
    source_fd = open(source_path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (source_fd < 0)
        goto fail;
    if (fstat(source_fd, &source_stat) != 0)
        goto fail;
    if (!S_ISREG(source_stat.st_mode) || source_stat.st_uid != geteuid() ||
        source_stat.st_nlink != 1 ||
        (source_stat.st_mode & (S_IWGRP | S_IWOTH)) != 0 ||
        source_stat.st_size < 0 ||
        (unsigned long long)source_stat.st_size > FIREWALL_HOOK_FILE_MAX_BYTES) {
        errno = EPERM;
        goto fail;
    }
    if (snprintf(template_path, sizeof(template_path), "%s/%sXXXXXX",
                 private_dir, prefix) < 0) {
        errno = EINVAL;
        goto fail;
    }
    staged_fd = mkstemp(template_path);
    if (staged_fd < 0)
        goto fail;
    created = 1;
    /* Keep the owner's execute intent for scripts while never copying group/other bits. */
    if (fchmod(staged_fd, S_IRUSR | S_IWUSR |
               ((source_stat.st_mode & S_IXUSR) ? S_IXUSR : 0)) != 0)
        goto fail;
    for (;;) {
        amount = read(source_fd, buffer, sizeof(buffer));
        if (amount < 0) {
            if (errno == EINTR)
                continue;
            goto fail;
        }
        if (amount == 0)
            break;
        total += (size_t)amount;
        if (total > FIREWALL_HOOK_FILE_MAX_BYTES) {
            errno = EFBIG;
            goto fail;
        }
        {
            ssize_t offset = 0;
            while (offset < amount) {
                ssize_t written = write(staged_fd, buffer + offset,
                                        (size_t)(amount - offset));
                if (written < 0) {
                    if (errno == EINTR)
                        continue;
                    goto fail;
                }
                if (written == 0) {
                    errno = EIO;
                    goto fail;
                }
                offset += written;
            }
        }
    }
    if (close(source_fd) != 0) {
        source_fd = -1;
        goto fail;
    }
    source_fd = -1;
    if (close(staged_fd) != 0) {
        staged_fd = -1;
        goto fail;
    }
    staged_fd = -1;
    if (snprintf(staged_path, staged_path_size, "%s", template_path) < 0 ||
        strlen(template_path) >= staged_path_size) {
        errno = ENAMETOOLONG;
        goto fail;
    }
    return 0;

fail:
    if (errno)
        saved_errno = errno;
    if (source_fd >= 0)
        close(source_fd);
    if (staged_fd >= 0)
        close(staged_fd);
    if (created)
        unlink(template_path);
    staged_path[0] = '\0';
    errno = saved_errno;
    return -1;
}

static void
firewall_hook_file_remove(char *staged_path)
{
    if (staged_path && staged_path[0]) {
        unlink(staged_path);
        staged_path[0] = '\0';
    }
}
#endif
