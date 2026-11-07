# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Firewall NVRAM rule-token hardening (host regression + CI PASS)

- Code/workflow HEAD before this handoff update: 551ac54f3d605c71f334b7c6163d393de15e93aa. Re-fetch live main before follow-on work because this documentation commit creates a later HEAD.
- Recovered the unfinished firewall input audit from the previous handoff. The previous round had fixed URL-pattern quoting/control-byte injection and ICMP-token injection, but raw port/protocol/TCP-flag/schedule arguments and current Actions verification remained open.
- Completed the bounded input pass in trunk/user/rc/src/firewall_ex.c with reusable validators in firewall_filter_policy.h: ICMP type 0..255; filter-LW port numbers/ranges; TCP flag suffix allowlist; numeric IP protocol 0..255; virtual-server public port/local-port/protocol-number; weekday bitset and HHMMHHMM schedule validation. Invalid rule fields are rejected before affected rules are formatted.
- Expanded tests/firewall_filter_policy.c for valid boundaries, numeric overflow, malformed/reversed ranges, disallowed separators and control bytes, TCP flag allowlist bypasses, weekday shape and invalid time fields. Added source-wiring assertions to the HTTPD Security workflow.
- Final matching Actions run: HTTPD Authentication and Security Regression 42389819329, completed/success, exact head SHA 551ac54f3d605c71f334b7c6163d393de15e93aa. Both GNU89 compile-and-run policy regressions and the complete HTTPD/firewall source-wiring step passed.
- Recovery trail: validator/call-site/tests commit 6419f7602771aa1dc33ebd58fbaad89aa2073cf6; policy-header string.h dependency correction 87b732a57d9038d9cc7e233d8d32f9b8fb97338b; workflow assertion fixes aa9d424a2d08dc584bac40143d45a871e7a9136d and 551ac54f3d605c71f334b7c6163d393de15e93aa.
- Important schedule caveat: malformed date/time returns an empty match suffix, so the legacy builder emits an unqualified/all-time rule. Malformed bytes no longer reach iptables-restore text, but an explicit fail-closed schedule policy remains open.
- Remote checkpoint comparison at this HEAD: refactor/bstream-tag-wire 1 ahead / 459 behind (SkipDBv2 BStreamTag/test/CI); refactor/dbapi-result-20261105 8 ahead / 540 behind (SkipDBv2 DBAPI/test/CI); refactor/dynlib-lifecycle 0 ahead / 463 behind; refactor/sorting-boundaries 0 ahead / 459 behind; refactor/uarray-character-ctype 2 ahead / 457 behind (SkipDBv2 UArray/test/CI). All outstanding deltas are stale and hundreds of commits behind; none is a firewall/HTTPD handoff.
- Firewall remains AUDITING. Remaining work includes other NVRAM-to-iptables/ip6tables sinks, interface/address arguments, shell construction, chain initialization/rollback, failure recovery and IPv6 parity.
- This is host regression/source-contract evidence only. Production firewall_ex.c target compilation, XY-C1 MIPS build, firmware artifact inspection, runtime restore testing and hardware validation are NOT RUN / NOT VERIFIED.

### Next exact actions

1. Start the next P1 audit at CGI sys_script() / SystemCmd and privileged script dispatch. Trace request variables to eval/_eval/doSystem and filesystem writes; identify a bounded unsafe sink and add a regression.
2. Continue HTTPD session/authorization coverage and privileged RC interfaces, then DNS/DHCP, WAN/PPP/IPv6 and service lifecycle audits.
3. Return to firewall for all remaining restore sinks, IPv6 parity and explicit invalid-schedule policy semantics; do not mark the module audited.
4. Run make XY-C1 TOOLCHAIN=mipsel-linux-musl only when actually executed and inspect the resulting firmware image. L3/L4 remain unverified.
5. Keep the roadmap, handoff and development log synchronized after each bounded task. Do not resume routine SkipDBv2 expansion while higher-risk userland remains open.
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
