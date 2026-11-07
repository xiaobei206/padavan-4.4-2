#!/bin/sh
#
# Upgrade only the one previously shipped stock AP script. User-edited,
# symlinked, or otherwise unknown persistent scripts are deliberately kept.
#
LEGACY_SHA256=8d94a244022c46b6f5e921065fd6b6cadcaa58134a034f8332a8fc12b5af3251

if [ "$#" -ne 2 ]; then
	exit 2
fi

stored_script=$1
rom_script=$2

# Never follow or replace a persistent symlink, and never migrate a file that
# is not currently executable (the normal autostart copy path handles that).
if [ ! -f "$stored_script" ] || [ ! -x "$stored_script" ] || [ -L "$stored_script" ]; then
	exit 0
fi
if [ ! -f "$rom_script" ] || [ -L "$rom_script" ]; then
	exit 0
fi

# Some BusyBox builds omit SHA256SUM. Without a strong digest, leave the
# persistent script untouched rather than risk overwriting user customizations.
if command -v sha256sum >/dev/null 2>&1; then
	digest_line=$(sha256sum "$stored_script" 2>/dev/null) || exit 0
	digest=$(printf '%s\n' "$digest_line" | cut -d ' ' -f 1)
	[ "$digest" = "$LEGACY_SHA256" ] || exit 0
else
	logger -t "AP script migration" "sha256sum unavailable; preserving persistent script" 2>/dev/null || :
	exit 0
fi

storage_dir=$(dirname "$stored_script") || exit 0

# Stage beside the destination so the final rename is atomic on this filesystem.
old_umask=$(umask)
umask 077
tmp_script=$(mktemp "$storage_dir/.ap_script.sh.XXXXXX") || {
	umask "$old_umask"
	logger -t "AP script migration" "cannot create same-directory staging file" 2>/dev/null || :
	exit 0
}
cleanup()
{
	[ -n "$tmp_script" ] && rm -f "$tmp_script"
}
trap cleanup 0
trap 'exit 0' HUP INT TERM

if ! cp "$rom_script" "$tmp_script" || ! chmod 700 "$tmp_script"; then
	logger -t "AP script migration" "cannot stage current ROM default; preserving old script" 2>/dev/null || :
	exit 0
fi

if ! mv -f "$tmp_script" "$stored_script"; then
	logger -t "AP script migration" "atomic replacement failed; preserving old script" 2>/dev/null || :
	exit 0
fi

tmp_script=
trap - 0 HUP INT TERM
umask "$old_umask"
logger -t "AP script migration" "replaced byte-identical legacy stock script with current ROM default" 2>/dev/null || :
exit 0
