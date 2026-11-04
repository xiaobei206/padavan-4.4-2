#!/bin/sh
# Compatibility wrapper: validate and apply Wi-Fi defaults safely.
if [ "$#" -ne 5 ]; then
	echo "Usage: $0 WIFI2G_SSID WIFI2G_PSK WIFI5G_SSID WIFI5G_PSK DEFAULTS_H" >&2
	exit 2
fi

script_dir=$(cd "$(dirname "$0")" && pwd) || exit 2
customization_json=$(python3 -c 'import json,sys; print(json.dumps({"wifi2gssid":sys.argv[1],"wifi2gpsk":sys.argv[2],"wifi5gssid":sys.argv[3],"wifi5gpsk":sys.argv[4]}))' "$1" "$2" "$3" "$4") || exit 2
CUSTOMIZATION_JSON=$customization_json
export CUSTOMIZATION_JSON
exec python3 "$script_dir/apply_customization.py" "$5" "$5"
