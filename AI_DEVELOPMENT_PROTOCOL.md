# Padavan 4.4 — AI Development Protocol

**Repository:** https://github.com/xiaobei206/padavan-4.4-2  
**Primary branch:** main  
**Mission:** repository-wide Padavan user-space stability bug discovery and repair first, with security hardening progressing in parallel; also improve performance, resource management, compatibility, and maintainability. Critical security vulnerabilities and severe system failures take immediate priority.

This file is the durable source of development rules. Every new ChatGPT conversation must read this file and the current handoff/roadmap before changing code. Repository state, commits, CI runs, and these documents take precedence over chat memory.

## 1. Operating principles

- Work autonomously within the connected GitHub permissions and available tools. Do not ask for redundant permission or repeat questions already answered.
- **Primary direction is stability:** actively find and fix reproducible or source-demonstrable reliability defects across the whole Padavan user space. Security work continues in parallel; confirmed critical security vulnerabilities, data corruption, boot failures, and severe service outages override ordinary stability work.
- Consider performance and resource management where they cause or materially worsen failures: leaks, stale state, unbounded retries, deadlocks, race conditions, unchecked return values, partial updates, cleanup failures, process lifecycle errors, and recovery paths.
- Do actual repository work; a plan or audit report alone is not a completed task.
- Never claim a change, build, test, CI result, firmware image, or hardware validation that was not actually verified.
- Do not optimize for commit count, line count, test count, or green badges.
- A successful test suite for one component is not evidence that the whole firmware is safe or working.
- Preserve existing Padavan behavior, configuration compatibility, device support, and recovery paths unless a change explicitly fixes a demonstrated defect.
- Keep Linux 4.4 kernel/MTK driver/DTS upgrades out of ordinary user-space modernization. Treat kernel modernization as a separately scoped project.

## 2. Repository-wide coverage is mandatory

Maintain an inventory of real modules discovered from the source tree, build manifests, init scripts, configuration, and call relationships. The inventory must be based on this repository; do not assume every example below exists.

At minimum inspect these areas:

1. Web and remote management: HTTPD, HTTPS/TLS, CGI, ASP/template handlers, authentication, sessions/cookies/tokens, authorization, command dispatch, configuration APIs, uploads/downloads, file access, frontend/backend interfaces.
2. Network services: dnsmasq, DHCP, DNS, PPP/PPPoE, WAN, IPv4/IPv6, routing, NTP, DDNS, UPnP, port forwarding.
3. Network security: iptables/ip6tables, firewall, NAT, conntrack, QoS, traffic shaping, VPN/tunnels, protocol parsers.
4. System/configuration: NVRAM, config persistence, boot/init, service lifecycle, process and signal handling, IPC, logging, upgrade/restore, privilege boundaries.
5. Wireless/hardware: wireless management, hostapd/wpa_supplicant integration, MTK driver interfaces, USB, storage, hotplug.
6. File and application services: FTP, Samba, print/media services, and other actually integrated services.
7. Shared infrastructure: BaseKit, SkipDBv2, UDB, common C libraries, memory/string/file/network helpers, concurrency, third-party components and local patches.
8. Build/delivery: Makefiles, CMake, toolchain, compile flags/macros, GitHub Actions, source contracts, firmware assembly and image validation.

Do not let an already well-tested subsystem monopolize development merely because it is easy to inspect. Whole-repository coverage outranks polishing a subsystem that has already received many rounds of work.

## 3. Stability-first risk-based selection

Select work from the whole-repository ledger in `REFACTOR_ROADMAP.md`, not from habit or from whichever module is easiest to inspect.

Use this priority order:

- **P0 — immediate:** confirmed critical security vulnerability; data corruption or loss; boot/recovery blocker; severe system-wide failure; release blocker.
- **P1 — stability:** reproducible or strongly evidenced failures in core paths such as WAN/PPP/DHCP/DNS/routing, firewall/NAT, service lifecycle, configuration persistence, storage/mount/hotplug, wireless, HTTPD management, or startup/shutdown. Also include serious security flaws and exposed privileged paths.
- **P2 — preventive reliability/security:** unchecked errors, race windows, stale callbacks/state, incomplete rollback, resource leaks, unsafe retry/backoff, missing regression coverage, and important unaudited attack surfaces.
- **P3 — performance/maintainability:** measurable resource/performance improvements, safe deduplication, cleanup, and style work that does not displace higher-impact defects.

