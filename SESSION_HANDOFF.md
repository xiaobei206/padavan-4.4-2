# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Firewall URL-filter rule-injection guard (CI pending)

- Baseline main HEAD before this bounded task: `259288902383060425b2288f0c1c4bd8f60012e3`. Earlier HTTPD authentication/session/MIME, stale-upload, archive-path and CGI service-ID checks remain host-verified by exact-SHA Actions runs documented below in `dev_log.md`; full target firmware and hardware remain unverified.
- P1 finding: `trunk/user/rc/src/firewall_ex.c::include_webstr_filter()` reads NVRAM `url_keyword_xN`, merges the strings and emits `-m webstr --url "%s"` directly into an `iptables-restore` rules file. A crafted value containing a quote and line break can escape the intended one-line quoted token and introduce additional rule text when the file is parsed. The UI's declared maximum length is not a server-side security boundary; common CGI variable processing persists request values to NVRAM. This is a firewall-config injection risk at the authenticated management boundary, not an unauthenticated exploit claim.
- Implementation commit: `259288902383060425b2288f0c1c4bd8f60012e3` (`fix(firewall): reject unsafe URL filter config bytes`). New `trunk/user/rc/src/firewall_filter_policy.h` rejects quote, backslash, DEL and ASCII control bytes; `include_webstr_filter()` skips unsafe patterns after URL-scheme stripping and before length/aggregation. The existing buffer-length checks remain unchanged.
- Added `trunk/user/rc/src/tests/firewall_filter_policy.c` with positive ordinary URL/query cases and negative quote/newline/backslash/CR/tab/DEL/NULL cases. `.github/workflows/httpd-security.yml` now triggers on the firewall files, compiles/runs the test with GNU89 `-Wall -Wextra -Werror`, and asserts the guard precedes aggregation and rule emission.
- Matching HTTPD security workflow run `42387739482` has exact head SHA `259288902383060425b2288f0c1c4bd8f60012e3` and status `completed` / conclusion `failure`: https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42387739482. The run failed; inspect the failed step before proceeding.

### Next exact actions

1. Matching run `42387739482` failed on exact SHA `259288902383060425b2288f0c1c4bd8f60012e3`. Review its failed step/logs before claiming verification; correct the defect without weakening the security contract.
2. Continue enumerating `firewall_ex.c` NVRAM-to-iptables-restore interpolation sites, then inspect IPv4/IPv6 parity, shell command construction, chain lifecycle and rollback.
3. Keep expanding beyond HTTPD/SkipDBv2: DHCP/DNS, WAN/PPP/IPv6, UPnP/port mapping, wireless services, storage/hotplug, FTP/Samba/NFS and init/service lifecycle; return to CGI `SystemCmd`/script dispatch after this bounded pass.
4. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` and inspect the complete firmware artifact only when actually executed; L3/L4 remain NOT VERIFIED.
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
