#ifndef PADAVAN_STORAGE_PARTITION_PROBE_H
#define PADAVAN_STORAGE_PARTITION_PROBE_H

#include <stdio.h>
#include <string.h>

/*
 * Return 1 when a disk has at least one child partition, 0 when no child
 * partition is present, and -1 when the source or disk name is invalid.
 * A negative result must be treated as "do not mount the whole disk".
 */
static int
storage_disk_name_is_valid(const char *disk_name, int is_mmc)
{
	const char *p;
	size_t length;

	if (!disk_name)
		return 0;

	length = strlen(disk_name);
	if (is_mmc) {
		if (length < 7 || strncmp(disk_name, "mmcblk", 6) != 0)
			return 0;
		for (p = disk_name + 6; *p; ++p) {
			if (*p < '0' || *p > '9')
				return 0;
		}
		return 1;
	}

	if (length < 3 || strncmp(disk_name, "sd", 2) != 0)
		return 0;
	for (p = disk_name + 2; *p; ++p) {
		if (*p < 'a' || *p > 'z')
			return 0;
	}

	return 1;
}

static int
storage_disk_has_partitions(FILE *proc_partitions, const char *disk_name,
			    int is_mmc)
{
	char line[128], partition_name[64];
	const char *suffix;
	size_t disk_name_len;
	unsigned long long disk_size;
	int major_number, minor_number, found = 0;

	if (!proc_partitions || !storage_disk_name_is_valid(disk_name, is_mmc))
		return -1;

	disk_name_len = strlen(disk_name);
	while (fgets(line, sizeof(line), proc_partitions)) {
		if (sscanf(line, " %d %d %llu %63[^\n \t]",
			   &major_number, &minor_number, &disk_size,
			   partition_name) != 4)
			continue;

		if (strncmp(partition_name, disk_name, disk_name_len) != 0)
			continue;

		suffix = partition_name + disk_name_len;
		if (is_mmc) {
			if (*suffix++ != 'p')
				continue;
		}

		if (!*suffix)
			continue;
		while (*suffix >= '0' && *suffix <= '9')
			suffix++;
		if (!*suffix) {
			found = 1;
			break;
		}
	}

	if (ferror(proc_partitions))
		return -1;

	return found;
}

#endif /* PADAVAN_STORAGE_PARTITION_PROBE_H */