Security remains an active parallel workstream; do not defer a confirmed serious vulnerability merely because the current round is stability-focused. Conversely, do not let security-only work crowd out stability defects indefinitely.

Do not let SkipDBv2, HTTPD, firewall, or any other already heavily worked subsystem monopolize the project. Rotate across real modules in the source tree and prioritize unaddressed stability risks. Keep an explicit whole-userland inventory and record why each selected task outranks the alternatives. Resume the exact unfinished task or pending/failed CI first; once it is closed, select the next highest-value stability defect from the global ledger, unless a P0 security/system failure requires immediate action.

## 4. Bounded, continuous delivery

Each round should deliver one coherent, independently reviewable fix or tightly related set of fixes. Keep the change bounded to protect handoff reliability; do not turn a conversation into an unbounded rewrite. A round may inspect several areas to choose the best issue, but implementation should stay focused and the roadmap must keep the broader rotation visible.

Each round has one primary independent module. Before editing:

- Read the relevant implementation, headers, build integration, callers/callees, startup/shutdown path, and error paths.
- Identify concrete defects or measurable maintainability risks.
- Check public/private declarations, extern/static ownership, macros, conditional compilation, ABI/layout, integer widths, alignment, pointer conversions, allocation ownership, and platform assumptions.
- Understand persistent formats, protocol behavior, and configuration compatibility.
- Define the smallest coherent change and its regression coverage.

Do not perform unrelated opportunistic edits. Related tests, private headers, build manifests, CI contracts, and documentation may change when required for the selected module.

Prefer targeted fixes over broad rewrites. Avoid unnecessary abstraction layers and API churn.

## 5. Build and platform safety

The development host may not have the complete MIPS firmware toolchain. Never treat host compilation as proof of target compatibility.

- Check the actual compiler, libc, kernel interfaces, and language mode used by the firmware.
- Avoid APIs or language features unsupported by the target toolchain.
- Verify function prototypes, header self-containment, symbol linkage, compile guards, and source registration.
- When adding/removing/renaming source or test files, update all applicable Makefiles/CMake manifests, firmware integration, CI paths/contracts, and packaging rules.
- Source contracts should test meaningful structure, integration, and safety properties. Avoid brittle dependence on whitespace, indentation, comment wording, or incidental code ordering.
- Do not weaken or delete a meaningful check just to obtain a green result.

## 6. CI decision process

Before each round, inspect the latest main HEAD and the relevant GitHub Actions runs. Match every run to its exact head SHA.

If CI is red:
1. Read the failed job and relevant logs.
2. Identify whether the failure is a source defect, regression, build/integration issue, brittle source contract, workflow error, or external infrastructure issue.
3. Fix the existing failure first. Do not start unrelated refactoring while a relevant failure remains unresolved.
4. Add or preserve regression coverage for real defects.
5. Do not suppress tests, skip required jobs, or mislabel infrastructure failures as code fixes.

If CI is green:
1. Confirm the run is for the current HEAD.
2. Confirm the relevant jobs and tests actually executed.
3. Do not infer firmware or hardware success from host regression success.
4. Resume an unfinished task before selecting a new module.
5. Otherwise select the highest-risk unfinished module from the roadmap.

Do not poll indefinitely. If a run remains pending when the current execution budget ends, record its URL, run ID, head SHA, and pending state for the next conversation. Never claim background work will continue after the conversation.

## 7. Validation levels — never conflate them

Record each level separately. During ongoing development, prioritize repeatable L1/L2 checks and exact-HEAD Actions feedback. Schedule full MIPS firmware/image builds and physical-device validation as consolidated milestones in the later stage of the main development programme, and before any release claim that requires them. Pull a target build or hardware test forward when a specific defect cannot be responsibly assessed without it; record the reason and any blocker honestly.

- **L1 Static review:** source inspection, static checks, declarations, dependency and contract checks.
- **L2 Host regression:** host build, unit tests, CTest, relevant integration tests.
- **L3 Target build:** actual MIPS cross-compile, firmware integration, complete image generation and artifact validation.
- **L4 Hardware validation:** flash/boot and real XY-C1 or other target-device behavior checks.

Use explicit states such as NOT RUN, PASS, FAIL, BLOCKED, or NOT VERIFIED. A Level 1/2 pass must never be described as an L3/L4 pass.

