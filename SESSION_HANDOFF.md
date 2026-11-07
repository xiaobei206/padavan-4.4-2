# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Prevent symlink-parent traversal in storage restore archives

- Baseline main HEAD before this bounded code task: `a6f284111241c28ba1577d12264d3288338e53eb`. Previous stale-upload cleanup commit `5877544ea347ab728bd185012187ec09e2aeb76c` has matching successful Actions run `42385963510`.
- P1 finding: `mtd_storage.sh::func_restore()` executes root BusyBox `tar xjf ... -C /tmp/storage`, checks only for `start_script.sh`, then copies the extracted tree into `/etc`. BusyBox tar normalizes archive member names but does not normalize symlink targets. `data_extract_all()` may create a symlink member (for example `escape -> /etc`), then process `escape/passwd`; with `ARCHIVE_UNLINK_OLD`, unlink/open on the child resolves through the prior symlink and can modify a path outside the extraction root. This is an archive path-traversal issue in authenticated restore, not a claim of unauthenticated access.
- Fix in this checkpoint: add a small shared `safe_extract_path.h` policy and call it at the start of BusyBox `data_extract_all()`, before leading-directory creation, unlink or file open. Reject absolute names, any `..` component, and any existing parent component that `lstat()` finds to be a symlink or non-directory. Missing parent components remain valid so normal tar directory creation works. A host regression tests safe paths, missing parents, regular-file/symlink parents, absolute/parent paths and NULL/empty input.
- The HTTPD Actions workflow now compiles/runs that regression and statically verifies the safety guard precedes archive extraction side effects and that `mtd_storage.sh` checks tar failure before accepting the marker/copying extracted data. Validation is PENDING for the new code SHA until its matching Actions run finishes. Full BusyBox/firmware target build, archive fuzzing and hardware validation remain NOT VERIFIED.

### Next exact actions

1. Verify the new HTTPD Actions run against its exact code SHA. If the policy regression passes, retain the targeted finding/fix and don't overstate it as complete archive fuzzing.
2. Audit NVRAM/image header arithmetic, archive link semantics and restore rollback; consider BusyBox extraction compatibility for valid symlink-based storage entries.
3. Review `do_apply_cgi` and `do_update_cgi` for user-controlled EJ handler names and argument/command/config dataflow; then TLS I/O and connection lifecycle.
4. Continue across firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file-sharing, storage/hotplug and init/service lifecycle. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` and inspect full firmware only when actually executed; L3/L4 remain NOT VERIFIED.
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
