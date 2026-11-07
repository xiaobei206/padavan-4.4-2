# Repository-Wide Modernization Roadmap

**Repository:** xiaobei206/padavan-4.4-2  
**Primary branch:** main  
**Initial baseline before this governance bootstrap:** 412eebb4d0c36f9cbaf43921b4ce1f2056665984
**Governance bootstrap commit:** 84e2fb77973393dd2804b9feb2d94b2ce74933c4 (verified on main)  
**Roadmap status:** INVENTORY IN PROGRESS / HTTPD MIME, AUTH, SESSION, UPLOAD, ARCHIVE PATH, CGI SERVICE-ID, SYSTEMCMD, GENERIC /tmp DISPATCH, EXECUTABLE SCRIPT-WRITE AND FIXED-NAME SCRIPT DISPATCH BOUNDARIES HAVE BOUNDED HOST-VERIFIED FIXES; FIREWALL URL/ICMP/PORT/PROTOCOL/TCP-FLAG GUARDS AND FAIL-CLOSED SCHEDULE DISPATCH HAVE EXACT-HEAD HOST CI PASSES; WHOLE-REPOSITORY, HTTPD AND FIREWALL AUDITS INCOMPLETE; HTTPD shared-/tmp script staging TOCTOU fixed with atomic mkstemp/fdopen and exact-head host Actions PASS (0bbd8062e8bf7842e4879032aca3c1781bd393fa / 42398764761)

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
- HTTPD Shadowsocks `dbconf` authorization fix: commit `7fc9b5d7fee3f5dcb15310456e657e567714d75b`; changed `web_ex.c` route policy from `need_auth=0` to `need_auth=1`, and extended `.github/workflows/httpd-security.yml` to assert route policy and authentication-gate ordering.
- HTTPD strict Basic-auth parser fix: production commit `b905c4baabd87922075700397c54cdbe6edd03fd`; introduced `b64_decode_strict()`, bounded `httpd_decode_basic_authorization()`, and host regression `tests/httpd_basic_auth.c`. Workflow-only correction commit: `3989c878b702595fdba530d79da1b1e327225c1c`.
- HTTPD Authentication and Security Regression run `42383489924` passed on exact SHA `3989c878b702595fdba530d79da1b1e327225c1c`: both GNU89 C regression executables compiled and ran, and private-key, dbconf route-auth, and strict Basic-auth wiring contracts passed.
- Initial run `42383463695` on `b905c4baabd87922075700397c54cdbe6edd03fd` failed in the shell compile step because doubled continuation backslashes were passed to the linker. The workflow-only correction is `3989...`; its matching rerun passed.
- No HTTPD MIPS cross-build, XY-C1 firmware image, fuzzing or hardware validation is established by these host checks.
- HTTPD Authentication and Security Regression run `42381700507` passed on exact SHA `7fc9b5d7fee3f5dcb15310456e657e567714d75b`: private-key policy C test and production route/auth wiring contract both passed.
- SkipDBv2 Regression run `42381700499` also passed on exact SHA `7fc9b5d7fee3f5dcb15310456e657e567714d75b`: XY-C1 integration contracts, configure, SkipDB target builds, and regression tests completed successfully. This is not the XY-C1 firmware image build.
- Recovery: intermediate commit `8756f91b4bda529e11c3f39f1a71cdb540a3f816` was created from a stale tree and its HTTPD run `42381654753` failed during the existing C test compile. Fast-forward recovery `7fc9b5d7fee3f5dcb15310456e657e567714d75b` restored the real `71ff09d1e01d080b6b3b79e1ececa49ef3589bf5` tree plus only the two intended file edits; compare confirms exactly those two paths changed. Do not use the intermediate SHA as a valid source checkpoint.
- HTTPD Security Regression run `42380444876` completed successfully against that exact code commit. It compiled and ran the C policy regression and passed the source-to-call-site wiring contract. It is a focused host regression, not a full HTTPD cross-build.
- SkipDBv2 Regression run `42376349424` remains successful for its exact older head `412eebb4d0c36f9cbaf43921b4ce1f2056665984`; it is not evidence for the HTTPD changes.
- Remote checkpoint branch comparison against source/workflow main checkpoint 31d94312ccac3451392f8c46fd901b295287dca4: refactor/bstream-tag-wire (1 ahead / 463 behind, SkipDBv2 BStreamTag/test/CI only); refactor/dbapi-result-20261105 (8 ahead / 544 behind, SkipDBv2 DBAPI/test/CI only); refactor/dynlib-lifecycle (0 ahead / 467 behind); refactor/sorting-boundaries (0 ahead / 463 behind); refactor/uarray-character-ctype (2 ahead / 461 behind, SkipDBv2 UArray/test/CI only). All are stale/behind; no unrelated branch delta was merged.

