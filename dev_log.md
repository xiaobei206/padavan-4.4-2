## 2026-11-06 01:26 CST — Centralize UDB record payload extent checks

### Baseline and scope
- Base commit: `e5b143842e7c1e4a4b0154239c8144881d1b287e` (`refactor(udb): unify record cursor position bounds`); SkipDBv2 Regression run #184 (`42074324193`) passed all checks.
- This iteration is limited to `udb/UDBRecordRead.c`.

### Changes
- Centralized file-boundary checks ensuring a record header and its declared payload fit in the records file.
- Reused the private helper before reading the header, after decoding its declared payload size, and before loading the payload.
- Updated existing CI source contracts to verify the helper has one definition and three call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and three call sites; the CI contract checks the same invariant.
- The remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:23 CST — Unify UDB record-cursor position bounds

### Baseline and scope
- Base commit: `4f51e6b398fcb1581873ea51641a380e6ac92507` (`refactor(skipdb): centralize record-link traversal bounds`); SkipDBv2 Regression run #183 (`42074088012`) passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBRecordsCursor.c`.

### Changes
- Centralized record-size and signed file-offset overflow checks for advancing from the current record and advancing relative to a supplied record.
- Kept the existing caller-specific null guards and EOF/corruption handling in `UDBRecords_recordAtPos_`; only duplicate next-position arithmetic moved into one private helper.
- Added CI contracts for the helper's single definition/two callers. No new source file, public declaration, or CMake source-registration change.

### Validation
- Static preflight confirms one helper definition and exactly two call sites; the CI contract checks the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:22 CST — Unify SkipDB record-link traversal bounds

### Baseline and scope
- Base commit: `140bca22ffc885a2118bac0540a1ad317ef7fa76` (`refactor(skipdb): unify bounded query traversal guards`); SkipDBv2 Regression run #182 (`42073805198`) passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBRecordLinks.c`.

### Changes
- Centralized validation of the database/index state and positive PID-population traversal bound in one private helper reused by quick search and last-record discovery.
- Preserved quick search's additional per-level allowance, last-record's PID-only bound, and the existing failure behavior that closes the underlying UDB on an invalid index length.
- Added CI source contracts for the helper's single implementation/two call sites and ensured the existing core regression explicitly covers last-record discovery. No new source file, public declaration, or CMake source-registration change.

### Validation
- Static preflight confirms one helper definition and two calls; the CI contract checks the same count.
- The pushed SkipDBv2 Regression run remains authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:20 CST — Centralize SkipDB bounded query traversal guards

### Baseline and scope
- Base commit: `d5d41642100a4f4b7b21dbf46e558b2b9d6c03eb` (`refactor(udb): centralize PID offset validation`).
- SkipDBv2 Regression run #181 (`42067370478`) passed. This iteration is limited to `skipdb/SkipDBQuery.c`.

### Changes
- Centralized the PID-index traversal limit lookup and invalid-index failure handling in one private helper shared by `SkipDB_count` and `SkipDB_mergeInto_`.
- Centralized per-record step counting and cycle/corrupt-link closure handling without changing operation-specific diagnostics, return values, count saturation, or merge start position.
- Extended the existing CI source contracts to require the private helpers' single ownership and both callers; the existing `skipdb_core_layers_test` exercises both query paths. No new C source, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms each helper has one definition and two call sites, and CI contracts check the same ownership/use counts.
- The pushed SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 — Centralize UDB PID offset validation

### Baseline and scope
- Base commit: `b40bca57ec639be8cb8f1c68884510df3912dc36` (`refactor(udb): centralize free PID insertion`).
- SkipDBv2 Regression run `42066950505` passed source contracts, CMake configuration, compilation, and CTest. This iteration is limited to PID-to-file offset mapping in `udb/UDBIndexMap.c`.

### Changes
- Added one private `static` offset calculator that checks PID multiplication overflow and `LONG_MAX` seekability before producing a byte offset.
- Reused it in both PID lookup and PID mapping writes; file-size containment and record-width checks remain specific to lookup, while reservation and write-rollback behavior remain in the existing write path.
- Extended `udb_index_layers_test` to verify the maximum PID is rejected by both mapping APIs and leaves the sentinel/next PID unchanged.
- Added CI contracts for the single private helper owner and overflow regression. No new files, public declarations, or CMake source registration.

### Validation
- Static review confirms the shared helper preserves the existing multiplication bound and signed seek limit, and the lookup still checks the complete fixed-width entry lies inside the index file.
- The pushed SkipDBv2 Regression is authoritative; no local CMake/compiler result is claimed.

---

## 2026-11-06 — Centralize UDB free-PID insertion

### Baseline and scope
- Base commit: `05384a079f06d8e3dc2bf4cd9e5e1a6f4c99189f` (`refactor(skipdb): unify cursor traversal guards`).
- SkipDBv2 Regression run `42066219631` passed source contracts, CMake configuration, compilation, and CTest. This iteration is limited to `udb/UDBIndexFreeList.c`.

### Changes
- Centralized checked, duplicate-free insertion into the reusable PID stack in one private `static` helper.
- Reused it for ordinary PID release and for retiring reservations that were allocated but not written before transaction commit. Reservation removal remains in the release path; commit still clears the reservation set only after every return-to-free-stack operation succeeds.
- Extended `udb_index_layers_test` to allocate an unwritten PID, commit, and verify the reservation is cleared and the PID is returned to the free stack.
- Added CI contracts for the private helper's single owner and the abandoned-reservation regression. No new source files, public declarations, or CMake source registration.

### Validation
- Static review confirms duplicate removal occurs before the checked list push in both call paths, and failed insertion still propagates failure to the transaction preparation layer.
- The pushed SkipDBv2 Regression is authoritative; no local CMake/compiler result is claimed.

---

## 2026-11-06 — Unify SkipDB cursor traversal step guards

### Baseline and scope
- Base commit: `3add79592a5eb263ee86276d13126eede46521ed` (`refactor(skipdb): centralize record link validation`).
- SkipDBv2 Regression run `42065572830` passed source contracts, CMake configuration, compilation, and CTest. This iteration is limited to `skipdb/SkipDBCursor.c`.

### Changes
- Centralized forward/reverse cursor preconditions, PID-index step bounds, direction-specific diagnostics, failure closure, and opposite-direction counter reset in one private `static` helper.
- Kept actual record movement and reverse-header sentinel handling in their original public operations.
- Extended the existing core-layer regression to move one active cursor forward, backward, and forward again after cache eviction/lazy reload.
- Added CI source-contract checks for exactly one private traversal guard implementation and both direction-switch assertions. No new files, public declarations, or CMake source registration.

### Validation
- Static review confirms invalid cursor/database/record inputs remain non-mutating, step-limit failures still close the database and clear the cursor record, and a successful move resets only the opposite direction counter.
- The pushed SkipDBv2 Regression is authoritative; no local compiler/CMake result is claimed.

---

## 2026-11-06 — Centralize SkipDB record-link level validation

### Baseline and scope
- Base commit: `5a9366af5ecf2d128113d69179fcddfd20217f0b` (`refactor(skipdb): isolate cache eviction phases`).
- SkipDBv2 Regression run `42064998700` passed source contracts, CMake configuration, compilation, and CTest. This iteration is limited to structural link validation in `skipdb/SkipDBRecordLinks.c`.

### Changes
- Added one private `static` predicate for the invariant shared by skip-list link operations: record level must be within `0..SKIPDB_MAX_LEVEL`, and a nonempty level must have a pointer array.
- Reused the predicate in level-index validation, bounded quick-search and tail traversal, predecessor repair, and link membership checks; operation-specific rules such as requiring a nonempty level remain at their call sites.
- Extended the existing record-storage regression to verify that negative and over-limit record levels are rejected by record-link lookup. Extended CI source contracts to require the common predicate and its single implementation owner.
- No new files, public declarations, or CMake source registrations.

### Validation
- Static review preserved each caller's existing failure response: invalid quick-search/tail linkage still closes the database, while invalid standalone level access and removal/membership guards remain non-mutating.
- The pushed SkipDBv2 Regression is authoritative; no local compiler/CMake result is claimed.

---

## 2026-11-06 — Separate SkipDB cache eviction phases

### Baseline and scope
- Base commit: `02fb3f37bd025c05fd57ccedc0a9540faf6314a4` (`refactor(skipd): centralize scheduled command setup`).
- The preceding SkipDBv2 Regression run `42064200459` passed source contracts, CMake configuration, compilation, and CTest. This iteration is limited to the cache-age/eviction manager in `skipdb/SkipDBCache.c`.

### Changes
- Split the monolithic cache eviction pass into private helpers for clearing stale marks, marking the recent-record retention set, preserving links from retained records to persistent PIDs, and evicting unmarked records.
- Preserved the original phase order and in-place traversal rule: the next age-list node is captured before a record is deallocated.
- Extended the existing `skipdb_core_layers_test` to keep a live cursor across cache eviction and verify that its current record survives and its next record can be lazily reloaded.
- Added CI source-contract checks for the private phase helpers and the active-cursor regression assertions. No new source file, public declaration, or CMake registration is needed.

