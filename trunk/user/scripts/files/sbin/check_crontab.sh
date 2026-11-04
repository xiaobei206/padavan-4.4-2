#!/bin/sh
# $1-5: crontab expression fields; "a" is the router's wildcard alias.
# $6: script basename under /usr/bin
[ "$#" -eq 6 ] || exit 2
[ -n "$6" ] || exit 0

for field in "$1" "$2" "$3" "$4" "$5"; do
	case "$field" in
		''|*[!0-9a*/,-]*) exit 2 ;;
	esac
	[ "${#field}" -le 32 ] || exit 2
done

case "$6" in
	[!A-Za-z0-9]*|*[!A-Za-z0-9_.-]*|*..*) exit 2 ;;
esac

cd /etc/storage || exit 1
exp=`printf '%s %s %s %s %s' "$1" "$2" "$3" "$4" "$5" | tr 'a' '*'`
cron_file="cron/crontabs/admin"
script_path="/usr/bin/$6"

if [ ! -f "$cron_file" ] || ! grep -F -- "$script_path" "$cron_file" >/dev/null 2>&1; then
	printf '%s %s > /dev/null 2>&1\n' "$exp" "$script_path" >> "$cron_file" && exit 1
fi
exit 0
