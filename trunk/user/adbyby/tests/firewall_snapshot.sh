#!/bin/sh
set -e
repo_root=$(CDPATH= cd -- "$(dirname "$0")/../../../.." && pwd)
test_root=$(mktemp -d /tmp/adbyby-firewall-snapshot.XXXXXX)
trap 'rm -rf "$test_root"' EXIT HUP INT TERM
mkdir -m 700 "$test_root/bin" "$test_root/private"
cat > "$test_root/bin/nvram" <<'EOF'
#!/bin/sh
case "$1" in
    get)
        case "$2" in
            http_username) printf '%s\n' root ;;
            *) printf '%s\n' 0 ;;
        esac
        ;;
    set) exit 0 ;;
    *) exit 0 ;;
esac
EOF
cat > "$test_root/bin/logger" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$test_root/bin/iptables-save" <<'EOF'
#!/bin/sh
if [ "${FAIL_IPTABLES_SAVE:-0}" = "1" ]; then exit 1; fi
cat <<'RULES'
*filter
:FORWARD ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
-A FORWARD -m set --match-set blockip dst -j DROP
-A OUTPUT -m set --match-set blockip dst -j DROP
COMMIT
*nat
:PREROUTING ACCEPT [0:0]
:OUTPUT ACCEPT [0:0]
:ADBYBY - [0:0]
-A OUTPUT -p tcp -j ADBYBY
-A PREROUTING -p tcp -j ADBYBY
COMMIT
RULES
EOF
cat > "$test_root/bin/iptables" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$TEST_COMMAND_LOG"
if [ -n "${FAIL_IPTABLES_MATCH:-}" ] && [ "$*" = "$FAIL_IPTABLES_MATCH" ]; then
    exit 1
fi
exit 0
EOF
cat > "$test_root/bin/ipset" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >> "$TEST_IPSET_LOG"
if [ "${FAIL_IPSET_RESTORE:-0}" = "1" ] && [ "$1" = "-!" ] && [ "$2" = "restore" ]; then
    exit 1
fi
exit 0
EOF
chmod 700 "$test_root/bin/"*
PATH="$test_root/bin:$PATH"
export PATH
TEST_COMMAND_LOG="$test_root/iptables.log"
TEST_IPSET_LOG="$test_root/ipset.log"
TEST_SNAPSHOT_LOG="$test_root/snapshot.log"
export TEST_COMMAND_LOG TEST_IPSET_LOG TEST_SNAPSHOT_LOG
. "$repo_root/trunk/user/adbyby/adbyby.sh" test-source >/dev/null
snapshot="$test_root/private/.adbyby.save";sentinel="$test_root/sentinel"
printf 'preserve-me\n' > "$sentinel";ln -s "$sentinel" "$snapshot"
save_adbyby_rule_snapshot "$test_root/private"
[ -f "$snapshot" ]&&[ ! -L "$snapshot" ];[ "$(stat -c %a "$snapshot")" = "600" ]
grep -F -- "-I OUTPUT 1 -p tcp -j ADBYBY" "$snapshot" >/dev/null
grep -F -- "-I PREROUTING 1 -p tcp -j ADBYBY" "$snapshot" >/dev/null
grep -F -- "-I FORWARD 1 -m set --match-set blockip dst -j DROP" "$snapshot" >/dev/null
grep -F -- "-I OUTPUT 1 -m set --match-set blockip dst -j DROP" "$snapshot" >/dev/null
grep -F -- ":ADBYBY - [0:0]" "$snapshot" >/dev/null
[ "$(cat "$sentinel")" = "preserve-me" ]
printf 'known-good\n' > "$snapshot"
FAIL_IPTABLES_SAVE=1
export FAIL_IPTABLES_SAVE
if save_adbyby_rule_snapshot "$test_root/private"; then
    echo "unexpectedly accepted failed iptables-save" >&2
    exit 1
fi
unset FAIL_IPTABLES_SAVE
[ "$(cat "$snapshot")" = "known-good" ]

# Disabling Adbyby removes the restore snapshot and abandoned staging files.
printf 'partial\\n' > "$test_root/private/.adbyby-save.raw.dead"
printf 'partial\\n' > "$test_root/private/.adbyby-save.next.dead"
if ! adbyby_remove_rule_snapshot "$test_root/private"; then
    echo "failed to remove private Adbyby restore snapshot" >&2
    exit 1
fi
[ ! -e "$snapshot" ]
[ ! -e "$test_root/private/.adbyby-save.raw.dead" ]
[ ! -e "$test_root/private/.adbyby-save.next.dead" ]
# A symlinked save directory must not redirect cleanup into its target.
mkdir -m 700 "$test_root/snapshot-target"
printf 'keep\\n' > "$test_root/snapshot-target/.adbyby.save"
ln -s "$test_root/snapshot-target" "$test_root/snapshot-link"
if adbyby_remove_rule_snapshot "$test_root/snapshot-link"; then
    echo "unexpectedly cleaned a symlinked save directory" >&2
    exit 1
