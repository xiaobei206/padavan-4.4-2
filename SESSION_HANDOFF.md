# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — HTTPD generic /tmp script trusted-session gate (CI pending)

- Main HEAD immediately before this handoff update: `f15276ff8989b6c672e6515346824cd3af01a3d8`; active branch: `main`. Re-fetch live main before follow-on work.
- Source/workflow checkpoint: `fb488c486462bdbf1dbedbaad559f21bd7f1785c`. Production authorization fix: `3ec7ab30c91b7fb95ad8834695c0f2b18bec9daf`. Workflow contract: `fc8f1af48cdc66ac007e86e7c7d1de5f49a8c587`; the initial fallback locator over-escaped newlines and was corrected in `fb488c486462bdbf1dbedbaad559f21bd7f1785c`. Roadmap checkpoint: `b2fb0e0120e352650308f74c6281d2ec41a9cf91`; chronological dev-log checkpoint: `f15276ff8989b6c672e6515346824cd3af01a3d8`.
- Audited `web_ex.c::sys_script()`, `ej_dump()`, `update_variables_ex()`, `apply_cgi()`, the authenticated CGI MIME routes, and the existing request-to-dispatch flow. `update_variables_ex()` and `apply_cgi()` take POST-controlled `action_script` and call the dispatcher; `ej_dump()` passes a template-supplied script argument. Fixed-name handlers precede a generic fallback.
- P1 finding: the generic fallback accepted a name, built `/tmp/<name>`, parsed arguments and called `_eval()` as root. The CGI routes require authentication, and the current character filters constrain argument syntax, but the generic dispatcher did not require a trusted session. A validly authenticated but non-trusted session could request execution of an existing matching executable in `/tmp`. This required valid credentials and a pre-existing target; this was not unauthenticated execution or arbitrary file upload.
- Fix: the generic fallback now checks `get_login_safe()` and returns before parsing the name/arguments or invoking `_eval()`. Fixed named handlers above the fallback retain their previous policies. The existing SystemCmd queue submit/consume guards are unchanged.
- Regression contract added to `.github/workflows/httpd-security.yml`: it verifies that the generic fallback contains the trusted-session gate before token parsing and `_eval()`, constructs the constrained `/tmp/%s` path, and is still reached from both POST-controlled `action_script` handlers. Existing MIME route/auth ordering assertions remain in force.
- Actions: the exact-head combined commit-status response for `fb488c486462bdbf1dbedbaad559f21bd7f1785c` was empty; the available commit/workflow-run query returned no PR-triggered runs. The push-triggered HTTPD Authentication and Security Regression for this source/workflow SHA is **PENDING / NOT RETRIEVED**. Earlier run `42390628940` is SUCCESS only for older SHA `31d94312ccac3451392f8c46fd901b295287dca4` and does not verify this gate. Do not claim a passing CI result for the current task until the exact SHA is checked.
- Validation: L1 source review and source-contract inspection PASS. No local compiler/CTest execution was performed. L2 Actions result PENDING / NOT RETRIEVED; production `web_ex.c` target compilation, MIPS image build, firmware inspection, runtime behavior and hardware validation are NOT RUN / NOT VERIFIED.
- Remote checkpoint branches compared against main at `5aad2437af53042eb0fe5af29cfa7ab1269c1d60`: `refactor/bstream-tag-wire` 1 ahead / 470 behind (SkipDBv2 BStreamTag/test/CI); `refactor/dbapi-result-20261105` 8 ahead / 551 behind (SkipDBv2 DBAPI/test/CI); `refactor/dynlib-lifecycle` 0 ahead / 474 behind; `refactor/sorting-boundaries` 0 ahead / 470 behind; `refactor/uarray-character-ctype` 2 ahead / 468 behind (SkipDBv2 UArray/test/CI). These branches are stale and contain no delta for this HTTPD task; none was merged.
- Whole-repository scope remains open. Firewall invalid-schedule fail-closed behavior, remaining iptables/ip6tables inputs and IPv6 parity also remain open; do not return to routine SkipDBv2 expansion.

### Next exact actions

1. Retrieve the push-triggered HTTPD Authentication and Security Regression result for exact SHA `fb488c486462bdbf1dbedbaad559f21bd7f1785c`. If it fails, inspect the failing step/log and fix that before starting unrelated work.
2. Enumerate legitimate generic `/tmp` script files and all template/request callers; confirm compatibility of the new trusted-session policy. Review the `_eval` argument semantics and `/tmp` file ownership/creation paths.
3. Trace `scripts.*` textarea writes, the `ap_script.sh` execution path, associated variable event masks and callers. Continue all remaining request/NVRAM-to-`eval`/`_eval`/`doSystem`/filesystem sinks.
4. Continue HTTPD logout/session/ACL and privileged RC reviews, then DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM and service lifecycle. Keep the firewall schedule fail-closed defect in the P1 follow-up queue.
5. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` only when actually executed; inspect the resulting firmware image. L3/L4 remain unverified.

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
