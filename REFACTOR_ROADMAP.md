# Repository-Wide Modernization Roadmap

**Repository:** xiaobei206/padavan-4.4-2  
**Primary branch:** main  
**Initial baseline before this governance bootstrap:** 412eebb4d0c36f9cbaf43921b4ce1f2056665984
**Governance bootstrap commit:** 84e2fb77973393dd2804b9feb2d94b2ce74933c4 (verified on main)  
**Roadmap status:** INVENTORY IN PROGRESS / HTTPD PRIVATE-KEY SUBSCOPE FIXED; WHOLE-REPO AUDIT INCOMPLETE

## Purpose and status semantics

This file tracks the whole firmware, not just the subsystem with the most recent commits. It must be updated after each bounded task.

Statuses:
- NOT INVENTORIED — source/build integration has not yet been mapped.
- NOT AUDITED — module identified, systematic review not yet evidenced.
- AUDITING — review is in progress.
- AUDITED — FINDINGS — review scope and findings are recorded.
- AUDITED — NO FINDINGS — review scope and rationale are recorded; this is not a claim of zero risk.
- FIXED — NOT VERIFIED — code changed, required verification not complete.
- HOST VERIFIED — host build/tests passed.
- TARGET BUILD VERIFIED — target MIPS build and firmware artifact verified.
- HARDWARE VERIFIED — relevant behavior checked on real target hardware.

Priorities:
- P0: confirmed critical vulnerability, data corruption, boot/release blocker.
- P1: remote/LAN attack surface, authentication/authorization, privileged operations, untrusted input.
- P2: critical service reliability, persistence, lifecycle, concurrency, error propagation.
- P3: non-critical cleanup, deduplication, performance/style.

## Verified repository baseline

- Governance baseline before the first user-space audit round: main HEAD `8152ec72c39d4a435114806a5e861e2aed925248`.
- HTTPD private-key access fix: commit `1ef811e5cb37613fc5ae0caf59041ecdf294e5d2` (parent exactly `8152ec72c39d4a435114806a5e861e2aed925248`).
- HTTPD Security Regression run `42380444876` completed successfully against that exact code commit. It compiled and ran the C policy regression and passed the source-to-call-site wiring contract. It is a focused host regression, not a full HTTPD cross-build.
- SkipDBv2 Regression run `42376349424` remains successful for its exact older head `412eebb4d0c36f9cbaf43921b4ce1f2056665984`; it is not evidence for the HTTPD changes.
- Remote checkpoint branch comparison against main: `refactor/bstream-tag-wire` (1 ahead / 415 behind, SkipDBv2 BStreamTag/test/CI delta); `refactor/dbapi-result-20261105` (8 ahead / 496 behind, SkipDBv2 DBAPI delta); `refactor/dynlib-lifecycle` (0 ahead / 419 behind); `refactor/sorting-boundaries` (0 ahead / 415 behind); `refactor/uarray-character-ctype` (2 ahead / 413 behind, SkipDBv2 UArray/test/CI delta). These are old, heavily behind, SkipDBv2-only checkpoints; none is an HTTPD handoff. Their unmerged deltas were not blindly cherry-picked.

- Main HEAD at initial bootstrap: 412eebb4d0c36f9cbaf43921b4ce1f2056665984.
- The latest available SkipDBv2 Regression run at that HEAD was #669 / 42376349424, conclusion success, exact head SHA matched. The preceding conversation's job inspection reported 57/57 CTests passed.
- This is evidence for that workflow and that SHA only. It is not a full-firmware MIPS build or hardware verification.
- The 273-commit comparison from 8a6fc2639fcd036790b02eae56cadb2c2935883d to 412eebb4 is dominated by SkipDBv2/BaseKit/UDB, its tests, dev_log, and CI. That recent work window does not show HTTPD source changes.
- Historical repository note GPT_AUDIT_NOTES.txt (dated 2026-11-04) records earlier edits in many other areas, including httpd.c response-write handling, https.c TLS handshake deadline, web_ex.c, ej.c, firewall_ex.c, WAN/IPv6, FTP/Samba, USB, and service helpers. These notes are historical pointers only: each item must be checked against current source and commit history. A past fix is not a complete module audit.
- Full repository inventory, all module paths, current test coverage, target build state, and hardware state remain to be established.

## Initial module register

