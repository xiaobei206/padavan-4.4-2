#ifndef PADAVAN_STORAGE_MOUNT_INVENTORY_POLICY_H
#define PADAVAN_STORAGE_MOUNT_INVENTORY_POLICY_H

/*
 * Preserve a three-state contract:
 *   >= 0: complete inventory (including a genuine empty inventory)
 *   -1: inventory could not be read completely
 */
static int
storage_mount_inventory_result(int count, int io_failed)
{
	return io_failed ? -1 : count;
}

static int
storage_mount_inventory_has_mounts(int count)
{
	return count > 0;
}

#endif /* PADAVAN_STORAGE_MOUNT_INVENTORY_POLICY_H */
