## Latest resumable checkpoint — AP and AliDDNS root-managed temporary executable staging hardened (exact-head Actions verified)

- Source checkpoint on `main`: `bd89f748a4c86c0f8267f4fa31d40c99dd4a448f` (`fix(security): isolate root script staging in private tmpfs`), parent `c3caea4906db771fada2388e5fad569202697041`. This checkpoint follows the previous HTTPD script-writer staging fix; fetch live `main` and Actions before starting the next task.
- Completed the previous checkpoint's bounded add-on consumer trace. The 28 `scripts.*` File entries (27 unique names) still flow through `validate_asp_apply()` to `/etc/storage/<filename>`, with `ej_dump()` reading the same storage prefix. RC consumers include startup/shutdown, WAN up/down, Internet-state, post-firewall, VPN up/down, button and AP paths.
- Add-on semantics now traced: `aliddns.sh` sources `/etc/storage/ddns_script.sh`; the built-in `DDNS_DONE_SCRIPT=/sbin/ddns_updated` is a separate inadyn callback. `frp.sh` executes `/etc/storage/frp_script.sh`. Shadowsocks `ss_*.sh` and `*dom*.sh` files are parsed as policy/address/domain lists; Adbyby `adbyby_*.sh` files are consumed as rule/block/host lists; SmartDNS `smartdns_*.conf` files are configuration data, not shell entry points.
- P1 static finding 1: the stock `trunk/user/scripts/files/usr/bin/ap_script.sh` wrote a generated root-executed daemon to predictable shared path `/tmp/sh_apauto.sh`, then changed it to mode 0777. Fix: claim a per-instance directory under `/home/root` with atomic `mkdir` and `umask 077`; write the generated body inside that directory; set the script to 0700; and remove the script/directory from an EXIT/signal cleanup trap. The generated basename remains `sh_apauto.sh` for the existing process check.
- P1 static finding 2: `trunk/user/aliddns/aliddns.sh` wrote its `_aliddns` PATH launcher to predictable `/tmp/script/_aliddns` and chmodded it 0777. Fix: move the helper to `/home/root/.aliddns/_aliddns` (the firmware mounts `/home` as tmpfs and creates `/home/root` mode 0700), create/retain a 0700 directory and helper, put the private directory first in PATH, and update the cleanup lookup. Both fixes keep transient launchers in root-private RAM rather than shared `/tmp` or persistent flash.
- CI updates: the HTTPD security workflow now triggers on `dev_init.sh`, the AP default script and AliDDNS script for both push and pull requests; the pull-request filter also now includes the previously omitted HTTPD script-policy header and regression source. The new job step checks both outer shell files, extracts and syntax-checks the generated AP daemon body, and asserts the private-directory/permission contracts.
- Exact-head Actions: run `42400922809` completed SUCCESS on exact source SHA `bd89f748a4c86c0f8267f4fa31d40c99dd4a448f`; job `128488273622` SUCCESS. GNU89 policy regression compile/run, root-managed script staging checks and HTTPD auth/route-wiring checks all passed.
- Validation boundary: targeted source/call-path review PASS; L2 host Actions PASS. The checks are shell syntax and source contracts, not a live symlink-race/concurrency test or AP/ALiDDNS device runtime test. No MIPS target build, firmware image, live router or hardware validation was run.
- Upgrade caveat: `autostart.sh` runs an existing executable `/etc/storage/ap_script.sh` and copies the ROM default only when that stored file is not executable. This change updates the ROM default template; it deliberately does not overwrite an existing user-managed executable copy. Existing devices with an older executable persistent copy need a separate safe migration decision; do not blindly replace user customizations.
- Next exact action: review a safe stock-only migration strategy for existing persistent AP script copies, then continue the highest-risk unfinished P1 work: HTTPD logout/session/ACL and same-NAT ownership semantics, followed by firewall restore/IPv6 parity. Continue beyond HTTPD/SkipDB into DNS/DHCP, WAN/PPP, file sharing, storage/hotplug and service lifecycle. The bounded script trace does not close the whole userland audit.

