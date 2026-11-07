#!/bin/sh
set -eu

tests_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
bin_dir="$tests_dir/../files/usr/bin"
helper="$bin_dir/ap_script_migrate.sh"
template="$bin_dir/ap_script.sh"
fixture="$tests_dir/fixtures/ap_script_legacy_stock.sh"
work=$(mktemp -d "/tmp/ap-script-migration.XXXXXX")
trap 'rm -rf "$work"' 0 HUP INT TERM

[ -x "$helper" ]
[ -f "$template" ]
[ -f "$fixture" ]

# A byte-identical legacy stock script is atomically upgraded to the ROM copy.
mkdir "$work/stock"
cp "$fixture" "$work/stock/ap_script.sh"
chmod 755 "$work/stock/ap_script.sh"
"$helper" "$work/stock/ap_script.sh" "$template"
cmp "$template" "$work/stock/ap_script.sh"
[ -x "$work/stock/ap_script.sh" ]
[ "$(stat -c '%a' "$work/stock/ap_script.sh")" = 700 ]
[ -z "$(find "$work/stock" -name '.ap_script.sh.*' -print -quit)" ]

# Any local customization changes the digest and must be preserved byte-for-byte.
mkdir "$work/custom"
cp "$fixture" "$work/custom/ap_script.sh"
printf '\n# user customization: keep this file\n' >> "$work/custom/ap_script.sh"
chmod 755 "$work/custom/ap_script.sh"
cp "$work/custom/ap_script.sh" "$work/custom/before"
"$helper" "$work/custom/ap_script.sh" "$template"
cmp "$work/custom/before" "$work/custom/ap_script.sh"

# A symlink is never followed or replaced, even when its target is the old stock file.
mkdir "$work/symlink"
cp "$fixture" "$work/symlink/target"
chmod 755 "$work/symlink/target"
ln -s "$work/symlink/target" "$work/symlink/ap_script.sh"
"$helper" "$work/symlink/ap_script.sh" "$template"
[ -L "$work/symlink/ap_script.sh" ]

echo "AP persistent stock-script migration regression: OK"
