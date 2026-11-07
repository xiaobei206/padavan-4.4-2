# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Firewall token audit + HTTPD SystemCmd queue authorization (host regression + CI PASS)

- Current code/workflow HEAD before this documentation update: 31d94312ccac3451392f8c46fd901b295287dca4. Re-fetch live main before any follow-on work.
- Recovered and completed the bounded firewall input pass: URL and ICMP tokens, filter-LW ports/TCP flags/protocol numbers, VTS public/local ports/protocol numbers, and schedule shape/range guards. Firewall remains AUDITING because other restore inputs, shell/interface arguments, rollback and IPv6 parity remain open.
- New P1 CGI/privileged-command finding: Advanced_Console_Content.asp disables the console with a client-side login_safe() check, but a direct authenticated POST to apply.cgi action_mode=' SystemCmd ' previously populated the process-global SystemCmd buffer without a server-side trusted-session check. sys_script('syscmd.sh') later executes that buffer only when get_login_safe() is true, but its denied branch previously left the queued command intact. Because the console page runs nvram_dump('syscmd.log','syscmd.sh') while rendering, a non-safe HTTP login with valid credentials could stage a command for execution when a trusted session later opened the console. This required valid authentication; it was not an unauthenticated route.
- Fix in web_ex.c: reject SystemCmd submissions unless get_login_safe() is true, clear the pending buffer and return HTTP 403 on denial; clear the buffer again whenever the syscmd execution gate denies a pending command. This enforces the restriction at the server boundary and prevents a stale cross-session command from surviving a denied run.
- Latest matching Actions run: HTTPD Authentication and Security Regression 42390628940 completed SUCCESS on exact SHA 31d94312ccac3451392f8c46fd901b295287dca4. The GNU89 policy regression step and HTTPD/firewall/SystemCmd source-wiring step passed. The new wiring assertion verifies trusted-session check -> queue clear/403 -> request capture/copy, and stale-queue clearing in the execution-denied branch.
- Relevant commits after the previous checkpoint: SystemCmd server gate and execution-denial queue clear e68e1ff12b92d22549d28cb20c9c0256dff1f492; workflow source-boundary correction 5aa2656eb1c8e641d759e453497f338282f13fae; escape-independent source assertions and final CI checkpoint 31d94312ccac3451392f8c46fd901b295287dca4. The two earlier workflow-only attempts failed on brittle source assertion expressions; the final exact-head run is green.
- The firewall schedule caveat remains: malformed date/time currently produces an empty match suffix, so the legacy rule becomes unqualified/all-time. Syntax injection is prevented, but fail-closed schedule semantics remain open.
- Remote checkpoint comparison at this HEAD: refactor/bstream-tag-wire 1 ahead / 463 behind (SkipDBv2 BStreamTag/test/CI); refactor/dbapi-result-20261105 8 ahead / 544 behind (SkipDBv2 DBAPI/test/CI); refactor/dynlib-lifecycle 0 ahead / 467 behind; refactor/sorting-boundaries 0 ahead / 463 behind; refactor/uarray-character-ctype 2 ahead / 461 behind (SkipDBv2 UArray/test/CI). They are stale and do not contain this userland security work; no delta was merged.
- Next risk areas remain: generic sys_script() /tmp _eval dispatch and script-file mutation/execution, remaining privileged doSystem/eval/RC command callsites, HTTPD session/ACL edges, then DNS/DHCP, WAN/PPP/IPv6 and service lifecycle. Do not return to routine SkipDBv2 expansion.
- Validation level: host C regression and static source-wiring checks only. Production web_ex.c/firewall_ex.c target compilation, XY-C1 MIPS build, firmware inspection, runtime behavior and hardware tests are NOT RUN / NOT VERIFIED.

### Next exact actions

1. Audit the generic sys_script() /tmp dispatcher and every request-controlled action_script caller. Determine its legitimate callers, enforce authorization for command execution, and test allowed/rejected script-name and argument paths.
2. Trace scripts.* textarea writes, ap_script.sh execution, and remaining eval/_eval/doSystem calls to their input trust and route authorization. Preserve the SystemCmd trusted-session boundary.
3. Continue HTTPD session/logout/ACL and privileged RC reviews; then DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM and system service lifecycle.
4. Return to firewall for other iptables/ip6tables sinks, IPv6 parity and explicit invalid-schedule semantics.
5. Run make XY-C1 TOOLCHAIN=mipsel-linux-musl only when actually executed; inspect the produced firmware image. L3/L4 remain unverified.
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