fi
[ "$(cat "$test_root/snapshot-target/.adbyby.save")" = "keep" ]
# Absence of the volatile private directory is normal during early boot.
adbyby_remove_rule_snapshot "$test_root/missing-private"
echo "Adbyby private firewall snapshot staging and stale-state cleanup regression: OK"

# Use an isolated configuration directory and avoid exercising the live snapshot path here.
PROG_PATH="$test_root/private"
DATA_PATH="$PROG_PATH/data"
mkdir -p "$DATA_PATH"
: > "$PROG_PATH/blockip.conf"
save_adbyby_rule_snapshot()
{
    printf 'saved\n' >> "$TEST_SNAPSHOT_LOG"
    return 0
}

# A partial nat-chain build must fail before the PREROUTING hook is installed.
: > "$TEST_COMMAND_LOG"
: > "$TEST_IPSET_LOG"
: > "$TEST_SNAPSHOT_LOG"
FAIL_IPTABLES_MATCH='-t nat -A ADBYBY -d 10.0.0.0/8 -j RETURN'
export FAIL_IPTABLES_MATCH
if add_rule; then
    echo "unexpectedly accepted a failed ADBYBY chain mutation" >&2
    exit 1
fi
unset FAIL_IPTABLES_MATCH
grep -F -- '-t nat -F ADBYBY' "$TEST_COMMAND_LOG" >/dev/null
if grep -F -- '-t nat -I PREROUTING -p tcp --dport 80 -j ADBYBY' "$TEST_COMMAND_LOG" >/dev/null; then
    echo "installed PREROUTING hook after a chain mutation failed" >&2
    exit 1
fi
[ ! -s "$TEST_SNAPSHOT_LOG" ]

# Failed ipset restore must propagate and trigger cleanup before traffic is redirected.
: > "$TEST_COMMAND_LOG"
: > "$TEST_IPSET_LOG"
: > "$TEST_SNAPSHOT_LOG"
FAIL_IPSET_RESTORE=1
export FAIL_IPSET_RESTORE
if add_rule; then
    echo "unexpectedly accepted a failed ipset restore" >&2
    exit 1
fi
unset FAIL_IPSET_RESTORE
grep -F -- '-! restore' "$TEST_IPSET_LOG" >/dev/null
grep -F -- '-t nat -F ADBYBY' "$TEST_COMMAND_LOG" >/dev/null
if grep -F -- '-t nat -I PREROUTING -p tcp --dport 80 -j ADBYBY' "$TEST_COMMAND_LOG" >/dev/null; then
    echo "installed PREROUTING hook after ipset restore failed" >&2
    exit 1
fi
[ ! -s "$TEST_SNAPSHOT_LOG" ]

# A failed final hook insertion must still request cleanup and must not publish a snapshot.
: > "$TEST_COMMAND_LOG"
: > "$TEST_IPSET_LOG"
: > "$TEST_SNAPSHOT_LOG"
FAIL_IPTABLES_MATCH='-t nat -I PREROUTING -p tcp --dport 80 -j ADBYBY'
export FAIL_IPTABLES_MATCH
if add_rule; then
    echo "unexpectedly accepted a failed PREROUTING insertion" >&2
    exit 1
fi
unset FAIL_IPTABLES_MATCH
grep -F -- '-t nat -D PREROUTING -p tcp --dport 80 -j ADBYBY' "$TEST_COMMAND_LOG" >/dev/null
grep -F -- '-t nat -F ADBYBY' "$TEST_COMMAND_LOG" >/dev/null
[ ! -s "$TEST_SNAPSHOT_LOG" ]

# The full path succeeds when every required firewall/ipset operation succeeds.
: > "$TEST_COMMAND_LOG"
: > "$TEST_IPSET_LOG"
: > "$TEST_SNAPSHOT_LOG"
if ! add_rule; then
    echo "unexpectedly rejected a successful firewall setup" >&2
    exit 1
fi
grep -F -- '-t nat -I PREROUTING -p tcp --dport 80 -j ADBYBY' "$TEST_COMMAND_LOG" >/dev/null
grep -F -- '-I FORWARD -m set --match-set blockip dst -j DROP' "$TEST_COMMAND_LOG" >/dev/null
grep -F -- '-I OUTPUT -m set --match-set blockip dst -j DROP' "$TEST_COMMAND_LOG" >/dev/null
[ -s "$TEST_SNAPSHOT_LOG" ]
echo "Adbyby firewall setup failure propagation and cleanup regression: OK"
