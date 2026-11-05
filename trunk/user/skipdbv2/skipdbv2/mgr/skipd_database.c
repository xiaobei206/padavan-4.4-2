#include "skipd_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* Database marker durability, startup, and copy-on-switch lifecycle. */
static int read_db_switch_marker(const char *path, int *db_id)
{
    char buf[4];
    ssize_t n;
    int fd, saved_errno;
    struct stat marker_stat;

    if (NULL == path || NULL == db_id)
        return -1;
#ifdef O_NOFOLLOW
    fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
#else
    errno = ENOTSUP;
    return -1;
#endif
    if (fd < 0) {
        if (errno == ENOENT) {
            *db_id = 0;
            return 0;
        }
        return -1;
    }
    if (verify_owned_regular_file_(fd, &marker_stat) != 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    do {
        n = read(fd, buf, sizeof(buf));
    } while (n < 0 && errno == EINTR);
    if (n < 0) {
        saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }
    if (close(fd) != 0)
        return -1;

    if (n == 1 && (buf[0] == '0' || buf[0] == '1')) {
        *db_id = buf[0] - '0';
        return 0;
    }
    if (n == 2 && (buf[0] == '0' || buf[0] == '1') &&
        buf[1] == '\n') {
        *db_id = buf[0] - '0';
        return 0;
    }
    errno = EINVAL;
    return -1;
}

/* Reject special or foreign-owned marker files with a stable policy errno. */
static int verify_owned_regular_file_(int fd, struct stat *file_stat)
{
    if (fd < 0 || NULL == file_stat) {
        errno = EINVAL;
        return -1;
    }
    if (fstat(fd, file_stat) != 0)
        return -1;
    if (!S_ISREG(file_stat->st_mode) || file_stat->st_uid != geteuid()) {
        errno = EPERM;
        return -1;
    }
    return 0;
}

/* Return -2 only when rename succeeded but directory fsync failed. */
static int write_db_switch_marker(const char *db_path, int db_id)
{
    char marker_path[SK_PATH_MAX];
    char temp_path[SK_PATH_MAX];
    char value;
    int fd = -1, dir_fd = -1, renamed = 0;
    int n;
    ssize_t written;
    struct stat temp_stat;

    if (NULL == db_path || (db_id != 0 && db_id != 1)) {
        errno = EINVAL;
        return -1;
    }
    n = snprintf(marker_path, sizeof(marker_path), "%s/switch", db_path);
    if (n < 0 || (size_t)n >= sizeof(marker_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    n = snprintf(temp_path, sizeof(temp_path), "%s/switch.tmp", db_path);
    if (n < 0 || (size_t)n >= sizeof(temp_path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

#ifdef O_NOFOLLOW
    fd = open(temp_path, O_WRONLY | O_CREAT | O_NOFOLLOW |
        O_NONBLOCK | O_CLOEXEC, 0600);
#else
    errno = ENOTSUP;
    return -1;
#endif
    if (fd < 0)
        return -1;

    /* Never truncate a symlink, a special file, or a foreign-owned path. */
    if (verify_owned_regular_file_(fd, &temp_stat) != 0 ||
        fchmod(fd, 0600) != 0 || ftruncate(fd, 0) != 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    value = (char)('0' + db_id);
    do {
        written = write(fd, &value, 1);
    } while (written < 0 && errno == EINTR);
    if (written != 1 || fsync(fd) != 0)
        goto fail;
    if (close(fd) != 0) {
        fd = -1;
        goto fail;
    }
    fd = -1;

    if (rename(temp_path, marker_path) != 0)
        goto fail;
    renamed = 1;

    dir_fd = open(db_path, O_RDONLY | O_CLOEXEC);
    if (dir_fd < 0 || fsync(dir_fd) != 0) {
        if (dir_fd >= 0)
            close(dir_fd);
        return -2;
    }
    if (close(dir_fd) != 0)
        return -2;
    return 0;

fail:
    if (fd >= 0)
        close(fd);
    if (!renamed)
        unlink(temp_path);
    return -1;
}

static int server_open_database(SkipDB *db)
{
    int header_pid;

    if (NULL == db)
        return 0;
    header_pid = SkipDB_open(db);
    if (!SkipDB_isOpen(db))
        return 0;

    /* A new empty DB has an in-memory root with PID 0 until its first commit. */
    if (header_pid <= 0) {
        SkipDB_beginTransaction(db);
        SkipDB_commitTransaction(db);
    }

    return SkipDB_isOpen(db) && SkipDB_headerPid(db) > 0;
}

static void server_open(skipd_server* server)
{
    char real_path[SK_PATH_MAX];
    char marker_path[SK_PATH_MAX];
    int n;

    if (NULL == server) {
        errno = EINVAL;
        exit(EXIT_FAILURE);
    }
    n = snprintf(marker_path, sizeof(marker_path), "%s/switch",
        server->db_path);
    if (n < 0 || (size_t)n >= sizeof(marker_path)) {
        skipd_log(SKIPD_DEBUG, "database switch marker path too long\n");
        exit(EXIT_FAILURE);
    }
    if (read_db_switch_marker(marker_path, &server->curr_db) != 0) {
        skipd_log(SKIPD_DEBUG, "invalid database switch marker: %s\n",
            marker_path);
        exit(EXIT_FAILURE);
    }

    n = snprintf(real_path, sizeof(real_path), "%s/%d",
        server->db_path, server->curr_db);
    if (n < 0 || (size_t)n >= sizeof(real_path)) {
        skipd_log(SKIPD_DEBUG, "database path too long\n");
        exit(EXIT_FAILURE);
    }
    server->db = SkipDB_new();
    if (NULL == server->db) {
        skipd_log(SKIPD_DEBUG, "database allocation failed\n");
        exit(EXIT_FAILURE);
    }
    SkipDB_setPath_(server->db, real_path);
    if (!server_open_database(server->db)) {
        skipd_log(SKIPD_DEBUG, "database open failed: %s\n", real_path);
        SkipDB_free(server->db);
        server->db = NULL;
        exit(EXIT_FAILURE);
    }
}

void server_switch(skipd_server* server)
{
    char real_path[SK_PATH_MAX];
    int n1, n2, marker_status, source_count;
    SkipDB *tmp, *other;

    if (NULL == server || NULL == server->db ||
        (server->curr_db != 0 && server->curr_db != 1) ||
        !SkipDB_isOpen(server->db))
        return;

    source_count = SkipDB_count(server->db);
    if (source_count < 0) {
        skipd_log(SKIPD_DEBUG, "active database record chain is corrupt; switch aborted\n");
        return;
    }
    other = SkipDB_new();
    if (NULL == other) {
        skipd_log(SKIPD_DEBUG, "database switch allocation failed\n");
        return;
    }
    n2 = 1 - server->curr_db;
    n1 = snprintf(real_path, sizeof(real_path), "%s/%d",
        server->db_path, n2);
    if (n1 < 0 || (size_t)n1 >= sizeof(real_path)) {
        SkipDB_free(other);
        skipd_log(SKIPD_DEBUG, "database switch path too long\n");
        return;
    }

    SkipDB_setPath_(other, real_path);
    SkipDB_delete(other);
    if (!server_open_database(other)) {
        skipd_log(SKIPD_DEBUG, "unable to open replacement database: %s\n",
            real_path);
        SkipDB_free(other);
        return;
    }

    if (!SkipDB_mergeInto_(server->db, other) ||
        !SkipDB_isOpen(server->db) || !SkipDB_isOpen(other) ||
        SkipDB_count(other) != source_count) {
        skipd_log(SKIPD_DEBUG, "database switch copy failed; retaining active database\n");
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }

    SkipDB_beginTransaction(other);
    if (!SkipDB_isOpen(other)) {
        skipd_log(SKIPD_DEBUG, "replacement database transaction could not start\n");
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }
    SkipDB_commitTransaction(other);
    if (!SkipDB_isOpen(other) || SkipDB_count(other) != source_count) {
        skipd_log(SKIPD_DEBUG, "replacement database commit failed; retaining active database\n");
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }

    marker_status = write_db_switch_marker(server->db_path, n2);
    if (marker_status != 0) {
        skipd_log(SKIPD_DEBUG, "database switch marker write failed\n");
        if (marker_status == -2) {
            /* The marker was renamed: preserve both databases and stop. */
            SkipDB_free(other);
            exit(EXIT_FAILURE);
        }
        SkipDB_delete(other);
        SkipDB_free(other);
        return;
    }

    tmp = server->db;
    server->db = other;
    server->curr_db = n2;
    SkipDB_delete(tmp);
    SkipDB_free(tmp);
}

int server_init(skipd_server* server, int max_queue) {
    int count;
    mode_t old_umask;

    server->switch_mark = 8*1024*1024;//default 8M

    /* Acquire the single-instance lock before touching journals or commit
     * markers. Two simultaneous starts must never recover the same database. */
    server->fd = unix_socket_init(&server->socket, server->sock_path,
        server->db_path, max_queue);
    if (server->fd < 0) {
        perror("invalid server socket path");
        return -1;
    }

    server_open(server);
    count = SkipDB_count(server->db);
    if (count < 0) {
        skipd_log(SKIPD_DEBUG, "SkipDB record chain is corrupt; refusing startup\n");
        SkipDB_close(server->db);
        close(server->fd);
        server->fd = -1;
        unix_socket_release_instance_lock();
        return -1;
    }
    skipd_log(SKIPD_DEBUG, "Load count=%d maxPos=%d\n", count, SkipDB_maxPos(server->db));

    server->socket_len = (int)(sizeof(server->socket.sun_family) +
        strlen(server->socket.sun_path) + 1);

    /* The socket grants access to a root-owned command/database service. */
    old_umask = umask(0177);
    if (-1 == bind(server->fd, (struct sockaddr*) &server->socket, server->socket_len)) {
        umask(old_umask);
        perror("echo server bind");
        exit(EXIT_FAILURE);
    }
    umask(old_umask);
    /*
     * bind() creates the socket with mode 0600 under the 0177 umask above.
     * Avoid chmod(path): following the pathname after bind can race with a
     * replacement symlink in a non-sticky or otherwise writable socket dir.
     */

    if (-1 == listen(server->fd, max_queue)) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return 0;
}