## Latest resumable checkpoint — HTTPD script-writer staging-file TOCTOU fixed (exact-head Actions verified)

- Main code HEAD before this checkpoint: `0bbd8062e8bf7842e4879032aca3c1781bd393fa`; source commit: `0bbd8062e8bf7842e4879032aca3c1781bd393fa` (`fix(httpd): create upload staging files safely`). Documentation changes are committed separately afterward; fetch live `main` before the next task.
- Recovered the previous exact next action: trace all `scripts.*` file variables from HTTPD registration/write/read paths into RC/init/service consumers, and enumerate executable `/tmp` producers/callers.
- Inventory performed: `variables.c` contains 28 `scripts.*` File rows (27 distinct names because `ap_script.sh` appears in both wireless bands). `validate_asp_apply()` maps every `scripts.*` File entry to `/etc/storage/<filename>` through the shared `write_textarea_to_file()`; `ej_dump()` reads the matching `scripts.*` prefix from the same storage directory. All file classes still pass through the common filename validation and writer; `validate_cgi()` skips File entries rather than persisting them as NVRAM.
- Bounded consumer trace: `rc.c` starts `started_script.sh` and runs `shutdown_script.sh`; `net_wan.c` invokes `post_wan_script.sh` on WAN up/down and `inet_state_script.sh` for Internet-state changes; `firewall_ex.c` invokes `post_iptables_script.sh`; VPN server/client code invokes the configured up/down hooks; button handling invokes `ez_buttons_script.sh`; AP startup/auto-start invokes `ap_script.sh`. The other add-on-specific DDNS/FRP/Shadowsocks/Adbyby scripts are inventoried as registered files but their complete downstream consumer semantics are not yet traced.
- P1 local TOCTOU finding: `write_textarea_to_file()` previously removed a predictable `/tmp/.<filename>` and then called `fopen(path, "w")` through `write_file_dos2unix()`. A local process able to create entries in shared `/tmp` could race that unlink/open gap with a symlink and redirect the privileged HTTPD write. This is a static source finding; an exploit race was not stress-tested.
- Fix: split the CRLF writer into `write_file_dos2unix_fp()` plus the existing path wrapper; staging now uses `mkstemp("/tmp/httpd-upload-XXXXXX")` and `fdopen()` on the exact returned descriptor. The unique file is created atomically with restrictive 0600 permissions and is not reopened by pathname; the final destination writer retains its existing path behavior. Write/close failures are now reported from the stream helper and the staging path is unlinked on error and after comparison.
- Source/workflow files changed: `trunk/user/httpd/src/web_ex.c` and `.github/workflows/httpd-security.yml`. Workflow contracts now require the mkstemp → fdopen → descriptor-writer ordering and reject the former predictable `/tmp/.<filename>` template.
- Exact-head Actions: HTTPD Authentication and Security Regression run `42398764761` completed SUCCESS on exact source SHA `0bbd8062e8bf7842e4879032aca3c1781bd393fa`; job `128481780467` completed SUCCESS. The GNU89 policy regression compile/run step passed, as did production-source wiring assertions, including the new secure staging-file contract and the existing auth/session/CGI/firewall checks. This is focused host/structural verification, not a complete `web_ex.c` production compilation.
- Validation boundary: L1 source/call-path review PASS; L2 existing GNU89 policy regressions and exact-head Actions PASS. A concurrent symlink-race stress test, MIPS target build, XY-C1 firmware image inspection, live router behavior and L4 hardware validation were NOT RUN.
- Remote checkpoint comparison at baseline `dea8ed96eed030a77e8f1fc416330b2ec563348f`: `refactor/bstream-tag-wire` 1 ahead / 486 behind (SkipDBv2 BStreamTag only); `refactor/dbapi-result-20261105` 8 ahead / 567 behind (SkipDBv2 DBAPI only); `refactor/dynlib-lifecycle` 0 ahead / 490 behind; `refactor/sorting-boundaries` 0 ahead / 486 behind; `refactor/uarray-character-ctype` 2 ahead / 484 behind (SkipDBv2 UArray only). No checkpoint contains relevant HTTPD work; none was merged.
- Next exact action: finish tracing the registered add-on script consumers and identify every alternate writer/producer for executable `/tmp` files; then continue HTTPD session/logout/ACL and remaining firewall restore/IPv6 review. Extend the inventory into DNS/DHCP, WAN/PPP, file sharing, storage/hotplug and service lifecycle. Keep routine SkipDBv2 expansion paused.