- Main HEAD at initial bootstrap: 412eebb4d0c36f9cbaf43921b4ce1f2056665984.
- The latest available SkipDBv2 Regression run at that HEAD was #669 / 42376349424, conclusion success, exact head SHA matched. The preceding conversation's job inspection reported 57/57 CTests passed.
- This is evidence for that workflow and that SHA only. It is not a full-firmware MIPS build or hardware verification.
- The 273-commit comparison from 8a6fc2639fcd036790b02eae56cadb2c2935883d to 412eebb4 is dominated by SkipDBv2/BaseKit/UDB, its tests, dev_log, and CI. That recent work window does not show HTTPD source changes.
- Historical repository note GPT_AUDIT_NOTES.txt (dated 2026-11-04) records earlier edits in many other areas, including httpd.c response-write handling, https.c TLS handshake deadline, web_ex.c, ej.c, firewall_ex.c, WAN/IPv6, FTP/Samba, USB, and service helpers. These notes are historical pointers only: each item must be checked against current source and commit history. A past fix is not a complete module audit.
- Full repository inventory, all module paths, current test coverage, target build state, and hardware state remain to be established.

## Initial module register

| Area | Known source clues / examples | Priority | Current status | Next action |
|---|---|---:|---|---|
| HTTPD / HTTPS / archive restore | `trunk/user/httpd/src/{httpd.c,httpd.h,https.c,upload.c,base64.c,httpd_basic_auth.h,httpd_login_policy.h}`; BusyBox `archival/libarchive/**`; `trunk/user/scripts/files/sbin/mtd_storage.sh` | P1 | AUDITING — bounded MIME/auth/session/upload/archive and executable-script trust fixes are host-verified; the predictable `/tmp/.<filename>` writer staging path has been replaced with atomic `mkstemp` + descriptor-backed `fdopen` (exact-head run `42398764761` SUCCESS on `0bbd8062e8bf7842e4879032aca3c1781bd393fa`). Full handlers/TLS/session/archive/final-path audit remains incomplete; no production MIPS build or hardware test | Continue session/ACL, TLS I/O, archive/restore compatibility and all destination-path/alternate-writer review; perform target build |
| CGI / ASP / Web templates | trunk/user/httpd/src/{cgi.c,ej.c,web_ex.c,variables.c,common.c}; trunk/user/www/{dict,n56u_ribbon_fixed} | P1 | AUDITING — unknown `sid_list`, SystemCmd, generic `/tmp` dispatch, script-write trust and fixed-name dispatch have bounded host-verified gates. The 28 `scripts.*` File rows (27 unique filenames) were mapped through `validate_asp_apply()` and `ej_dump()`; initial consumers traced across startup/shutdown, WAN, firewall, VPN, button and AP paths. A local shared-`/tmp` symlink TOCTOU in the staging writer is fixed with `mkstemp`/`fdopen`; exact-head Actions run `42398764761` passed on `0bbd8062e8bf7842e4879032aca3c1781bd393fa`. Add-on script consumer coverage, alternate writers, `/tmp` executable producer inventory, EJ output encoding, group semantics and caller matrix remain incomplete | Complete DDNS/FRP/proxy/Adbyby consumers, alternate file writers and executable `/tmp` producer/caller inventory; continue EJ output/group mutation and request/NVRAM-to-shell review |
| Authentication / session / authorization | `httpd.c::http_login_check/auth_check/get_login_safe`; `web_ex.c::mime_handlers/dump_file`; `httpd_basic_auth.h`; `httpd_login_policy.h` | P1 | AUDITING — disclosure gates and strict Basic parsing corrected; inactivity timeout now measures successful protected-route activity; IP-only ownership and route matrix remain open | Review logout/ACL behavior, same-NAT effects, and every conditional `need_auth` exception |
| Privileged command/configuration interfaces | web_ex.c::sys_script/apply_cgi, trunk/user/rc/src/auth.c, services*.c, NVRAM and shared helpers | P1 | AUDITING — SystemCmd pending-queue trust boundary host source-wiring verified by run 42390628940. Generic /tmp _eval dispatch is HOST VERIFIED by run 42392318537 on exact SHA fb488c486462bdbf1dbedbaad559f21bd7f1785c. Executable script mutation is HOST VERIFIED by run 42393893775 on exact SHA 5025bcb42d92c715d55595c15ff161cf13eb920e: the common .sh writer rejects non-trusted sessions before touching the filesystem; ap_script.sh direct execution and both wireless variable entries are trust-gated. Fixed-name `sys_script()` branches are explicitly allowlisted before dispatch and covered by GNU89 tests (run `42397188114`, exact SHA `3859fc59fa3b890a6ce08a2956719c6640a88337`). The executable-script staging writer now uses `mkstemp` plus `fdopen` to remove the predictable shared-`/tmp` symlink race; exact-head run `42398764761` on `0bbd8062e8bf7842e4879032aca3c1781bd393fa` is SUCCESS. These are bounded defenses, not a complete audit of existing /tmp targets, all scripts.* consumers, remaining eval/_eval/doSystem calls, filesystem sinks or RC trust boundaries | Inventory script producers/consumers and root execution paths; trace request/NVRAM values into all eval/_eval/doSystem and file operations; review generated configuration escaping and RC authorization |
| Firewall / NAT / QoS | trunk/user/rc/src/firewall_ex.c, firewall_filter_policy.h, tests/firewall_filter_policy.c; trunk/user/iptables/**; ipset/**; sqm-qos/** | P1 | AUDITING — URL/ICMP tokens, filter-LW ports/TCP flags/protocols, VTS ports/local ports/protocol numbers, and schedule syntax/token validation remain covered by earlier host checks. New fail-closed schedule dispatch: `firewall_schedule_is_active()` rejects malformed schedules and `0000000`; `timematch_conv()` now returns eligibility, and MAC, IPv4/IPv6 URL, and IPv4 LAN-WAN rule consumers skip inactive schedules instead of emitting unqualified all-time rules. Focused GNU89 host harness and final-source wiring review PASS; exact-head Actions run `42396303871` SUCCESS on `166274833a1df1ca8a8312fa1d250df72934b738`. All remaining restore inputs, shell/interface arguments, chain rollback/lifecycle, and IPv6 parity remain open | Continue the unreviewed NVRAM-to-iptables/ip6tables sinks, shell/interface arguments, chain initialization and restore failure/rollback; review IPv6 independently |
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
- **P1 credential-disclosure finding (fixed):** under `APP_SHADOWSOCKS`, the `dbconf` MIME route used `need_auth=0`. Its `do_dbconf()` output accepts the `p` query parameter as one or more database prefixes and streams all matching key/value pairs to JavaScript. The shipped `Shadowsocks.asp` consumer explicitly reads server `password` and SOCKS `server_pwd` fields from `ssconf_basic_json_*`; this was a credential-bearing unauthenticated data export.
- Changed the route to `need_auth=1`, so `handle_request()` enforces Basic authentication / the existing same-IP management session before calling `do_dbconf()`. The frontend endpoint and payload contract were not changed.
- Added workflow assertions for exactly one `dbconf` route, its authenticated flag, and the request handler's auth gate preceding output dispatch.
- The host C regression covers unauthenticated, authenticated-but-untrusted, and authenticated trusted-network cases. A source-wiring check ensures `dump_file()` calls the new predicate and that the existing HTTPS behavior is unchanged.

### Review notes / remaining work

- `handle_request()` currently applies a 15-second request/header alarm, rejects unsupported Transfer-Encoding, rejects duplicate or malformed Content-Length, caps the body at 50,000,000 bytes, and applies a 30-second receive timeout to the body. This is a source review, not fuzzing.
- `mime_handlers[]` review confirmed the conditional Shadowsocks `dbconf` handler was unauthenticated while its consumer page was authenticated. `do_dbconf()` serializes caller-selected database-prefix entries into JavaScript; `Shadowsocks.asp` reads `ssconf_basic_json_*` records containing `password` and `server_pwd` form values. Fixed by setting the route to `need_auth=1`; run `42381700507` passes the route policy and authentication-gate wiring contract. Other conditional routes and the complete MIME matrix still need review.
- Added a table-driven host source contract in `.github/workflows/httpd-security.yml` to assert callback and `need_auth` values for the known public cached assets and all known dynamic/configuration/download/CGI/upgrade/restore routes, including `APP_SCUT`, `APP_MENTOHUST`, `APP_OPENVPN`, and `APP_SHADOWSOCKS` conditional entries. The public JavaScript exceptions must precede the protected `**.js` catch-all. Initial commit `4cef7d95812786dad0d0ac34dce461b0a1c0bb64` failed run `42385599174` on the new source-contract check (`AssertionError: failed to parse the HTTPD MIME route table`) because the regex was over-escaped. Correction commit `015b820fc61cb8e3cc1283a0a87a48751205eae6` fixes the parser; run `42385661621` passed on exact SHA `015b820fc61cb8e3cc1283a0a87a48751205eae6`. The host route matrix now checks known public cached asset exceptions, public-JS ordering and the known protected page/config/export/CGI/upgrade/restore entries. It checks declarations and auth-gate ordering only; handler internals remain unaudited.
- **Session inactivity defect found and fixed (host-verified):** `http_login_check()` returned the owning IP's state before checking timeout, while `login_timestamp` was set only on initial login and never refreshed after later successful protected requests. Consequently the single-client lock expired 30 seconds after initial login rather than after inactivity, allowing another IP to displace an actively used management client. Added separate volatile `login_last_activity`; it is initialized/reset with session state and refreshed only after a protected request passes Basic authentication, while `login_timestamp` retains initial-login metadata semantics. The new GNU89 boundary/wraparound regression compiled and ran, and production timeout/authentication wiring checks passed in Actions run `42385373380` on exact SHA `cfd8da8ffa51f29795920f90b68777630a9c81bc`.
- Login state remains globally keyed by client IP. Same-NAT behavior, logout/ACL semantics, and session isolation remain unaudited.
- **P1 parser finding (fixed in the Basic-auth path):** `auth_check()` previously used legacy `b64_decode()`, which ignored non-alphabet characters and did not validate exact quartet length, padding location, or unused pad bits. The decoded value was then compared as a NUL-terminated string without rejecting embedded NUL bytes. Added strict two-pass `b64_decode_strict()` and bounded `httpd_decode_basic_authorization()`; legacy decoder behavior remains for unrelated callers. Regression covers canonical forms, malformed/non-canonical input, no partial output, embedded NUL and credential delimiter.
- HTTPD Authentication and Security Regression run `42385373380` passed on exact head `cfd8da8ffa51f29795920f90b68777630a9c81bc`; the private-key, strict Basic-auth and new session-policy C executables compiled and ran under GNU89 `-Wall -Wextra -Werror`, and source-to-call-site wiring passed. Previous successful run `42383489924` remains evidence for its exact older head `3989c878b702595fdba530d79da1b1e327225c1c`. These are focused host checks only, not a MIPS build or full HTTPD audit.
- TLS handshake code has a 10-second deadline in the current source. Post-handshake TLS read/write behavior, protocol/cipher policy, connection pool exhaustion and response-write paths remain open.
- Firmware/NVRAM/storage upload handlers are auth-gated in the MIME table. Fixed and host-verified a stale-artifact reuse path in `upload.c`: a malformed multipart body could fail before clearing a prior upload file, while `handle_request()` still called the restore output callback. The shared uploader now clears its destination before parsing; storage restore clears its destination before rejecting a bad boundary. HTTPD Actions run `42385963510` passed on exact SHA `5877544ea347ab728bd185012187ec09e2aeb76c`. Header arithmetic, archive extraction/rollback, fuzzing and target validation remain open.
- **Storage restore archive traversal hardening (host-verified):** `mtd_storage.sh` runs BusyBox `tar` as root into `/tmp/storage` then copies it under `/etc`. The first parent-path guard passed run `42386491809` on exact SHA `71bfeb3093cd3cdfc6c60352bd011814661dcdf8`; subsequent review also found directory entries can otherwise apply metadata through an existing symlink at the leaf. Follow-up `a0e700bb6e61fbb7c11623821563449ed3d0ffa9` adds leaf validation for directory entries before extraction side effects, and run `42386675924` passed on that exact SHA. This is a source-level policy regression, not integration fuzzing or a full firmware build; archive fuzzing, restore compatibility and target validation remain open.
- **Hardlink source path check host-verified:** tar `file_header->name` passes through `strip_unsafe_prefix()`, but `link_target` is accepted from the archive header and was passed directly to `link()`. Hardlink source targets now use the same relative-path and parent-safety policy before any destination side effects; HTTPD Authentication and Security Regression run `42387366333` passed against exact SHA `14c6658c06ed030f84dbdf7afb53c4d66b436540`. Absolute/parent/symlink-parent cases reuse the policy regression, but extraction integration/fuzzing remains open.
- **Firewall URL-pattern line-injection risk identified:** `include_webstr_filter()` writes raw NVRAM `url_keyword_xN` values inside a quoted iptables-restore rule. Quotes, backslashes or control bytes can break the intended one-line `--url` argument and potentially add restore directives. Commit `259288902383060425b2288f0c1c4bd8f60012e3` rejects those byte classes before aggregation and adds a C regression/source-order contract; CI failed run `42387739482` on exact SHA `259288902383060425b2288f0c1c4bd8f60012e3`; correction required. Other firewall rule builders remain unaudited.
- **HTTPD CGI service-ID bounds finding (host-verified):** `update_variables_ex()` and `apply_cgi()` walked the service table to its NULL sentinel for unknown POST `sid_list` values, then passed the sentinel index to code that dereferences `GetVariables(sid)` entries. Replaced both scans with existing `LookupServiceId()` and skip unknown tokens before validators/group mutation. New executable regression linked to production `common.c`, plus call-site ordering assertions, passed run `42387172293` against exact SHA `b046e3cbb84d73f4b3420f014c35d7b65925f26e`. This bounds malformed authenticated management requests; output encoding, command dispatch and broader handler semantics remain open.

### Strict Basic-auth parser sub-scope — 2026-11-07

- Audited `httpd.c::auth_check()` and legacy `base64.c::b64_decode()`. The latter explicitly ignored non-Base64 bytes and did not enforce complete quartets, legal padding or canonical unused pad bits. The HTTPD caller compared the decoded bytes as a C string without rejecting embedded NUL.
- Added a separate strict RFC 4648 decoder so unrelated legacy callers retain their existing behavior. It validates the complete token, padding and output capacity before writing, and the lookup table uses explicit signed-char storage for the -1 invalid-byte sentinel.
- Added bounded `httpd_basic_auth.h` parsing: case-insensitive Basic scheme, valid SP delimiter, trailing line-ending/OWS trimming, encoded-token cap, decoded-size bound, embedded-NUL rejection and required user/password colon. `auth_check()` now calls this parser.
- Added `tests/httpd_basic_auth.c`; run `42383489924` passed on exact SHA `3989c878b702595fdba530d79da1b1e327225c1c`. Both GNU89 C regression binaries compiled and ran, then the source-wiring checks passed. Initial run `42383463695` failed due to doubled shell continuation backslashes; workflow-only correction `3989...` produced the successful rerun.
- This sub-scope is HOST VERIFIED. No end-to-end HTTP transaction test, MIPS compile, full firmware image, fuzzing or hardware validation is claimed. Duplicate Authorization-header policy and broader login/session/ACL semantics remain open.

### Verification evidence

- GitHub Actions: HTTPD Authentication and Security Regression run `42381700507`, conclusion **success**, exact head `7fc9b5d7fee3f5dcb15310456e657e567714d75b`.
- GitHub Actions: HTTPD Authentication and Security Regression run `42383489924`, conclusion **success**, exact head `3989c878b702595fdba530d79da1b1e327225c1c`; strict Basic decoder/parser, private-key policy and route/auth wiring checks passed.
- Failed first run `42383463695` on `b905c4baabd87922075700397c54cdbe6edd03fd` was a doubled shell-continuation issue; workflow-only commit `3989c878b702595fdba530d79da1b1e327225c1c` corrected it and its exact-head rerun passed.
- Passed steps: compile/run the private-key policy C test with `-std=gnu89 -Wall -Wextra -Werror`; verify private-key getter / `dump_file()` wiring, authenticated `dbconf` route, and auth-gate-before-output ordering.
- GitHub Actions: SkipDBv2 Regression run `42381700499`, conclusion **success**, exact same head `7fc9b5d7fee3f5dcb15310456e657e567714d75b`; integration contracts, configure, SkipDB target build, and regression tests passed. This workflow does not produce the full router firmware image.
- No full HTTPD MIPS object build, XY-C1 firmware build, image inspection or hardware validation was run in this round.

## HTTPD executable-script mutation trust boundary — 2026-11-08

- Scope: web_ex.c::write_textarea_to_file(), validate_asp_apply(), variables.c, and the management UI / service-variable integration for custom scripts.
- P1 finding: the shared HTTPD file writer classifies .sh payloads as executable files and writes them into root-managed storage with mode 0755. The writer itself did not require a trusted session. Most built-in General custom-script variables already carried EVM_BLOCK_UNSAFE, but the two scripts.ap_script.sh entries in the 2.4 GHz and 5 GHz wireless tables did not. In addition, the ap_script.sh branch directly called /etc/storage/ap_script.sh after processing the field without a server-side trusted-session check. This required a valid authenticated management session; this was a missing trust-boundary authorization for executable script mutation/execution, not unauthenticated access.
- Fix in source commit 5025bcb42d92c715d55595c15ff161cf13eb920e: added a focused script-file policy helper and GNU89 regression; the common writer now denies .sh writes before temp/target filesystem operations unless get_login_safe() is true; both wireless scripts.ap_script.sh variable definitions carry EVM_BLOCK_UNSAFE; and the direct AP script execution branch also requires a trusted session. The existing get_login_safe() HTTPS allowance is preserved.
- Validation: local cc -std=gnu89 -Wall -Wextra -Werror compile/run of tests/httpd_script_policy.c PASS. HTTPD Authentication and Security Regression run 42393893775 completed SUCCESS on exact code head 5025bcb42d92c715d55595c15ff161cf13eb920e; both the policy compile/run step and the production source-wiring assertions passed. L1 source review and L2 focused host regression PASS. The earlier generic /tmp dispatch gate CI is confirmed SUCCESS by run 42392318537 on exact SHA fb488c486462bdbf1dbedbaad559f21bd7f1785c.
- Residual scope: this does not prove every existing /tmp executable, template caller, scripts.* consumer, service restart path or request/NVRAM-to-shell sink has been audited. Continue tracing those consumers and eval/_eval/doSystem callsites; review HTTPD session/logout/ACL and privileged RC paths separately. Full MIPS production compilation, XY-C1 firmware image build/inspection and hardware validation remain NOT VERIFIED.

## HTTPD executable-script staging path / shared-/tmp TOCTOU — 2026-11-08

- Scope: `trunk/user/httpd/src/web_ex.c::write_textarea_to_file()`, its `write_file_dos2unix()` helper, and the HTTPD Security Regression workflow.
- P1 local trust-boundary finding: the writer used a deterministic `/tmp/.<filename>` staging path, unlinked it, then reopened it via `fopen(path, "w")`. Because HTTPD performs this operation with elevated privileges and `/tmp` is shared, a local process able to create files there could race the unlink/open gap with a symlink and redirect the write. This was found by static source/call-path inspection; exploitability was not demonstrated by a runtime stress test.
- Fix in source commit `0bbd8062e8bf7842e4879032aca3c1781bd393fa`: the CRLF conversion logic now writes to an already-open `FILE *` through `write_file_dos2unix_fp()`; staging uses `mkstemp("/tmp/httpd-upload-XXXXXX")` and `fdopen()` on the returned descriptor. The staging object is uniquely and atomically created with restrictive permissions and is not reopened by name. The final storage destination still uses the existing path-based writer; final destination symlink/path ownership review remains open.
- Workflow contract asserts the trusted-session policy precedes staging, `mkstemp` precedes `fdopen`, descriptor-backed writing follows, and the old predictable path construction is absent.
- Validation: HTTPD Authentication and Security Regression run `42398764761` completed SUCCESS on exact source SHA `0bbd8062e8bf7842e4879032aca3c1781bd393fa`; job `128481780467` passed the existing GNU89 policy regression compilation/execution and the complete source-wiring assertions. This does not compile the full production HTTPD object for MIPS.
- L1 static source review PASS; L2 focused policy host regression + exact-head Actions PASS. No symlink-race stress test, target MIPS/firmware build, runtime router validation or hardware test was run.
- The same consumer trace enumerated 28 `scripts.*` File variable rows / 27 distinct filenames and mapped the common writer/reader. Initial RC consumer trace covers startup/shutdown, WAN up/down and Internet-state notifications, firewall post-script, VPN callbacks, button script and AP script. DDNS/FRP/Shadowsocks/Adbyby consumer details, alternate executable-script writers and `/tmp` producer inventory are incomplete.

## Firewall / NAT / QoS — NVRAM rule-token validation checkpoint (2026-11-07)

### Findings and production changes

- The URL-filter path rejects quote, backslash, DEL and ASCII control bytes from url_keyword_xN before values enter a quoted iptables-restore rule.
- ICMP filter_lw_icmp_x tokens must be decimal values 0..255. This closes the ASCII-space-only tokenizer gap where tabs/newlines could remain inside a token and enter --icmp-type.
- Added format-specific server-side allowlists at the production rule boundary:
  - filter-LW source/destination ports: optional empty value or one decimal port / inclusive ordered low:high range, endpoints 1..65535.
  - TCP flag suffix: empty or the fixed UI enum ALL/SYN/ACK/FIN/RST/URG/PSH.
  - filter-LW and VTS protocol numbers: decimal IP protocol values 0..255.
  - VTS public port: required single port or ordered range; translated local port: optional single port 1..65535.
  - schedule date: exactly seven weekday bits; schedule time: exactly HHMMHHMM with valid hours and minutes.
- Malformed port/protocol/flag entries are skipped before rule generation. The same checks are applied in virtual-server filter and DNAT generation; browser-side form validation is not treated as a server-side trust boundary.
- Regression coverage includes numeric bounds/overflow, range ordering and shape, control-byte/newline injection, unsupported TCP flags, weekday shape and invalid hour/minute values.
- Schedule semantics updated by the 2026-11-08 checkpoint below: malformed date/time and the explicit `0000000` weekday mask now mark the schedule inactive; consumers suppress the corresponding rule/jump. A valid `1111111` + `00002359` schedule remains an intentional all-time rule.

### Exact checkpoint and verification

- Code/workflow checkpoint: 551ac54f3d605c71f334b7c6163d393de15e93aa.
- Validator/call-site/test commit: 6419f7602771aa1dc33ebd58fbaad89aa2073cf6. Policy-header string.h dependency correction: 87b732a57d9038d9cc7e233d8d32f9b8fb97338b. Workflow matcher/source-contract corrections: aa9d424a2d08dc584bac40143d45a871e7a9136d and 551ac54f3d605c71f334b7c6163d393de15e93aa.
- HTTPD Authentication and Security Regression run 42389819329 completed success on the exact same SHA 551ac54f3d605c71f334b7c6163d393de15e93aa. Both Compile and run policy regression and Verify HTTPD auth and route-access wiring passed.
- The C step compiled/executed the HTTPD private-key, Basic-auth, login-policy, service-ID, BusyBox archive-path and firewall-policy regression programs with cc -std=gnu89 -Wall -Wextra -Werror. The wiring step reported HTTPD MIME authorization, sid_list service-ID validation, stale upload cleanup, archive symlink-parent protection, URL, ICMP and expanded firewall token guards OK.
- Earlier CI attempts exposed workflow/test harness issues and were repaired: missing policy-header string.h include, malformed source-contract line continuation, and an overly exact flag-guard substring assertion. The final exact-head run above is green.
- This is host regression/source-contract evidence only. Production firewall_ex.c target compilation, XY-C1 MIPS build, firmware image inspection, runtime restore testing and hardware validation remain NOT RUN / NOT VERIFIED.

### Fail-closed firewall schedule dispatch — 2026-11-08

- P1 finding: `timematch_conv()` previously returned an empty match suffix both for invalid date/time bytes and for the explicit all-zero weekday mask. Its consumers then emitted ordinary rules with no time qualifier, silently widening a scheduled rule to all-time.
- Production fix on source checkpoint `166274833a1df1ca8a8312fa1d250df72934b738`: added `firewall_schedule_is_active()` to the policy header; changed `timematch_conv()` to return whether the schedule is active; valid all-days/all-day continues to return active with an empty suffix. MAC entries, IPv4/IPv6 URL jumps and the IPv4 LAN-WAN filter jump now skip inactive schedules. `mac_num` is incremented before the MAC schedule check so whitelist mode retains its existing terminal-drop behavior when configured entries are all inactive.
- Commits: policy `01b0aae8758a638347cc8968a4e81bd021032697`; production `e39d4556136ce6982117bcb81a4e6c6ea0c3d40e`; regression `fd8f5306fb23b315fabb2ec8621ee4d21b3f14fa`; CI source contract `034d4de4d98103402c9980b7a9dfefaa877fa85f`; indentation cleanup `166274833a1df1ca8a8312fa1d250df72934b738`.
- Regression adds valid full-day/selected-day cases plus zero-weekday, malformed date/time and NULL rejection cases. Local `cc -std=gnu89 -Wall -Wextra -Werror` focused schedule-policy harness PASS; final source consumer assertions were checked against all four dispatch paths.
- Actions status: combined commit status is empty and the available commit-workflow query is PR-only; no exact-head push-triggered Actions result was retrieved for `166274833a1df1ca8a8312fa1d250df72934b738` (PENDING / NOT VERIFIED). Do not claim the workflow passed.
- Validation boundary: no production `firewall_ex.c` MIPS object build, XY-C1 firmware image build/inspection, runtime `iptables-restore`/`ip6tables-restore` exercise or hardware validation. Firewall remains AUDITING.

### Remaining firewall scope

- Firewall remains AUDITING. Continue tracing every remaining NVRAM-derived iptables-restore and ip6tables-restore argument outside the reviewed URL/MAC/LAN-WAN/VTS paths; inspect interface/address inputs, shell command construction, chain initialization, restore failures and rollback. The schedule fail-open defect described above has a bounded source fix, but its exact-head Actions run is not yet verified.
- Review IPv6 independently. The current regression covers selected IPv4 rule builders and policy helpers; it does not establish full IPv6-path safety.
- Next P1 scope is CGI sys_script() / SystemCmd and privileged script dispatch. Then continue DNS/DHCP, WAN/PPP/IPv6 and system service lifecycle auditing rather than routine SkipDBv2 expansion.
---

## Mandatory next work

1. Inventory generic /tmp executable targets and all sys_script()/ej_dump()/action_script callers; review remaining fixed-name handlers as a separate authorization matrix.
2. Trace each scripts.* shell file from variable registration and web write path to its service/init consumer, including AP, VPN, DDNS, FRP and custom scripts; search for alternate file-write paths and root eval/_eval/doSystem calls that accept request/NVRAM data.
3. Continue HTTPD session/logout/ACL semantics (including IP-only ownership and same-NAT behavior), then audit privileged RC command/configuration boundaries and remaining request/NVRAM-to-eval/_eval/doSystem/filesystem sinks.
4. Continue the firewall audit beyond the bounded schedule fix: audit unreviewed iptables/ip6tables values, interface/address and shell arguments, chain rollback, restore failure handling and IPv6 parity.
5. Extend source-tree/build-manifest inventory into DNS/DHCP, WAN/PPP, wireless, file sharing, storage/hotplug and init/service lifecycle. Keep routine SkipDBv2 expansion paused.
6. Run make XY-C1 TOOLCHAIN=mipsel-linux-musl only when actually executed; inspect the produced firmware artifact. Do not label host regression as target or hardware validation.
7. Update the audit table with exact evidence and commits; never mark a whole module complete based on a bounded fix.

## Verification gaps

- Full MIPS cross-compile: NOT VERIFIED by the baseline SkipDBv2 Regression result.
- Complete firmware image generation: NOT VERIFIED.
- XY-C1 real-device validation: NOT VERIFIED.
- Whole-repository security audit: NOT COMPLETE.
