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

# Padavan bundles dosfstools 4.2. In that version fsck.fat returns 1 when
# changes were queued for writing; the writer does not report flush errors back
# to the exit status. Recheck after a correction and permit a RW mount only
# when the final pass reports no further changes.
storage_fsck_fat_status_needs_verification()
{
	case "$1" in
		1) return 0 ;;
		*) return 1 ;;
	esac
}

storage_fsck_fat_status_allows_mount()
{
	case "$1" in
		0) return 0 ;;
		*) return 1 ;;
	esac
}

# chkntfs is supplied outside the tracked source packages, so its nonzero
# status meanings cannot be verified here. Fail closed for every nonzero value.
storage_chkntfs_status_allows_mount()
{
	case "$1" in
		0) return 0 ;;
		*) return 1 ;;
	esac
}
