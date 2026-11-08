#!/bin/sh
set -eu

test_dir=$(dirname "$0")
. "$test_dir/../files/sbin/storage_fsck_status.sh"

storage_fsck_fat_status_allows_mount 0
storage_fsck_fat_status_needs_verification 1
if storage_fsck_fat_status_allows_mount 1; then
	echo "fsck.fat status 1 must be verified again before mounting" >&2
	exit 1
fi

for status in 2 3 4 8 16 32 128 255 invalid ''; do
	if storage_fsck_fat_status_allows_mount "$status"; then
		echo "unexpectedly allowed fsck.fat status: $status" >&2
		exit 1
	fi
	if storage_fsck_fat_status_needs_verification "$status"; then
		echo "unexpectedly requested fsck.fat verification for status: $status" >&2
		exit 1
	fi
done

storage_chkntfs_status_allows_mount 0
for status in 1 2 3 4 8 16 32 128 255 invalid ''; do
	if storage_chkntfs_status_allows_mount "$status"; then
		echo "unexpectedly allowed chkntfs status: $status" >&2
		exit 1
	fi
done

echo "FAT verification and NTFS fail-closed checker status policy: PASS"
