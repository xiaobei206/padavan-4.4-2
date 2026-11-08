#!/bin/sh
set -eu

if ! command -v flock >/dev/null 2>&1; then
	echo "flock is required for this regression" >&2
	exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/padavan-mountpoint-lock.XXXXXX")
trap 'rm -rf "$tmpdir"' 0 HUP INT TERM
lock_file="$tmpdir/automount_mountpoint.lock"
events="$tmpdir/events"

contender()
{
	tag=$1
	(
		flock 9
		printf '%s-start\n' "$tag" >> "$events"
		sleep 1
		printf '%s-end\n' "$tag" >> "$events"
	) 9>>"$lock_file"
}

contender A &
pid_a=$!
contender B &
pid_b=$!
wait "$pid_a"
wait "$pid_b"

awk '
/-start$/ {
	if (active != "")
		exit 1
	tag = $0
	sub(/-start$/, "", tag)
	if (seen[tag]++)
		exit 1
	active = tag
	count++
	next
}
/-end$/ {
	tag = $0
	sub(/-end$/, "", tag)
	if (active != tag)
		exit 1
	active = ""
	count++
	next
}
{ exit 1 }
END {
	if (count != 4 || active != "")
		exit 1
}
' "$events"

echo "Concurrent automount operations serialize their mountpoint critical section: PASS"
