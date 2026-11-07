# Session Handoff — Padavan 4.4

## Latest resumable checkpoint — Firewall NVRAM rule-token guards (policy test PASS; current Actions pending)

- Verified code HEAD immediately before this documentation checkpoint: `895a1700fb498a5f9ec190a83b7282e5adb19416`. This is the current source checkpoint; fetch live `main` again on resume because this handoff update itself creates a later documentation commit.
- Previous bounded task recovery: HTTPD Security workflow run `42387739482` was tied to exact old head `259288902383060425b2288f0c1c4bd8f60012e3` and failed in the C regression compile step because `tests/firewall_filter_policy.c` used `NULL` without `<stddef.h>`. The prior HTTPD/archive test executables had run; firewall policy test and subsequent wiring step had not. This is a test-source compile defect, not a product test failure.
- Added `#include <stddef.h>` to the firewall test in `ed5b3a270205857220ba49e641b232da0fbf2de0`.
- Existing URL rule-injection guard remains in `259288902383060425b2288f0c1c4bd8f60012e3`: `include_webstr_filter()` rejects quote, backslash, DEL and ASCII control bytes in `url_keyword_xN` before aggregation into the quoted `--url` iptables-restore rule.
- Second P1 issue found in the same firewall module: `include_lw_filter()` read NVRAM `filter_lw_icmp_x`, tokenized it with a macro that splits only on ASCII spaces, then inserted each token directly into `--icmp-type %s`. The firewall UI's `validate_portlist()` is client-side only; the HTTPD `validate_cgi()` storage path calls `nvram_set()` for ordinary POSTed variables. A token carrying newline/control bytes could reach the restore rules file.
- Fix: `firewall_icmp_type_is_safe()` accepts decimal digits only and bounds the numeric value to 0..255 with overflow-safe accumulation; invalid tokens are skipped before the rule `fprintf()`. Production call-site integration is in `firewall_ex.c`; C regression and workflow wiring assertions were added. Final source preserves the original C `\\n` newline escape.
- Relevant commits: test include repair `ed5b3a270205857220ba49e641b232da0fbf2de0`; ICMP helper `3f99db8651379751753e6125e917b86b7ba9adc9`; initial call-site integration `e713e06d65bced142cdd3a00b46007a9a24ce990`; regression cases `b0d8e11d3829ac836e577d9b4463e364b498e709`; workflow source-wiring assertion `5296929c05bef7d8828612abe1be7cc5228a1d3d`; final newline-escape correction `895a1700fb498a5f9ec190a83b7282e5adb19416`.
- Local validation: exact fetched policy header + C regression compiled with `cc -std=gnu89 -Wall -Wextra -Werror` and executed successfully (**PASS**). This verifies the policy helper executable only; it does not compile production `firewall_ex.c` or run the whole workflow.
- Actions status for current code HEAD: combined commit status is empty. The connected workflow-run query only returns PR-triggered runs and returned no run for the current push; the current push run ID/result is **PENDING / NOT RETRIEVED**. The old run `42387739482` remains failed on its old exact SHA; do not claim current CI passed until a run is found and its head SHA/steps are checked.
- Validation levels: focused GNU89 policy test **PASS**; current full Actions/source-wiring result **PENDING / NOT RETRIEVED**; production source/target compilation **NOT RUN**; XY-C1 firmware artifact/runtime/hardware **NOT RUN**.
- Firewall remains **AUDITING**. Additional unresolved direct interpolation sites include raw port strings from `portrange_conv()`, TCP flag suffixes from `protoflag_conv()`, VTS port/protocol-number rules, and date/time-derived rule arguments. Also open: complete NVRAM-to-iptables/ip6tables sweep, shell command construction, chain lifecycle/rollback, IPv4/IPv6 parity.
 
### Next exact actions

1. Retrieve the push-triggered HTTPD Security run for current main and verify exact head SHA, compile/test step and Python source-wiring step. If it failed, read logs and fix the actual cause; do not weaken guards or describe the old run as current verification.
2. Continue the firewall pass on `portrange_conv()`, `protoflag_conv()`, VTS port/protocol values and date/time arguments; add only format-appropriate allowlists and regression cases, then inspect all remaining restore command inputs and failure rollback.
3. Run the supported `make XY-C1 TOOLCHAIN=mipsel-linux-musl` only when actually executed and inspect the produced firmware image; L3/L4 remain unverified.
4. After closing this bounded firewall input pass, move to P1 CGI `SystemCmd`/script dispatch and broaden DNS/DHCP, WAN/PPP/IPv6 and system service lifecycle auditing. Do not get trapped in SkipDBv2-only work.

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