| Area | Known source clues / examples | Priority | Current status | Next action |
|---|---|---:|---|---|
| HTTPD / HTTPS | `trunk/user/httpd/src/{httpd.c,httpd.h,https.c,upload.c,base64.c}`; `trunk/user/httpd/src/Makefile`; `trunk/user/httpd/Makefile` | P1 | AUDITING — private-key disclosure sub-scope FIXED and host-regression verified | Continue request parsing, connection lifecycle, TLS I/O, response writes, upload handling, and file-access audit; perform target build |
| CGI / ASP / Web templates | `trunk/user/httpd/src/{cgi.c,ej.c,web_ex.c,variables.c}`; `trunk/user/www/{dict,n56u_ribbon_fixed}` | P1 | PARTIAL INVENTORY — NOT AUDITED | Build the complete MIME/EJ/CGI route matrix; audit unescaped values, output encoding, command dispatch, and handler authorization |
| Authentication / session / authorization | `httpd.c::http_login_check/auth_check/get_login_safe`; `web_ex.c::mime_handlers/dump_file` | P1 | AUDITING — key-download trust boundary corrected; broader session/route model open | Review IP-only session semantics, Basic-auth parsing, logout, ACLs, unauthenticated routes, and all `need_auth` exceptions |
| Privileged command/configuration interfaces | `web_ex.c::sys_script`, `trunk/user/rc/src/auth.c`, `services*.c`, NVRAM and shared helpers | P1 | NOT AUDITED — call sites inventoried only | Trace every external value to `eval`/`_eval`/`doSystem`/filesystem writes; review generated configuration escaping and source trust |
| Firewall / NAT / QoS | `trunk/user/rc/src/firewall_ex.c`; `trunk/user/iptables/**`; `ipset/**`; `sqm-qos/**` | P1/P2 | NOT AUDITED | Review rule construction, input validation, IPv4/IPv6 parity, chain lifecycle, and rollback |
| DNS / DHCP / resolver integration | `trunk/user/{dnsmasq,dnsproxy,chinadns-ng,smartdns}/**`; RC service orchestration | P1/P2 | INVENTORIED — NOT AUDITED | Trace config generation, dynamic updates, DNS rebinding boundaries, leases, restart/rollback behavior |
| WAN / PPP / routing / IPv6 | `trunk/user/rc/src/{net.c,net6.c,net_wan.c,net_wan6.c,net_ppp.c}`; `pppd`, `pppoe`, `rp-l2tp`, `xl2tpd`, `uqmi`, VPN packages | P1/P2 | INVENTORIED — NOT AUDITED | Trace state transitions, route/address parsing, helper arguments, and failure recovery |
| Wireless management / MTK interfaces | `trunk/user/rc/src/{net_wifi.c,ralink.c,switch*.c}`; `wpa_supplicant`, `wireless-tools`, board/driver interfaces | P1/P2 | INVENTORIED — NOT AUDITED | Review ioctl buffers, scan/association parsing, credential/config escaping, and radio restart paths |
| NVRAM / configuration persistence | `trunk/user/nvram/**`; `trunk/user/httpd/src/nvram_x.c`; RC NVRAM callers | P1/P2 | PARTIAL INVENTORY — NOT AUDITED | Review length handling, trust boundaries, commit semantics, configuration migration and error handling |
| FTP / Samba / file sharing | `trunk/user/{vsftpd,samba36,nfs-utils,minidlna}/**`; `trunk/user/httpd/src/aidisk.c` | P1/P2 | INVENTORIED — NOT AUDITED | Review authentication, path normalization, permissions, shell invocation, and share lifecycle |
| USB / storage / hotplug | `trunk/user/rc/src/{hotplug_stor.c,hotplug_usb.c,services_stor.c,services_usb.c}`; `libdisk`, `mtd-utils`, `ntfs-3g`, `e2fsprogs` | P2 | INVENTORIED — NOT AUDITED | Review sysfs parsing, mount paths, hotplug races, cleanup, and destructive-operation gates |
| System init / services / process lifecycle | `trunk/user/rc/src/{rc.c,init.c,services.c,services_ex.c,watchdog.c}`; `trunk/user/scripts/**`; BusyBox integration | P1/P2 | INVENTORIED — NOT AUDITED | Map init entry points, signal handling, process ownership, restart dependencies, and privilege transitions |
| IPC / common helpers | `trunk/user/shared/src/**`; HTTPD `common.c`; RC `common_ex.c`; FIFO and notify paths | P2 | PARTIAL INVENTORY — NOT AUDITED | Inventory public helper contracts and all callers; inspect framing, ownership, races and failure propagation |
| SkipDBv2 / BaseKit / UDB | `trunk/user/skipdbv2/skipdbv2/**` | P2 | Existing focused host regression evidence; full audit not asserted | Pause routine expansion; handle only demonstrated regressions/integration blockers while broadening to other userland |
| Build system / toolchain / firmware image | `trunk/Makefile`, `trunk/user/Makefile`, `toolchain/**`, `.github/workflows/{build,xy-c1-firmware-validation}.yml` | P1/P2 | INVENTORIED — TARGET BUILD NOT VERIFIED | Run the supported XY-C1 MIPS cross-build after source changes and inspect the produced TRX |
| Linux 4.4 kernel / MTK drivers / DTS | `trunk/linux-4.4.x/**` | Separate project | NOT IN CURRENT USER-SPACE SCOPE | Keep kernel upgrades separate from routine user-space modernization |
 