### Validation
- Static review confirms marks are still cleared before cursor marking, recent records and the header are marked before reference detachment, and eviction occurs only after cached links have been converted to persistent PIDs.
- The pushed SkipDBv2 Regression is authoritative; no local CMake/compiler result is claimed.

---

## 2026-11-06 — Consolidate delay/time command scheduling in skipd_command

### Baseline and scope
- Base commit: `2694b1da32150ce340e96ea104316dcaaa257ade` (`fix(test): expose skipd private headers to safety test`).
- SkipDBv2 Regression run `42061962339` passed all source contracts, CMake configuration, compilation, and CTest. The single-module target is the command engine's duplicated `delay`/`time` scheduling path in `mgr/skipd_command.c`.

### Changes
- Centralized shared schedule-key parsing, prefixed-key formatting, duplicate-task detection, and database transaction/write/sync behavior in private `static` helpers within `skipd_command.c`.
- Kept command-specific payload validation order intact: `delay` validates its NUL-terminated command payload before parsing the tick; `time` validates the time token and computes its next daily run before checking the complete stored payload.
- All `ccrReturn`/`ccrStop` control flow remains in `client_run_command`; no coroutine suspension was moved across function boundaries.
- No cross-translation-unit symbol, public header, CMake source registration, or new source file was introduced. CI source contracts require the helpers and enforce one owner for scheduled-task persistence.

### Validation
- Static review retained the original length-delimited request bounds, terminating-NUL checks, key validation, error responses, duplicate detection, transaction ordering, and timer configuration.
- Remote SkipDBv2 Regression is the required post-push validation. No local compiler/CMake result is claimed.

---

## 2026-11-06 — Repair skipd script regression include path

### Baseline and failure
- Base commit: `4916e3289da06244ec0a5815336e5dce2e6aacc7` (`fix(ci): correct SkipDB prefix target contract`).
- SkipDBv2 Regression run `42061060499` passed the integration source contracts, Prefix ownership/wiring assertions, and CMake configure. The build then failed while compiling `tests/skipd_script.c`: `skipd_internal.h: No such file or directory`.

### Changes
- Added a target-scoped `${PROJECT_SOURCE_DIR}/mgr` include directory to `skipd_script_test`, allowing the test to include the private manager header used by `mgr/skipd_script.c` without broadening include paths for unrelated targets.
- Strengthened the workflow CMake contract to require both the extracted implementation source and its private-header include path in the script-test target block.
- No new files or production C behavior changes.

### Validation
- Confirmed `skipd_internal.h` exists under `mgr/` and is directly included by both the test and extracted implementation; the failing compile lacked only that include search path.
- The next remote SkipDBv2 Regression run is authoritative. No local CMake/compiler result is claimed.

---

## 2026-11-06 00:50 CST — Correct SkipDB prefix regression target contract

### Baseline and failure
- Base commit: `ea0215423bf34177f69cf5e6d8a0dc9bebfefa3d` (`fix(ci): align SkipDB prefix source contract`).
- The follow-up SkipDBv2 Regression run `42058571347` passed all file-content contracts but failed the Prefix CMake wiring assertion. The assertion searched for `TARGET_LINK_LIBRARIES` after the `add_test()` line even though the actual target link declaration correctly appears between `ADD_EXECUTABLE()` and `add_test()`.

### Changes
- Corrected the contract to inspect the complete `skipdb_prefix_test` target block and require both the test source and its `skipdb m` link declaration inside that block.
- No production C source, API declaration, CMake target, or new file was changed; only the faulty CI assertion and this handoff log were updated.

### Validation
- Compared the contract's inspected range with the existing CMake target order; confirmed the source and link declarations are present before `add_test()`.
- The next remote SkipDBv2 Regression run is authoritative. No local CMake/compiler result is claimed.

---

## 2026-11-05 23:46 CST — Repair stale SkipDB prefix source contract

### Baseline and failure
- Base commit: `4bede96fc371f524ddff943d71eacdc66aa2b98a` (`refactor(skipdb): isolate prefix query iteration`).
- Latest SkipDBv2 Regression run failed in `Verify XY-C1 firmware integration contracts` before CMake configuration. The contract still expected `SkipDB_list_prefix` in `SkipDBQuery.c` after the implementation had moved to `SkipDBPrefix.c`.

### Changes
- Updated the workflow source-contract table to assert prefix implementation ownership in `SkipDBPrefix.c`, not the query facade.
- Kept the change limited to the CI contract and this handoff log; no C implementation, public API, or build source registration changed.

### Validation
- Compared the failing assertion in run `42047509654` with current source-contract entries and confirmed the stale path/symbol expectation.
- Remote SkipDBv2 Regression is the required post-push validation; no local CMake or compiler result is claimed.

---

## 2026-11-05 22:26 CST — Isolate SkipDB prefix-query iteration

### Baseline
- Base commit: `4e29094fb6ef99c1ffca7f8ddbcb8687064190dc` (`refactor(skipd): isolate asynchronous client lifecycle`).
- Mainline source ownership is stable after the recent skipd/IPC extractions. The host-side SkipDBv2 Regression remains the required post-push gate; no local CMake/compiler/CTest result is claimed.

### Changes
1. **Prefix-query module — `skipdb/SkipDBPrefix.c`**
   - Moved the shared prefix predicate and the callback scan plus cursor-based `SkipDB_list_first/next` operations out of the mixed ordered-query facade.
   - Kept the prefix predicate `static` and local to the new translation unit. The three public API declarations already exist in `SkipDB.h`; no new cross-unit extern surface was introduced.
2. **Ordered-query facade — `skipdb/SkipDBQuery.c`**
   - Retained record count, first/last/lower-bound navigation, cursor registration/removal, database merge, existence lookup, and maximum-position reporting.
   - Removed the now-unneeded string/stdlib/assert includes along with the extracted code.
3. **Regression/build contracts — `tests/skipdb_prefix.c`, `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Added host coverage for ordered prefix cursor iteration, strict prefix-boundary termination, callback scans, empty-string prefix behavior, and no-match cursor results.
   - Registered the production module in the `skipdb` static library and a dedicated `skipdb_prefix_test` CTest.
   - Added CI source contracts for implementation ownership, existing public declarations, private predicate visibility, source registration, and test linkage.

### Validation
- Static source-contract preflight checks that all three moved public functions have one implementation owner, the private helper remains static, and the module/test are registered in CMake and CI.
- The first remote regression attempt stopped in the source-contract step because its `SkipDB.h` declaration needle used different pointer spacing than the actual public prototype; the contract now matches the header verbatim. CMake/build/CTest had not started on that attempt.
- No local CMake configure, compiler, or CTest run is claimed. The remote SkipDBv2 Regression workflow is the authoritative post-push validation.

### Follow-up
- Keep prefix semantics and prefix-bound iteration in `SkipDBPrefix.c`; keep general navigation and cursor registration in `SkipDBQuery.c` and cursor stepping/lifetime in `SkipDBCursor.c`.

---

## 2026-11-05 21:48 CST — Isolate skipd script execution and secure log ownership

### Baseline and regression state
- Base commit: `e514439b8e9a8452b36d55f5f174bba30ea77c38` (`fix(ipc): link request formatter in protocol fixtures`).
- The latest mainline tree includes the IPC request formatter link repair. The host-side SkipDBv2 Regression remains the post-push validation gate; no local CMake/compiler/CTest result is claimed.

### Changes
1. **Script execution module — `mgr/skipd_script.c`**
   - Moved secure `/tmp/skipd.log` creation/verification, persisted script-name validation, shell-command path validation, and script launch out of the libev event-loop facade.
   - Made the script log descriptor private to this module and replaced use of the daemon's shared scratch buffer with a bounded local command buffer.
   - Added explicit script-log initialization and cleanup entry points; initialization failure retains the existing fail-closed `/dev/null` output behavior.
2. **Daemon lifecycle — `mgr/skipd.c`, `mgr/skipd_internal.h`**
   - Kept signal handling, event-loop ownership, client lifecycle, and server startup in `skipd.c`.
   - Declared the script module's init/cleanup and validation/launch entry points in the private manager contract; removed the exported log-descriptor state.
   - Ensured the script log descriptor is closed on normal shutdown and both post-initialization startup-failure exits.
3. **Regression/build contracts — `tests/skipd_script.c`, `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Added host tests for safe/unsafe script names, path components, shell metacharacters, and rejection before execution.
   - Registered the production script module in the `skipd` executable and a standalone host regression target.
   - Moved the secure-log source contract to the new implementation owner and added ownership, declaration, CMake-link, and test-registration assertions.

### Validation
- Static migration audit checks that the moved functions and secure descriptor have one owner, all cross-translation-unit entry points are declared, and the new source/test are linked and registered.
- The test cases exercise traversal, repeated separators, shell metacharacters, whitespace, malformed termination, and valid script names/paths.
- No local CMake/compiler/CTest run is claimed. The pushed revision must pass the complete remote SkipDBv2 Regression workflow.

