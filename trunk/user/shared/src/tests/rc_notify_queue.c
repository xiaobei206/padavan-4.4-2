#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "rc_notify_queue.h"

static void
write_text(const char *path, const char *value)
{
	FILE *fp = fopen(path, "w");
	assert(fp != NULL);
	assert(fputs(value, fp) >= 0);
	assert(fclose(fp) == 0);
}

static void
check_text(const char *path, const char *expected)
{
	char buf[64];
	FILE *fp = fopen(path, "r");
	assert(fp != NULL);
	assert(fgets(buf, sizeof(buf), fp) != NULL);
	assert(fclose(fp) == 0);
	assert(strcmp(buf, expected) == 0);
}

int
main(void)
{
	char root[] = "/tmp/rc-notify-test.XXXXXX";
	char queue[512], unsafe[512], victim[512], hardlink[512], linkdir[512];
	char marker[1024], long_name[130];
	struct stat st;
	uid_t owner = geteuid();
	int dir_fd, bad_fd, ret;

	assert(rc_notify_event_name_is_safe("restart_firewall"));
	assert(rc_notify_event_name_is_safe("restart.reboot-1"));
	assert(!rc_notify_event_name_is_safe(NULL));
	assert(!rc_notify_event_name_is_safe(""));
	assert(!rc_notify_event_name_is_safe("."));
	assert(!rc_notify_event_name_is_safe(".."));
	assert(!rc_notify_event_name_is_safe("../victim"));
	assert(!rc_notify_event_name_is_safe("restart/reboot"));
	assert(!rc_notify_event_name_is_safe("restart\nreboot"));
	memset(long_name, 'x', sizeof(long_name) - 1);
	long_name[sizeof(long_name) - 1] = '\0';
	assert(!rc_notify_event_name_is_safe(long_name));

	assert(mkdtemp(root) != NULL);
	snprintf(queue, sizeof(queue), "%s/queue", root);
	snprintf(unsafe, sizeof(unsafe), "%s/unsafe", root);
	snprintf(victim, sizeof(victim), "%s/victim", root);
	snprintf(hardlink, sizeof(hardlink), "%s/hardlink", root);
	snprintf(linkdir, sizeof(linkdir), "%s/linkdir", root);
	assert(mkdir(queue, 0700) == 0);
	assert(chmod(queue, 0700) == 0);
	dir_fd = rc_notify_queue_open_dir(queue, owner);
	assert(dir_fd >= 0);

	ret = rc_notify_queue_touch_at(dir_fd, "restart_firewall", owner);
	assert(ret == 1);
	assert(rc_notify_queue_touch_at(dir_fd, "restart_firewall", owner) == 0);
	assert(rc_notify_queue_marker_is_safe_at(dir_fd, "restart_firewall", owner) == 0);
	snprintf(marker, sizeof(marker), "%s/restart_firewall", queue);
	assert(lstat(marker, &st) == 0 && S_ISREG(st.st_mode));
	assert(st.st_uid == owner && st.st_nlink == 1);
	assert((st.st_mode & 0777) == 0600);
	assert(unlink(marker) == 0);

	write_text(victim, "do-not-truncate\n");
	assert(symlink(victim, marker) == 0);
	assert(rc_notify_queue_touch_at(dir_fd, "restart_firewall", owner) < 0);
	check_text(victim, "do-not-truncate\n");
	assert(unlink(marker) == 0);

	write_text(marker, "wrong-mode\n");
	assert(chmod(marker, 0644) == 0);
	assert(rc_notify_queue_touch_at(dir_fd, "restart_firewall", owner) < 0);
	assert(unlink(marker) == 0);

	write_text(marker, "hard-linked\n");
	assert(link(marker, hardlink) == 0);
	assert(rc_notify_queue_touch_at(dir_fd, "restart_firewall", owner) < 0);
	assert(unlink(hardlink) == 0);
	assert(unlink(marker) == 0);

	assert(mkfifo(marker, 0600) == 0);
	assert(rc_notify_queue_touch_at(dir_fd, "restart_firewall", owner) < 0);
	assert(unlink(marker) == 0);

	assert(mkdir(linkdir, 0700) == 0);
	assert(symlink(linkdir, unsafe) == 0);
	assert(rc_notify_queue_open_dir(unsafe, owner) < 0);
	assert(unlink(unsafe) == 0);

	assert(mkdir(unsafe, 0777) == 0);
	assert(chmod(unsafe, 0777) == 0);
	bad_fd = rc_notify_queue_open_dir(unsafe, owner);
	assert(bad_fd < 0);
	assert(rmdir(unsafe) == 0);

	assert(close(dir_fd) == 0);
	assert(rmdir(queue) == 0);
	assert(unlink(victim) == 0);
	assert(rmdir(linkdir) == 0);
	assert(rmdir(root) == 0);
	puts("RC notification queue policy: PASS");
	return 0;
}