## Latest resumable checkpoint — Fixed-name sys_script dispatch audit (source and exact-head host CI verified)

- Current main HEAD before this handoff update: `c4f7a8d8dc12abda6f872c0471bedac03e1b3a12`; active branch: `main`. Source commit `3859fc59fa3b890a6ce08a2956719c6640a88337` adds an explicit fixed-name dispatch policy to HTTPD.
- Recovered the prior handoff's next action: audit the fixed-name `sys_script()` branches separately from the arbitrary `/tmp/<name>` `_eval()` fallback.
- Audited `trunk/user/httpd/src/web_ex.c::sys_script`, its `ej_dump()`, `update_variables_ex()`, and `apply_cgi()` callers, the MIME route table, `ej.c::call_asp()`, `httpd.c::get_login_safe()`, relevant ASP call-site examples, `variables.c`, existing policy/tests and the HTTPD workflow.
- Fixed-handler matrix: `syscmd.sh` retains its dedicated trusted-session check and clears stale command state on denial; `ddnsclient` signals an existing inadyn process; `hostname_check` and `dlna_rescan` send fixed RC notifications; `lpr_remove` sends SIGUSR2 to the fixed lpd pidfile process; `mfp_requeue`/`mfp_monopolize` write one fixed byte to the USB printer FIFO and record the current login IP; remaining fixed names are no-op/read-only compatibility handlers. These names accept no request-supplied free-form command arguments. This is a bounded call-path review, not proof that all templates or downstream RC consumers are audited.
- Hardening: `httpd_script_policy.h` now carries an exact fixed-name allowlist; `sys_script()` checks it before entering the fixed dispatch chain. Any unlisted name still requires `get_login_safe()`; the generic fallback's existing second guard, bounded token parsing and `_eval()` argv dispatch remain intact. Existing fixed-handler behavior is intentionally unchanged.
- Regression: `tests/httpd_script_policy.c` covers every fixed name, unknown names for trusted/untrusted sessions, and NULL. Workflow source contracts assert allowlist contents and gate ordering. The HTTPD workflow pull-request path filter now includes `variables.c`, matching the push filter.
- Audited routes: dynamic `*.asp`/`*.htm`, `update.cgi`, and `apply.cgi` are behind `need_auth=1`. `ej_dump()` receives file/script values from server-side ASP templates. `get_login_safe()` requires an active login and permits the existing HTTPS path; otherwise it uses the login-time safe-IP classification (or the existing AP-mode rule). No broad session-policy change was made.
- Exact-head Actions: run `42397188114` completed SUCCESS on push event, exact head SHA `3859fc59fa3b890a6ce08a2956719c6640a88337`; job `128476978918` completed SUCCESS. The GNU89 policy regression compilation/execution and HTTPD source-wiring contract steps both passed. Job log explicitly reports “HTTPD fixed sys_script dispatch allowlist: OK”, along with the existing auth/session/upload/firewall guards. Earlier firewall schedule work is also verified: run `42396303871` SUCCESS on exact source SHA `166274833a1df1ca8a8312fa1d250df72934b738`.
- L1 review: bounded source/call-path review performed. L2 focused GNU89 regression and exact-head Actions are PASS. L3 MIPS target build, complete firmware image validation, live router behavior and L4 hardware validation: NOT RUN. HTTPD/CGI full audit remains IN PROGRESS.
- The five remote `refactor/*` branches were re-compared against the base main checkpoint: all are 481–564 commits behind; ahead-only changes remain limited to SkipDBv2 tests/CI/BaseKit/DBAPI/UArray or none. No stale branch changes were merged.
- Next exact action: trace every `scripts.*` shell file from the HTTPD writer and variable registration to its actual RC/init/service consumer (including alternate write routes and root `eval`/`_eval`/`doSystem` sinks), then enumerate executable `/tmp` producers/callers and continue HTTPD session/ACL plus firewall restore/IPv6 review. Keep routine SkipDBv2 expansion paused.

# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Firewall schedule fail-closed dispatch (source fix; exact-head Actions pending)

- Main documentation HEAD immediately before this handoff update: `dde51661ccc1ee35732938b4d3d6df5c4968f3c8`. Production source checkpoint: `166274833a1df1ca8a8312fa1d250df72934b738`; roadmap checkpoint: `7af500fac248839e2a2df8bdde8372365d0eee29`; dev-log checkpoint: `dde51661ccc1ee35732938b4d3d6df5c4968f3c8`. This handoff update is the final documentation write; fetch live `main` again before follow-on work.
- Prior handoff task recovery: the HTTPD executable-script write/execution trust-boundary fix was already complete on source SHA `5025bcb42d92c715d55595c15ff161cf13eb920e`, with HTTPD Actions run `42393893775` SUCCESS on that exact SHA. This round selected the next explicit P1 gap rather than repeating that work.
- P1 finding: `timematch_conv()` produced an empty suffix for invalid weekday/time data and `0000000`. MAC, URL and LAN-WAN consumers treated the empty suffix as no schedule restriction, turning the affected configured rule/jump into an all-time rule.
- Fix: added `firewall_schedule_is_active()` in `trunk/user/rc/src/firewall_filter_policy.h`; changed `timematch_conv()` in `firewall_ex.c` to return eligibility; valid all-days/all-day still means a legitimate all-time match. MAC filter entries, IPv4 and IPv6 URL jumps, and the IPv4 LAN-WAN filter jump skip malformed/disabled schedules. The MAC configured-entry count remains incremented before skip so whitelist mode retains its terminal DROP fallback.
- Production/test/workflow files changed: `firewall_filter_policy.h`, `firewall_ex.c`, `tests/firewall_filter_policy.c`, `.github/workflows/httpd-security.yml`. Commits: `01b0aae8758a638347cc8968a4e81bd021032697` (policy), `e39d4556136ce6982117bcb81a4e6c6ea0c3d40e` (production), `fd8f5306fb23b315fabb2ec8621ee4d21b3f14fa` (tests), `034d4de4d98103402c9980b7a9dfefaa877fa85f` (workflow contracts), `166274833a1df1ca8a8312fa1d250df72934b738` (style cleanup).
- Validation: local standalone host harness for the schedule-policy helper compiled and ran with `cc -std=gnu89 -Wall -Wextra -Werror` — PASS. Final source review assertions confirm the active check is wired to all four consumers and the test/workflow contracts match. This is not a production `firewall_ex.c` compile.
- Actions: combined status for exact source SHA `166274833a1df1ca8a8312fa1d250df72934b738` was empty; the available commit-workflow query returned no PR-triggered run. Exact push-triggered HTTPD Security workflow run `42396303871` completed SUCCESS on SHA `166274833a1df1ca8a8312fa1d250df72934b738`; it ran the focused GNU89 policy regressions and source-wiring checks.
- Not run: full MIPS target compile, XY-C1 firmware build/artifact inspection, live `iptables-restore`/`ip6tables-restore` behavior, and hardware validation. Firewall and whole-repository audits remain IN PROGRESS.
- Remote `refactor/*` checkpoint branches compared at baseline `5e465c371fe4ff585280ee6724152355be42fa94` were all hundreds of commits behind; their ahead-only deltas were old SkipDBv2 BStreamTag/DBAPI/UArray or empty. None was relevant or merged.

### Next exact actions

