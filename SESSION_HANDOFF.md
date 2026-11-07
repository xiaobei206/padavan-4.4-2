# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — HTTPD MIME route authorization matrix host-verified

- Verified main HEAD before this docs-only checkpoint: `252fa82f6cc8b07b0a6dcd89f4bfd137b5d4123d`; latest implementation/test commit: `015b820fc61cb8e3cc1283a0a87a48751205eae6` (parent matrix-contract commit `4cef7d95812786dad0d0ac34dce461b0a1c0bb64`).
- The first route-matrix run `42385599174` failed on exact SHA `4cef7d95812786dad0d0ac34dce461b0a1c0bb64` because the new regex was over-escaped (`AssertionError: failed to parse the HTTPD MIME route table`). This was a test-contract failure, not a production C failure. Fix commit `015b820fc61cb8e3cc1283a0a87a48751205eae6` corrected it.
- Follow-up HTTPD Authentication and Security Regression run `42385661621` completed SUCCESS against exact head `015b820fc61cb8e3cc1283a0a87a48751205eae6`: https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42385661621. C regressions compiled/ran; source contract passed for explicit public cached assets, public-JS ordering, protected page/dynamic-JS routes, config/storage/log exports, and conditional SCUT/MENTOHUST/OpenVPN/Shadowsocks routes plus apply/update/upgrade/restore handlers.
- Scope boundary: this matrix verifies route-table declarations and the existing `handle_request()` auth gate ordering. It does not audit handler internals, template escaping, command-dispatch dataflow, TLS behavior or the full HTTPD attack surface. HTTPD remains AUDITING. Full MIPS build, artifact inspection, fuzzing and hardware validation remain NOT VERIFIED.
- Previous code commit `cfd8da8ffa51f29795920f90b68777630a9c81bc` separately fixed login-owner inactivity tracking; matching run `42385373380` passed on that exact SHA.

### Next exact actions

1. Re-fetch live main before edits. Audit `trunk/user/httpd/src/upload.c` and its callers for firmware upload, NVRAM restore and storage restore: multipart boundary handling, Content-Length use, temp-file open/write/close errors, size limits, path selection, cleanup, archive extraction and rollback.
2. Read the related `do_upgrade_fw_post/do_upgrade_fw_cgi`, `do_restore_nv_post/do_restore_nv_cgi`, `do_restore_st_post/do_restore_st_cgi`, and relevant frontend form submission paths. Only fix a demonstrated defect and add a focused host regression/source contract.
3. Keep HTTPD/CGI status AUDITING; after upload/restore work, resume privileged `do_apply_cgi`/`do_update_cgi` command/configuration paths and then broaden into firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file-sharing and init/service lifecycle.
4. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` and inspect the full firmware artifact after the HTTPD series; until actually executed, L3/L4 remain NOT VERIFIED.
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
