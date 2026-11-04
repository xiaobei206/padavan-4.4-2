#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <syslog.h>
#include <ev.h>

void skipd_daemonize(char * path)
{
#ifndef __MINGW32__
    /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
        int fd = -1;
        int failed = 0;
        struct stat st;
        char pid_text[32];
        int pid_len;
        size_t written = 0;

        if (NULL != path) {
#ifdef O_NOFOLLOW
            fd = open(path, O_WRONLY | O_CREAT | O_NOFOLLOW | O_NONBLOCK,
                0600);
#else
            errno = ENOTSUP;
#endif
        } else {
            errno = EINVAL;
        }
        if (fd < 0) {
            fprintf(stderr, "Unable to create pid file: %s\n",
                strerror(errno));
            kill(pid, SIGTERM);
            exit(EXIT_FAILURE);
        }

        /* Never truncate a non-regular, foreign-owned, or symlinked path. */
        if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
            st.st_uid != geteuid() || fchmod(fd, 0600) != 0 ||
            ftruncate(fd, 0) != 0) {
            if (errno == 0)
                errno = EPERM;
            fprintf(stderr, "Unsafe pid file: %s\n", strerror(errno));
            close(fd);
            kill(pid, SIGTERM);
            exit(EXIT_FAILURE);
        }

        pid_len = snprintf(pid_text, sizeof(pid_text), "%ld\n", (long)pid);
        if (pid_len < 0 || (size_t)pid_len >= sizeof(pid_text)) {
            failed = 1;
        } else {
            while (written < (size_t)pid_len) {
                ssize_t n = write(fd, pid_text + written,
                    (size_t)pid_len - written);
                if (n < 0 && errno == EINTR)
                    continue;
                if (n <= 0) {
                    failed = 1;
                    break;
                }
                written += (size_t)n;
            }
        }

        if (close(fd) != 0)
            failed = 1;
        if (failed) {
            fprintf(stderr, "Unable to write pid file\n");
            remove(path);
            kill(pid, SIGTERM);
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }


    /* Cancel certain signals */
    signal(SIGCHLD, SIG_DFL); /* A child process dies */
    signal(SIGTSTP, SIG_IGN); /* Various TTY signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, SIG_IGN); /* Ignore hangup signal */

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        /* Log the failure */
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    if (!freopen("/dev/null", "r", stdin)) {
        fprintf(stderr, "unable to freopen() stdin, code %d (%s)",
                                                   errno, strerror(errno));
    }

    if (!freopen("/dev/null", "w", stdout)) {
        fprintf(stderr, "unable to freopen() stdout, code %d (%s)",
                                                   errno, strerror(errno));
    }

    if (!freopen("/dev/null", "w", stderr)) {
        fprintf(stderr, "unable to freopen() stderr, code %d (%s)",
                                                   errno, strerror(errno));
    }

    /* Close out the standard file descriptors */
    /* close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO); */
#endif
}
