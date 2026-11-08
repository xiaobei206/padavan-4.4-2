#include <assert.h>
#include <stdio.h>

#include "storage_mount_inventory_policy.h"

int
main(void)
{
	assert(storage_mount_inventory_result(0, 0) == 0);
	assert(storage_mount_inventory_result(3, 0) == 3);
	assert(storage_mount_inventory_result(0, 1) == -1);
	assert(storage_mount_inventory_result(4, 1) == -1);

	assert(!storage_mount_inventory_has_mounts(-1));
	assert(!storage_mount_inventory_has_mounts(0));
	assert(storage_mount_inventory_has_mounts(1));

	puts("Storage mount inventory tri-state policy: PASS");
	return 0;
}
