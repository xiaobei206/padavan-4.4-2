#!/bin/sh

[ ! -x /sbin/hdparm ] && exit 1

# check param
[ -z "$1" ] && exit 1
[ "$#" -eq 1 ] || exit 2
case "$1" in
	[!A-Za-z0-9]*|*[!A-Za-z0-9_-]*) exit 1 ;;
esac

# Refuse to spin down a mounted disk or any of its partitions.
if awk -v dev="/dev/$1" '
{
	source = $1
	if (source == dev) {
		found = 1
	} else if (index(source, dev) == 1) {
		suffix = substr(source, length(dev) + 1)
		if (suffix ~ /^[0-9]+$/ || suffix ~ /^p[0-9]+$/)
			found = 1
	}
}
END { exit !found }
' /proc/mounts; then
	exit 1
fi

if [ -r "/sys/block/$1/removable" ] ; then
	dev_removable=`cat "/sys/block/$1/removable"`
	if [ "$dev_removable" = "0" ] ; then
		/sbin/hdparm -y "/dev/$1"
	fi
fi

exit 0