---

## 2026-11-05 21:24 CST — Repair IPC request formatter regression links

### Baseline and failure
- Base commit: `624a260d25a646a305b653204edb06a6a92ef551` (`refactor(ipc): centralize request frame serialization`).
- SkipDBv2 Regression run #163 (`42034645359`) passed the firmware integration contracts and CMake configure, then failed while linking `dbus_protocol_test` and `dbapi_protocol_test`: both fixtures compile their CLI implementation but did not link the newly extracted `mgr/ipc_request.c` symbols.

### Changes
1. **Host regression target linkage — `CMakeLists.txt`**
   - Added `mgr/ipc_request.c` to both protocol fixture executables, matching the source already linked by the production DBus/DBAPI executables.
2. **CI source contracts — `.github/workflows/skipdbv2-regression.yml`**
   - Raised the formatter source-registration expectation from three to five targets.
   - Added explicit per-target assertions that both protocol fixtures link the shared formatter; the standalone formatter test remains registered.

### Validation
- In-memory structural checks confirm exactly five CMake registrations and the source-contract text checks both fixture target blocks.
- This is a link-only integration correction; no production C implementation or protocol behavior changed.
- No local compiler/CMake/CTest result is claimed. The pushed revision must pass the complete remote SkipDBv2 Regression workflow.

---

## 2026-11-05 20:59 CST — Centralize IPC request-frame serialization

### Baseline and regression state
- Base commit: `50a9bacaa95878fe00afbf8448c768df177c873f` (`refactor(dbapi): isolate response result handling`).
- The host-side SkipDBv2 Regression remains the required post-push gate. No local CMake/compiler/CTest result is claimed.

### Changes
1. **Shared request-frame module — `mgr/ipc_request.c`, `mgr/ipc_request.h`**
   - Centralized printf-style body formatting and the common `magicv1 ` + seven-digit body length + separator header.
   - Added a finalizer for DBus's incrementally assembled request bodies; it validates the newline terminator, seven-digit length bound, capacity including the final NUL, and integer return range before moving the body behind the header.
2. **CLI callers — `mgr/dbus.c`, `mgr/dbapi.c`**
   - Routed ordinary DBus/DBAPI request frames through the shared formatter and retained DBus's incremental environment/prefix-set assembly through the shared finalizer.
   - Kept command validation/routing, socket lifecycle, writes, response parsing, and JSON/shell output in their existing owners.
3. **Regression — `tests/ipc_request.c`**
   - Added standalone coverage for exact frame bytes, an empty list prefix, incremental body finalization, invalid input, and formatted-length mismatch.
4. **Build and CI contracts — `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Linked the formatter into both CLI executables, registered its host regression target, and added source ownership/delegation/link-count assertions.

### Validation
- Static migration audit confirms both CLIs use the shared request formatter, no CLI retains the duplicated seven-digit header format, and DBus's incremental request bodies pass through the shared finalizer.
- Both cross-translation-unit entry points are declared in `ipc_request.h`; implementation-local formatting state remains private to `ipc_request.c`.
- No local compiler or CTest run is claimed. The pushed commit must pass the complete SkipDBv2 Regression workflow.

### Follow-up
- Keep request header/body framing in `ipc_request.c`; keep command semantics and request-field validation in `dbus.c` and `dbapi.c`. Response envelope/result handling remain in their current result modules.

---

## 2026-11-05 20:42 CST — Extract DBAPI response processing from CLI facade

### Baseline and regression state
- Base commit: `ee3706212e901f2d035ba89ff4bd1da8e3b6c74e` (`refactor(dbus): isolate response result handling`).
- The combined status endpoint returned no status records for this baseline; no CI pass/fail is inferred. The pull-request run of SkipDBv2 Regression is the pre-merge validation gate.

### Changes
1. **DBAPI response-result module — `mgr/dbapi_result.c`, `mgr/dbapi_internal.h`**
   - Extracted bounded response-frame reads, write acknowledgement/error matching, list-frame validation, terminal/error frame handling, Datum terminator validation, and callback dispatch from the CLI translation unit.
   - Centralized the private `dbclient` layout and `fn_db_parse` callback type in one internal header. Declared the buffer-growth bridge and both cross-unit result entry points; frame reader remains `static`.
2. **CLI facade — `mgr/dbapi.c`**
   - Retained UNIX socket startup, nonblocking setup, request-field validation, request serialization/writes, buffer ownership and allocation, lifecycle cleanup, and JSON output.
   - Exposes only `dbapi_check_buf()` for shared buffer growth and delegates the write response and list response paths to the result module.
3. **Protocol regression — `tests/dbapi_protocol.c`**
   - Kept the existing socketpair coverage for list values equal to the terminal payload, empty values, explicit terminal frames, list errors, write success/error responses, and invalid request fields; redirected direct parser calls to the declared result-module entry point.
4. **Build and CI contracts — `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Linked `dbapi_result.c` into both the DBAPI executable and its protocol regression target.
   - Updated source ownership contracts to require the private declarations and parser implementation in their new owners, prevent result parsing from drifting back into `dbapi.c`, and assert both CMake links.

### Validation
- Static extraction audit confirms each moved response function has one source owner, all cross-translation-unit functions are declared, and both DBAPI targets include the new translation unit.
- Existing DBAPI protocol test cases remain enabled; the workflow still configures, builds every host target, and runs the complete CTest suite.
- No local compiler/CMake/CTest run is claimed. The PR regression workflow must pass before the squash commit is applied to `main`.

### Follow-up
- Keep request framing, command/key validation, socket setup, writes, buffer ownership, and JSON rendering in `dbapi.c`.
- Keep response envelope validation, write acknowledgements, list streaming, and callback dispatch in `dbapi_result.c`; keep private cross-unit declarations in `dbapi_internal.h`.

---

## 2026-11-05 20:19 CST — Extract DBus response processing from CLI facade

### Baseline and regression state
- Base commit: `70f0e9df2b3ed03c76dd0fcac951e17d451c7acf` (`fix(ipc): resolve parser header in standalone test`).
- Latest SkipDBv2 Regression run #151 (`42022572550`) passed on the baseline commit. This is the pre-change result; the new commit relies on the same workflow for post-push validation.

### Changes
1. **DBus response-result module — `mgr/dbus_result.c`, `mgr/dbus_internal.h`**
   - Moved length-delimited response-frame validation, common/get result handling, list-frame splitting and streaming, shell identifier validation, quoting, and script export rendering out of the CLI translation unit.
   - Moved the private `dbclient` layout into one internal header. Declared the cross-unit `dbus_read_util()` helper and all four result-parser entry points there; frame-specific helpers remain `static`.
2. **CLI facade — `mgr/dbus.c`**
   - Retained socket connection, nonblocking setup, buffer ownership/allocation, command validation, request framing, command routing, and writes.
   - Kept the transport read helper as the sole bounded-buffer owner and exposed only its narrow internal entry point to the result module.
