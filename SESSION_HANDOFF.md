# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — HTTPD strict Basic authentication

- Verified main immediately before this handoff update: `3989c878b702595fdba530d79da1b1e327225c1c`. Production implementation: `b905c4baabd87922075700397c54cdbe6edd03fd`; workflow-only shell repair: `3989c878b702595fdba530d79da1b1e327225c1c`. Fetch live main before every subsequent write.
- Scope: HTTPD Basic Authorization parsing and Base64 validation; bounded to `trunk/user/httpd/src/{httpd.c,base64.c,httpd_basic_auth.h,tests/httpd_basic_auth.c}` and `.github/workflows/httpd-security.yml`.
- P1 finding: `auth_check()` used legacy `b64_decode()`, which silently discarded bytes outside the Base64 alphabet and did not enforce quartet length, padding placement, or canonical unused pad bits. It then compared the result as a C string without rejecting embedded NUL bytes. Malformed credentials could be normalized into a different credential string instead of rejected.
- Fix: added a dedicated strict RFC 4648 decoder for the HTTP Basic path; it validates the entire token and output capacity before writing, rejects non-alphabet bytes, missing/misplaced padding and non-zero unused pad bits, and leaves the legacy decoder behavior for unrelated callers. Added a bounded Basic-field parser with case-insensitive scheme handling, separator validation, trailing HTTP OWS/line-ending handling, encoded/decoded limits, and embedded-NUL/colon checks. `auth_check()` now uses this helper.
- Regression: canonical padded/unpadded examples; invalid alphabet/padding; non-canonical pad bits; truncation; output-capacity/no-partial-write behavior; Basic scheme casing and OWS/CRLF; malformed scheme spacing; embedded NUL; and missing colon.
- Commits: implementation `b905c4baabd87922075700397c54cdbe6edd03fd`; workflow shell correction `3989c878b702595fdba530d79da1b1e327225c1c`. First Actions attempt `42383463695` failed because newly added shell continuation lines contained doubled backslashes and the linker treated a literal backslash as an input file. The workflow-only correction fixed those lines.
- Validation: HTTPD Authentication and Security Regression run `42383489924` completed **successfully** on exact SHA `3989c878b702595fdba530d79da1b1e327225c1c`. Both GNU89 `-Wall -Wextra -Werror` C test executables (private-key policy and strict Basic-auth decoder/parser) compiled and ran; source wiring checks passed. The failed predecessor run `42383463695` is retained and explained.
- Validation boundary: L1 source/dependency/build integration review completed for this sub-scope; L2 focused host regression PASS. Full HTTPD MIPS cross-build, XY-C1 firmware image generation/inspection, protocol fuzzing and hardware validation remain NOT VERIFIED. HTTPD and the authentication subsystem remain AUDITING, not complete.
- Startup branch review: five remote `refactor/*` branches were compared with main. Their unmerged deltas were old SkipDBv2/BaseKit-only work or empty/behind; none was an HTTPD checkpoint, so none was merged.

### Next exact actions

1. Continue P1 HTTPD session/authorization review from `httpd.c`: trace `http_login_check()`, `http_login()`, `http_logout()`, `login_ip/login_timestamp/login_safe`, and the special `logout` path. Determine same-IP/NAT session-sharing behavior and timeout/ACL edge cases before changing semantics.
2. Build and verify the complete conditional `mime_handlers[]` route authorization matrix in `web_ex.c`; map each handler to its page/API consumer. Then audit CGI/EJ dispatch, output escaping, and privileged command/configuration call chains.
3. Continue TLS post-handshake read/write behavior, connection-pool/FD/timeouts, response writes, and multipart firmware/storage restore bounds.
4. Broaden to privileged command/configuration, firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file sharing, storage/hotplug, and init/service lifecycle. Keep the repository-wide roadmap authoritative; do not resume routine SkipDBv2 work.
5. Run the supported `make XY-C1 TOOLCHAIN=mipsel-linux-musl` firmware validation path after the HTTPD series, then inspect the generated artifact. Until then, do not claim target-build or hardware verification.
---

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
