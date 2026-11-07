# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — HTTPD generic dispatch and executable-script write gates (host verified)

- Main HEAD immediately before this handoff update: 5025bcb42d92c715d55595c15ff161cf13eb920e; active branch: main. This documentation checkpoint advances main; always fetch live main before follow-on work.
- Previous unfinished task recovered: generic sys_script() /tmp/<name> _eval fallback trusted-session gate. Its exact push-triggered HTTPD regression is confirmed SUCCESS: run 42392318537, exact head SHA fb488c486462bdbf1dbedbaad559f21bd7f1785c.
- Follow-up P1 finding: write_textarea_to_file() classified .sh payloads as executable root-managed files and wrote them as mode 0755, but the common writer had no trusted-session gate. Most General custom-script variable definitions used EVM_BLOCK_UNSAFE, while both 2.4 GHz / 5 GHz scripts.ap_script.sh entries did not. validate_asp_apply() also directly executed /etc/storage/ap_script.sh without checking trusted-session policy. This required valid authenticated management access; it was not unauthenticated access.
- Source commit: 5025bcb42d92c715d55595c15ff161cf13eb920e (fix(httpd): restrict executable script writes to trusted sessions). Changes: added httpd_script_policy.h and GNU89 regression; the shared .sh writer rejects non-trusted sessions before temporary or destination file writes; both wireless AP-script entries now carry EVM_BLOCK_UNSAFE; direct AP-script execution requires get_login_safe(). Existing HTTPS allowance in get_login_safe() remains unchanged.
- GitHub Actions: HTTPD Authentication and Security Regression run 42393893775 completed SUCCESS on exact code SHA 5025bcb42d92c715d55595c15ff161cf13eb920e. Both job steps passed: policy regression compilation/execution and production source-wiring assertions. The job log includes “HTTPD executable script write and execution trust boundary: OK” and confirms generic /tmp, SystemCmd, auth-route, service-ID, stale-upload/archive, firewall token, strict Basic-auth and private-key checks. Run URL: https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42393893775
- Validation: L1 targeted source review PASS. L2 focused GNU89 host policy executable PASS and Actions PASS on exact source SHA. No full production web_ex.c MIPS target compilation, firmware image generation/inspection, live-router runtime validation or L4 hardware validation was performed; all remain NOT RUN / NOT VERIFIED.
- Branch recovery: at the preceding main SHA fa1c6a32b40c84facfff03f75da81c03d7502791, the five surviving remote refactor/* branches were all substantially behind; ahead-only deltas were SkipDBv2 BStreamTag, DBAPI and UArray test/CI changes, or empty. None contained the HTTPD task delta or was merged. Re-fetch and re-compare before any future branch reuse.
- Whole-repository scope remains open. The new script gate is bounded; complete HTTPD/CGI/ACL and script-consumer coverage is not claimed. Firewall malformed schedules can still fall back to unqualified/all-time rules; remaining firewall inputs and IPv6 parity remain open. DNS/DHCP, WAN/PPP/IPv6, wireless, file services and system lifecycle are not fully inventoried/audited.

### Next exact actions

1. Enumerate real /tmp executable targets and all sys_script(), ej_dump(), and POST action_script callers; review the remaining fixed-name handlers as a separate authorization matrix.
2. Trace each scripts.* shell file from variable registration and web write path to its service/init consumer, including AP, VPN, DDNS, FRP and custom scripts; search for alternate file-write paths and root eval/_eval/doSystem calls that accept request/NVRAM data.
3. Continue HTTPD session/logout/ACL review, especially process-global IP-owned session and same-NAT behavior, then continue privileged RC command/configuration review.
4. Resume firewall audit: make malformed date/time schedules fail closed, then audit unreviewed iptables/ip6tables values, chain rollback and IPv6 parity.
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