1. Retrieve/verify the exact-head Actions run for `166274833a1df1ca8a8312fa1d250df72934b738`; inspect/fix any failure before treating the regression as green.
2. Enumerate real `/tmp` executable targets and all `sys_script()`, `ej_dump()` and POST `action_script` callers; audit fixed-name handler authorization separately.
3. Trace every `scripts.*` shell-file variable from web writer through its RC/init/service consumer (AP, VPN, DDNS, FRP, proxy and custom scripts), plus alternate write paths and root `eval`/`_eval`/`doSystem` sinks.
4. Continue HTTPD session/logout/ACL and privileged RC authorization review; then continue remaining firewall restore inputs, command/interface arguments, rollback and IPv6 parity.
5. Expand the whole-userland inventory/audit through DNS/DHCP, WAN/PPP, wireless, file sharing, storage/hotplug and init/service lifecycle. Keep routine SkipDBv2 expansion paused.
6. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` only when actually executed; inspect the produced image. Do not claim target build or hardware validation from host tests.
## Latest resumable checkpoint — HTTPD generic dispatch and executable-script write gates (host verified)

- Main HEAD immediately before this handoff update: 5025bcb42d92c715d55595c15ff161cf13eb920e; active branch: main. This documentation checkpoint advances main; always fetch live main before follow-on work.
- Previous unfinished task recovered: generic sys_script() /tmp/<name> _eval fallback trusted-session gate. Its exact push-triggered HTTPD regression is confirmed SUCCESS: run 42392318537, exact head SHA fb488c486462bdbf1dbedbaad559f21bd7f1785c.
- Follow-up P1 finding: write_textarea_to_file() classified .sh payloads as executable root-managed files and wrote them as mode 0755, but the common writer had no trusted-session gate. Most General custom-script variable definitions used EVM_BLOCK_UNSAFE, while both 2.4 GHz / 5 GHz scripts.ap_script.sh entries did not. validate_asp_apply() also directly executed /etc/storage/ap_script.sh without checking trusted-session policy. This required valid authenticated management access; it was not unauthenticated access.
- Source commit: 5025bcb42d92c715d55595c15ff161cf13eb920e (fix(httpd): restrict executable script writes to trusted sessions). Changes: added httpd_script_policy.h and GNU89 regression; the shared .sh writer rejects non-trusted sessions before temporary or destination file writes; both wireless AP-script entries now carry EVM_BLOCK_UNSAFE; direct AP-script execution requires get_login_safe(). Existing HTTPS allowance in get_login_safe() remains unchanged.
- GitHub Actions: HTTPD Authentication and Security Regression run 42393893775 completed SUCCESS on exact code SHA 5025bcb42d92c715d55595c15ff161cf13eb920e. Both job steps passed: policy regression compilation/execution and production source-wiring assertions. The job log includes “HTTPD executable script write and execution trust boundary: OK” and confirms generic /tmp, SystemCmd, auth-route, service-ID, stale-upload/archive, firewall token, strict Basic-auth and private-key checks. Run URL: https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42393893775
- Validation: L1 targeted source review PASS. L2 focused GNU89 host policy executable PASS and Actions PASS on exact source SHA. No full production web_ex.c MIPS target compilation, firmware image generation/inspection, live-router runtime validation or L4 hardware validation was performed; all remain NOT RUN / NOT VERIFIED.
- Branch recovery: at the preceding main SHA fa1c6a32b40c84facfff03f75da81c03d7502791, the five surviving remote refactor/* branches were all substantially behind; ahead-only deltas were SkipDBv2 BStreamTag, DBAPI and UArray test/CI changes, or empty. None contained the HTTPD task delta or was merged. Re-fetch and re-compare before any future branch reuse.
- Whole-repository scope remains open. The script gates are bounded; complete HTTPD/CGI/ACL and script-consumer coverage is not claimed. At this earlier checkpoint, the firewall malformed-schedule fail-open issue was still open; it is superseded by the 2026-11-08 checkpoint above. Other firewall inputs and IPv6 parity remain open. DNS/DHCP, WAN/PPP/IPv6, wireless, file services and system lifecycle are not fully inventoried/audited.

### Next exact actions

1. Enumerate real /tmp executable targets and all sys_script(), ej_dump(), and POST action_script callers; review the remaining fixed-name handlers as a separate authorization matrix.
2. Trace each scripts.* shell file from variable registration and web write path to its service/init consumer, including AP, VPN, DDNS, FRP and custom scripts; search for alternate file-write paths and root eval/_eval/doSystem calls that accept request/NVRAM data.
3. Continue HTTPD session/logout/ACL review, especially process-global IP-owned session and same-NAT behavior, then continue privileged RC command/configuration review.
4. Continue the firewall audit beyond the bounded schedule fix: audit unreviewed iptables/ip6tables values, interface/address and shell arguments, chain rollback, restore failure handling and IPv6 parity.
5. Extend source-tree/build-manifest inventory into DNS/DHCP, WAN/PPP, wireless, file sharing, storage/hotplug and init/service lifecycle. Keep routine SkipDBv2 expansion paused.
6. Run make XY-C1 TOOLCHAIN=mipsel-linux-musl only when actually executed; inspect the produced firmware artifact. Do not label host regression as target or hardware validation.


---

**Purpose:** resume work across ChatGPT conversations. GitHub is the source of truth; re-check all values below against live repository state before acting.

## Bootstrap checkpoint

- Repository: xiaobei206/padavan-4.4-2
- Primary branch: main
- Parent HEAD immediately before this governance bootstrap: 412eebb4d0c36f9cbaf43921b4ce1f2056665984
- This governance bootstrap creates/updates the protocol, bootstrap prompt, roadmap, this handoff, and dev_log.md in one documentation commit. On startup, fetch live main HEAD to obtain the actual resulting commit SHA; do not assume the parent SHA is still current.
- Previous latest relevant CI at the pre-bootstrap parent HEAD: SkipDBv2 Regression run 42376349424, conclusion success, head SHA 412eebb4d0c36f9cbaf43921b4ce1f2056665984. Earlier inspection reported all 57 CTests passed. This does not establish full firmware build or hardware validation.

## Current task state

- Governance bootstrap: COMPLETE; commits 84e2fb77973393dd2804b9feb2d94b2ce74933c4 and 8152ec72c39d4a435114806a5e861e2aed925248 remain in history.
- HTTPD private-key disclosure trust-boundary fix: COMPLETE; production-code commit 1ef811e5cb37613fc5ae0caf59041ecdf294e5d2 is on main and its focused Actions regression passed.
- Broader HTTPD/HTTPS/CGI/authentication review: IN PROGRESS.
- Whole-userland audit: IN PROGRESS; inventory is preliminary and most modules remain unaudited.
- No full MIPS firmware build or hardware validation is claimed.

## Next conversation — mandatory first actions

1. Fetch live main HEAD and verify the governance bootstrap commit is present.
2. Read AI_DEVELOPMENT_PROTOCOL.md.
3. Read this file and REFACTOR_ROADMAP.md.
4. Read the newest dev_log.md entries.
5. Inspect remote branches/checkpoints and latest Actions runs, matching each run to its exact head SHA.
6. Perform a recursive inventory of the current source tree, build manifests, init scripts and service integration. Record real paths and feature gates in REFACTOR_ROADMAP.md.
7. Validate the historical audit pointers in GPT_AUDIT_NOTES.txt against current code and commit history.
8. Prioritize a systematic HTTPD/HTTPS/CGI/authentication/authorization review. In particular, a historical response-write fix is not evidence that HTTPD has been fully audited.
9. Work on one bounded module only; add tests/integration, validate, update all three records, and commit coherently.

## Known facts and cautions

- The recent 273-commit comparison from 8a6fc2639fcd036790b02eae56cadb2c2935883d to 412eebb4 was concentrated on SkipDBv2/BaseKit/UDB, tests, CI, and dev_log; HTTPD was not among the changed paths in that comparison.
- The repository's older GPT_AUDIT_NOTES.txt does contain historical pointers to httpd.c, https.c, web_ex.c, ej.c and other service code. Treat them as leads to verify, not as proof of complete security review.
- Do not restart SkipDBv2 work by default.
- Do not claim MIPS build, complete firmware artifact, or XY-C1 hardware validation unless actually run and inspected.
- If this task's code cannot be completed within the current conversation, persist it to a remote checkpoint branch when supported and record the exact branch/SHA here.
