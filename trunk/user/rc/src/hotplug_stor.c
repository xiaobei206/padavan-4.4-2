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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

#include <dev_info.h>
#include <disk_initial.h>

#include "rc.h"

static int
check_root_partition(const char *devname, int is_mmc)
{
	FILE *procpt;
	char line[128], ptname[32];
	const char *suffix;
	size_t devname_len;
	int ma, mi;
	unsigned long long dev_sz;

	if (!devname || !*devname)
		return 0;
	devname_len = strlen(devname);
	if (devname_len >= sizeof(ptname))
		return 0;

	procpt = fopen(PROC_PARTITIONS_FILE, "r");
	if (!procpt)
		return 0;

	while (fgets(line, sizeof(line), procpt)) {
		if (sscanf(line, " %d %d %llu %31[^\n ]", &ma, &mi, &dev_sz, ptname) != 4)
			continue;
		if (strncmp(ptname, devname, devname_len) != 0)
			continue;

		suffix = ptname + devname_len;
#if defined (USE_MMC_SUPPORT)
		if (is_mmc) {
			if (*suffix++ != 'p')
				continue;
		} else
#endif
		{
			if (!*suffix)
				continue;
		}

		if (!*suffix)
			continue;
		while (*suffix >= '0' && *suffix <= '9')
			suffix++;
		if (!*suffix) {
			fclose(procpt);
			return 1;
		}
	}

	fclose(procpt);
	return 0;
}

#if defined (USE_BLK_DEV_SD)
int
mdev_sd_main(int argc, char **argv)
{
	int isLock;
	size_t len, disk_len, i;
	const char *p;
	char share_name[32], disk_name[32];
	char *device_name, *action, *partno;

	if (argc != 3){
		printf("Usage: %s [device_name] [action]\n", argv[0]);
		return 1;
	}

	device_name = argv[1];
	action = argv[2];

	if (get_device_type_by_device(device_name) != DEVICE_TYPE_SCSI_DISK)
		return 1;

	// Check Lock.
	if ((isLock = file_lock(device_name)) == -1)
		return 1;

	// If remove the device?
	if (!get_hotplug_action(action)) {
		try_device_swapoff(device_name);
		if (strlen(device_name) < 4)
			notify_rc("on_unplug_mass_storage");
		
		goto out_unlock;
	}

	len = strlen(device_name);
	if (len >= sizeof(disk_name) || strncmp(device_name, "sd", 2) != 0)
		goto out_unlock;

	disk_len = len;
	while (disk_len > 2 &&
	       device_name[disk_len - 1] >= '0' &&
	       device_name[disk_len - 1] <= '9')
		disk_len--;
	if (disk_len <= 2)
		goto out_unlock;

	memcpy(disk_name, device_name, disk_len);
	disk_name[disk_len] = '\0';
	if (disk_len == len) {
		char *tune_argv[] = { "/sbin/hddtune.sh", disk_name, NULL };

		/* Whole disk: tune it and wait for partition-specific hotplug events. */
		_eval(tune_argv, NULL, 0, NULL);
		if (check_root_partition(disk_name, 0))
			goto out_unlock;
		partno = "1";
	} else {
		partno = device_name + disk_len;
	}

	if (snprintf(share_name, sizeof(share_name), "AiDisk_%s%s", disk_name + 2, partno) >= (int)sizeof(share_name))
		goto out_unlock;

	umask(0000);
	{
		char *mount_argv[] = { "/sbin/automount.sh", device_name, share_name, NULL };
		if (_eval(mount_argv, NULL, 0, NULL) == 0)
			notify_rc("on_hotplug_mass_storage");
	}

out_unlock:
	file_unlock(isLock);

	return 0;
}
#endif

#if defined (USE_MMC_SUPPORT)
int
mdev_mmc_main(int argc, char **argv)
{
	int isLock;
	size_t len, disk_len;
	char share_name[32], disk_name[32];
	char *device_name, *action, *partno;

	if (argc != 3){
		printf("Usage: %s [device_name] [action]\n", argv[0]);
		return 1;
	}

	device_name = argv[1];
	action = argv[2];

	if (get_device_type_by_device(device_name) != DEVICE_TYPE_MMC)
		return 1;

	// Check Lock.
	if ((isLock = file_lock(device_name)) == -1)
		return 1;

	len = strlen(device_name);

	// If remove the device?
	if (!get_hotplug_action(action)) {
		try_device_swapoff(device_name);
		if (len < 9)
			notify_rc("on_unplug_mass_storage");
		
		goto out_unlock;
	}

	if (len >= sizeof(disk_name) || strncmp(device_name, "mmcblk", 6) != 0)
		goto out_unlock;

	{
		char *partition = strrchr(device_name, 'p');
		if (partition) {
			disk_len = (size_t)(partition - device_name);
			partno = partition + 1;
			if (disk_len <= 6 || !*partno)
				goto out_unlock;
		} else {
			disk_len = len;
			partno = "1";
			if (check_root_partition(device_name, 1))
				goto out_unlock;
		}
	}

	for (i = 6; i < disk_len; i++) {
		if (device_name[i] < '0' || device_name[i] > '9')
			goto out_unlock;
	}
	for (p = partno; *p; p++) {
		if (*p < '0' || *p > '9')
			goto out_unlock;
	}

	memcpy(disk_name, device_name, disk_len);
	disk_name[disk_len] = '\0';
	if (snprintf(share_name, sizeof(share_name), "AiCard_%s%s",
		     disk_name + 6, partno) >= (int)sizeof(share_name))
		goto out_unlock;

	umask(0000);
	{
		char *mount_argv[] = { "/sbin/automount.sh", device_name, share_name, NULL };
		if (_eval(mount_argv, NULL, 0, NULL) == 0)
			notify_rc("on_hotplug_mass_storage");
	}

out_unlock:
	file_unlock(isLock);

	return 0;
}
#endif
