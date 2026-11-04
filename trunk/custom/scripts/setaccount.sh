#!/bin/sh
# Compatibility wrapper: validate and apply account defaults safely.
if [ "$#" -ne 3 ]; then
	echo "Usage: $0 ACCOUNT PASSWORD DEFAULTS_H" >&2
	exit 2
fi

script_dir=$(cd "$(dirname "$0")" && pwd) || exit 2
customization_json=$(python3 -c 'import json,sys; print(json.dumps({"signaccount":sys.argv[1],"signpassword":sys.argv[2]}))' "$1" "$2") || exit 2
CUSTOMIZATION_JSON=$customization_json
export CUSTOMIZATION_JSON
exec python3 "$script_dir/apply_customization.py" "$3" "$3"
