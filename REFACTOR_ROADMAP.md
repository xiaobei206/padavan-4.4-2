# Repository-Wide Modernization Roadmap

**Repository:** xiaobei206/padavan-4.4-2  
**Primary branch:** main  
**Initial baseline before this governance bootstrap:** 412eebb4d0c36f9cbaf43921b4ce1f2056665984  
**Roadmap status:** INITIAL / INVENTORY INCOMPLETE

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

- Main HEAD at initial bootstrap: 412eebb4d0c36f9cbaf43921b4ce1f2056665984.
- The latest available SkipDBv2 Regression run at that HEAD was #669 / 42376349424, conclusion success, exact head SHA matched. The preceding conversation's job inspection reported 57/57 CTests passed.
- This is evidence for that workflow and that SHA only. It is not a full-firmware MIPS build or hardware verification.
- The 273-commit comparison from 8a6fc2639fcd036790b02eae56cadb2c2935883d to 412eebb4 is dominated by SkipDBv2/BaseKit/UDB, its tests, dev_log, and CI. That recent work window does not show HTTPD source changes.
- Historical repository note GPT_AUDIT_NOTES.txt (dated 2026-11-04) records earlier edits in many other areas, including httpd.c response-write handling, https.c TLS handshake deadline, web_ex.c, ej.c, firewall_ex.c, WAN/IPv6, FTP/Samba, USB, and service helpers. These notes are historical pointers only: each item must be checked against current source and commit history. A past fix is not a complete module audit.
- Full repository inventory, all module paths, current test coverage, target build state, and hardware state remain to be established.

## Initial module register

| Area | Known source clues / examples | Priority | Current status | Next action |
|---|---|---:|---|---|
| HTTPD / HTTPS | Historical notes mention httpd.c response writes and https.c TLS handshake deadline | P1 | NOT AUDITED | Map request parsing, connection lifecycle, response writes, TLS timeout, privilege and file access paths |
| CGI / ASP / Web templates | Historical notes mention ej.c and web_ex.c parsing/output handling | P1 | NOT AUDITED | Inventory handlers, input sources, escaping, command dispatch and authorization |
| Authentication / session / authorization | Web management surface; paths to be discovered | P1 | NOT INVENTORIED | Trace login, cookies/session, per-handler permission checks and state invalidation |
| Privileged command/configuration interfaces | Historical notes mention shutils.c, notify_rc.c, NVRAM helpers, VPN hooks | P1 | NOT AUDITED | Trace all untrusted values to exec/system/ioctl/config writes; verify argv separation and bounds |
| Firewall / NAT / QoS | Historical notes mention firewall_ex.c and WAN route parsing | P1/P2 | NOT AUDITED | Inventory rule generation and validation; test IPv4/IPv6 and malformed inputs |
| DNS / DHCP / dnsmasq integration | Actual integration paths not yet inventoried | P1/P2 | NOT INVENTORIED | Identify configs, lifecycle, leases, dynamic updates and failure handling |
| WAN / PPP / routing / IPv6 | Historical notes mention net_wan.c, net6.c, net_wan6.c | P1/P2 | NOT AUDITED | Trace route/address parsing and state transitions on target |
| Wireless management / MTK interface | Historical notes mention ralink.c survey parsing | P1/P2 | NOT AUDITED | Map user-space driver interactions, buffers, ioctl inputs and recovery behavior |
| NVRAM / configuration persistence | Historical notes mention nvram_linux.c and nvram_x.c | P1/P2 | NOT AUDITED | Review bounds, consistency, migration, failure handling and callers |
| FTP / Samba / file sharing | Historical notes mention vsftpd asus_ext.c and disk_share.c | P1/P2 | NOT AUDITED | Review authentication, path handling, permissions, shell invocation and share lifecycle |
| USB / storage / hotplug | Historical notes mention usb_info.c, aidisk.c, disk_io_tools.c | P2 | NOT AUDITED | Review sysfs parsing, path bounds, hotplug races and cleanup |
| System init / services / process lifecycle | Paths not yet inventoried | P1/P2 | NOT INVENTORIED | Map init scripts, process ownership, signal handling, restart and privilege boundaries |
| IPC / common helpers | Paths not yet inventoried | P2 | NOT INVENTORIED | Inventory shared APIs and consumers; inspect framing, ownership and failure paths |
| SkipDBv2 / BaseKit / UDB | Recent intensive work under trunk/user/skipdbv2/skipdbv2 | P2 | HOST REGRESSION EVIDENCE EXISTS; FULL AUDIT NOT ASSERTED | Pause routine expansion; address only newly demonstrated defects or required integration work |
| Build system / toolchain / firmware image | Makefile, toolchain, trunk build tree | P1/P2 | NOT AUDITED | Map supported targets, target compiler and complete XY-C1 firmware build path |
| Linux 4.4 kernel / MTK drivers / DTS | Separate kernel-level source area | Separate project | NOT IN CURRENT USER-SPACE SCOPE | Do not mix kernel upgrade into routine user-space modernization |

## Mandatory next work

1. Build a recursive source-tree inventory from the current main branch; identify real service directories, build manifests, init scripts, and feature gates.
2. Validate all historical pointers in GPT_AUDIT_NOTES.txt against current source and git history.
3. Establish the actual HTTPD/HTTPS/CGI/authentication call graph and record reviewed files/functions.
4. Select the highest-risk concrete finding and implement one bounded fix with a regression or documented target validation.
5. Update this table with evidence and exact commits; never mark an entire area complete based on one historical patch.

## Verification gaps

- Full MIPS cross-compile: NOT VERIFIED by the baseline SkipDBv2 Regression result.
- Complete firmware image generation: NOT VERIFIED.
- XY-C1 real-device validation: NOT VERIFIED.
- Whole-repository security audit: NOT COMPLETE.
