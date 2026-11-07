# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — HTTPD management-session inactivity timeout

- Verified main immediately before this documentation checkpoint: cfd8da8ffa51f29795920f90b68777630a9c81bc. Implementation commit cfd8da8ffa51f29795920f90b68777630a9c81bc (parent aea93c782f8a1064a91d0d81a5c8d3341286a44a); current main HEAD must be re-fetched before the next source write. Prior strict Basic-auth/private-key/dbconf fixes remain on main.
- Active scope: trunk/user/httpd/src/httpd.c, new httpd_login_policy.h and tests/httpd_login_policy.c, .github/workflows/httpd-security.yml, plus this handoff, roadmap and dev log.
- Finding: the global login-owner timeout was measured from the first successful login. http_login_check() let the owner IP return state 3 before checking the deadline; successful subsequent protected requests did not refresh login_timestamp. Another IP could therefore expire/replace the ownership lock after 30 seconds even while the owner was active.
- Implementation in this checkpoint: keep login_timestamp as the initial-login/NVRAM timestamp; use separate login_last_activity for the 30-second idle policy; refresh it only after a protected request's Basic credentials are accepted; clear it with session reset. The timeout calculation is factored into a private helper with host-test coverage for boundary conditions and unsigned uptime wraparound.
- Validation: HTTPD Authentication and Security Regression run 42385373380 completed SUCCESS on exact implementation SHA cfd8da8ffa51f29795920f90b68777630a9c81bc (https://github.com/xiaobei206/padavan-4.4-2/actions/runs/42385373380). Job 128441510900 passed all steps: private-key policy, strict Basic-auth and session-policy C tests compiled/ran with GNU89 -Wall -Wextra -Werror; source wiring and route-access assertions passed. Local equivalent session-helper regression also compiled and ran. L1 PASS for this bounded change; L2 focused host regression PASS. Full MIPS firmware build, artifact inspection, fuzzing and hardware validation remain NOT VERIFIED.
- Remote checkpoint review against current baseline aea93c782f8a1064a91d0d81a5c8d3341286a44a: refactor/bstream-tag-wire (1 ahead / 424 behind), refactor/dbapi-result-20261105 (8 ahead / 505 behind), refactor/dynlib-lifecycle (0 ahead / 428 behind), refactor/sorting-boundaries (0 ahead / 424 behind), and refactor/uarray-character-ctype (2 ahead / 422 behind). All divergent deltas are old SkipDBv2-only work; none is a viable HTTPD checkpoint.

### Next exact actions

1. Fetch live main and reconcile the resulting docs-only checkpoint SHA. The implementation SHA and matching successful run are verified above; do not treat the docs-only follow-up as a new test run.
2. Continue the P1 HTTPD route audit from web_ex.c::mime_handlers[]: enumerate each conditional route and its caller/page consumer, especially apply.cgi, update.cgi, upgrade and restore handlers. Test the authorization matrix without changing existing contracts blindly.
3. Trace logout/ACL and same-NAT behavior, then continue TLS post-handshake I/O, connection pool/FD handling, response writes, and multipart firmware/storage restore bounds.
4. Broaden across privileged command/configuration paths, firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file services, storage/hotplug and init/service lifecycle. Do not return to routine SkipDBv2 work absent a demonstrated blocker.
5. Run make XY-C1 TOOLCHAIN=mipsel-linux-musl and inspect the full firmware artifact after the current HTTPD audit series. Until executed, L3/L4 remain NOT VERIFIED.
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
