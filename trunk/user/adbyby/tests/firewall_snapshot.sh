#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname "$0")/../../../.." && pwd)
test_root=$(mktemp -d /tmp/adbyby-firewall-snapshot.XXXXXX)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
mkdir -m 700 "$test_root/bin" "$test_root/private"
cat > "$test_root/bin/nvram" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$test_root/bin/logger" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$test_root/bin/iptables-save" <<'EOF'
#!/bin/sh
if [ "${FAIL_IPTABLES_SAVE:-0}" = "1" ]; then exit 1; fi
cat <<'RULES'
*nat
:PREROUTING ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:ADBYBY - [0:0]
-A OUTPUT -p tcp -j ACCEPT
-A PREROUTING -p tcp -j ADBYBY
COMMIT
RULES
EOF
chmod 700 "$test_root/bin/"*
PATH="$test_root/bin:$PATH";export PATH
. "$repo_root/trunk/user/adbyby/adbyby.sh" test-source
snapshot="$test_root/private/.adbyby.save";sentinel="$test_root/sentinel"
printf 'preserve-me\n' > "$sentinel";ln -s "$sentinel" "$snapshot"
save_adbyby_rule_snapshot "$test_root/private"
[ -f "$snapshot" ]&&[ ! -L "$snapshot" ];[ "$(stat -c %a "$snapshot")" = "600" ]
grep -F -- "-I OUTPUT 1 -p tcp -j ACCEPT" "$snapshot" >/dev/null
grep -F -- "-I PREROUTING 1 -p tcp -j ADBYBY" "$snapshot" >/dev/null
grep -F -- ":ADBYBY - [0:0]" "$snapshot" >/dev/null
[ "$(cat "$sentinel")" = "preserve-me" ]
printf 'known-good\n' > "$snapshot"
if FAIL_IPTABLES_SAVE=1 save_adbyby_rule_snapshot "$test_root/private"; then echo "unexpectedly accepted failed save" >&2;exit 1;fi
[ "$(cat "$snapshot")" = "known-good" ]
echo "Adbyby private firewall snapshot staging regression: OK"
