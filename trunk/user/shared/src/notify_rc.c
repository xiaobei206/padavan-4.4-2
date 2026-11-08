/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "notify_rc.h"
#include "rc_notify_queue.h"

static void
notify_rc_internal(const char *event_name, int wait_sec)
{
	int notify_dir_fd = -1;
	int incomplete_dir_fd = -1;
	int incomplete_created = 0;
	int ret, i, saved_errno;

	if (!rc_notify_event_name_is_safe(event_name)) {
		errno = EINVAL;
		fprintf(stderr, "notify_rc: invalid event name\\n");
		return;
	}
	notify_dir_fd = rc_notify_queue_open_dir(DIR_RC_NOTIFY, 0);
	if (notify_dir_fd < 0)
		goto fail;
	incomplete_dir_fd = rc_notify_queue_open_dir(DIR_RC_INCOMPLETE, 0);
	if (incomplete_dir_fd < 0)
		goto fail;
	ret = rc_notify_queue_touch_at(incomplete_dir_fd, event_name, 0);
	if (ret < 0)
		goto fail;
	incomplete_created = (ret == 1);
	if (rc_notify_queue_touch_at(notify_dir_fd, event_name, 0) < 0)
		goto fail;

	close(notify_dir_fd);
	notify_dir_fd = -1;
	if (kill(1, SIGUSR1) != 0)
		fprintf(stderr, "notify_rc: failed to signal init (errno=%d)\\n", errno);

	if (wait_sec > 0) {
		for (i = 0; i < wait_sec; ++i) {
			if (rc_notify_queue_marker_is_safe_at(incomplete_dir_fd,
						      event_name, 0) != 0)
				break;
			sleep(1);
		}
	}
	close(incomplete_dir_fd);
	return;

fail:
	saved_errno = errno ? errno : EIO;
	if (incomplete_created && incomplete_dir_fd >= 0)
		unlinkat(incomplete_dir_fd, event_name, 0);
	if (notify_dir_fd >= 0)
		close(notify_dir_fd);
	if (incomplete_dir_fd >= 0)
		close(incomplete_dir_fd);
	errno = saved_errno;
	fprintf(stderr, "notify_rc: refused unsafe notification queue operation (errno=%d)\\n",
		 saved_errno);
}

void notify_rc(const char *event_name)
{
	notify_rc_internal(event_name, 0);
}

void notify_rc_and_wait(const char *event_name, int wait_sec)
{
	notify_rc_internal(event_name, wait_sec);
}