3. **Build and CI contracts — `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Linked `dbus_result.c` into the DBus executable and the existing standalone DBus protocol regression target.
   - Updated source ownership contracts for the CLI, internal header, result module, and CMake source list; added explicit checks preventing result-parser ownership from drifting back into `dbus.c`.

### Validation
- Static extraction audit confirms each moved parser/helper body has one owner, the shared client layout is defined once, every cross-translation-unit function used by the result module is declared, and CMake links the result module in both required targets.
- Existing `dbus_protocol_test` cases remain enabled, including binary get payloads, server error frames, malformed envelope/command cases, list terminal-frame collision, and shell exports with empty values.
- No local compiler/CMake/CTest run is claimed. The pushed revision triggers the unchanged SkipDBv2 Regression workflow.

### Follow-up
- Keep command argument validation, request construction, socket setup, buffer ownership, and request writes in `dbus.c`.
- Keep response envelope validation, list/script stream handling, and terminal/export formatting in `dbus_result.c`; keep cross-unit declarations in `dbus_internal.h`.

---

## 2026-11-05 19:31 CST — Centralize SkipDB IPC response-envelope parsing

### Baseline and regression state
- Base commit: \`47e63934c960a96d30714bec1132037b22ca8dfa\` (\`fix: commit index bootstrap and preserve delete predecessor\`).
- The SkipDBv2 host regression remains the validation gate. The baseline commit had no combined status checks reported by the repository status endpoint; no CI result is inferred from that empty response.

### Changes
1. **Shared IPC envelope parser — \`mgr/ipc_protocol.c\`, \`mgr/ipc_protocol.h\`**
   - Added one bounded parser for the common command/key/payload response envelope used by both CLI clients.
   - Returns non-owning slices with explicit lengths, rejects missing separators, empty command/key tokens, and embedded NUL bytes in command/key tokens, while preserving arbitrary binary payload bytes including embedded NUL.
2. **dbus response/list parsing — \`mgr/dbus.c\`**
   - Routed ordinary response envelopes and list/script frames through the shared parser.
   - Kept command matching, dedicated \`__error__\`/\`__end__\` handling, raw Datum output, shell identifier validation, and shell quoting in the dbus caller.
3. **dbapi response/list parsing — \`mgr/dbapi.c\`**
   - Reused the same envelope split for write acknowledgements and callback-based list results.
   - Preserved dbapi's stricter string-Datum terminator rules and callback/error semantics.
4. **Regression and build contracts — \`tests/ipc_protocol.c\`, \`CMakeLists.txt\`, \`.github/workflows/skipdbv2-regression.yml\`**
   - Added isolated parser coverage for binary payloads, empty values, terminal frames, malformed separators, and NUL-bearing command/key tokens.
   - Linked the new module into both CLI executables and both existing protocol harnesses, and registered a dedicated CTest.
   - Extended source contracts to verify declarations, implementation ownership, client delegation, all five CMake source links, and the regression test registration.

### Validation
- Static source audit confirms both clients call the same parser, while command-specific output and callback policies remain in their original callers.
- The CMake manifest has explicit links for dbus, dbapi, dbus protocol tests, dbapi protocol tests, and the standalone IPC parser test.
- No local CMake/compiler/CTest run is claimed. The pushed revision triggers the unchanged SkipDBv2 Regression workflow.

### Follow-up
- Keep wire-envelope tokenization in \`ipc_protocol.c\`; do not move command-specific response policies or text rendering back into the shared transport.
- Keep socket readiness, deadlines, exact I/O and buffer allocation in \`ipc_transport.c\`.
- Keep dbus shell/export and raw-value presentation behavior in \`dbus.c\`; keep dbapi callback and string-Datum validation in \`dbapi.c\`.

---

## 2026-11-05 18:20 CST — Split skipd runtime engines and repair UDB record symbol ownership

### Baseline and regression state
- Base commit: `e230e7eb4bc878f2dcf547c6077dcd5916932044` (`refactor(udb): isolate lifecycle, recovery and transaction engines`).
- Latest `SkipDBv2 Regression` run #144 (`42007982499`) passed XY-C1 integration contracts and CMake configure, then failed while linking `udb_reserved_pid_test` and `skipd`: `UDBRecord_isEmpty` had duplicate external definitions in `UDBRecord.c` and `UDBRecordWrite.c`. The build stopped before CTest.
- The host regression remains the automatic validation gate. The full target build and full CTest suite remain enabled; no firmware workflow or test is bypassed.

### Changes
1. **Single UDB record predicate owner — `udb/UDBRecord.c`, `udb/UDBRecordWrite.c`**
   - Kept `UDBRecord_isEmpty` with record metadata and removed the duplicate write-module definition. The public declaration and behavior are unchanged.
2. **Shared skipd implementation contract — `mgr/skipd_internal.h`**
   - Moved private server/client/timer structures and cross-module prototypes into one internal header. The public `skipd.h` protocol constants and client-facing API are unchanged.
3. **Command and coroutine engine — `mgr/skipd_command.c`**
   - Extracted request parsing, command dispatch, and coroutine processing. The event-loop facade retains socket readiness callbacks and response-buffer ownership; shared send/read helpers are exposed only through the private header.
4. **Database lifecycle and switch engine — `mgr/skipd_database.c`**
   - Isolated durable active-database marker read/write, startup/open, integrity gate, and copy-on-switch. The successful switch still commits the replacement database before atomically switching the marker and retiring the old database.
5. **Scheduler and synchronization engine — `mgr/skipd_scheduler.c`**
   - Extracted persisted delay/time restoration, timer callbacks, periodic sync and deferred database switch checks. Existing command formats and timer semantics are retained.
6. **UNIX socket and instance-lock owner — `mgr/skipd_socket.c`**
   - Isolated nonblocking descriptor setup, stale-socket liveness checks, socket creation and advisory lock ownership. Added a narrow lock-release function so database startup no longer reaches into socket-module static state.
7. **Build and CI contracts — `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Registered each skipd engine explicitly and added source-boundary checks for ownership, private contracts, CMake registration and the single UDB predicate owner. The complete host build and CTest remain intact.

### Validation
- Static extraction audit confirms command/coroutine, scheduler, database-switch and UNIX-socket definitions each have one source owner, with the shared cross-module contract declared centrally.
- The #144 duplicate-symbol link blocker is removed at its owning source; the workflow now checks the removal structurally as well as compiling/linking the complete skipd target.
- No local compiler/CTest result is claimed. The pushed commit triggers the unchanged host regression pipeline.

### Follow-up
- Keep the event-loop facade, client allocation, accept callback, CLI and process entry in `mgr/skipd.c`.
- Keep parsing/coroutines in `skipd_command.c`, persisted timers/sync in `skipd_scheduler.c`, database marker/switch protocol in `skipd_database.c`, and socket/lock state in `skipd_socket.c`.
- Keep `UDBRecord_isEmpty` implemented only in `UDBRecord.c`; do not reintroduce implementation ownership in the write layer.

---

## 2026-11-05 18:01 CST — Decompose UDB lifecycle, dual-file recovery, and transaction engines

