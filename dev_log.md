# Development handoff log

## 2026-11-05 — SkipDB IPC transport deduplication

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


## 2026-11-05 — Extract UDB durable commit-marker subsystem

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
