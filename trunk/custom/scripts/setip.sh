#!/bin/sh
# Compatibility wrapper: validate and apply a custom LAN address safely.
if [ "$#" -ne 3 ]; then
	echo "Usage: $0 LAN_IP DEFAULTS_H CN_DICT" >&2
	exit 2
fi

script_dir=$(cd "$(dirname "$0")" && pwd) || exit 2
customization_json=$(python3 -c 'import json,sys; print(json.dumps({"lanip":sys.argv[1]}))' "$1") || exit 2
CUSTOMIZATION_JSON=$customization_json
export CUSTOMIZATION_JSON
exec python3 "$script_dir/apply_customization.py" "$2" "$3"