## 8. Commits and branch safety

- Prefer a single coherent commit for one bounded module task.
- Push completed, appropriately validated work to main, unless the repository's current workflow or an existing unfinished task requires a different safe route.
- Never overwrite a branch head based on stale state. Re-fetch the current ref before updating it.
- Do not force-push or rewrite published history as a convenience.
- If work cannot be completed safely in the current conversation, save the exact unfinished state to a clearly named remote checkpoint branch when the available tools support it. Record branch and SHA in SESSION_HANDOFF.md.
- Never claim a local-only or uncommitted change has been preserved remotely.
- Do not create a checkpoint merely for a task that has already been coherently completed and committed to main.

## 9. Three required project records

### dev_log.md — chronological history
For every code or documentation commit, add a newest-first entry with:
- Real Beijing time (UTC+8).
- Base SHA and resulting commit SHA when known.
- Scope and changed paths.
- Defect/problem and fix.
- Tests actually run and results.
- CI run ID/URL and exact head SHA, or PENDING/NOT RUN.
- Remaining risks and follow-up.

Do not rewrite or truncate the existing history.

### REFACTOR_ROADMAP.md — whole-repository status
Track each real module's source paths, responsibilities, exposure, priority, audit scope, findings, implementation state, test levels, last commit, and next action. Distinguish:
- NOT INVENTORIED
- NOT AUDITED
- AUDITING
- AUDITED — FINDINGS
- AUDITED — NO FINDINGS
- FIXED — NOT VERIFIED
- HOST VERIFIED
- TARGET BUILD VERIFIED
- HARDWARE VERIFIED

A historical commit or audit note is evidence of a past edit, not proof that the current module has been completely audited or is safe.

### SESSION_HANDOFF.md — exact resumable checkpoint
Record:
- current main SHA and active branch;
- current task and selected module;
- files/functions reviewed;
- evidence and findings;
- completed and incomplete changes;
- working-tree/remote persistence state;
- test status;
- CI URL/run ID/head SHA/status;
- blockers and risks;
- exact first action for the next conversation.

Update the handoff after meaningful milestones, after commits, when CI is pending/failing, and before stopping. Be specific; “continue HTTPD” is insufficient.

## 10. Cross-conversation recovery is mandatory

Every new conversation must do this before coding:

1. Read this protocol.
2. Fetch current main HEAD.
3. Read SESSION_HANDOFF.md, REFACTOR_ROADMAP.md, and the newest dev_log.md entries.
4. Inspect remote branches/checkpoints and latest relevant Actions runs.
5. Compare recorded SHAs with actual GitHub state.
6. Resume an unfinished task or pending CI before choosing another module.
7. Only select a new task when the previous task is complete or explicitly closed with a documented reason.

Never rely on hidden memory or the previous chat transcript. GitHub is the project memory.

Before the current conversation's execution/context budget is exhausted:
- stop starting new modules;
- finish the current atomic task if safe;
- otherwise persist unfinished work remotely when possible;
- update the three records;
- provide the exact next action.

If the available connector cannot perform a needed operation, state that limitation. Do not invent an action or claim success.

## 11. Initial and ongoing deliverables

The project is not complete merely because there are many commits or one component's tests are green.

A credible completion assessment requires:
- repository-wide module inventory;
- broad stability-defect triage and fixes across core services, lifecycle, persistence, networking, storage, management, and other integrated user-space modules;
- ongoing parallel security review with confirmed critical/high-impact findings handled promptly;
- documented audits of critical attack surfaces and services;
- evidenced fixes and regression coverage for confirmed defects;
- build/integration verification;
- actual MIPS firmware build and artifact checks;
- real target-device validation for release-critical behavior;
- preserved configuration and functional compatibility;
- current, consistent roadmap and handoff records.

At every stop, report briefly:
- what was actually changed;
- commit and branch;
- tests and exact CI result;
- what remains unverified;
- the next highest-priority action.

**Non-negotiable:** stability bug discovery and repair is the primary development direction; security hardening continues in parallel and urgent security issues remain immediate priorities. Do not keep polishing a familiar subsystem while high-risk or failure-prone parts of the firmware remain unaudited. Do not lose unfinished work when a ChatGPT conversation ends. Do not equate local tests with a working firmware image.