## HTTPD / HTTPS / authentication — bounded review checkpoint

### Source and build map

- HTTP daemon: `trunk/user/httpd/src/httpd.c`; request dispatch and client loop run in a single-threaded select/pool server.
- HTTPS: `trunk/user/httpd/src/https.c`; enabled conditionally by `CONFIG_FIRMWARE_INCLUDE_HTTPS` in the HTTPD source Makefile.
- Routing and application logic: `web_ex.c` (MIME table, EJ functions, scripts, file exports), `ej.c`, `cgi.c`, `variables.c`, `upload.c`, `base64.c`.
- Build: `trunk/user/httpd/src/Makefile` compiles `httpd` and links the staged shared libraries; `trunk/user/httpd/Makefile` packages it into `/usr/sbin/httpd`.

### Findings and changes

- **P1 trust-boundary finding (fixed):** `httpd.c::get_login_safe()` intentionally returned true for HTTPS sessions regardless of whether the authenticated peer belonged to a trusted LAN/VPN interface. Most management operations still use this broad transport-aware predicate. But `web_ex.c::dump_file()` also used it to decide whether `.key` files were masked as write-only. An authenticated off-LAN HTTPS peer could therefore pass the key-file visibility check solely because TLS was in use.
- Added `get_login_private_key_access()`, backed by a small testable policy helper. It requires an active login and the original trusted-network result (`login_safe`); HTTPS is not treated as network trust for private-key disclosure. `dump_file()` now uses this narrower predicate.
- Preserved the existing `get_login_safe()` HTTPS behavior for unrelated remote-management operations; the fix is deliberately limited to the private-key disclosure gate.
- The host C regression covers unauthenticated, authenticated-but-untrusted, and authenticated trusted-network cases. A source-wiring check ensures `dump_file()` calls the new predicate and that the existing HTTPS behavior is unchanged.

### Review notes / remaining work

- `handle_request()` currently applies a 15-second request/header alarm, rejects unsupported Transfer-Encoding, rejects duplicate or malformed Content-Length, caps the body at 50,000,000 bytes, and applies a 30-second receive timeout to the body. This is a source review, not fuzzing.
- `mime_handlers[]` places HTML/ASP/translated JS and configuration-changing/upload routes behind `need_auth=1`. A conditional `dbconf` handler is `need_auth=0`; its intended exposure and data sensitivity remain to be checked.
- Login state is globally keyed by client IP with a 30-second timeout; same-IP requests reuse the active login. Session isolation, logout behavior, and same-NAT client interaction remain unaudited.
- `auth_check()` uses the legacy Base64 decoder, which ignores non-alphabet characters. Strict Basic-auth syntax and compatibility need a separate focused audit; no auth bypass is claimed here.
- TLS handshake code has a 10-second deadline in the current source. Post-handshake TLS read/write behavior, protocol/cipher policy, connection pool exhaustion and response-write paths remain open.
- Upload handlers are auth-gated in the MIME table; firmware and storage restore logic in `upload.c` still needs an independent bounds/atomicity audit.

### Verification evidence

- GitHub Actions: HTTPD Security Regression run `42380444876`, conclusion **success**, exact head `1ef811e5cb37613fc5ae0caf59041ecdf294e5d2`.
- Passed steps: compile/run the policy C test with `-std=gnu89 -Wall -Wextra -Werror`; verify the production getter and `dump_file()` wiring.
- No full HTTPD MIPS object build, XY-C1 firmware build, image inspection or hardware validation was run in this round.

## Mandatory next work

1. Continue the HTTPD/HTTPS audit: strict Basic-auth parsing, session/IP semantics, route/handler authorization matrix (including `dbconf`), TLS I/O, response writes and multipart upload handling.
2. Validate the remaining historical pointers in GPT_AUDIT_NOTES.txt against current source and git history; do not restart SkipDBv2 by default.
3. Continue the full userland audit through privileged command/configuration interfaces, firewall/NAT, DNS/DHCP, WAN/PPP/IPv6, wireless, NVRAM, file sharing, storage/hotplug, and init/service lifecycle.
4. Run the XY-C1 `mipsel-linux-musl` target build after the HTTPD changes; inspect the resulting firmware artifact. No target build is claimed yet.
5. Update this table with exact evidence and commits; never mark an entire area complete based on one bounded fix.

## Verification gaps

- Full MIPS cross-compile: NOT VERIFIED by the baseline SkipDBv2 Regression result.
- Complete firmware image generation: NOT VERIFIED.
- XY-C1 real-device validation: NOT VERIFIED.
- Whole-repository security audit: NOT COMPLETE.
