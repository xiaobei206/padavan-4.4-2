/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include "notify_rc.h"

static int
is_valid_event_name(const char *event_name)
{
	const unsigned char *p;
	size_t len;

	if (!event_name || !*event_name ||
	    !strcmp(event_name, ".") || !strcmp(event_name, ".."))
		return 0;

	len = strlen(event_name);
	if (len > 128)
		return 0;
	for (p = (const unsigned char *)event_name; *p; ++p) {
		if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.')
			return 0;
	}
	return 1;
}

static void notify_rc_internal(const char *event_name, int wait_sec)
{
	FILE *fp;
	int i;
	char *full_name;

	/* Event names become filenames under the init notification directories. */
	if (!is_valid_event_name(event_name)) {
		fprintf(stderr, "notify_rc: invalid event name\\n");
		return;
	}

	full_name = (char *)(malloc(strlen(event_name) + 32));
	if (!full_name) {
		fprintf(stderr, "notify_rc: failed to alloc event memory!\n");
		return;
	}

	sprintf(full_name, "%s/%s", DIR_RC_INCOMPLETE, event_name);
	fp = fopen(full_name, "w");
	if (fp)
		fclose(fp);

	sprintf(full_name, "%s/%s", DIR_RC_NOTIFY, event_name);
	fp = fopen(full_name, "w");
	if (fp)
		fclose(fp);

	kill(1, SIGUSR1);

	if (wait_sec > 0) {
		sprintf(full_name, "%s/%s", DIR_RC_INCOMPLETE, event_name);
		for(i=0;i<wait_sec;i++) {
			fp = fopen(full_name, "r");
			if (!fp)
				break;
			fclose(fp);
			sleep(1);
		}
	}

	free(full_name);
}

void notify_rc(const char *event_name)
{
	notify_rc_internal(event_name, 0);
}

void notify_rc_and_wait(const char *event_name, int wait_sec)
{
	notify_rc_internal(event_name, wait_sec);
}
