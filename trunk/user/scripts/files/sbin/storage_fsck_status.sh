#!/bin/sh

# e2fsck status 0 means clean; status 1 means errors were corrected.
# Status 2 requires a reboot. Other statuses can leave errors or indicate
# an operational failure. Never continue to a read-write mount in those cases.
storage_e2fsck_status_allows_mount()
{
	case "$1" in
		0|1) return 0 ;;
		*) return 1 ;;
	esac
}
