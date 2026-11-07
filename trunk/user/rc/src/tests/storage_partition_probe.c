#include <assert.h>
#include <stdio.h>
#include "storage_partition_probe.h"

static int
probe(const char *contents, const char *disk_name, int is_mmc)
{
	FILE *stream = tmpfile();
	int result;

	assert(stream != NULL);
	assert(fputs(contents, stream) >= 0);
	rewind(stream);
	result = storage_disk_has_partitions(stream, disk_name, is_mmc);
	assert(fclose(stream) == 0);
	return result;
}

int
main(void)
{
	const char *partitions =
		"major minor  #blocks  name\n"
		"   8        0  1953525168 sda\n"
		"   8        1   976762584 sda1\n"
		"   8       16   976762584 sdb1\n"
		"   8       32   976762584 sdaa1\n"
		" 179        0    62333952 mmcblk0\n"
		" 179        1    31165440 mmcblk0p1\n"
		" 179       16    31165440 mmcblk1p1\n";

	assert(storage_disk_has_partitions(NULL, "sda", 0) == -1);
	assert(probe(partitions, "sda", 0) == 1);
	assert(probe(partitions, "sdb", 0) == 1);
	assert(probe(partitions, "sdc", 0) == 0);
	assert(probe(partitions, "mmcblk0", 1) == 1);
	assert(probe(partitions, "mmcblk1", 1) == 1);
	assert(probe(partitions, "mmcblk2", 1) == 0);

	assert(probe("8 0 100 sda\n8 1 20 sdaX\n", "sda", 0) == 0);
	assert(probe("179 0 100 mmcblk0\n179 1 20 mmcblk0pX\n",
		     "mmcblk0", 1) == 0);
	assert(probe(partitions, "sda1", 0) == -1);
	assert(probe(partitions, "sd/", 0) == -1);
	assert(probe(partitions, "mmcblk0p1", 1) == -1);

	puts("Block-storage partition detection and fail-closed semantics: PASS");
	return 0;
}
