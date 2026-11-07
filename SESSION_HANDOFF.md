# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Prevent stale firmware/NVRAM/storage upload reuse

- Baseline main HEAD before this bounded change: `87ce7c634721c2be7a7b3f52b970ad4f6f991892`. MIME route matrix test commit `015b820fc61cb8e3cc1283a0a87a48751205eae6` is host-verified by Actions run `42385661621` on its exact SHA.
- P1 finding in `trunk/user/httpd/src/upload.c`: `handle_request()` invokes a MIME handler's output callback after the input callback even when `do_upload_file()` fails. Previously, `do_upload_file()` only unlinked its destination after it had found the expected multipart `Content-Disposition` and skipped headers. A malformed new upload could therefore leave a stale file from an interrupted prior request, which `do_restore_nv_cgi()`/`do_restore_st_cgi()` could see via `f_exists()` and pass to privileged restore commands. Separately, `do_restore_st_post()` returned on missing/invalid/oversized boundary before touching the old storage archive.
- Fix in this checkpoint: clear the fixed upload destination before validating/reading the new request envelope, and clear the storage-restore destination before any early boundary rejection. Existing failure cleanup and payload/header validation remain in place. The CI source contract checks cleanup ordering in both shared uploader and storage preflight.
- Validation: HTTPD Authentication and Security Regression run `42385963510` completed SUCCESS against exact SHA `5877544ea347ab728bd185012187ec09e2aeb76c`: https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42385963510. Existing C regression executables compiled/ran with GNU89 `-Wall -Wextra -Werror`; source-wiring checks confirmed stale destination clearing before multipart parsing and storage boundary rejection. This is a targeted stale-artifact lifecycle regression, not full multipart fuzzing, archive extraction safety or firmware validation.

### Next exact actions

1. Actions run `42385963510` passed on exact head `5877544ea347ab728bd185012187ec09e2aeb76c`; the prior auth/timeout/MIME route checks also passed. Continue with independent validation of upload header arithmetic and restore/archive extraction behavior.
2. Audit image/NVRAM/storage header arithmetic, multipart size limits and archive-extraction behavior in `/sbin/mtd_storage.sh` plus the restore command paths; add focused regressions where practical.
3. Continue reviewing `do_apply_cgi` and `do_update_cgi` for user-controlled EJ handler names, argument parsing, command/config dataflows, then audit TLS/connection lifecycle.
4. Broaden to firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file-sharing, storage/hotplug and init/service lifecycle. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` and inspect artifact only when actually executed; L3/L4 remain NOT VERIFIED.
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
