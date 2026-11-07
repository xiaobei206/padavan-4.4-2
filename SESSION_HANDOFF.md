# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — HTTPD private-key access policy

- Current production-code checkpoint: `1ef811e5cb37613fc5ae0caf59041ecdf294e5d2`; its parent was `8152ec72c39d4a435114806a5e861e2aed925248`. The documentation update for this checkpoint is a follow-up commit on main; always fetch live main before continuing.
- Changed production paths: `trunk/user/httpd/src/httpd.c`, `httpd.h`, `web_ex.c`; added `httpd_access_policy.h`.
- Added host regression: `trunk/user/httpd/src/tests/httpd_private_key_policy.c`; added workflow `.github/workflows/httpd-security.yml`.
- Finding: `get_login_safe()` treated HTTPS as sufficient to mark a peer safe. `web_ex.c::dump_file()` reused that predicate to expose `.key` files, despite the adjacent write-only secret-key comment. That let an authenticated off-LAN HTTPS session pass the private-key visibility gate.
- Fix: keep the broader HTTPS behavior for existing management functions, but gate `.key` disclosure on a new predicate requiring an active login and the trusted LAN/VPN network flag.
- Validation: GitHub Actions run `42380444876` completed **successfully** on exact code SHA `1ef811e5cb37613fc5ae0caf59041ecdf294e5d2`; the host C policy regression and production call-site wiring checks both passed.
- Not performed: complete HTTPD/CGI audit, MIPS cross-build, firmware image validation, XY-C1 hardware test.
- Remote checkpoints inspected: all outstanding branch deltas are old SkipDBv2/BaseKit work and far behind main; do not cherry-pick without a fresh diff review.

### Next work

1. Continue HTTPD auth review: strict Basic-auth parser, IP-only session semantics, logout/ACL behavior, all MIME route permissions and the conditional unauthenticated `dbconf` route.
2. Continue HTTPD protocol/lifecycle review: TLS post-handshake read/write, connection pool/FD limits, request timeout behavior, response writes and multipart restore/upload bounds.
3. Expand into privileged command/config generation, firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file sharing, storage, and init/service lifecycle. Record each scope separately.
4. Run `make XY-C1 TOOLCHAIN=mipsel-linux-musl` through the repository's firmware validation workflow before claiming target-build verification.

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