### Baseline and regression state
- Base commit: `1351999ea6d734ec64522ca8f33aa5cac6a59adc` (`refactor(udb-record): separate metadata, persistence, reads, and compaction`).
- Latest `SkipDBv2 Regression` run `41999798454` (#143) passed XY-C1 integration contracts and CMake configure, then failed at 57% while compiling `udb/UDBRecordRead.c`: `UDBRecord_data` was implemented but absent from the shared `UDBRecord.h` declaration surface. CTest was skipped.
- `SkipDBv2 Regression` remains the automatic validation gate. The full host build and full CTest suite remain enabled; firmware workflows remain manual-only.

### Changes
1. **Lifecycle and path ownership — `udb/UDBLifecycle.c`**
   - Extracted UDB construction/destruction, database artifact cleanup, main/index/data/journal/marker path derivation, log-path configuration, path access, open-state inspection, and close cleanup.
   - Kept path replacement atomic with respect to allocation and commit-marker path validation; existing suffixes and default paths are unchanged.
2. **Two-file startup and crash recovery — `udb/UDBRecovery.c`**
   - Isolated the coordinated index/data startup state machine and promoted the shared stream-health predicate to the private implementation contract.
   - Preserved missing-marker refusal when artifacts exist, first-open marker initialization, validation of both journals before replay, deferred replay, replay-both-before-disarm ordering, journal cleanup, and parent-directory synchronization.
3. **Transaction engine — `udb/UDBTransaction.c`**
   - Isolated transaction admission, pre-commit flush/prepare, durable recovery-marker arming, index/data journal application, marker disarming, and post-commit clipping.
   - Preserved the exact prepare → arm → apply index → apply data → disarm → clip sequence and fail-closed behavior.
4. **Record ownership API and compile boundary — `udb/UDBRecord.h`, `udb/UDBRecordRead.c`, `udb/UDBInternal.h`**
   - Declared the existing lazy `UDBRecord_data` accessor where all record translation units can see it, fixing the #143 implicit-declaration failure.
   - Made the mutable owned payload conversion explicit at the `Datum` boundary and declared the cross-unit stream-health contract.
5. **Build and regression contracts — `CMakeLists.txt`, `tests/udb_transaction_lifecycle.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Replaced the monolithic storage source with three explicit implementation units and added lifecycle/transaction regression coverage for path derivation, idempotent open/begin, append/commit, and close/reopen persistence.
   - Updated source ownership, private contract, source-manifest, and CTest registration assertions. No build or test stage was disabled.

### Validation
- Static extraction audit partitions all UDBStorage functions into lifecycle, recovery, and transaction owners without changing their bodies or public API; no old storage implementation remains in the source manifest.
- The #143 compiler blocker is repaired at the declaration boundary; the mutable owned-data cast is explicit.
- No local CMake/compiler/CTest result is claimed. The pushed commit triggers the unchanged full host build and complete CTest suite.

### Follow-up
- Keep UDB construction, paths, destruction, and close in `UDBLifecycle.c`.
- Keep startup marker checks, journal validation/replay, and directory durability in `UDBRecovery.c`.
- Keep transaction prepare/apply/disarm ordering in `UDBTransaction.c`; any ordering change must preserve dual-file crash recovery coverage.
- Keep lazy UDBRecord byte-array ownership declared by `UDBRecord.h`; do not reintroduce implicit cross-translation-unit calls.

---

## 2026-11-05 17:00 CST — Separate UDB record metadata, persistence, validation, and relocation

### Baseline and regression state
- Base commit: `90a117d79069447bef244dcbcb93be6d417a0cf1` (`refactor(skipdb): split record metadata, cache, links, and storage engines`).
- Latest `SkipDBv2 Regression` run `41997198592` (#141) passed XY-C1 integration contracts and CMake configure, then failed while compiling `tests/skipdb_core_layers.c`: the test used `SkipDB_list_first` and `SkipDB_list_next`, which were implemented but missing from the public `SkipDB.h` declarations. CTest did not run.
- The latest regression workflow remains the sole automatic validation gate. No host build/test stage is disabled, and the manual-only firmware build/validation workflows are unchanged.

### Changes
1. **Public prefix-cursor contract — `skipdb/SkipDB.h`**
   - Declared the existing prefix first/next cursor functions with the same signatures as their implementations. This repairs the real compile failure without weakening the prefix traversal regression.
2. **UDB record ownership and metadata — `udb/UDBRecord.c`**
   - Retained allocation/clone/free, lazy byte-array ownership, JFile/position/PID/size accessors, empty-tag inspection, total-size calculation, and diagnostics.
3. **Record write and tombstone engine — `udb/UDBRecordWrite.c`**
   - Isolated tombstone removal, header writes, append serialization, and the journal/non-journal rollback helper. Existing native header bytes, append-only behavior, and rollback rules are preserved.
4. **Bounded read engine — `udb/UDBRecordRead.c`**
   - Isolated record-header validation and binary payload loading, preserving tag/PID/size validation, file bounds, and stream-position restoration.
5. **Compaction relocation engine — `udb/UDBRecordMove.c`**
   - Isolated full-record buffered relocation with source and destination bounds checks.
6. **Build, regression, and source contracts — `CMakeLists.txt`, `tests/udb_record_storage.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Registered the three implementation units and a focused binary payload/header/tombstone regression covering embedded NUL and high-bit bytes, invalid writes, out-of-range relocation, and truncated headers.
   - Added workflow assertions for module ownership, source registration, test registration, and the public prefix-cursor declaration.

### Validation
- Static extraction audit confirms the persistence/read/relocation definitions have one implementation owner each and are absent from the metadata facade.
- The known #141 compile blocker is directly addressed by declaring the already-implemented prefix cursor API; the existing regression test is retained.
- No local CMake/compiler/CTest run is claimed. The pushed revision triggers the unchanged full host build and complete CTest suite.

### Follow-up
- Keep object ownership and metadata accessors in `UDBRecord.c`.
- Keep header writes, append serialization, tombstone changes, and rollback in `UDBRecordWrite.c`.
- Keep malformed-header bounds checks and payload reads in `UDBRecordRead.c`.
- Keep physical record relocation in `UDBRecordMove.c`; preserve the native `UDBRecordHeader` layout and on-disk byte representation.

---

## 2026-11-05 16:47 CST — Split SkipDB record lifetime, cache links, traversal, and persistence

### Baseline and regression state
- Base commit: `37ed202c94521550176dc2eeee66f1f0adeafd69` (`docs: add Beijing commit times to dev log headings`); current work is based on the latest `main` head.
- Latest `SkipDBv2 Regression` run `41994089069` (#139), on parent implementation `52ff74f0e63f15e984ca30f882e5216ef1bd992e`, passed XY-C1 source contracts and CMake configure, then failed compiling `SkipDB.c`: `SkipDB_dealloc` was called before a private declaration. The build stopped before CTest. Earlier run `41992385670` (#138) passed on the preceding UDB-index baseline.
- Preliminary host run `41996717318` (#140) on candidate `64eb81b0ee0c2892c9a30b30f42c08b266ba6326` reached compilation and exposed translation-unit include leaks: the cache unit lacked the SkipDB API/constants context, and the storage unit needed an explicit private declaration for the suffix setter. Both boundaries are repaired in this replacement tree; an unused static pointer accessor was removed.
- The host regression remains the only automatic validation loop. Firmware validation remains manual-only; no regression stage or test is disabled.

### Changes
1. **Private lifecycle contract and baseline compile repair — `skipdb/SkipDBInternal.h`**
   - Declared `SkipDB_dealloc` in the private implementation contract before `SkipDB_new` uses it, eliminating the implicit-declaration error without changing public API or object lifetime behavior.
2. **Record core ownership — `skipdb/SkipDBRecord.c`**
   - Retained record allocation/destruction, database/PID ownership, dirty state, key/value access, level-array allocation, and user-object accessors.
   - Kept the debug-only freed-record tracker with the deallocator that owns its state.
3. **Cache metadata — `skipdb/SkipDBRecordCache.c`**
   - Isolated age-list linking/unlinking, pointer emptiness inspection, and stale-cache reference detachment before eviction.
4. **Skip-list links and traversal — `skipdb/SkipDBRecordLinks.c`**
   - Moved validated pointer-slot access, PID/prefix linkage, lazy record resolution, bounded forward search, last-record traversal, unlink repair, reverse predecessor reconstruction, and diagnostics into one link engine.
   - Promoted only the slot accessor, PID materializer, and suffix-aware PID setter to private cross-unit helpers in `SkipDBRecordInternal.h`; public record structures and API remain unchanged.
5. **Tagged record persistence — `skipdb/SkipDBRecordStorage.c`**
   - Isolated tagged serialization, strict bounded deserialization, and UDB persistence. Existing key/value encoding, level/PID/suffix order, signed-PID compatibility, and trailing-byte rejection are preserved.
6. **Build and regression contracts — `CMakeLists.txt`, `tests/skipdb_record_storage.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Registered the three new implementation units and a round-trip regression using embedded NUL/high-bit bytes, skip-list PID/suffix links, truncated input, trailing data, and invalid tags.
   - Extended CI source-boundary assertions and retained the full host build plus complete CTest invocation.

### Validation
- Structural extraction audit: all 57 original `SkipDBRecord.c` definitions were classified: 56 retained implementations are assigned once across the core, cache, links, and storage units, and the one unreferenced static pointer accessor was removed. All cross-unit helpers are declared in the private header.
- The prior compiler failure is directly addressed. No local compiler/CTest execution is claimed; the pushed revision triggers the unchanged full host regression workflow.
- No persistent UDB/JFile/SkipDB binary representation, firmware trigger, test, or CI validation stage is removed or bypassed.

### Follow-up
- Keep record allocation, ownership, dirty state, and user object metadata in `SkipDBRecord.c`.
- Keep age links and cache-reference cleanup in `SkipDBRecordCache.c`.
- Keep all skip-list pointer and predecessor semantics in `SkipDBRecordLinks.c`.
- Keep BStream wire-format and UDB writes in `SkipDBRecordStorage.c`; any future format change must retain round-trip and malformed-stream coverage.

---

## 2026-11-05 16:23 CST — Split SkipDB core lifecycle, cache, transactions, search, writes, and queries

### Baseline and regression state
- Base commit: `25271f1f0d49c975088b808b5693817fc3283655` (`refactor(udb): split index lifecycle, free-list, and mapping layers`); `main` is the only branch.
- Baseline `SkipDBv2 Regression` run `41992385670` (#138) completed successfully. Integration contracts, CMake configure, all SkipDBv2 targets, and the full CTest suite all passed.
- The preceding #137 contract-only failure is corrected at the baseline. Host regression remains the fast feedback loop; firmware workflows remain manual-only.

### Changes
1. **Lifecycle facade — `skipdb/SkipDB.c`**
   - Reduced the monolith to construction/destruction, path and database lifecycle, accessors, object callbacks, and diagnostics. Preserved the exposed `SkipDB` structure and public signatures.
2. **Cache ownership — `skipdb/SkipDBCache.c`**
   - Moved age notifications, dirty/PID bookkeeping, watermarks, cursor-aware eviction, and cache clearing. Preserved cursor marking and reference invalidation before eviction.
3. **Transaction synchronization — `skipdb/SkipDBTransaction.c`**
   - Isolated dirty-row persistence, pending PID removal, commit synchronization, and root-record recovery, retaining root PID reservation and fail-closed checks.
4. **Search and mutation engines — `skipdb/SkipDBSearch.c`, `skipdb/SkipDBWrite.c`**
   - Separated skip-list level/search-path construction, PID lookup and reads from insert/update/unlink mutation logic. Preserved link ordering, dirty propagation, removal handling, and persistence layout.
5. **Ordered query surface — `skipdb/SkipDBQuery.c`**
   - Isolated bounded traversal/count, first/last/lower-bound lookup, cursor registration, prefix scans, existence, merge, and max-position reporting.
6. **Private contracts, build, and regression — `skipdb/SkipDBInternal.h`, `CMakeLists.txt`, `tests/skipdb_core_layers.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Added narrowly scoped private cross-unit declarations, replaced SkipDB's recursive source glob with an explicit eight-source manifest, and registered integration coverage for CRUD, prefix traversal, cache eviction/reload, updates/removals, and merge.
   - Added CI source-boundary and test-registration assertions. No workflow stage or test was disabled.

### Validation
- Static extraction audit: all 62 original SkipDB function definitions are assigned exactly once across six implementation units; no definition was duplicated or omitted.
- Build manifest, CTest registration, and CI contract anchors were checked before publishing.
- No local compiler/CTest run is claimed; the pushed revision triggers the complete host `SkipDBv2 Regression`.
- The manual-only XY-C1 firmware workflow and on-disk UDB/JFile/SkipDB representation are unchanged.

### Follow-up
- Keep object lifetime/path setup and accessors in `SkipDB.c`; cache lifetime/eviction in `SkipDBCache.c`.
- Keep persistence ordering in `SkipDBTransaction.c`, predecessor search in `SkipDBSearch.c`, mutation in `SkipDBWrite.c`, and ordered traversal/prefix/merge in `SkipDBQuery.c`.

---

## 2026-11-05 16:09 CST — Separate UDBIndex lifecycle, free-PID pool, and mapping layers

### Baseline and regression state
- Base commit: `6732021b6172178b3d196546b2b272d544beeee8` (`refactor(jfile): unify journal scanning and durable recovery`); main is the only branch.
- Latest `SkipDBv2 Regression` run `41987436381` (#137) failed in the integration-contract step before CMake. The checker still expected the old inline `sizeof(long) + sizeof(size_t) + 1` expression although the scanner now names its header size and adds the terminator separately.
- The previous host regression run `41985451059` reached CTest and reported 8/9 passing, with `jfile_journal_test` exposing the torn-tail replay issue now addressed by the JFile scanner. The latest available XY-C1 firmware validation run `41985449861` succeeded; firmware validation remains manually dispatchable.

### Changes
1. **Index lifecycle facade — `udb/UDBIndex.c`, `udb/UDBIndexInternal.h`**
   - Reduced the public implementation unit to allocation/destruction, path configuration, open/recovery lifecycle, and transaction orchestration.
   - Added private validity, data-stream, and journal-stream predicates shared across the implementation units. Public `UDBIndex.h` declarations and the exposed structure remain unchanged.
2. **PID reservation/free-list engine — `udb/UDBIndexFreeList.c`**
   - Moved hole discovery, next/allocate PID selection, transaction reservation finalization, and reserved-PID validation out of the lifecycle facade.
   - Centralized reserve, release, forget, and duplicate-hole normalization so mapping changes no longer manipulate the free/reserved lists themselves.
3. **On-disk mapping layer — `udb/UDBIndexMap.c`**
   - Isolated bounded PID-entry reads/writes, offset validation, mapping count, and diagnostics.
   - Kept the native `UDBIndexEntry` layout and routed successful map mutations through the free-list ownership helpers.
4. **Build and regression contracts — `CMakeLists.txt`, `tests/udb_index_layers.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Registered both new translation units in the explicit UDB source manifest.
   - Added a focused integration regression for sentinel initialization, consecutive PID reservations, commit-time reservation retirement, hole reuse, and free-list reconstruction after reopen.
   - Corrected the stale JFile scanner contract and added source-boundary/build/test-registration assertions for all three index layers.

### Validation
- Static extraction review confirms the former `UDBIndex.c` responsibilities now have one implementation owner each; public call signatures and persisted index-entry structure are retained.
- The full host workflow remains enabled: integration contracts, CMake configure, all SkipDBv2 targets, and CTest. No host compiler/CTest execution is claimed from this editing session; the pushed commit triggers remote validation.
- The XY-C1 full firmware workflow remains manual-only; no firmware build trigger was disabled or test skipped.

### Follow-up
- Keep path/file lifecycle and transaction commit orchestration in `UDBIndex.c`.
- Keep PID allocation, pending reservations, and reusable-hole list mutation in `UDBIndexFreeList.c`.
- Keep persisted PID-to-record offset serialization and bounds checks in `UDBIndexMap.c`.

---

## 2026-11-05 15:28 CST — Unify JFile journal scanning and crash-tail recovery

### Baseline and regression state
- Base commit: `2a55f14707969b3a54fa820dd6a377184f9f41f1` (`Update xy-c1-firmware-validation.yml`).
- Latest `SkipDBv2 Regression` run `41985451059` built every target successfully, then reported 8/9 CTests passing. The sole failure was `jfile_journal_test`: startup replay rejected the committed prefix after a complete uncommitted record and a one-byte torn trailing header.
- The latest mainline contains only `main`. Automatic push/pull-request triggers for `XY-C1 Firmware Validation` had already been removed in the base commit; its manual `workflow_dispatch` trigger remains. The host regression workflow remains active.

### Changes
1. **Single journal boundary scanner — `jfile/JFileJournalScan.c`, `jfile/JFileInternal.h`**
   - Added a private scan result containing the physical log end, last structurally valid record end, last durable transaction boundary, and record counts.
   - Replaced stream-position-dependent tail discovery with bounded absolute-offset parsing. Partial headers, incomplete payloads, and invalid terminators are treated as discardable suffixes while previously committed transactions remain addressable.
   - Kept the journal's native `long + size_t + payload + terminator` layout and legacy zero-length records unchanged.
2. **Unified validation, clipping, and replay — `jfile/JFileJournal.c`**
   - Made commit-position lookup, full-log verification, startup clipping, and replay consume the same scan result.
   - Added a whole-log validation pass before replay so no record is applied until the committed boundary is proven complete.
   - Replay remains idempotent and retains recovery evidence when called by the UDB two-file recovery sequence.
3. **Durable physical truncation — `jfile/JFileSync.c`**
   - Centralized journal truncation, checked `off_t` conversion and descriptor validity, and fsynced truncation when full-sync mode is enabled.
   - Reused the helper for both incomplete-tail clipping and post-replay log clearing; data-file truncation now also syncs the resulting file length.
4. **Deterministic build and crash-recovery regression — `CMakeLists.txt`, `tests/jfile_journal.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Replaced JFile's recursive source glob with an explicit implementation manifest containing the lifecycle, journal, scanner, and sync translation units.
   - Extended the crash test to verify that the scanner preserves the prior committed offset, then closes the stdio streams and appends a durable one-byte tail through a separate append-only descriptor before reopening.
   - Strengthened workflow contracts for the scanner API, explicit build manifest, shared scan usage, durable truncate helper, and the regression fixture.

### Validation
- The first post-push run `41987216040` exposed a stale source-contract comparison string (`scan.logEnd != scan.committedEnd`) before CMake; the implementation correctly uses `scan.committedEnd != scan.logEnd`. Corrected the contract without weakening or skipping any test.
- The corrected `SkipDBv2 Regression` push run will execute configure, complete host builds, and all CTests. No local compiler or CTest result is claimed.
- XY-C1 automatic cross-build remains manually dispatchable and is not re-enabled for every iteration.

### Follow-up
- Keep all journal framing and durable commit-boundary decisions in `JFileJournalScan.c`.
- Keep replay and transaction orchestration in `JFileJournal.c`.
- Keep descriptor-level fsync and physical truncation in `JFileSync.c`; do not truncate logs directly from the scanner.
- Preserve the persistent journal byte format and the UDB marker's verify-both / replay-both / clear-marker ordering.

---

## 2026-11-05 15:11 CST — Split JFile persistence, journal replay, and durability layers

### Baseline and regression state
- Base commit: `1cdef91dccdad2f7a9506c08f7937bc4fa372868` (`refactor(udb): split record-store lifecycle, cursor, and free-list layers`).
- Baseline `SkipDBv2 Regression` run `41982709173` completed successfully: integration contracts, CMake configure, all SkipDBv2 targets, and CTest passed.
- Baseline XY-C1 firmware cross-build `41982709153` also completed successfully, including TRX verification. The branch contains only `main`.
- The regression workflow compiles the host-side implementation and runs CTest; the separate XY-C1 cross-build remains enabled.

### Changes
1. **Private persistence contracts — `jfile/JFileInternal.h`**
   - Centralized transaction terminator constants and private cross-translation-unit declarations.
   - Exposed the managed-file opener only to implementation units; the public `JFile.h` API and persisted record format remain unchanged.
2. **Journal engine — `jfile/JFileJournal.c`**
   - Moved journal parsing/replay, durable transaction completion, commit orchestration, log validation, last-commit scanning, and interrupted-tail clipping out of the original monolith.
   - Preserved the native journal layout, legacy zero-length record handling, terminator rules, replay order, and recovery-evidence retention.
3. **Durability and file maintenance — `jfile/JFileSync.c`**
   - Isolated fsync barriers, log commit-marker sync, managed journal reopen/truncation, and data-file truncation.
   - Kept file ownership and symlink checks in the single managed-file opener and reused it when the journal must be reopened.
4. **Lifecycle and call-site facade — `jfile/JFile.c`**
   - Left path configuration, secure file opening, close/delete, raw read/write, and logical cursor movement in the core translation unit.
   - Routed cross-module helpers through the private header without changing caller-visible behavior.
5. **Regression/build/CI integration — `tests/jfile_journal.c`, `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Added coverage for commit-before-replay, retained recovery evidence, repeatable replay, safe clipping, crash-style startup replay, and truncation of an interrupted trailing record.
   - Registered the test in the existing host regression CTest path and added source-boundary/build-registration contracts.
   - Kept the XY-C1 firmware cross-build enabled.

### Validation
- Structural extraction checks verify single ownership for every moved definition and preserve the lifecycle call sites in `JFile.c`.
- The new test is wired into the same `SKIPDB_BUILD_TESTS=ON` workflow as the existing UDB and IPC tests.
- No local compiler/CTest result is claimed. The commit triggers the host regression and XY-C1 firmware validation workflows.

### Follow-up
- Keep transaction format, parser, and replay behavior in `JFileJournal.c`.
- Keep fsync, durable journal terminators, safe clipping, and physical truncation in `JFileSync.c`.
- Keep file paths, secure open, and stream lifecycle in `JFile.c`.

---

# Development handoff log

## 2026-11-05 14:46 CST — Split UDB record-store lifecycle, cursor, and free-list layers

### Baseline and regression state
- Base commit: `2ec8f3f2996a319dc88a1c741b3f06d01baf5c7b` (`refactor(udb): split record read/write pipelines and formalize internal contracts`).
- `SkipDBv2 Regression` run `41981099079` compiled all targets; seven of eight CTests passed. `udb_reserved_pid_test` failed when it tried to remove `<database-path>.committed`, exposing a marker filename compatibility regression from the earlier marker extraction.
- The pre-extraction implementation constructed the durable marker filename as `<database-path>.committed`; preserve that persistent on-disk contract.

### Changes
1. **Marker-path compatibility — `udb/UDBStorage.c`, `tests/udb_reserved_pid.c`**
   - Restored the `.committed` suffix when deriving the marker path, while retaining all-or-nothing allocation and path replacement.
   - Added an explicit regression assertion that opening a fresh database creates the expected `.committed` marker before exercising missing-marker rejection.
2. **Lifecycle and startup validation — `udb/UDBRecords.c`, `udb/UDBRecordsStorage.c`**
   - Kept construction, ownership, path configuration, and deletion in the facade.
   - Moved file opening, first-empty metadata recovery, journal/transaction delegation, close, and truncation into a storage lifecycle translation unit.
3. **Physical cursor and free-list metadata — `udb/UDBRecordsCursor.c`, `udb/UDBRecordsFreeList.c`**
   - Isolated validated record-boundary traversal and record mutation helpers from persisted first-empty-offset management.
   - Preserved the mutable cursor object and shared `UDBRecords` layout; exported declarations remain in `UDBRecords.h`.
4. **Build and CI contracts — `udb/UDBRecords.h`, `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Added the three implementation layers to the explicit UDB source manifest.
   - Added source-level assertions for function ownership, manifest registration, and the durable marker filename convention.
   - Kept both `SkipDBv2 Regression` and `XY-C1 Firmware Validation` enabled; no validation job was bypassed.

### Validation
- Static split checks: each original UDBRecords function block is assigned to exactly one translation unit; the lifecycle, cursor, and free-list anchors were verified before publishing.
- The existing remote regression failure and the new marker-path assertion are documented above. No local compiler/CTest result is claimed; the pushed commit triggers the enabled remote workflows.

### Follow-up
- Keep record-file lifecycle and open-time recovery in `UDBRecordsStorage.c`.
- Keep validated record cursor movement/header checks in `UDBRecordsCursor.c`.
- Keep durable first-empty offset repair and free-list traversal in `UDBRecordsFreeList.c`.
- Keep database marker naming synchronized with the legacy `.committed` suffix.

---

## 2026-11-05 13:04 CST — SkipDB IPC transport deduplication

### Baseline
- Base commit: `7851a923b4032a2024d261553cb285c003d01d2e` (`fix: harden GPIO signal and event handling`).
- Scope: SkipDBv2 host-side Unix-domain-socket clients and their CTest registration.
- Firmware protocol framing, database on-disk representation, daemon lifecycle, and GPIO behavior are intentionally unchanged.

### Changes
1. **Shared monotonic I/O primitives — `mgr/ipc_io.h`**
   - Consolidated the identical `monotonic_milliseconds()` and `wait_socket_until()` implementations that had drifted in parallel inside `dbus.c` and `dbapi.c`.
   - Kept absolute `CLOCK_MONOTONIC` deadlines, checked millisecond conversion, `INT_MAX` poll-timeout clamping, and `EINTR` retry behavior in one implementation.
   - The wait helper continues to report readiness/timeout/error as `1/0/-1`; callers retain their existing command-specific timeout windows and error mapping.

2. **Client integration — `mgr/dbus.c`, `mgr/dbapi.c`**
   - Both clients now include the shared header and use the same helper names.
   - Removed duplicate helper bodies and redundant repeated system-header includes.
   - Preserved each client's protocol parser, frame limits, output behavior, and timeout policy.

3. **Regression coverage — `tests/ipc_io.c`, `CMakeLists.txt`**
   - Added a focused CTest executable for monotonic-clock argument validation, invalid descriptors, already-expired deadlines, idle-socket timeout, readable readiness, and writable readiness.
   - The readiness test verifies that polling does not consume or mutate the payload.
   - The existing `SKIPDB_BUILD_TESTS=ON` CI path now builds and runs this test automatically.

### Validation
- Source-level checks performed before commit: both removed helper bodies were byte-identical; both call sites were migrated; CMake test registration was added within the existing test option.
- Automated execution is delegated to the repository's `SkipDBv2 Regression` workflow after push. No local firmware build or local CTest run is claimed in this handoff.

### Follow-up notes
- Keep future socket timeout or readiness fixes in `mgr/ipc_io.h`; do not fork the helper into individual client files.
- This extraction deliberately does not merge higher-level client framing logic because the clients have different buffer and protocol responsibilities.

### CI contract maintenance
- The first post-push regression run exposed stale source-location assertions: the workflow still expected the monotonic clock and `poll()` implementation to live in each client source file.
- Updated the integration contract to assert the implementation in `mgr/ipc_io.h`, the inclusion/call sites in both clients, and registration/execution coverage for the new CTest. The wall-clock/busy-poll prohibition now also scans the shared helper.
- The initial contract-only failure occurred before CMake configure/build/CTest; the corrected workflow is included in the final single mainline commit so the actual build/test stages can run against the refactor.

- Header self-containment review: added `<stddef.h>` directly to `mgr/ipc_io.h` so `NULL` is defined when the helper is included by a translation unit that does not include another standard header first.


## 2026-11-05 13:33 CST — Extract UDB durable commit-marker subsystem

### Baseline
- Base: `93c6ede078ab684a2d43e4845d00d0e8b2d53081` (`refactor: centralize SkipDB IPC readiness handling`).
- Scope: move the one-byte UDB commit-marker lifecycle and filesystem validation out of transaction orchestration.
- Marker bytes, data/index journal formats, transaction ordering, and public UDB API remain unchanged.

### Changes
1. **Dedicated persistence component — `udb/UDBCommitMarker.h`, `udb/UDBCommitMarker.c`**
   - Encapsulated marker path/stream ownership, secure open/create, state parsing and durable writes, journal verification, parent-directory syncing, and artifact-state inspection.
   - Retained no-follow, nonblocking, regular-file, owner, inode-identity, and restrictive-mode checks at the descriptor boundary.
   - Made lifecycle explicit through set-path, open, close, and clear operations.

2. **Transaction coordinator integration — `udb/UDB.h`, `udb/UDB.c`**
   - Replaced separate marker path/stream fields with one `UDBCommitMarker` member.
   - Removed marker filesystem helpers and read/write routines from the UDB coordinator.
   - Kept multi-file recovery policy in UDB: validate both journals before replay, apply both while retaining recovery evidence, durably clear the marker only after both files are applied, then clip journals.
   - Preserved missing-marker rejection and first-open empty-artifact recovery behavior.

3. **Regression and integration contracts — `tests/commit_marker.c`, `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Added coverage for marker state transitions, invalid and empty marker files, path ownership, symlink rejection, empty/nonempty artifact classification, parent-directory syncing, and complete versus incomplete journals.
   - Registered the test in the existing `SKIPDB_BUILD_TESTS=ON` CTest path and added source-level CI assertions for the extracted module and coordinator wiring.

### Validation
- Structural checks: old marker fields and routines removed from `UDB.c`; marker state writes and the multi-file commit sequence remain in their original order.
- Remote CI is expected to run after push. No local firmware build or completed remote CI run is claimed.

### Follow-up
- Keep marker-file security and durability primitives inside `UDBCommitMarker.c`.
- Keep multi-file transaction decisions in `UDB_open` and `UDB_commitTransaction`; do not move transaction ordering into the marker component.


## 2026-11-05 13:46 CST — Split UDB storage lifecycle and compaction modules

### Baseline
- Base commit: fd99ecc7385cc64a6f3a5b3a7659d5c658e50bed (UDB commit-marker extraction).
- main and the two current codex branches were identical at this baseline; no branch-only work was dropped.
- gpt-audit/partition-parser-fix was 650 commits behind main and was not used as a base.

### Changes
1. Lifecycle and transaction coordinator — udb/UDBStorage.c
   - Moved construction/destruction, path configuration, open/recovery, close, transaction begin, and transaction commit into a dedicated storage-lifecycle translation unit.
   - Kept paired index/data recovery and durable marker transition ordering unchanged.
   - Preserved public declarations and the exposed UDB layout.
2. Record-space compaction — udb/UDBCompact.c
   - Isolated free-extent traversal, adjacent-hole merging, safe live-record relocation, and tail truncation from CRUD and transaction orchestration.
   - Kept existing size checks and transaction boundaries intact.
3. Core API and build manifest
   - Reduced UDB.c to PID operations and record CRUD, preserving append rollback, PID reservation checks, and read-error handling.
   - Replaced the recursive UDB source glob with an explicit seven-source manifest.
4. Regression/CI maintenance
   - Replaced the removed UDB_setCommittedFlag_ test-only call with UDBCommitMarker_write.
   - Corrected the regression workflow's whitespace-sensitive marker assertion.
   - Removed the duplicate CMakeLists.txt dictionary key and retained both IPC and commit-marker CTest contracts.
   - Added CI source contracts for the lifecycle and compaction module boundaries.

### Validation
- Structural validation: extracted function bodies are moved intact; all seven UDB translation units are listed in CMake.
- Existing udb_reserved_pid_test covers PID reservation, index-only crash recovery, compaction hole handling, adjacent extent merging, live-record relocation, and tail truncation.
- Latest baseline regression stopped before CMake/build/CTest on a formatting-sensitive assertion; the test source also contained a stale removed helper reference. Both are corrected here.
- No local firmware build or local CTest execution is claimed. Remote regression and XY-C1 workflows are triggered by this commit.

### Follow-up
- Keep transaction ordering and paired-file recovery policy in UDBStorage.c.
- Keep physical record-space mutations in UDBCompact.c.
- Keep marker bytes, filesystem security, and durability primitives in UDBCommitMarker.c.


## 2026-11-05 14:16 CST — Extract shared SkipDB IPC client transport

### Branch and CI baseline
- Base commit: `80729573d560fd93384a45734024762a9bbe5e07` (`refactor(skipdb): split UDB storage and compaction layers`).
- Remote branch inventory showed only `main`; no `codex/...` branch refs remained, so this work is based directly on current mainline without deleting or dropping branch-only commits.
- The latest SkipDBv2 Regression run stopped before configure/build/CTest because a source-contract assertion expected `UDBCommitMarker_write(&self->commitMarker,0)` while the implementation contains the equivalent call formatted with a space. Updated the assertion; the latest XY-C1 firmware validation had passed.

### Changes
1. **Shared transport layer — `mgr/ipc_transport.h`, `mgr/ipc_transport.c`**
   - Centralized Unix-domain socket connection, nonblocking descriptor setup, bounded buffer growth, protocol-token validation, and deadline-based exact reads/writes.
   - Preserved legacy read result codes, partial-read byte counts, EOF behavior, monotonic deadlines, EINTR retry, and MSG_NOSIGNAL writes.
2. **CLI integration — `mgr/dbus.c`, `mgr/dbapi.c`**
   - Replaced duplicated transport helper bodies with thin wrappers over the shared module.
   - Kept command envelopes, response parsing, per-client token limits, and timeout budgets in their original callers.
3. **Regression/build integration — `CMakeLists.txt`, `tests/ipc_transport.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Linked the implementation into both binaries and both existing protocol fixtures.
   - Added focused tests for Unix-socket path validation/connect, buffer growth/reuse, token validation, nonblocking setup, exact writes, successful reads, partial EOF, and timeout behavior.
   - Updated CI contracts for the new transport ownership boundary and added the module to the no-wall-clock/no-busy-poll scan.
   - Corrected the stale UDB marker assertion so the regression job can proceed to compile and execute CTest.

### Validation
- Pre-commit structural checks confirm that both CLI files delegate socket creation, nonblocking setup, buffer growth, token validation, exact reads, and exact writes to the shared transport module.
- No local compiler or CTest run is claimed. The pushed revision triggers SkipDBv2 Regression and XY-C1 firmware validation workflows.
- Follow-up: protocol framing and response parsing remain caller-owned; only transport mechanics belong in `ipc_transport.c`.

## 2026-11-05 14:32 CST — Separate UDB record read and write pipelines

### Baseline
- Base commit: `08ba960f5fcb2795ad4e103da099bb014bf70ecb` (`refactor(skipdb): share IPC client transport primitives`).
- Latest `SkipDBv2 Regression` run `41979358813` failed in `Build all SkipDBv2 targets`: `UDB.c` called `UDB_isInTransaction` without a declaration after lifecycle code had been split into `UDBStorage.c`. The integration-contract and CMake configure steps passed; CTest was skipped because compilation stopped.
- Only `main` was present in the branch inventory; no branch-only work was omitted.

### Changes
1. **Private cross-module contract — `udb/UDBInternal.h`**
   - Declares the transaction-state predicate implemented by `UDBStorage.c` and the validated PID-to-record lookup shared by read and write operations.
   - Keeps these implementation details out of the public `UDB.h` API surface.
2. **Read pipeline — `udb/UDBRead.c`**
   - Owns PID lookup validation, record/PID consistency checks, payload reading, and read-failure handling.
3. **Write pipeline — `udb/UDBWrite.c`**
   - Owns append rollback, record append/index mapping, transactional replacement, and record removal.
   - Preserves append ordering, reserved-PID validation, rollback behavior, and close-on-uncertain-write semantics.
4. **Facade/build/regression — `udb/UDB.c`, `udb/UDBStorage.c`, `CMakeLists.txt`, `tests/udb_record_api.c`, `.github/workflows/skipdbv2-regression.yml`**
   - Reduced `UDB.c` to PID allocation facade and diagnostics; wired the storage implementation to the private declarations.
   - Added both source files to the explicit UDB manifest and a focused CTest for transaction-only writes, invalid-datum rejection, insert/update/remove persistence, and reopen behavior.
   - Updated source-boundary assertions so the regression job follows the new ownership split instead of looking for moved functions in `UDB.c`.

### Validation
- Pre-push structural validation checks that extracted function bodies are preserved byte-for-byte and that the workflow contract needles match the resulting sources and test registration.
- Remote `SkipDBv2 Regression` is the authoritative compiler/CTest validation after push. No local C build or CTest run is claimed.
- The XY-C1 firmware workflow remains enabled; no CI job or firmware trigger was disabled.

### Follow-up
- Keep PID-to-record consistency and payload read failures in `UDBRead.c`.
- Keep journal append rollback and multi-step record mutations in `UDBWrite.c`; transaction ordering across both storage files remains in `UDBStorage.c`.


## 2026-11-05 22:13 CST — Extract skipd asynchronous client lifecycle

### Baseline
- Base commit: `f0ab31f34ea01c8cbeb4273aa9da67cec45316a8` (`refactor(skipd): isolate script execution and log lifecycle`).
- The latest `SkipDBv2 Regression` run (`42041114446`) failed before CMake configuration: the workflow read `cmake_text` before assigning it. The failure was in the Python source-contract script; no C compilation or CTest result was produced.
- Scope: skipd's accepted-client lifecycle, libev readiness callbacks, coroutine socket I/O, and response-frame writes. Command dispatch, database switching, scheduling, Unix-socket setup, and script execution remain outside this module.

### Changes
1. **Client connection subsystem — `mgr/skipd_client.c`**
   - Moved client allocation/cleanup, accept handling, read/write watchers, coroutine read/write loops, output buffering, and response-frame size validation out of the event-loop facade.
   - Kept internal event callbacks and write-coroutine state private to this translation unit.
   - Exported only the server accept callback and the existing send/read helpers consumed by the command dispatcher.
   - Preserved nonblocking behavior, EINTR/EAGAIN handling, partial-write offsets, MSG_NOSIGNAL, frame limits, cursor cleanup, and per-server in-flight accounting.

2. **Private interface and facade — `mgr/skipd_internal.h`, `mgr/skipd.c`**
   - Added the explicit accept-callback declaration and registered it from `main`.
   - Removed the moved implementation and its obsolete forward declaration from `skipd.c`; command coroutine and all request semantics remain unchanged.

3. **Build and regression contracts — `CMakeLists.txt`, `.github/workflows/skipdbv2-regression.yml`**
   - Added `skipd_client.c` to the production `skipd` executable source list.
   - Added source-path and ownership assertions for the extracted module and a check that the facade no longer owns client lifecycle functions.
   - Fixed the regression workflow's `cmake_text` initialization order so the existing script-safety CMake assertions execute instead of raising `NameError`.

### Validation
- Structural preflight: confirmed the moved client functions have a single implementation owner, all cross-module functions are declared in `skipd_internal.h`, the accept callback is wired from `main`, and CMake/CI source contracts include the new module.
- No local compiler, CMake configure, firmware build, or CTest result is claimed. Remote `SkipDBv2 Regression` is the authoritative build/test check after this commit.

### Follow-up
- Keep accepted-client ownership and event-driven socket I/O in `mgr/skipd_client.c`.
- Keep command framing semantics and persistence decisions in their current command/database modules; do not move command dispatch into the event-loop facade.
