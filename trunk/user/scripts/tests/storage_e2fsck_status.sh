#!/bin/sh
set -eu

test_dir=$(dirname "$0")
. "$test_dir/../files/sbin/storage_fsck_status.sh"

storage_e2fsck_status_allows_mount 0
storage_e2fsck_status_allows_mount 1

for status in '' 2 3 4 8 16 32 128 255 invalid; do
	if storage_e2fsck_status_allows_mount "$status"; then
		echo "unexpectedly allowed e2fsck status: $status" >&2
		exit 1
	fi
done

echo "e2fsck mount gate policy: PASS"
