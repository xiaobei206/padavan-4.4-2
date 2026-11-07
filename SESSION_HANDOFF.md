# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Validate hardlink source paths during BusyBox storage restore

- Baseline main HEAD before this bounded archive follow-up: `2b475fca3ac41f59a22059625d5e56dfb9b49fba`. HTTPD auth/session/MIME, stale upload, archive member-path and CGI service-ID changes remain host-verified by the previous exact-SHA Actions runs recorded below in `dev_log.md`.
- New P1 finding in `trunk/user/busybox/busybox-1.24.x/archival/libarchive/get_header_tar.c` and `data_extract_all.c`: the tar parser strips unsafe prefixes from `file_header->name`, but assigns `link_target` directly from the archive linkname. For hardlinks, `data_extract_all()` passes that target to `link(file_header->link_target, file_header->name)` without first applying the safe relative-path / parent-component policy. A crafted authenticated storage restore could ask the root extractor to resolve a hardlink source outside the scratch directory, subject to filesystem hardlink restrictions.
- Fix: the same fail-closed `bb_archive_path_has_unsafe_component()` policy now validates hardlink targets (`S_ISREG && link_target && size == 0`) before creating leading directories or unlinking any destination. It rejects absolute targets, `..` and source paths with symlink/non-directory ancestors; valid archive-root-relative hardlink targets remain subject to BusyBox's existing link handling.
- The policy test already exercises absolute, parent traversal and symlink-parent strings, now explicitly documented as also covering hardlink source paths. The workflow source contract asserts that the hardlink-target check precedes directory creation/unlink/link operations. HTTPD Authentication and Security Regression run `42387366333` completed SUCCESS on exact SHA `14c6658c06ed030f84dbdf7afb53c4d66b436540` (https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42387366333); both the C regression step and the source-wiring step passed. Full BusyBox MIPS build, tar integration fuzzing, valid-archive compatibility and hardware remain NOT VERIFIED.

### Next exact actions

1. Completed: run `42387366333` passed on exact SHA `14c6658c06ed030f84dbdf7afb53c4d66b436540`; archive path and hardlink ordering assertions and C regressions succeeded. Continue root-restore compatibility review, then return to CGI/system command and network/firewall services.
2. Continue archive compatibility review (hardlink ordering, symlink semantics and restore rollback) without claiming full archive safety from source contracts alone.
3. Return to HTTPD CGI/system management: inspect `sys_script()`/`SystemCmd`, script allowlisting, NVRAM group writes, output encoding and `do_update_cgi()`; then broaden to firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, file-sharing and init/service lifecycle.
4. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` and inspect the full firmware artifact after source rounds. Until executed, L3/L4 remain NOT VERIFIED.
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
