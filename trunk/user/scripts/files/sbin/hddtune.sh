#!/bin/sh

[ ! -x /sbin/hdparm ] && exit 1

if [ "$#" -gt 1 ]; then
	exit 2
fi
if [ -n "$1" ]; then
	case "$1" in
		[!A-Za-z0-9]*|*[!A-Za-z0-9_-]*) exit 1 ;;
	esac
fi

hdd_spindt=`nvram get hdd_spindt`
case "$hdd_spindt" in
	1|2|3|4|5|6|7|8|9) ;;
	*) hdd_spindt=0 ;;
esac

hdd_apmoff=`nvram get hdd_apmoff`
case "$hdd_apmoff" in
	''|*[!0-9]*) hdd_apmoff=0 ;;
	*) case "$hdd_apmoff" in *[1-9]*) ;; *) hdd_apmoff=0 ;; esac ;;
esac


HDPARM_S="-S0"
HDPARM_B=""

case "$hdd_spindt" in
1)
	HDPARM_S="-S180"
	;;
2)
	HDPARM_S="-S241"
	;;
3)
	HDPARM_S="-S242"
	;;
4)
	HDPARM_S="-S243"
	;;
5)
	HDPARM_S="-S244"
	;;
6)
	HDPARM_S="-S245"
	;;
7)
	HDPARM_S="-S246"
	;;
8)
	HDPARM_S="-S247"
	;;
9)
	HDPARM_S="-S248"
	;;
esac

if [ "$hdd_apmoff" != "0" ] ; then
	HDPARM_B="-B254"
fi

if [ -z "$1" ] ; then
	for dev_path in /sys/block/sd* ; do
		[ -d "$dev_path" ] || continue
		dev_name=${dev_path##*/}
		removable=1
		[ -r "$dev_path/removable" ] && removable=`cat "$dev_path/removable"`
		[ "$removable" = "0" ] && /sbin/hdparm $HDPARM_S $HDPARM_B "/dev/$dev_name"
	done
else
	if [ -e "/sys/block/$1" ] ; then
		removable=1
		[ -r "/sys/block/$1/removable" ] && removable=`cat "/sys/block/$1/removable"`
		if [ "$removable" = "0" ] ; then
			if [ "$hdd_spindt" != "0" ] ; then
				logger -t hdparm "Set spindown timeout to device /dev/$1"
				/sbin/hdparm $HDPARM_S "/dev/$1"
			fi
			if [ "$hdd_apmoff" != "0" ] ; then
				logger -t hdparm "Set APM to device /dev/$1"
				/sbin/hdparm $HDPARM_B "/dev/$1"
			fi
		fi
	fi
fi

exit 0
