## 2026-11-07 20:34 CST — Keep Date timezone refresh atomic on clock errors

- Baseline: `3a517037eb67ff816d75f7c7ac202f6936486e72`; main SkipDBv2 Regression run #660 passed the source contracts, full target build, and all CTests.
- Scope: `Date_setToLocalTimeZone()`.
- The helper passed `&self->tz` directly to `gettimeofday()` and ignored its return value. A failed system query could therefore leave the receiver with partially updated timezone metadata.
- Read into a local `struct timezone`, reject a failed query, and publish the complete zone only after success. Extend the existing Actions source contract to require the checked call and the final assignment.
- Existing C source and workflow only; no new headers, symbols, tests, or CMake targets. No local build is claimed; remote Actions is the validation gate.

---

## 2026-11-07 20:33 CST — Defer DynLib unload until init callbacks return

- Baseline: `b7f2ea43c11738302e5ae91e4a567f56e2ef6b98`; main SkipDBv2 Regression run #659 passed.
- Scope: the `DynLib_open()` / `DynLib_close()` initializer lifecycle.
- An init callback may receive its owning `DynLib` through `initArg` and call `DynLib_close()`. The old path unloaded the shared object while its initializer was still executing, so the callback could return into unmapped code.
- Track the active init callback and defer close requests until it returns; drain the request before `DynLib_open()` returns. Extend the existing reentrant fixture and lifecycle test to verify callback completion and the final closed state.
- Reuse existing source, fixture, CTest target, and workflow contracts. No new files or CMake targets. No local firmware build is claimed; remote Actions is the validation gate.

---

## 2026-11-07 20:16 CST — Match escaped-percent contract to the actual branch

- Failed head: `84a3053d3282d53f7ae7a78a3371eff101a898dd`; run #658 passed workflow-script parsing, then failed the static source contract before configuration or compilation.
- The formatter correctly introduces the escape handler as `if (*format == '%')`, followed by `else if` for the other directives. The new contract mistakenly expected the escape handler itself to be an `else if`.
- Correct the single source-token assertion. No production source or regression test changes.

---

## 2026-11-07 20:15 CST — Repair escaped-percent source-contract quoting

- Failed head: `4764f40d2af8748d41396c0fc84d67e15567e506`; run #657 failed while parsing the Python source-contract script, before any firmware contract check or C build.
- The added test-source needle had two backslashes before its embedded quotes; Python therefore terminated the string literal early. Reduce this to the single escape used by the surrounding contract entries.
- Workflow-only CI syntax repair. The UArray implementation and test changes are unchanged.

---

## 2026-11-07 20:14 CST — Complete UArray percent escaping and execute alias regression

- Baseline: `f8ecb05f3a7d8163139e8db2ae39414439b25196`; SkipDBv2 Regression run #656 passed all XY-C1 contracts, CMake configuration, the full target build, and all CTests.
- Scope: `UArray_fromVargs_()` in `basekit/UArray_format.c` and its existing `uarray_format_trailing_percent_test`.
- The formatter already preserves a trailing lone `%`, but a conventional escaped `%%` was discarded instead of emitting one literal percent. Add the explicit escape case.
- The existing format-alias regression had accidentally been placed inside the failure branch of the preceding ordinary-format assertion, so the successful path never executed it. Close that assertion before the alias case, then add a literal-percent regression.
- Reuse the current CTest target; extend the Actions source contract. No new files, headers, exported symbols, or CMake target changes. Validation is delegated to the current remote Actions run.

---

## 2026-11-07 20:07 CST — Align MainArgs replacement-order contract with sentinel cleanup

- Failed head: `3d147dd2fe54162b5e02b01b3cd7bb84dbe633de`; run #655 passed the static source/path inventory but failed the dynamic MainArgs replacement-order check before CMake.
- The checker still searched for the removed `oldArgc` parameter in the old-vector release call. Cleanup now derives its bound from the owned vector's NULL sentinel, so the new call is `MainArgs_freeArguments_(oldArgv);`.
- Update only the dynamic source-contract needle; retain its allocate → validate → copy → publish → release ordering assertion. No C source, test, or CMake changes.

---

## 2026-11-07 20:06 CST — Retarget MainArgs sentinel source contract

- Failed head: `8fd452a201548b0ca1bae935225a39b8b9aa7ab9`; SkipDBv2 Regression run #654 stopped in the XY-C1 source-contract stage before CMake configuration or compilation.
- The new contract expected a `while` loop, but the implementation uses a `for` loop with the same NULL-sentinel termination condition. The C source and regression logic were not at fault.
- Retarget the contract to the actual invariant token `NULL != argv[i]`. This is a workflow-only CI repair; no production code, tests, or build targets change.

---

## 2026-11-07 20:05 CST — Bound MainArgs teardown by its owned argv sentinel

- Baseline: `b329b69d538c28e579cfce18ceba9a9b6831b1ca`; latest mainline SkipDBv2 Regression run #653 passed.
- Scope: `MainArgs_freeArguments_()` and the existing `main_args_lifecycle_test`.
- The owned argument vector is zero-allocated with a terminating NULL slot, but cleanup trusted the public, caller-visible `argc`. A negative count leaked all copied strings; an oversized count could walk beyond the vector and free unrelated memory.
- Make the vector's NULL sentinel the sole cleanup boundary, including partial-copy rollback and replacement of an existing value. Extend the lifecycle test to corrupt `argc` to both -1 and `INT_MAX`, and compile it with the tracked allocator to verify all allocations are released.
- Reuse existing source, test, and CMake targets; update the existing Actions source contracts. No new files or exported symbols. No local compiler/CTest result is claimed; remote Actions is the validation gate.

---

## 2026-11-07 19:41 CST — Keep regression workflow under the Actions file-size boundary

- Baseline: `0b64cf993d17adba15e64a2c42d34a779cd055fe`; runs #650–#652 failed before creating any jobs or logs after the workflow contract additions.
- The workflow grew from 511,665 characters on green run #649 to 512,140 after the PHash static-contract update; the latest failed run had zero jobs/check records and reverted to a path-based workflow name. Treat this as a workflow-definition size/instantiation boundary rather than a C build failure.
- Remove the redundant dynamic SHash destructor body-order scan. Keep its static source/test needles, along with the PHash destructor source/test contract and both production regressions. The resulting workflow is 511535 characters, with no changes to C behavior or CMake targets.
- Re-run Actions; do not resume refactoring until the workflow instantiates and reaches the build/test stages.

---

## 2026-11-07 19:41 CST — Simplify PHash teardown workflow contract

- Baseline: `00e572c6f47e5717ce71669ece8546a05e659394`; run #651 terminated with no jobs, logs, or check records after the indentation correction.
- Keep the existing strict source-contract dictionary assertions for `PHash_free()` and the teardown regression, but remove the added dynamic body-order parser. This avoids extending the already-large Python contract script while preserving the explicit source and test needles; the production fix and regression remain unchanged.
- Trigger a fresh main workflow and do not resume refactoring until it completes.

---

## 2026-11-07 19:38 CST — Repair PHash teardown CI contract indentation

- Baseline: `11f9262d847e577da81387376a632b5e8f5284f5`; run #650 failed immediately after adding the PHash teardown contract.
- The dynamic workflow source-contract block inherited eight extra spaces on the following `copy_start` assignment, producing another Python `IndentationError`. Align that one statement with the surrounding block; no production C or regression behavior changed.
- Re-run Actions before resuming the next core-module pass.

---

## 2026-11-07 19:38 CST — Validate PHash backing storage before teardown

- Baseline: `aa6094a63f407a3202e93fbe4e741b5cd82df1d4`; main CI run #649 passed after the SHash teardown-contract repair.
- Scope: `PHash_free()` and the existing `phash_compact_test` lifecycle coverage.
- PHash already rejects misaligned backing storage in normal operations, but its destructor bypassed the validator and unconditionally freed `records`. Corrupted alignment metadata could still reach the allocator during teardown.
- Guard backing release with `PHash_hasValidStorage_()`, always release the PHash object, and add a teardown regression that retains and frees the genuine allocation separately after presenting a misaligned alias.
- Update existing static and dynamic Actions contracts; reuse the current CTest target without adding files or targets. No local compiler/CTest result is claimed.

---

## 2026-11-07 19:36 CST — Repair SHash teardown CI contract indentation

- Baseline: `12c6e9493175c90acfb928bbf7fc3172d445bb9e`; run #648 stopped in the Python-based firmware/source-contract step with an `IndentationError` before configuration, compilation, or CTests.
- Cause: the newly added `SHash_free()` source-contract block left the following compaction-contract statement over-indented. Correct that single line to the surrounding Python block indentation; no production C or test behavior changed.
- Re-run the main workflow to verify the strict source contract and all build/test stages.

---

## 2026-11-07 19:35 CST — Validate SHash backing storage before teardown

- Baseline: `7ab39ac305831b584354ef380914b3584ffb91f3`; main CI run #647 passed all source/integration contracts, the full build, and all 57 CTests.
- Scope: `SHash_free()` and its existing compaction/lifecycle regression target.
- The table operations now reject a misaligned backing pointer through `SHash_hasValidStorage_()`, but destruction bypassed that shared invariant and unconditionally called `io_free()` on the pointer. A corrupted table could therefore turn cleanup into an invalid allocator release.
- Gate backing release on the existing storage validator while always freeing the SHash object itself. Add a regression that corrupts only the backing pointer alignment, verifies teardown completes, and then explicitly releases the saved genuine allocation.
- Extend existing source and dynamic workflow contracts. No new files, declarations, or CMake targets; no local compiler/CTest result is claimed.

---

## 2026-11-07 19:32 CST — Reject misaligned SHash backing storage

- Baseline: `def32970755e938fa0c119eb2c69639dca95e3e7`; latest main CI run #646 passed the source/integration contracts, complete build, and all 57 CTests.
- Scope: the shared `SHash_hasValidStorage_()` predicate and existing `shash_compact_test` regression.
- The SHash validator checked table dimensions, mask, and key-count bounds but accepted an unaligned `records` pointer. A malformed shallow view could therefore pass every inline guard and access `SHashRecord` slots at an invalid alignment.
- Include the fixed-width pointer-integer declaration and reject backing pointers that are not aligned to pointer slots before any indexed access. Extend the existing compaction test with a misaligned shallow view, covering reads, sizing, clone/copy, enumeration, and mutation fail-closed behavior while confirming the true allocation remains unchanged.
- Update existing source and dynamic Actions contracts; reuse the existing CMake target, with no new files or build targets. No local compiler/CTest result is claimed.

---

## 2026-11-07 19:25 CST — Recover RandomGen from an impossible zero index

- Baseline: `c6e4948bc4187189f4d417db1b006ac5409cd6d3`; latest main CI run #645 passed the integration/source contracts, full build, and all 57 CTests.
- Scope: MT19937 state validation in `RandomGen.c`.
- The generator treated `mti == 0` as acceptable whenever any MT word was nonzero. But the index is zero only transiently after the internal twist, and the same call consumes that word before returning. A corrupted object entering the next call at index zero skipped the twist and emitted a state word out of sequence.
- Reseed on every call-entry `mti == 0`, retain the existing all-zero-state recovery at the normal block boundary, and add a deterministic regression comparing the recovery output with a fresh generator seeded to the documented fallback seed.
- Existing files only; update source-contract checks. No local build/CTest result is claimed.

---

## 2026-11-07 19:21 CST — Update BStream read-alias CI contract

- Failed head: `79e4fdeb3aca3ad5c209d1efc80f0b8f69f091da`; Actions run #644 reached source-contract validation and failed before configure/build.
- The runtime source check searched for the old one-line alias guard. The refactor moved the alias check ahead of malformed-metadata handling and added conservative extent guards, so that exact expression no longer exists.
- Retarget the checker to the new alias-guard comment, preserving the existing ordering and no-zeroing assertions. Only the workflow contract and handoff log are changed; no source or regression behavior is altered in this CI repair.

---

## 2026-11-07 19:19 CST — Guard BStream read aliases on malformed metadata

- Baseline: `8181b9ded7a74a45ba70726a611981db68d3c0b4`; SkipDBv2 Regression run #643 passed integration/source contracts, full build, and all 57 CTests.
- Scope: `BStream_sourceOverlapsStream_()` and `BStream_readNumber_size_()`.
- The overlap guard ran only after checking that the stream item width was one byte. If `itemSize` was corrupted, the invalid-stream path could zero the caller's output buffer even when it pointed inside the stream's own allocation, corrupting input bytes.
- Check known/possible aliases before any error path writes to the destination; calculate byte extents with overflow guards and conservatively reject output writes when malformed metadata leaves no reliable extent. Extend the existing regression for item widths 2 and 0, checking the cursor and backing bytes stay unchanged.
- Update only existing C, test, workflow, and handoff-log files. No new files or CMake targets; no local build/CTest result is claimed.

---

## 2026-11-07 19:16 CST — Fix PID-preview workflow contract indentation

- Failing head: `e0c680468fb4623067844b069ef469842b9c4633`; Actions run #642 stopped before compilation with a Python `IndentationError` in the newly added PID-preview source-order check.
- The contract body was inserted one space deeper than the surrounding workflow script, so Python rejected the checker before any build/test stage.
- Normalize the inserted block to the surrounding ten-space level; leave the C implementation and regression unchanged. Re-run Actions to validate the contract and build.

---

## 2026-11-07 19:14 CST — Keep next-PID preview ahead of active reservations

- Baseline: `c7066d819c5ff66873d11663229438d233ba403e`; SkipDBv2 Regression run #641 passed source contracts, configuration, full build, and all 57 CTests.
- Scope: `UDBIndex_nextPid()` in `UDBIndexFreeList.c`.
- During an active transaction, the allocator tracks newly reserved IDs in `self->maxPid`, while index mappings remain journaled and file EOF does not advance. The preview path used only EOF, so it could return a PID already reserved by an earlier allocation in that same transaction.
- Mirror the allocator's `maxPid`-aware fallback with overflow and offset checks, preserving free-hole priority. Extend `udb_index_layers.c` to check preview results after reserving PID 1 and then PID 2; strengthen existing Actions source and ordering contracts.
- Existing source/test/workflow files only; no CMake changes or new declarations. No local build/CTest result is claimed.

---

## 2026-11-07 19:12 CST — Correct newline escape in UArray source contract

- Failed head: `c0a947efbb97e3ebd221d4bd164eff92ab804c2d`; Actions run #640 still stopped in the source-contract checker before compilation.
- The intended definition-only search had a double-escaped newline in the Python script, so it searched for a literal backslash sequence instead of the actual newline before the opening brace. The guard consequently still found no validator body.
- Correct the Python string to use one newline escape; preserve the existing 16-occurrence contract and all implementation files.

---

## 2026-11-07 19:11 CST — Locate UArray layout-validator definition precisely

- Failed head: `8989a3bb30a48f46d5dfa164ba9f935b81156cf5`; Actions run #639 failed in the source-contract checker before compilation.
- The preceding count repair passed the dictionary contracts, but a dynamic source-order contract used an unqualified `find()` for the validator signature. The new forward declaration matched before the actual function body, causing the validator-body geometry check to inspect the wrong span.
- Anchor the dynamic check to the signature followed by its opening brace, so it selects the definition rather than the forward declaration. This is a workflow-only correction; no C logic, tests, build targets, or workflow triggers change.

---

## 2026-11-07 19:10 CST — Repair UArray layout source-count contract

- Failing head: `e6d1a4eb84c657db4d135456614f5bc0b9c5863c`; GitHub Actions run #638 reached the integration-contract checker and failed before configure/build.
- Root cause: the UArray teardown hardening correctly added one file-scope forward declaration and two guarded destructor call sites, but the strict workflow still required exactly 13 textual occurrences of `UArray_hasValidItemLayout_()`. The current file contains 16 (one declaration, one definition, fourteen calls).
- Update only the stale source-count assertion/message; no product code, test behavior, build target, or trigger changed. Re-run full CI as the validation gate.

---

## 2026-11-07 19:09 CST — Validate UArray backing ownership before teardown

- Baseline: `c596819019796a5b32f4383b8551b214345cbc57`; SkipDBv2 Regression run #637 passed integration/source contracts, full build, and all 57 CTests.
- Scope: `UArray_stackFree()` and `UArray_free()` typed backing-storage release.
- UArray already uses `UArray_hasValidItemLayout_()` to validate type width, size arithmetic, and data alignment before typed access, but both destructors bypassed it and unconditionally released any non-stack-allocated data pointer. A corrupted or misaligned typed pointer could therefore reach the allocator.
- Reuse the existing layout guard in both teardown paths. Extend the existing UArray lifecycle test to exercise malformed shallow-view cleanup and heap-object destruction while preserving and explicitly freeing the real owned allocation.
- Update the existing workflow source contracts; no new files, exported declarations, or build targets. No local build/CTest result is claimed.

---

## 2026-11-07 19:07 CST — Initialize the journal fixture cleanup path

- Baseline: `a6b424eca9ad97f4584a06ef14f559685ae0b4f4`; SkipDBv2 Regression run #636 passed the source contracts, full build, and all 57 CTests.
- Scope: failure cleanup in `tests/jfile_journal.c`.
- The two earliest test guards jump to shared cleanup before `link_path` is populated, but cleanup always reads `link_path[0]` and may pass the uninitialized stack buffer to `unlink()`. A failure in an unrelated fixture could therefore trigger undefined behavior or target an unintended relative path.
- Zero-initialize the path buffer; retain the existing conditional cleanup and add a source-contract needle so the invariant remains enforced. No new files or build changes; no local build is claimed.

---

## 2026-11-07 19:05 CST — Guard Stack backing release during teardown

- Baseline: `796411d5f2d86e9df3053c18942dc4d9d470c2ef`; SkipDBv2 Regression run #635 passed integration contracts, full build, and all 57 CTests.
- Scope: `Stack_free()` in the Stack lifecycle module.
- All normal Stack operations used the existing storage validator, but destruction unconditionally passed `self->items` to `io_free`. If the backing pointer was corrupted or misaligned, teardown could hand an invalid pointer to the allocator.
- Validate the existing storage geometry before releasing the backing allocation; always release the Stack object itself. Extend the existing lifecycle test with a deliberately misaligned backing pointer and safely free the saved true allocation after teardown.
- Update existing source-contract assertions only; no new files, declarations, or build targets. No local build/CTest result is claimed.

---

## 2026-11-07 19:02 CST — Restore Actions workflow startup

- Failed head: `6a20f802d6008c11dadadab8ca8d5b208087c165`; run #634 failed immediately and GitHub reported zero jobs/check runs, so no step-level log was available.
- Root cause addressed in-place: the workflow had grown to 512783 characters and contained seven duplicate keys in the Python `checks` dictionary. Python keeps only each key's last value, so the earlier blocks were shadowed and added no effective validation.
- Remove only those seven overwritten blocks, retaining every effective source-contract block, including the new PID-map extent and regression assertions. The workflow falls to 505807 characters; no C sources, tests, build targets, or workflow triggers are changed.
- CI #635 passed after the duplicate-contract cleanup, and #636 subsequently passed source contracts, configuration, the full build, and all 57 CTests.

---

## 2026-11-07 19:01 CST — Reject PID-map writes on invalid index extents

- Baseline: `83f9a595ed8841cb9e43f93293330f723622e42e`; SkipDBv2 Regression run #632 passed the integration contracts, full build, and all 57 CTests.
- Scope: `UDBIndex_setPos_forPid_()` in `UDBIndexMap.c`.
- The mapping writer validated the PID offset and reservation state but not the current index-file extent. PID 0 / position 0 could therefore journal a write even when the index extent was negative or ended in a partial fixed-width entry, bypassing the startup alignment check.
- Reject negative and misaligned extents before reading mappings or writing to the journal. Preserve the valid empty index initialization path (`maxPos == 0`).
- Extend the existing `udb_index_layers_test` to assert both invalid extents leave journal length and cursor state unchanged; strengthen workflow source-order and regression contracts. No new files or CMake changes; no local build is claimed.

---

## 2026-11-07 18:59 CST — Reject misaligned PHash backing storage

- Baseline: `638680f1bd5268a9e24448aadc90d33842707d53`; SkipDBv2 Regression run #632 passed the integration contracts, full build, and all 57 CTests.
- Scope: `PHash_hasValidStorage_()` and its existing storage-bounds regression.
- The central PHash validator checked capacity, mask, and key-count geometry but accepted a misaligned `records` pointer. Valid-looking table metadata could therefore allow inline lookups and mutations to perform undefined unaligned `PHashRecord` accesses.
- Require naturally aligned backing storage before any inline/C operation may proceed, explicitly include `stdint.h`, and extend the existing PHash test to exercise reads, writes, enumeration, sizing, copy, grow, and compaction against a malformed shallow view while confirming the actual source table remains intact.
- Update the existing GitHub Actions source contracts; no new files, exports, or CMake targets. No local build/CTest result is claimed.

---

## 2026-11-07 18:59 CST — Reject free-list metadata writes on invalid file extents

- Baseline: `52d6fdef7b95662d9e116afd228188b9baff83da`; latest completed SkipDBv2 Regression run #631 passed.
- Scope: `UDBRecords_firstEmptyRecordPos_()` in `UDBRecordsFreeList.c`.
- The setter let `pos == 0` bypass file-extent validation. After a stream extent became negative or represented a truncated metadata prefix, it could still enqueue a write over the reserved first-empty metadata word.
- Reject negative extents and nonempty files shorter than `sizeof(PID_TYPE)` before touching cursor state, in-memory free-list state, or the journal. Preserve valid zero-length-file initialization and validate nonzero free-list offsets against the checked extent.
- Extend the existing `udb_record_storage_test` to prove invalid extents leave the journal, logical cursor, and free-list pointer unchanged; strengthen static and dynamic workflow contracts. No new files or CMake changes; no local build is claimed.

---

## 2026-11-07 18:56 CST — Enforce List backing-slot geometry

- Baseline: `be8165380da6b6f824cffccd63c69de3d10222d6`; SkipDBv2 Regression run #630 passed integration/source contracts, full build, and all 57 CTests.
- Scope: `List_hasValidStorage_()` and `List_memorySize()` storage validation.
- The shared List validator accepted byte capacities not divisible by `sizeof(void *)`, and `List_memorySize()` bypassed that validator. Malformed backing geometry could therefore appear valid and report a plausible allocation size.
- Require pointer-slot-aligned capacity bytes in the central validator; route memory-size reporting through the same invariant. Extend the existing storage-bounds test to verify reads, sizing, views, and mutations fail closed without changing the source allocation. No new files or CMake targets.
- No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---

## 2026-11-07 18:53 CST — Reject foreign records from dirty and PID caches

- Baseline: `e5fbfd7d87efdf80b431b8dbd264d76a378b80b7`; SkipDBv2 Regression run #629 passed the integration contracts, full build, and all 57 CTests.
- Scope: `SkipDB_noteDirtyRecord_()` and `SkipDB_noteAssignedPidToRecord_()` in `SkipDBCache.c`.
- Both notification paths validated NULL/open state but not record ownership. A record from another database could be queued for saving in the wrong transaction or overwrite that database's PID-to-record cache entry.
- Reject records unless `record->sdb == self` before queue insertion or PID-cache mutation. Extend the existing two-database core regression to assert the foreign record leaves dirty-queue length and PID-cache mapping unchanged, and strengthen static/dynamic Actions source contracts.
- Reuse existing files, declarations, and CTest target; no new source files or CMake changes. No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---

## 2026-11-07 18:51 CST — Validate cached forward-link PID accessors

- Baseline: `40917e5041902c20bef3c038f14cb5386e78e2fe`; SkipDBv2 Regression run #628 passed the integration/source contracts, complete build, and all 57 CTests.
- Scope: cached forward-link ownership and PID resolution in `SkipDBRecordLinks.c`.
- The record-pointer accessor already rejected foreign owners and mismatched cached PIDs, but the PID-only accessors could return/cache a foreign PID or allocate through a foreign record before applying the same invariant. The link setter also accepted a cross-database target and deferred failure until a later traversal.
- Centralize the owner/PID check, apply it before PID reads/allocations and disk-loaded cache registration, and refuse cross-owner setters before mutating the pointer slot. Extend the existing two-database core regression and strict workflow contracts; no new files, CMake targets, or exported declarations.
- No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---

## 2026-11-07 18:34 CST — Reject foreign records at SkipDB cache-age entry points

- Baseline: `d7a3fae074f2c7972a5819746c3227a830dfed6c`; SkipDBv2 Regression run #627 passed the integration contracts, complete build, and all 57 CTests.
- Scope: `SkipDB_noteNewRecord_()` and `SkipDB_noteAccessedRecord_()` in `SkipDBCache.c`.
- The cache age-list mutators checked only for NULL. Passing a live record owned by another SkipDB could unlink it from its real owner's age list, splice it into the wrong database, and/or inflate the wrong cache count, breaking eviction invariants.
- Require `record->sdb == self` before changing the cached-record count or touching age links. Normal record construction binds the owning database before cache registration, so legitimate insertion remains unchanged.
- Extend the existing two-database core regression to call both entry points with a foreign record and verify both age-list roots, ownership links, and cached counts remain unchanged. Add workflow source-order checks; no new files or build targets.
- No local compiler/CTest result is claimed; GitHub Actions remains the validation gate.

---

## 2026-11-07 18:32 CST — Preserve the UDB PID-index cursor while counting slots

- Baseline: `81a4e8660c2d618b57ee33c8080fe0f31d12b27a`; SkipDBv2 Regression run #626 passed the source contracts, complete build, and all 57 CTests.
- Scope: `UDBIndex_pidCount()` in the fixed-width PID map module.
- Counting slots used `JFile_setPositionToEnd()` and returned the file wrapper's logical position at EOF; malformed extents also returned with the mutated cursor. Traversal callers therefore could unexpectedly disturb an index cursor just by asking for a bound.
- Snapshot and restore the logical position/extent before validating the observed EOF and alignment, keeping invalid counts fail-closed and the cursor unchanged on every return.
- Extend `udb_index_layers_test` for valid and misaligned PID counts, with workflow source-order assertions. Existing test target reused; no new files or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the validation gate.

---

## 2026-11-07 18:29 CST — Validate PID free-list storage before direct traversal

- Baseline: `f91746bcdb1a6705fe624cdf1d50da93cc9f4544`; SkipDBv2 Regression run #625 passed the integration/source contracts, complete build, and all 57 CTests.
- Scope: PID reservation/free-hole pool in `UDBIndexFreeList.c`.
- Several operations iterated `reserved->items` directly, and allocator paths treated a `List_top()` NULL result as an empty pool. If list metadata described a nonzero size with missing/undersized storage, these paths could dereference invalid memory or allocate a new PID while the free stack was unusable.
- Add one private validator for both PID stacks, use it before list reads/mutations and before reservation iteration in commit preparation, and strengthen the push postcondition. Invalid internal storage now fails closed rather than being interpreted as an empty stack.
- Extend `udb_index_layers_test` with malformed hole/reservation storage fixtures and strengthen the workflow source contract. Existing CMake target reused; no new files.
- No local compiler/CTest result is claimed; GitHub Actions remains the validation gate.

---

## 2026-11-07 18:26 CST — Repair workflow source-contract syntax after run #624

- Baseline: `3cdbd676241342f869ea301f9fb2b6db0a250e26`; run #624 stopped during the source-contract script parse, before configuration, compilation, or CTests.
- The PID lookup regression source-contract list was missing its closing quote on the existing `UDB_close(db);` token, causing Python's “unterminated string literal” error.
- Correct only the malformed workflow token and rerun the unchanged source/build/test gate. The PID lookup implementation and regression remain untouched in this repair.
- No compiler or runtime result was produced by run #624.

---

## 2026-11-07 18:25 CST — Preserve the UDB PID-index cursor during lookups

- Baseline: `c0e91489cb7946d13bd50b21d421380814895e98`; SkipDBv2 Regression run #623 passed the source contracts, full build, and all 57 CTests.
- Scope: `UDBIndex_posForPid_()` in the fixed-width PID-to-offset mapping module.
- A normal lookup sought to the PID slot and returned without restoring `JFile.pos`. Besides making a read accessor mutate caller state, this caused `UDBIndex_setPos_forPid_()` to snapshot the already-advanced cursor rather than the caller's original cursor for its failure path.
- Snapshot and restore the logical cursor and file extent around the slot read, including read failure; retain the error flag so callers can still fail closed.
- Extend `udb_index_layers_test` and the workflow's strict source contract to cover successful lookup cursor preservation and enforce restore-before-return ordering. No new files or CMake targets.
- No local compiler/CTest result is claimed; GitHub Actions remains the validation gate.

---

## 2026-11-07 18:23 CST — Preserve UDB record cursor during free-list metadata I/O

- Baseline: `804b982d1a7665b4f6303ad9b7dbde4fe8c3f32d`; SkipDBv2 Regression run #622 passed the XY-C1 source contracts, full build, and all 57 CTests.
- Scope: `UDBRecords_firstEmptyRecordPos()` and `UDBRecords_firstEmptyRecordPos_()` in the existing free-list module.
- The getter sought to offset zero and returned with the tracked cursor at the metadata boundary. The setter restored `pos/maxPos` on a failed write but left the cursor changed after a successful write. Both APIs therefore leaked internal metadata I/O into the caller's record-stream state.
- Snapshot and restore the tracked `JFile` position and extent on successful metadata reads and writes; leave the existing fail-closed read-error behavior and failed-write rollback untouched.
- Extend the existing `udb_record_storage_test` to exercise both APIs inside a transaction with the cursor positioned at a live record. Add workflow source contracts checking the snapshots/restores and test coverage. No new source files or build targets.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 18:20 CST — Centralize tagged-array alias snapshots in BStream

- Baseline: `3bd47426de801b9eb5d0eaad0829cb8e404a9beb`; SkipDBv2 Regression run #621 passed the source contracts, complete build, and all 57 CTests.
- Scope: `BStream_writeTaggedUArray_()` in the BaseKit binary-stream module.
- The tagged UArray wrapper duplicated overlap detection and cloned the array before handing it to `BStream_writeTaggedData_length_()`, which already owns the full framing operation, inclusive trailing-terminator overlap snapshot, and rollback path. The duplicate range logic used a strict end bound and could drift from the shared helper.
- Keep source/stream CTYPE and extent validation in the wrapper, then delegate the byte view directly to the shared writer. This removes the redundant clone/range implementation and leaves alias detection and rollback with one owner.
- Retain the existing `testBStreamAliasedWrites()` trailing-terminator regression and extend the dynamic workflow contract to require delegation and forbid duplicate wrapper snapshots.
- No local compiler/CTest result is claimed; GitHub Actions remains the validation gate.

---

## 2026-11-07 18:17 CST — Restore single-owner stream readiness validation

- Baseline: `6c2ae64a76df92f82f4233d217bd505c4faf55cf`; run #620 stopped at the source-contract gate before configuration, compilation, or CTests.
- The staged decoder repeated `SkipDBRecord_streamIsReady_()` after the public entry point had already validated the caller and stream, changing the established centralized-helper count from three to four.
- Remove the redundant private guard; the private decoder is only called by the checked public wrapper with a newly allocated record. Keep error-buffer reset and all bounds/tag checks unchanged.
- No build or runtime test ran in failed run #620; the new commit will rerun the complete Actions gate.

---

## 2026-11-07 18:16 CST — Make SkipDB record deserialization atomic

- Baseline: `2740ec2ecf0c63a808f54b21cdb171ced29f29d0`; SkipDBv2 Regression run #619 passed the integration/source contracts, full build, and all 57 CTests.
- Scope: `SkipDBRecord_fromStream_()` and its existing record-storage regression target.
- The decoder previously wrote key/value arrays and pointer metadata directly into the caller's record before proving that the tagged stream was complete. A later truncation, invalid PID/link, or trailing byte could return failure after leaving the reused record partially overwritten.
- Decode into a detached record, and swap payload/link fields into the caller only after the full stream validates. Preserve caller identity, database/cache metadata, PID, and old key-ownership semantics; release the old payload through the temporary record's destructor.
- Extend the existing test to verify truncated and trailing-data failures preserve key, value, level, link PID, and suffix metadata. Update the workflow's static contract and add a source-order assertion. No new files, exported symbols, or CMake target changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 18:14 CST — Align time-sliced compaction count with split-hole merges

- Baseline: `23f854636f1bfe1bd073375c46990665ba91b83d`; SkipDBv2 Regression run #618 passed the integration contracts and full build, but CTest failed only in `udb_reserved_pid_test`.
- Scope: the existing time-sliced compaction assertion and its workflow source contract; no production algorithm change or new files.
- The larger-hole fix now preserves the residual free extent and tombstones the old live-row extent. In this fixture, the subsequent time slice performs two successful adjacent-hole merges before the trailing-hole truncation, so `UDB_compactStepFor_(db, 5.0)` correctly reports 2 rather than the old expected count of 1.
- Update the regression's expected compaction count and assert that exact expectation in the existing workflow contract. Keep the database-open, live-value, removed-PID, and tail-shrink assertions unchanged.
- No local compiler/CTest result is claimed; GitHub Actions remains the validation gate.

---

## 2026-11-07 18:00 CST — Preserve record boundaries when compacting into larger holes

- Baseline: `3c631b805bfdefd44fa90d5b937881a59f04e9e0`; SkipDBv2 Regression run #617 passed the source contracts, full build, and all 57 CTests.
- Scope: `UDB_compactStepOnce_()` and the existing UDB record-storage regression.
- The compactor accepted any hole at least as large as the next live row, copied the row to the hole start, and wrote an empty header at `hole_start + row_size`. For a larger hole, that position is still inside the old hole, while the original live row at the old hole end remained non-empty. The persisted free-list pointer, physical scan boundaries, and live row could therefore disagree after a successful compaction.
- Validate both persisted extents before mutation. Reject a nonzero remainder smaller than a complete record header. When the hole is larger, write the correctly sized residual empty extent and tombstone the old source row before changing the PID mapping. Exact-fit moves preserve the existing single-tombstone path.
- Extend `udb_record_storage_test` with a large-hole/small-row fixture, assert the moved value and residual pointer, and walk every physical row through EOF. Add workflow source contracts for split/tombstone/map ordering. No new files, CMake changes, or exported declarations.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---
## 2026-11-07 17:56 CST — Avoid double-committing the journal before truncation

- Baseline: `bd066c3d32c2c36d58e52a24101d797d90849b58`; SkipDBv2 Regression run #616 passed the workflow contracts, full build, and all 57 CTests.
- Scope: `JFile_truncate_()` and the existing journal regression test.
- `JFile_truncate_()` called `JFile_commitToLog()` and then `JFile_commitToFile()`, which itself calls `JFile_commitToLog()`. The first call converts the terminal item marker into a transaction marker; the second then expects an item marker, treats the already committed journal as invalid, closes the journal stream, and returns before truncation.
- Let `JFile_commitToFile()` own the single prepare/replay sequence. Add a regression that truncates with a pending journal entry, verifies both streams remain open and the data extent is empty, then performs a successful write/commit on the same object.
- Extend the existing source contracts to ensure truncation does not directly double-commit and that the regression remains registered. No new files or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---
## 2026-11-07 17:54 CST — Validate UArray layouts before insert-every arithmetic

- Baseline: `8906776f32240d305181efa4afa6cec3cb450ba3`; SkipDBv2 Regression run #615 passed all source contracts, the full build, and 57 CTests.
- Scope: `UArray_insert_every_()` and its existing malformed-layout regression.
- The routine checked only `itemSize` and multiplication bounds. When `itemType` claimed a wider C type than `itemSize`, raw byte arithmetic could produce a buffer sized for the forged width, then transfer it to `UArray_setData_type_size_copy_()` using the wider type, whose terminator write could exceed that allocation.
- Reuse `UArray_hasValidItemLayout_()` for both source and destination before deriving byte counts; keep the overflow guard for `itemCount`. Extend the existing test to cover malformed source and destination while verifying data and metadata stay unchanged. Update static/dynamic workflow contracts and the expected validator call count; no new files or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---
## 2026-11-07 17:52 CST — Snapshot UArray copies that alias the trailing terminator

- Baseline: `9c8058e133fcd525380fdc77416ced290865694e`; SkipDBv2 Regression run #614 passed the source contracts, full build, and all 57 CTests.
- Scope: `UArray_copyData_()` overlap detection and the existing UArray stream-I/O regression target.
- The copy helper excluded empty destinations and used a strict `< selfBytes` bound for source starts inside the current buffer. A valid borrowed one-byte view of the trailing NUL therefore bypassed the snapshot; resizing could invalidate the source before the copy.
- Include the logical end position in the overlap test and do not gate it on a nonzero logical byte count. This safely snapshots a trailing-terminator alias for both populated and empty arrays before `UArray_setSize_()` runs.
- Extend the existing `uarray_stream_io_test` with populated/empty terminator-view cases and add a workflow source-order contract requiring inclusive-tail detection and snapshot-before-resize. No new files, declarations, or CMake target changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---
## 2026-11-07 17:50 CST — Repair workflow contract quoting

- CI run #613 stopped in the early XY-C1 integration-contract step, before compilation: the new assertion needle in the Python source-contract list was missing its closing quote.
- Restore the closing quote only; the C build and CTest stages had not run in this attempt.

---
## 2026-11-07 17:49 CST — Correct fail-closed regression expectation

- CI run #612 compiled all targets and passed the source/build gates; `skipdb_cursor_previous_test` reached the new corruption case and showed the implementation closed UDB as intended.
- The new test assertion inverted the expected closed-state check (`!SkipDB_isOpen` was mistakenly treated as a failure). Correct it to fail only if last-record traversal returns a row or the database remains open, and align the source contract with that assertion.
- No production-code change; this is the minimal in-place correction for the red mainline test.

---
## 2026-11-07 17:48 CST — Reject cached zero-level rows during tail traversal

- Baseline: `4015dc39d12fc59e9e75559f35115587a84737f0`; mainline SkipDBv2 Regression run #611 passed the strict source contracts, full build, and all 57 CTests.
- Scope: `SkipDBRecord_findLastRecord()` and the existing cursor/link regression.
- Disk-loaded non-root records with level zero are rejected, and key search rejects zero-level forward targets. But tail traversal allowed a malformed cached row through `SkipDBRecord_hasValidLevel_()` because level zero is within the generic range; its empty level loop made it appear to be a valid last record.
- Reject a level-zero current record unless it is the database header, clear the cached update vector, and close the UDB on corruption. Extend the existing test to persist a zero-level final row, query the last record while it remains cached, and then reopen to verify on-disk corruption still fails closed.
- Update the workflow's static and dynamic source contracts to enforce the guard before the link walk. Existing test/CMake paths are reused; no new source files or exported declarations.
- No local compiler/CTest result is claimed; GitHub Actions remains the build/runtime gate.

---
## 2026-11-07 17:45 CST — Validate length-delimited IPC keys before framing

- Baseline: `ff01036bacfde1078e3287806bdbba23cbbc8152`; SkipDBv2 Regression run #610 passed the strict source contracts, full build, and all 57 CTests.
- Scope: shared IPC protocol-field validation and the `dbus` environment-backed bulk writer.
- `dbus bulk_key()` copied environment-variable names directly into the space-delimited request key. Environment entries can contain spaces/control bytes even if typical shell assignment syntax cannot create them; the receiver then parses the malformed key as a different key plus payload, risking writes to an unintended setting.
- Add a length-aware validator that scans exactly the supplied slice (including NUL/control rejection) and make the existing NUL-terminated helper delegate to it. Validate the complete environment-derived key before copying/framing it.
- Extend existing IPC transport cases to cover non-NUL-terminated valid slices, embedded whitespace/NUL, empty-field policy, and length bounds. Update source contracts to verify both validator ordering and validation-before-copy; all files already participate in existing CMake targets, so no target/file additions are needed.
- No local compiler/CTest result is claimed; GitHub Actions is the compiler/runtime gate.

---
## 2026-11-07 17:35 CST — Preflight PID-hole capacity before clearing index mappings

- Baseline: `28479b7674e71529c2e0020277284b669a755e7b`; mainline SkipDBv2 Regression run #609 passed the source contracts, complete build, and all 57 CTests.
- Scope: `UDBIndex_setPos_forPid_()` and its existing PID index regression.
- A deletion wrote the zero-valued index entry to the journal before `UDBIndex_releasePid_()` attempted to insert the freed PID into the reusable-hole list. If list growth failed, the API returned failure after mutating the journal, so a failed delete could still be replayed by a later commit.
- Validate the hole-list storage and preallocate one slot before the mapping write when a release is required. Verify the resulting capacity, while avoiding allocation when the PID already exists in the hole list. Failure now exits before touching the index journal; normal release still centralizes deduplication and reservation transfer in the free-list owner.
- Extend `udb_index_layers_test` for both reserved and already-mapped PIDs, asserting failed hole allocation does not advance the journal or clear a committed mapping. Add Actions source-order contracts for the capacity preflight. No new files, exported declarations, or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---
## 2026-11-07 17:30 CST — Unify UDBIndex partial-construction cleanup

- Baseline: `a76d9ad9138659d72a9dec052ea04d5f8a7dbe72`; mainline SkipDBv2 Regression run #608 passed source contracts, the complete build, and all 57 CTests.
- Scope: `UDBIndex_new()` and its canonical `UDBIndex_free()` lifecycle path.
- Allocation failure and default-path setup failure duplicated cleanup of the JFile and both PID lists. These error branches could drift from the destructor as the index acquires more owned state.
- Route both failures through one rollback label and `UDBIndex_free(self)`. The destructor and `UDBIndex_close()` are NULL-safe for partially initialized fields; successful construction and ownership semantics remain unchanged.
- Extend the existing Actions source map and dynamic constructor-order/count contract. No new files, symbols, CTest targets, or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 17:28 CST — Unify UDBRecords partial-construction cleanup

- Baseline: `5fbf29156cbf1b0a4f1ce7cd94b9d6bc8f82cd39`; mainline SkipDBv2 Regression run #607 passed source contracts, the full build, and all 57 CTests.
- Scope: the `UDBRecords_new()` constructor and its existing `UDBRecords_free()` lifecycle path.
- Allocation failure and default-path initialization failure previously duplicated the same four resource releases. This makes constructor rollback easy to drift from the normal destructor when fields are added or ownership changes.
- Route both failures through one cleanup label and the canonical NULL-safe destructor. The destructor already handles partially initialized `JFile` and `UDBRecord` members; no resource is published and successful construction remains unchanged.
- Tighten the existing Actions source contract and add an ordering/count assertion for the single rollback path. No new files, symbols, CTest targets, or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 17:25 CST — Reject Windows wall-clock seconds outside timeval range

- Baseline: `bcc82f840626a8594a190d24b888cdbf166ee748`; mainline SkipDBv2 Regression run #606 passed the XY-C1 source contracts, complete build, and all 57 CTests.
- Scope: the Windows `gettimeofday()` adapter in `basekit/PortableGettimeofday.c`.
- On 64-bit Windows, CRT `time_t` can represent dates beyond the signed 32-bit range of `long`, while the adapter's `timeval.tv_sec` field remains a `long`. Directly casting `_timeb.time` can therefore wrap/lose the real timestamp after the 2038 boundary.
- Check both `LONG_MIN` and `LONG_MAX` immediately after the successful clock read. Return `-1` with `EOVERFLOW` before writing either timeval field when the timestamp cannot be represented; normal in-range timestamps retain the existing behavior.
- Extend the existing Actions contract and order assertion to require the range check before assignment. Reuse the existing `portable_gettimeofday_test`; no new files, declarations, or CMake changes. The Linux CI build does not execute the Windows-only branch, so the new Windows behavior is statically contract-checked rather than claimed as runtime-tested here.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 17:21 CST — Check JFile path sizing additions before arithmetic

- Baseline: `ede6b8582d2f87c2046cc03a73605b6dbd907731`; mainline SkipDBv2 Regression run #605 completed successfully.
- Scope: the two path-construction setters in `jfile/JFile.c` and their existing `jfile_journal_test`.
- The setters used chained expressions such as `SIZE_MAX - name_len - 6` and `SIZE_MAX - ext_len - 2`. If a component length approaches the representable limit, the subtraction itself can wrap before the guard compares it, so later allocation-size additions are not proven safe.
- Replace chained subtraction with sequential checked additions: validate the suffix, separator, and prefix one at a time for `JFile_setLogPath_()`; separately account for the dot, extension bytes, and terminating NUL in `JFile_setPath_withExtension_()`. Make the basename helper file-local and reject a null object/path before dereferencing it.
- Add path-setter alias regressions using `self->path` as the input to both setters. Extend the current Actions source-order contract so each size check must precede its corresponding addition. Existing CMake/CTest registration is reused; no new files or exported declarations.
- No local compiler/CTest result is claimed; GitHub Actions remains the compile/runtime gate.

---

## 2026-11-07 17:19 CST — Revalidate the SkipDB eviction cursor after object callbacks

- Baseline: `d94121ab3c7116ab5e1244b73ce7d37beecc2fcf`; mainline SkipDBv2 Regression run #604 passed the XY-C1 source contracts, full build, and all 57 CTests.
- Scope: `SkipDB_evictUnmarkedCacheRecords_()` and the existing `skipdb_core_layers_test`.
- The LRU walk cached `record->olderRecord` before deallocation. `SkipDBRecord_dealloc()` detaches the current row, then may invoke an arbitrary object-free callback; a callback that calls `SkipDB_clearCache()` can free the saved next row before the outer walk resumes, causing a use-after-free/double-free.
- Snapshot the next row's PID and integer address before deallocation. After a callback, resume only when the PID cache still maps that PID to the exact same address; otherwise stop the stale walk. Clear all surviving cache marks even when the callback interrupted traversal.
- Extend the existing core-layers test to make the finalizer reenter cache clearing during actual high-water eviction, with adjacent LRU rows and persistence/reopen assertions. Extend static and function-order Actions contracts. No new files, public symbols, or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 17:14 CST — Fix SkipDBRecord level contract newline boundary

- Baseline: `6322614f6db948772625987c879a17e60623234f`; run #603 failed in the XY-C1 source-contract step before CMake configuration.
- The new function-body check searched for the two literal characters `\\n` instead of the newline separating `SkipDBRecord_level_()` from `SkipDBRecord_object_()`. The implementation and regression were not reached by the build.
- Use `chr(10)` as the function boundary delimiter. Keep the guard-order assertions unchanged; no production C or test behavior changed.
- No compiler/CTest result was reached in run #603.

---

## 2026-11-07 17:12 CST — Reject malformed existing SkipDB record levels before resize

- Baseline: `1c45c3cce044c500bfbfa03c30b30f59459d772e`; mainline SkipDBv2 Regression run #601 passed the integration contracts, complete build, and all 57 CTests.
- Scope: `SkipDBRecord_level_()` and the existing `skipdb_core_layers_test`.
- The setter validated the requested level but trusted `self->level` as the old allocation extent. If a record's public metadata was corrupted to a negative level, growing the pointer array formed `new_pointers + old_level` before the allocation and passed a derived oversized length to `memset`, causing an out-of-bounds write.
- Reject an existing level outside `[0, SKIPDB_MAX_LEVEL]` before equality checks, pointer arithmetic, or `realloc`. Leave both the malformed metadata and pointer ownership untouched; normal valid-level growth/shrink behavior remains unchanged.
- Add regressions for negative and over-maximum existing levels. Extend the static source contract and a function-order check so the old-level guard must precede `realloc`. Reuse the current CTest target; no new files or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 17:07 CST — Detach array-heap storage before destructor callbacks

- Baseline: `48e8c1d526a9c36a00624854be4de6854a36398b`; SkipDBv2 Regression run #601 passed XY-C1 integration contracts, complete build, and all 57 CTests.
- Scope: the ownership/lifecycle path in `mgr/array-heap.c:array_free()`, its existing `array_heap_test`, and the API comment.
- `array_free()` called element destructors while the array still exposed its owned backing store. A destructor that recursively freed or reinitialized the same array could re-enter the old element walk, double-release the original storage, or have the outer call free newly created storage.
- Snapshot the old backing pointer and valid callback count, reset the public struct before the first callback, run callbacks only over the detached storage, then free that old allocation exactly once. Recursive frees now see an empty array; callback-time reinitialization remains owned by the caller.
- Add a regression whose first destructor recursively frees and reinitializes the array; assert the original callbacks run once and the new allocation survives the outer free. Reuse the existing `array_heap_test` CMake/CTest target; no new files or symbols.
- No local compiler/CTest result is claimed; GitHub Actions remains the compile/runtime gate.

---

## 2026-11-07 17:04 CST — Reject UArray trailing-terminator ownership aliases

- Baseline: `4d221738e60d90f07ccd2e05223fe37531520896`; `SkipDBv2 Regression` run #600 passed the XY-C1 integration contracts, complete build, and all 57 CTests.
- Scope: `UArray_setData_type_size_copy_()` in `basekit/UArray.c` and its existing `uarray_stack_resize_test`.
- Both source-alias checks excluded the exact end of the logical byte range. For owned UArrays, that address is the allocation's trailing terminator: `copy=0` could free the allocation and then publish an interior pointer as the new owner, causing an invalid free later; `copy=1` could realloc before reading the aliased terminator, invalidating the source if realloc moved the block.
- Treat the logical end as an alias boundary. The copying path snapshots exactly the single trailing terminator byte before realloc; other reads extending beyond the logical payload are rejected. The no-copy path rejects pointers at or inside the current logical extent before freeing or publishing any storage.
- Add focused regressions for both no-copy ownership rejection and copy-before-realloc semantics. Reuse the existing CTest target; no new files, public declarations, or CMake source-list changes. Extend the Actions test and source-order contracts.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 17:03 CST — Match ConvertUTF overlap regression diagnostics to source contracts

- Baseline: `fec862cb3ed9c5af8fc356d9d67264e25d9b6ae5`; SkipDBv2 Regression run #599 stopped in the static source-contract stage before CMake configuration.
- The new overlap tests built diagnostic messages by concatenating a function label with a common suffix, while the existing workflow contract deliberately requires each full regression diagnostic as a literal source needle.
- Keep the test strict and explicit: make each full error message a literal argument and let the shared macro print it unchanged. No production-code change, test-target change, or workflow relaxation.
- No compiler/CTest result was reached in run #599.

---

## 2026-11-07 17:02 CST — Reject overlapping ConvertUTF source and target ranges

- Baseline: `e4a645471e9c3dbb6437573e84ec4eb6d32165b4`; mainline SkipDBv2 Regression run #598 passed the source-contract stage, full build, and all 57 CTests.
- Scope: the six public buffer converters in `basekit/ConvertUTF.c`, their shared private range check, and the existing `convert_utf8_bounds_test`.
- Each converter walks input and output forward. If caller-supplied source and destination spans overlap, an early output write can overwrite source code units that have not yet been decoded.
- Add one overflow-checked private overlap predicate using byte extents and `uintptr_t` address differences. Reject overlapping spans with `sourceIllegal` before reading or writing caller data or changing the cursors; document the non-overlap contract.
- Extend the existing test across all six conversions, identical-start and partial overlap, state preservation, and a disjoint-range success case. Extend static and function-order Actions contracts. Reuse the current CTest target; no new files, public symbols, or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions remains the compiler/runtime gate.

---

## 2026-11-07 16:42 CST — Preserve BStream_show's full-width cursor

- Baseline: `1f24db0ecc4328cf76b9f16996d81d5dba367f6a`; SkipDBv2 Regression run #597 passed the source-contract stage, full build, and all 57 CTests.
- Scope: cursor save/restore in `basekit/BStream.c:BStream_show()` and the existing `bstream_tag_boundaries_test`.
- `BStream.index` is a `size_t`, but the debug display routine saved it in an `int`. On 64-bit builds (and at values beyond `INT_MAX`), invoking the diagnostic could truncate the caller's cursor when restoring it.
- Store the saved position as `size_t`. Add a focused regression using a tiny valid tagged stream and a synthetic cursor at `INT_MAX + 17`; `BStream_show()` must scan from zero and restore the exact original position without setting a read error.
- Reuse the current CTest target. Tighten the BStream source contract and add a focused workflow contract for save/reset/restore ordering and the wide-cursor regression. No new files, public declarations, header changes, or CMake changes.
- No local compiler/CTest result is claimed; GitHub Actions is the compiler/runtime gate.

---

## 2026-11-07 16:40 CST — Accept Date_second's guarded zero fallback in the accessor contract

- Baseline: `10b77b183a6d4033e93669eba40fcdbb76cd611d`; SkipDBv2 Regression run #596 passed the XY-C1 inventory and earlier Date contracts, then stopped in the Date accessor source-order assertion.
- `Date_second()` correctly checks the reentrant conversion and returns `0.0` when conversion fails. The contract matcher found the start of the `if (!Date_localTime_(...))` guard, which is textually just before the call itself, and mistakenly treated that guard position as the fallback result.
- Keep the accessor check strict: when no ternary `: 0` fallback exists, require `return 0.0;` after the explicit local-time failure guard. No production C or test changes.

---

## 2026-11-07 16:39 CST — Count the isolated Symbian localtime call, not its comment

- Baseline: `5ad3f6c9d8eb8c1ab64e1bab2678e82e26ba3b4d`; SkipDBv2 Regression run #595 passed the main source needles and failed the focused Date parsing/formatting contract before configure/build.
- The new assertion counted every `localtime(` substring in Date.c, so it counted both the intentional Symbian compatibility call and the explanatory comment describing the legacy API. The function-level checks already ensure parsing and formatting do not call it.
- Narrow the count to the exact Symbian fallback statement, `struct tm *shared = localtime(&timestamp);`. No production C logic, tests, CMake targets, or source lists changed.

---

## 2026-11-07 16:38 CST — Replace the active Date parsing template with reentrant conversion

- Baseline: `21caba8bf10a2912114d771d10b10adc17a95435`; SkipDBv2 Regression run #594 reached the focused Date source contract and rejected the still-active `EmptyTM()` implementation.
- Review of the contract exposed that the prior edit had inserted a caller-owned `EmptyTM()` before the format section instead of replacing the original definition. The old, later definition remained the one used by parsing and still called shared-buffer `localtime()`.
- Remove the accidental duplicate and update the sole original `EmptyTM()` to use `Date_localTimeForTimestamp_()`. Verify one definition remains and that the only raw `localtime()` call is isolated inside the Symbian compatibility branch. No public API, test target, or CMake change.

---

## 2026-11-07 16:37 CST — Align Date formatting contract with local calendar storage

- Baseline: `6e67144049dac0ddfd1bdf9d00e14cc597bf4730`; SkipDBv2 Regression run #593 stopped in the XY-C1 source-contract stage before configure/build.
- The production formatter now passes its stack-owned `calendar` to `strftime()`, but both the static integration needle and the focused Date formatting contract still described the former shared `tm` pointer.
- Update both contract expressions to require `&calendar`. No C source or test behavior changed; the next Actions run remains the compile/CTest gate.

---

## 2026-11-07 16:36 CST — Update Date local-calendar NULL guard contract

- Baseline: `4e71b2e311bdd5040acdc1e6b394442b95e241dd`; SkipDBv2 Regression run #592 passed the file/path inventory but stopped in the XY-C1 source-contract stage before configure/build.
- The source-contract map still expected the former two-argument Date local-time helper's NULL check. The reentrant helper now also requires a non-NULL caller-owned `struct tm` destination.
- Update that single static contract needle to require all three pointer checks. No C implementation, CTest, CMake, or build target changed; the next run is the compiler/runtime gate.

---

## 2026-11-07 16:35 CST — Align Date leap-year source contract with caller-owned calendar

- Baseline: `e3bc72ce0d503882bc8591ef347b43ddeac3ae63`; SkipDBv2 Regression run #591 stopped in the XY-C1 source-contract stage before CMake configure/build.
- The Date refactor correctly replaced the shared `tm` pointer with a stack-owned `calendar`, but the earlier static XY-C1 contract still required the old `tm->tm_year` expression. The dynamic Date contract had already been updated to the new storage.
- Change the single stale Date.c source needle to `calendar.tm_year`. No C implementation, test, CMake registration, or build target changed; the next run is the compile/CTest gate.

---

## 2026-11-07 16:34 CST — Use caller-owned calendar storage for Date local-time conversion

- Baseline: `8a97b71efe4b30de934e0b1f130ee6c1061ac2fb`; SkipDBv2 Regression run #590 passed source contracts, configure, full build, and all 57 CTests.
- Scope: the shared calendar conversion boundary in `basekit/Date.c`, all Date component accessors/setters, parsing's `EmptyTM()` template, and the existing Date formatting path.
- The shared `localtime()` result points to process-global static storage. Concurrent Date calls could overwrite that `struct tm` between conversion and use, making independent calendar reads race and setters normalize a different date/time than the one they started with.
- Convert into caller-owned `struct tm` storage via `localtime_r()` on POSIX and `localtime_s()` on Windows. Keep the legacy Symbian fallback isolated behind its platform guard. Route component getters/setters, parsing's epoch template, and `Date_asString()` through the shared conversion helpers; no Date entry point on the supported Windows/POSIX paths reads shared `localtime()` storage.
- Strengthen the existing Actions source/order contracts to require the reentrant platform calls, the three-argument receiver helper, caller-owned calendar fields, and the absence of direct shared-buffer use in formatting/parsing. The existing Date/Date-string CTest targets and CMake registration are reused; no new files, public declarations, or build-list changes.
- No local compiler/CTest result is claimed; remote Actions remains the compiler/runtime gate.

---

## 2026-11-07 16:24 CST — Refuse JFile journaling-mode changes while streams are live

- Baseline: `92f20c2df9a7e8c62f4b835103d6f83647ffa20a`; mainline SkipDBv2 Regression run #589 passed the XY-C1 source contracts, CMake configure, complete build, and all 57 CTests.
- Scope: the mode transition boundary in `jfile/JFile.c` and the existing `jfile_journal_test`.
- The public setter could disable journaling while the data and journal streams were open. If a previously committed journal had not yet been replayed, subsequent writes went directly to the data file while the old committed journal remained pending; the next commit/replay could overwrite those newer bytes with stale journal content.
- Permit mode changes only when both managed streams are closed. This preserves the existing pre-open configuration use while making an unsafe live transition a no-op.
- Extend the existing journal regression to reproduce a committed-but-unapplied record, attempt the live mode change, and prove the newer journaled value wins. Also verify that changing modes after close still enables the direct-write path and survives reopen. Extend Actions source, ordering, and runtime contracts. Existing CMake target reused; no new files, public declarations, or build-list changes.
- No local compiler/CTest result is claimed; remote Actions is the compiler/runtime gate.

---

## 2026-11-07 16:19 CST — Keep UDB commit-marker files exactly one byte

- Baseline: `db077c098b219810250a1cbf0b44f6c91191a6d2`; mainline SkipDBv2 Regression run #588 passed the source contracts, full build, and all 57 CTests.
- Scope: the durable marker writer in `udb/UDBCommitMarker.c` and its existing `commit_marker_test`.
- The marker reader rejects any trailing byte, but the writer only overwrote byte zero. A stale tail could therefore survive a successful write and durability sync, leaving a marker that the next startup would reject as corrupt.
- After flushing the new flag, truncate the opened marker to exactly one byte before the shared EINTR-safe sync barrier. Truncation failure propagates as a failed write, preserving the caller's fail-closed behavior.
- Extend the existing marker regression by appending trailing bytes, confirming the reader rejects the malformed representation, then rewriting and verifying the canonical one-byte file. Tighten the Actions source contract to require truncate-before-sync. Existing CMake target reused; no new files, APIs, or declarations.
- No local compiler/CTest result is claimed; remote Actions is the compiler/runtime gate.

---

## 2026-11-07 16:02 CST — Align Date overflow source contracts with errno capture

- Baseline: `e4c18fdc66a1e8aa65e6100b1d7ee247419dddda`; SkipDBv2 Regression run #587 failed in the early XY-C1 source-contract step before configure/build.
- The implementation and focused regression now capture `conversionErrno`, restore the caller's `errno`, then classify the `mktime()` result. A separate top-level required-source list still expected the old inline `errno` check and rejected the corrected implementation.
- Updated that remaining source-contract list to require the new capture/restore/failure-check sequence. No C source or test behavior changed in this correction.
- Remote Actions will rerun the full source contracts, configure, build, and 57-test CTest suite.

---

## 2026-11-07 16:01 CST — Restore caller errno after Date calendar failures

- Baseline: `5b6ddb24de439828ff48f793cd98b5f4641262eb`; SkipDBv2 Regression run #586 passed the full build and all 57 CTests.
- Scope: the private `Date_calendarToTime_()` conversion boundary and existing `date_time_init_test`.
- The helper saved the caller's `errno`, but restored it only after a successful `mktime()`. When an unrepresentable calendar value made `mktime()` return `(time_t)-1` with an error, the early return leaked that internal error into the caller's ambient errno state.
- Capture `mktime()`'s errno separately, restore the caller's errno on both paths, and test the captured conversion error when distinguishing a valid `-1` timestamp from overflow. Calendar and timestamp publication remain all-or-nothing.
- Extend the existing overflow regression to verify that both `Date_fromLocalTime_()` and `Date_setYear_()` preserve a pre-existing errno on conversion failure. Tighten the existing Actions source contract to enforce capture/restore/check order. No new files, public APIs, or CMake changes.
- No local build is claimed; remote Actions remains the compiler/CTest gate.

---

## 2026-11-07 15:59 CST — Repair Datum delimiter source-contract newline

- Baseline: `d57c81841b717e6d63a6ac3f5740422cf28ce53e`; SkipDBv2 Regression run #584 failed in the Python source-contract step before CMake configure/build.
- Failure cause: the newly added function-body assertion encoded its newline delimiter with one extra backslash. Python searched for the literal characters `\\n}` instead of the source newline followed by a closing brace, so it extracted an empty body and rejected the otherwise intended contract.
- Corrected the workflow delimiter to the normal Python newline escape. No Datum implementation or test code changed in this repair.
- Rerun the source contracts, configure, full build, and CTest on main.

---

## 2026-11-07 15:58 CST — Reject malformed Datum delimiter Lists

- Baseline: `2845f1c94e30be00eae2e27582922507964adfbd`; SkipDBv2 Regression run #583 passed source contracts, configure, full build, and CTest.
- Scope: delimiter traversal in `Datum_find_()` and `Datum_split_()`, plus the existing `datum_boundaries_test`.
- Both APIs trusted the public List size without checking its backing-storage metadata. A forged size beyond the pointer capacity could turn a tiny malformed descriptor into an oversized scan; additionally, the loops narrowed `size_t` indices to `long` even though `List_at_()` accepts `size_t`.
- Validate delimiter List storage before traversal, snapshot the validated element count once, and pass `size_t` indices directly to `List_at_()`.
- Extend the existing Datum boundary regression with a valid lookup and a malformed shallow List descriptor that both lookup and split must reject. Extend Actions source/runtime contracts; reuse the existing CMake target. No new files, public APIs, or cross-translation-unit symbols.
- No local build is claimed; remote Actions remains the compiler/CTest gate.

---

## 2026-11-07 15:56 CST — Preserve the argv NULL sentinel in MainArgs

- Baseline: `4ef5aafca2a92b3b5832b11d7974f8a8125928ab`; SkipDBv2 Regression run #583 passed source contracts, configure, full build, and CTest.
- Scope: `MainArgs_argc_argv_()` ownership/allocation semantics and the existing `main_args_lifecycle_test`.
- The setter deep-copied exactly `argc` entries but did not reserve the conventional `argv[argc] == NULL` terminator. It also represented `argc == 0` as a NULL vector, so callers could not safely use the normal C argv termination convention.
- Allocate an overflow-checked `argc + 1` pointer array for every valid replacement, including an empty vector. `io_calloc` initializes the trailing sentinel; the existing transactional copy/publish/free sequence remains unchanged. Document the invariant in the public header.
- Extend the existing lifecycle regression to require the sentinel after initial assignment, replacement, and empty assignment. Extend Actions source/header/runtime contracts. Existing CMake target reused; no new files, exports, or build-list changes.
- Remote Actions remains the compiler/CTest gate; no local build is claimed.

---
## 2026-11-07 15:53 CST — Snapshot UArray format strings that alias the destination

- Baseline: `47751ce1ef0306ec1613215a5da4214380afc7ba`; mainline run #582 passed source contracts, configure, full build, and CTest.
- Scope: `UArray_fromVargs_()` in `basekit/UArray_format.c` and the existing `uarray_format_trailing_percent_test`.
- The formatter advanced through the caller's `format` pointer while appending literal and conversion output to the same `UArray`. When `format` pointed into that array's backing bytes, append could overwrite its terminator or reallocate the backing store, leaving the parser to consume mutated data or a dangling pointer.
- Detect overlap between the complete C format string and the receiver's logical storage using checked byte-size arithmetic and `uintptr_t` address differences. Snapshot only an overlapping format before any append, preserve the receiver unchanged if snapshot allocation fails, and release the snapshot through a shared cleanup path.
- Extend the existing regression by using the receiver's own `value %i` bytes as the format and verifying the appended result is `value %ivalue 7`. Extend the existing Actions source-order and runtime contracts. Existing CMake/CTest registration reused; no new files, exports, or build-list changes.
- No local build is claimed; remote Actions is the compiler/CTest gate.

---
## 2026-11-07 15:42 CST — Avoid reading tm_hour for standalone PortableStrptime %p

- Baseline: `17736e0ffdf6ed591947587e8f4a51d93b784c72`; mainline run #581 passed source contracts, configure, full build, and CTest.
- Scope: `%I`/`%l` and `%p` handling in `basekit/PortableStrptime.c`, plus the existing `portable_strptime_bounds_test`.
- `%p` read `tm->tm_hour` even when no 12-hour field had initialized it. Callers can parse a meridiem token into a partially initialized `struct tm`; inspecting that indeterminate field is undefined behavior. Format order also meant `%p %I` lost the meridiem adjustment.
- Track whether `%I`/`%l` has supplied an hour and retain the AM/PM marker independently. Apply the marker only after the 12-hour value exists, so `%p` alone consumes its input without reading or modifying unrelated `struct tm` fields, and `%p` before `%I` still yields the correct 24-hour result.
- Extend the existing regression for standalone `%p` against byte-patterned `struct tm`, 12 AM/12 PM/1 PM conversions, and meridiem-before-hour ordering. Extend Actions source and semantic contracts. Existing CMake target is reused; no new files, exports, or build-list changes.
- No local build is claimed; remote Actions remains the compiler/runtime gate.

---
## 2026-11-07 15:39 CST — Correct BStream numeric-read contract delimiters

- Baseline: `9be024ebbea4798a96f3f2aba5297f6beef5f394`; run #580 failed in the Python source-contract phase before configure/build.
- The new contract used double-escaped newline and tab sequences when extracting `BStream_readNumber_size_()`. Python therefore searched for literal backslash characters, failed to find the function boundary and alias-guard closing brace, and rejected the valid C implementation.
- Correct the two Python string delimiters to match the actual source newlines and tab indentation. No C implementation or runtime regression code changed.
- Rerun the full contract, configure, build, and CTest gates.

---
## 2026-11-07 15:38 CST — Reject numeric-read destinations that alias BStream storage

- Baseline: `af336c118844f99de94318e83c5f4e18124f0850`; mainline run #579 passed source contracts, configure, full build, and CTest.
- Scope: `BStream_readNumber_size_()` in `basekit/BStream.c` and the existing `bstream_tag_boundaries_test`.
- The numeric reader copied bytes with `memcpy()` into a caller-provided destination without checking whether that destination overlapped the stream's own backing allocation. Overlapping ranges make the copy undefined; the subsequent error/zero-fill path could also corrupt bytes that are still the stream's input.
- Reuse the existing byte-range overlap helper before the latched-error branch and before `memcpy()`. Reject an aliased destination, preserve an earlier latched error, leave the cursor unchanged, and never zero-fill the aliased buffer because it is stream storage.
- Add a regression that aliases the destination into the stream buffer, checks the first rejection preserves all bytes and cursor state, then repeats the call with the error latched to prove the second call cannot zero stream data. Declare the existing internal function explicitly in the test translation unit and extend the current Actions source/order contracts.
- Existing `bstream_tag_boundaries_test` CMake registration is reused; no new files, exported APIs, or build-list changes. No local build is claimed; Actions is the compiler/CTest gate.

---
## 2026-11-07 15:34 CST — Repair Date directive token quoting in CI source contract

- Baseline: `63c2f096ceebdbec1c0fbb36456569c2f037d5f9`; run #578 stopped inside the Python source-contract script before configure/build.
- The test-token contract wrapped a C macro containing double quotes in an unescaped Python double-quoted string, causing a Python `SyntaxError`.
- Switch that single contract needle to a Python single-quoted string. The Date implementation and C regression are unchanged.
- Rerun the full mainline contract, compiler, and CTest gates.

---
## 2026-11-07 15:32 CST — Retry Date formatting when directives expand past the initial buffer

- Baseline: `576b5cfd2e70351adf13f9745c29ae37859f6753`; mainline run #577 passed source contracts, configure, build, and CTest.
- Scope: `Date_asString()` in `basekit/Date.c` and the already registered `date_string_bounds_test`.
- The existing allocation was sized from format-string bytes plus 1024, but `strftime()` output can be substantially longer than its input directives. On a long repeated `%Y-%m-%d` format, `strftime()` returned zero for insufficient capacity and the API silently returned an empty string.
- Retry `strftime()` with geometrically growing UArray capacity, checking each allocation and bounding retries to a saturating 64x format-length ceiling. Preserve the existing empty-result behavior when formatting still returns zero; no exported symbols or CMake targets changed.
- Extend the existing test with 1,500 repeated date directives and verify every expanded token against a single-token result. Extend the Actions source/runtime contracts. No new files or public API changes.
- Static review confirms size arithmetic is guarded, buffer capacity is revalidated after each resize, and the final UArray size comes from the formatter's reported byte count. No local build is claimed; remote Actions is the compiler/CTest gate.

---
## 2026-11-07 15:29 CST — Repair adjacent SkipDB source-contract entries

- Baseline: `5f3e2fa7d3a2e5de4381afe028e3e1c0a02e6dc3`; run #576 stopped in the XY-C1 source-contract phase before configure/build.
- The new regression token was inserted into the existing `skipdb_core_layers.c` contract array without a trailing comma, concatenating it with the following cursor token and making the contract parser search for a nonexistent combined string.
- Restore the array delimiter only. The SkipDB code and regression fixture are unchanged.
- No local build is claimed; rerun source contracts, compile, and CTest on main.

---
## 2026-11-07 15:27 CST — Validate cached SkipDB forward-link identity

- Baseline: `d2771b90bb65ea3465d6934c79638503f9816cab`; SkipDBv2 Regression run #575 passed source contracts, configure, full build, and CTest.
- Scope: `SkipDBRecord_recordAtLevel_()` in `skipdb/SkipDBRecordLinks.c` and the existing `skipdb_core_layers` regression.
- A cache-resident `SkipDBPointer.record` was returned without checking that the pointed record still belonged to the source database or that its nonzero cached PID still matched the pointed record. A stale/cross-database pointer could therefore bypass the authoritative on-disk PID link and send ordered traversal into another database's records.
- Before returning a cached target, require matching database ownership and, when the link carries a nonzero PID, exact PID identity. Invalid cache metadata closes the source UDB rather than being reported as an ordinary end-of-list; PID-zero transient in-memory links remain supported.
- Extend the existing core-layers test with cross-database pointer and same-database PID-mismatch corruption fixtures. Each must fail closed and permit recovery from the persisted source database. Extend existing Actions source contracts; the CTest target is already registered and no CMake or public-header change is needed.
- Static review confirms no new cross-translation-unit symbols or source files; remote Actions remains the compiler/runtime gate. No local build is claimed.

---
## 2026-11-07 15:23 CST — Correct free-callback contract newline delimiter

- Baseline: `550aef6dc0f938f5074dbeeb7ae592ffd4893def`; run #574 stopped in the Actions source-contract phase before CMake configure.
- The new callback-order contract searched for a literal backslash-n sequence instead of the newline preceding the C function's closing brace, so it extracted an empty function body and rejected the otherwise matching implementation.
- Change the source-contract delimiter to the normal Python newline escape. The C implementation, regression, CMake registration, and public interfaces are unchanged.
- No local build is claimed; rerun the mainline Actions contract, compiler, and CTest gates.

---
## 2026-11-07 15:20 CST — Detach SkipDB cache records before object-free callbacks

- Baseline: `8d5c717bb6fd31aeb69ed5adf63261aa5a89dade`; SkipDBv2 Regression run #573 completed successfully.
- Scope: `SkipDB_noteWillFreeRecord_` in `skipdb/SkipDBCache.c` and its existing core-layers regression.
- The cache teardown path invoked the user-supplied object-free callback while the record was still linked in the age list and PID cache. If that callback reentered `SkipDB_clearCache()`, nested teardown could discover the same record again, invoke the callback twice, and free the record while the outer deallocator was still using it.
- Validate record ownership, remove the record from the age list and PID cache, decrement the cache count, and clear its database/object association before invoking arbitrary callback code. No database/cache state is accessed after the callback.
- Extend the existing `skipdb_core_layers` test with a callback that recursively clears the same database and requires exactly one finalization plus successful database recovery. Extend the existing Actions token and ordering contracts. The test is already registered in CMake; no new files, exported symbols, or build-list changes.
- Static review confirms the database ownership guard and all cache detachment precede the callback; remote Actions remains the compiler/CTest gate. No local build is claimed.

---
## 2026-11-07 15:08 CST — Preserve UArray path source-contract function discovery

- Baseline: `3613cef7c01adac5480c3fbf1f1f0b45cad847d5`; run #572 completed its file-token checks but a legacy function-body contract failed before configure.
- Cause: the path file already defines each private helper before its first use. The extra forward declarations added during the alignment change were unnecessary and caused an existing source-contract parser to mistake the `UArray_pathComponentBounds_` prototype for the function definition.
- Removed only those four redundant static prototypes. All helper definitions remain file-local and ordered before callers; the per-CTYPE alignment guard and new regression stay unchanged.
- This restores the existing contract's function-body discovery without weakening or rewriting the check. No header, CMake, test behavior, or exported symbol changed; remote Actions remains the compiler/CTest gate.

---

## 2026-11-07 15:06 CST — Make path alignment CI anchors newline-safe

- Baseline: `6b3d50451b21b87a49ecb9b492c97b7f7bd17ad2`; run #571 reached the regression-contract step, where the source-range assertion still failed.
- Failure cause: the YAML-embedded Python string contained a double-escaped newline, so its search literal represented the two characters `\\n` rather than the source file's line break. The previous function-definition anchors were therefore not found.
- Replaced both escaped-newline literals with explicit `chr(10)` concatenation. This preserves exact function-definition matching without depending on nested YAML/Python/JavaScript escape interpretation.
- No C implementation or test logic changed. Remote Actions remains the compiler/CTest gate.

---

## 2026-11-07 15:03 CST — Fix path alignment contract function-boundary anchors

- Baseline: `0b29e3d0061ce8d6ee2ce66c688439b868af375a`; run #570 passed the file-content contracts but failed the newly added ordering assertion before CMake configure.
- Failure cause: the assertion located `UArray_hasValidPathStorage_` and `UArray_pathComponentBounds_` by signatures that also matched their forward declarations. With static prototypes present, the extracted range was empty.
- Tightened both anchors to include the function-opening brace and the complete component-bounds definition signature, so the assertion examines the validator definition rather than declarations.
- No C implementation, test behavior, build registration, or public interfaces changed. Remote Actions remains the compile/CTest gate.

---

## 2026-11-07 15:00 CST — Repair UArray path alignment CI source contract

- Baseline: `244450e4c451a94b2fed96a0643775504f561b73`; SkipDBv2 Regression run #569 stopped in the XY-C1 source-contract step before configure/build.
- Failure cause: the workflow contract spelled the static probe as `UArrayPathUint64AlignmentProbe`, while the implementation's identifier is `UArrayPathUInt64AlignmentProbe`. The implementation and regression fixture were not reached by compilation; this was a source-contract token mismatch.
- Corrected only the workflow token to match the actual static probe identifier. No C source, test logic, CMake registration, or public interface changed.
- Validation: compared the exact identifier against `UArray_path.c`; remote Actions remains the authoritative compiler and CTest gate.

---

## 2026-11-07 14:55 CST — Reject misaligned UArray path storage

- Baseline: `cee9633ef50b18c87d97cec5155e3a11f2038008`; main's SkipDBv2 Regression run #568 completed successfully.
- Scope: `UArray_hasValidPathStorage_` in `basekit/UArray_path.c`.
- The path preflight checked CTYPE width, extent overflow, LONG_MAX bounds, and backing-storage presence, but not the natural alignment required by typed first/last-item access and typed searches. Offset borrowed views could therefore pass validation and reach undefined typed reads on strict-alignment targets.
- Added static per-CTYPE alignment probes and reject misaligned data before path queries, conversions, mutations, or append operations. Extended the existing path regression with a one-byte-offset uint64 view; bytes are initialized and checked with memcpy/memcmp.
- Extended the existing Actions source and regression contracts. The CTest target was already registered; no new files, public declarations, CMake source-list changes, or exported symbols.
- Validation: source-contract ordering requires the alignment preflight before the shared path component scanner and all public entry points. Remote Actions remains the compiler/CTest gate; no local build is claimed.

---

## 2026-11-07 14:52 CST — Validate alignment in UArray distance vectors

- Baseline: `23a6f0cbbbafa1d0c476624ccfec70cb31058508`; Datum byte-extent validation passed SkipDBv2 Regression run #570.
- Scope: distance-vector preflight in `basekit/UArray_math.c`.
- The general numeric math validator already enforces each CTYPE's natural alignment, but the dedicated float32/float64 distance-vector validator checked only item width, extent overflow, and non-NULL storage. `UArray_distanceTo_` then dereferenced the backing pointer as a typed float array, leaving a strict-alignment undefined-behavior path.
- Reused the existing per-CTYPE alignment lookup in the distance validator. Extended the existing distance precision test with deliberately offset float32 buffers; their mathematically valid distance is 5, so the API must reject the misaligned inputs and return its invalid-input sentinel 0.
- Updated the existing Actions source contracts. The test is already registered in CMake; no build-file changes, new files, or exported symbols.
- Validation: fixture bytes are populated only through `memcpy`; remote CI is the compiler/CTest gate, with no local build claimed.

---

## 2026-11-07 14:48 CST — Reject impossible SIZE_MAX string-item extents

- Baseline: `1c7610362f1525d401d4f70e6f4e1a361927fc36`; core UArray SIZE_MAX layout regression passed SkipDBv2 Regression run #566.
- Scope: shared typed-string layout validation in `basekit/UArray_string.c`.
- The string-item validator enforced CTYPE width, multiplication overflow, and natural alignment, but still admitted an exact `SIZE_MAX` byte extent for one-byte strings. String mutators use this guard before indexing or iterating, so forged public metadata could bypass the storage invariant that reserves a trailing NUL byte.
- Added the exact-size sentinel rejection after the overflow bound. Extended the existing unescape/string regression with a borrowed-size forgery and a two-byte swap operation; it must leave the byte sequence and cached hash unchanged.
- Updated existing Actions source contracts. The test is already registered in CMake; no build-file changes, new files, or exported symbols.
- Validation: the regression operation would touch only the two real bytes if the guard were absent, avoiding out-of-bounds test behavior while detecting acceptance. Remote CI remains the compile/CTest gate; no local build is claimed.

---

## 2026-11-07 14:44 CST — Close the core UArray SIZE_MAX typed-layout gap

- Baseline: `5bfc80f78043e1ff7a91150b340e01c49cb6108f`; character-array SIZE_MAX regression passed SkipDBv2 Regression run #565.
- Scope: shared `UArray_hasValidItemLayout_` preflight in `basekit/UArray.c`.
- The central CTYPE validator rejected multiplication overflow and misaligned storage, but like the specialized stream/character validators it still admitted an exact `SIZE_MAX` byte extent for one-byte arrays. This left core scalar writes and stack-range construction able to trust metadata that cannot reserve UArray's mandatory trailing NUL byte.
- Added the exact byte-extent sentinel rejection after the division-based overflow guard. Extended the existing stack/resize regression to require scalar writes to preserve the backing bytes and hash, and require stack-range creation to return an empty safe view for a forged SIZE_MAX byte extent.
- Updated the existing Actions source contracts. The test is already registered in CMake; no build-file changes, new files, or exported symbols.
- Validation: every multiplication is evaluated only after the overflow bound; the test uses a two-byte borrowed buffer and exercises only guarded entry points. Remote CI remains the compile/CTest gate; no local build is claimed.

---

## 2026-11-07 14:40 CST — Reject impossible SIZE_MAX character-array extents

- Baseline: `6d50d548c067278d1796ba892a41eed6145ad316`; UArray stream extent regression passed SkipDBv2 Regression run #564.
- Scope: the shared byte-array preflight in `basekit/UArray_character.c`.
- The character API accepts only one-byte CTYPEs, but its multiplication-overflow test could never reject the exact `SIZE_MAX` byte extent. UArray storage requires a trailing NUL byte, so such a logical extent is not representable; the predicate/case-map loops could otherwise traverse an impossible public-struct size.
- Added an explicit exact-SIZE_MAX rejection after the overflow guard. Extended the existing ctype test with a shallow borrowed view whose size is forged to SIZE_MAX; every predicate/case-map entry point must leave the underlying bytes and cached hash untouched, while case queries return false.
- Updated the existing Actions source contracts. The test is already registered in CMake; no build-file changes, new files, or exported symbols.
- Validation: the byte-count multiplication is evaluated only after its division bound; the regression uses existing live storage but exercises only the preflight path. Remote CI remains the compile/CTest gate; no local build is claimed.

---

## 2026-11-07 14:37 CST — Validate byte-array geometry before BStream attachment

- Baseline: `247619af3bcb549ac31ef727da6efab8cefaf1e7`; main's SkipDBv2 Regression run #567 completed successfully.
- Scope: `BStream_setUArray_` and the existing `testBStreamSetUArrayOwnership` case.
- The attachment boundary accepted any UArray with `itemSize == 1`, even when its declared CTYPE had a different natural width, and also accepted an exact `SIZE_MAX` logical extent. Attaching such a view could discard the currently owned buffer and make subsequent stream reads trust invalid byte geometry.
- Require the uint8 CTYPE width to match and reject the unrepresentable exact-size sentinel before alias checks or ownership transfer. Add borrowed-view regressions for both malformed metadata cases and verify the original owned buffer, ownership flag, and cursor remain unchanged.
- Extend the existing BStream source and test contracts. The boundary test is already registered in CMake; no new files, public declarations, or build-list changes.
- Validation: the test never reads the forged length and keeps the real external byte buffer unchanged. Remote Actions remains the compiler/CTest gate; no local build is claimed.

---

## 2026-11-07 14:36 CST — Reject the unrepresentable SIZE_MAX UArray stream extent

- Baseline: `b8bbfbc937a4d81f7ffdbab1f02852fbe33cbfd3`; BStream expected-tag validation passed SkipDBv2 Regression run #563.
- Scope: the shared UArray stream layout preflight in `basekit/UArray_stream.c`.
- UArray storage reserves one extra byte for its trailing NUL, and core allocation paths reject a logical byte extent equal to `SIZE_MAX`. The stream validator rejected multiplication overflow but admitted the exact `SIZE_MAX` result for one-byte items. Direct fread/fwrite could then trust impossible public-struct metadata and issue I/O against an allocation that cannot represent that extent.
- The validator now rejects the sentinel extent after its overflow guard. Extended the existing stream I/O regression with a deliberately forged SIZE_MAX byte-array extent and non-reading/non-writing cookie callbacks; all direct and buffered readers plus direct fwrite must reject it without invoking I/O or mutating the array.
- Updated existing Actions source contracts. The stream test is already registered in CMake; no build-file changes, new files, or exported symbols.
- Validation: the byte-count multiplication is evaluated only after the preceding division bound proves it cannot overflow. Remote CI remains the compile/CTest gate; no local build is claimed.

---

## 2026-11-07 14:33 CST — Validate expected BStream tag fields before reading

- Baseline: `f745eb4ace0b51178bd26179effb94d090ce0421`; DynLib callback-reentrancy fixture passed the full SkipDBv2 Regression run #562.
- Scope: the internal `BStream_readTag` field-validation and cursor-consumption boundary.
- The writer rejects tag fields outside the wire format (`array <= 1`, `type <= 3`, `byteCount <= 31`), but the reader constructed its expected tag directly into narrow bit-fields. Invalid caller expectations could therefore truncate and alias a valid wire tag, while consuming the stream byte before reporting the mismatch.
- Added the same range validation to the reader before calling `BStream_readUint8`; invalid expectations now set a deterministic error and preserve the cursor and stream byte. Extended the existing BStream tag-boundary test for all three invalid fields and the maximum valid combination.
- Updated the existing Actions source contracts. The test is already registered in CMake; no build-file changes, new files, or exported symbols.
- Validation: reviewed the guard ordering so caller arguments are checked before any read; remote CI remains the compile/CTest gate, no local build is claimed.

---

## 2026-11-07 14:32 CST — Reject impossible UArray extents at the Datum boundary

- Baseline: `4b531381b1dc5a19ed766bac3be67d5334725410`; SkipDBv2 Regression run #565 passed the source contracts, configure, full build, and CTest.
- Scope: `Datum_FromUArray_` and the existing `datum_boundaries_test`.
- The adapter checked multiplication overflow but accepted an exact `SIZE_MAX` byte extent. A forged public UArray view could therefore be exposed as a Datum span even though UArray's storage contract cannot represent that extent with its trailing terminator.
- Compute the byte count only after the division-based overflow guard, reject the exact sentinel before obtaining the raw byte view, and reuse the checked count for the returned Datum. Extend the existing boundary test with a forged one-byte-item UArray extent; restore the fixture metadata before freeing it.
- Extend the existing Actions source and regression contracts. The CTest target was already registered; no new files, public declarations, or CMake changes.
- Validation: source-contract ordering now requires CTYPE validation, overflow validation, multiplication, sentinel rejection, and only then raw-byte exposure. Remote CI remains the compiler/CTest gate; no local build is claimed.

---

## 2026-11-07 14:29 CST — Repair DynLib fixture CMake contract gate

- Baseline: `5f571bc7b3109649b292bbba49b4b5a173b69880`; run #561 failed only in the XY-C1 integration-contract step before CMake configure/build.
- The new reentrant DynLib fixture and parameterized CTest command were present in `CMakeLists.txt`, but the workflow's Python contract still asserted the old no-argument `add_test` line.
- Updated that existing assertion to require the fixture shared-library target, source path, test dependency, and generator-expression argument. No production or test logic changed.
- Validation: exact CMake strings were compared against the committed target block; remote CI will validate configure/build/CTest.

---

## 2026-11-07 14:25 CST — Prevent recursive DynLib free-callback unload

- Baseline: `51cba03394b7bd86a0bdc28f91b0de5df37026ea`; UArray pointer-alignment regression passed SkipDBv2 Regression run #560.
- Scope: `DynLib_close` callback/loader-handle lifecycle.
- The old close path invoked the plugin free callback while `self->handle` still referenced the open library. If the callback re-entered `DynLib_close(self)`, the same callback was resolved and invoked recursively, potentially unbounded, and a nested close could unload code that was still executing.
- Snapshot the original handle, resolve the callback, then detach `self->handle` before executing plugin code. Reentrant close now observes the detached/closing state and is a no-op; the outer call performs the single OS close. If `dlclose` fails, restore the original handle only when the callback did not reopen the object.
- Added a test-only shared-library fixture and extended `dynlib_lifecycle_test` to invoke a callback that recursively calls `DynLib_close`; CMake now builds the fixture and passes its target path to the test. Updated the workflow's source and CMake contracts for the new fixture.
- Validation: source-level review confirms callback lookup happens while the handle is open, detachment precedes callback invocation, and the outer close owns the final `dlclose`. Remote CI remains the compile/runtime gate; no local build is claimed.

---

## 2026-11-07 14:19 CST — Reject misaligned UArray pointer arrays

- Baseline: `3b8e9d8315e4bc2143c14ee769b9520e645cde91`; core CTYPE alignment passed SkipDBv2 Regression run #558.
- Scope: pointer-array access guard `UArray_hasValidPointerStorage_` and its existing `uarray_pointer_access` test.
- Pointer getters and setters cast backing bytes to `void **`. The existing guard verified pointer width and allocation bounds but accepted misaligned borrowed storage, leaving undefined typed reads/writes on strict-alignment targets.
- Added a `void *` alignment probe and reject misaligned backing addresses before raw/bounds-checked pointer reads, writes, or appends. The regression uses a byte-offset borrowed pointer slot and verifies getters return NULL while setters preserve the original bytes, size, and cached hash.
- Updated existing Actions source contracts. The test was already registered in CMake; no source lists, new files, or exported symbols changed.
- Validation: source review confirms all pointer getters/setters route through the shared guard before casting to `void **`. Remote CI remains the build/CTest gate; no local build is claimed.

---

## 2026-11-07 14:16 CST — Enforce CTYPE alignment in core UArray layout validation

- Baseline: `8d98d011854e7ad1e179be8e4c2654c1522ca5a2`; numeric math alignment passed SkipDBv2 Regression run #557.
- Scope: the shared `UArray_hasValidItemLayout_` guard in `basekit/UArray.c`, which protects typed copy/conversion, scalar writes, sorting, and stack-range creation.
- The core validator checked CTYPE width, extent overflow, and non-empty data presence, but still accepted a correctly sized yet misaligned borrowed pointer. Those call sites subsequently use typed C lvalues or comparators, which can be undefined on strict-alignment platforms.
- Added a per-CTYPE alignment lookup using `offsetof` probes and reject misaligned backing storage before the shared typed operations run. Extended the existing stack/copy regression test to cover misaligned copy sources and destinations, stack ranges, and scalar writes; fixtures are initialized and inspected through byte operations.
- Updated existing Actions source contracts. The test was already registered in CMake; no build files, new files, or exported symbols changed.
- Validation: reviewed every call site of the shared validator to ensure the check precedes typed access. Remote CI is the compile/CTest gate; no local build is claimed.

---

## 2026-11-07 14:11 CST — Validate alignment before UArray numeric math

- Baseline: `876df8eb6db3b47274bff6cbef3297d311509186`; numeric conversion alignment passed SkipDBv2 Regression run #556.
- Scope: shared typed-numeric storage preflight in `basekit/UArray_math.c`.
- Math entry points use typed iteration, scalar writes, reductions, and a float32 vector fast path after validating item width and byte geometry. They did not validate the alignment required by the declared CTYPE, so misaligned borrowed storage could reach typed reads/writes.
- Added per-CTYPE alignment probes to the shared numeric-storage validator. Extended the existing extrema/math regression test to verify clear/fill operations leave misaligned uint64 storage unchanged and dot-product rejects it.
- Updated existing Actions source contracts only; CMake registration and source lists are unchanged. No new files or exported symbols.
- Validation: static review confirms all typed math paths use the shared validator before dispatch; remote CI remains the compiler/CTest gate, no local build is claimed.

---

## 2026-11-07 14:06 CST — Reject misaligned numeric UArray conversion sources

- Baseline: `b2bbd1ff47146eec03713ac249a1ec8ab9fd8aee`; SkipDBv2 Regression run #554 passed after the typed string-storage alignment change.
- Scope: numeric-array conversion validation in `basekit/UArray_utf.c`.
- The number-array formatter uses typed iteration after checking CTYPE width and allocation bounds, but previously accepted unaligned borrowed storage. Since numeric UArrays are converted through this path by all UTF output conversions, misaligned numeric data could trigger undefined typed reads.
- Added per-CTYPE alignment probes to the existing numeric-layout validator and extended `uarray_encoding_conversion_test` with a misaligned uint64-backed numeric source. UTF-8/16/32 conversion entry points must now reject it without modifying the borrowed bytes.
- Updated existing Actions source contracts. CMake registration and source lists are unchanged; no new files, exported symbols, or unrelated modules.
- Validation: static review confirms numeric formatting validates before `UARRAY_INTFOREACH`; remote CI is the build/CTest gate, with no local build claimed.

---

## 2026-11-07 14:00 CST — Reject misaligned typed UArray string storage

- Baseline: `c7771fb5b3cba1afb8ea2ff38d36b37362392784`; preceding Unicode alignment commit passed SkipDBv2 Regression run #553, including configure, all targets, and CTest.
- Scope: typed-item layout validation in `basekit/UArray_string.c`.
- The existing string-item validator checked CTYPE width and allocation bounds but not per-type alignment. String operations such as unescape and trim can read elements through typed UArray accessors, making borrowed or stack-backed misaligned numeric storage undefined on strict-alignment architectures.
- Added exact per-CTYPE alignment probes and reject misaligned backing pointers before typed iteration or mutation. Extended the existing unescape regression test with misaligned uint64-backed inputs that must remain byte-for-byte unchanged.
- Updated the existing Actions source contracts for the invariant and regression. CMake registration and source lists remain unchanged; no new files or exported symbols.
- Validation: source-level review verified alignment is checked before every operation using the shared validator, and test fixtures deliberately offset byte storage while keeping accesses byte-oriented. Remote CI is the build/test gate; no local build is claimed.

---

## 2026-11-07 13:55 CST — Reject misaligned UTF-16/UTF-32 UArray storage

- Baseline: `30e3da52588ef464c7ab7f687e1b97a3edd70d4f`; SkipDBv2 Regression run #552 passed XY-C1 integration contracts, configure, all targets, and CTest.
- Scope: Unicode encoding layout validation in `basekit/UArray_utf.c` and its existing `uarray_encoding_conversion_test`.
- The encoding validator checked element width and byte capacity but not the alignment required when ConvertUTF reads UTF-16/UTF-32 code units through typed pointers. A caller-provided stack-backed or borrowed UArray could therefore pass validation and trigger undefined behavior on strict-alignment targets.
- Added alignment checks using `offsetof` probes for the actual UTF code-unit types; included `<stdint.h>` directly for `uintptr_t`. The existing conversion test now exercises misaligned UTF-16 and UTF-32 inputs across the conversion entry points and requires rejection.
- Updated the existing GitHub Actions source contracts to bind the invariant and regression coverage. CMake registration and source lists are unchanged; no new files or exported symbols.
- Validation: static review confirmed validation precedes all conversion dispatch and the test's deliberately offset byte buffers are never dereferenced as UTF code units. Remote CI remains the build/CTest gate; no local build is claimed.

---

## 2026-11-07 13:52 CST — Repair List alignment regression test dependency

- Baseline: main commit `181bc9b8f98fdc07dbf961b9bf391249fa24498b`; SkipDBv2 Regression run #551 failed in the XY-C1 source-contract stage before CMake configure/build/CTest.
- The existing `list_storage_bounds.c` regression uses `uintptr_t` to construct a deliberately misaligned List view, but the test omitted its direct `<stdint.h>` include. The workflow correctly requires the test to declare that dependency itself.
- Added the missing standard header to the existing test. No production logic, new files, CMake registration, or workflow contracts changed.
- Validation: verified the failing contract's missing token against the test source and confirmed the implementation-side `List_inline.h` already includes `<stdint.h>` for its own `uintptr_t` use. Remote CI remains the compiler/CTest gate; no local build is claimed.

---

## 2026-11-07 13:47 CST — Reject misaligned List backing storage

- Baseline: main commit `8ffeb9ce6e50096f55e9918b0c968c57ce994d02`; SkipDBv2 Regression run #550 passed the source contracts, configure, full build, and all CTest regressions.
- Scope: the shared storage invariant in `basekit/List_inline.h` and the existing `list_storage_bounds_test`.
- `List_hasValidStorage_()` checked logical size against allocated byte capacity and null storage, but accepted a non-NULL `items` pointer that was not aligned for `void *`. The inline readers and mutators could then dereference misaligned pointer elements, which is undefined behavior and can fault on strict-alignment targets.
- Add a `uintptr_t` alignment check before any element access. This brings List's backing-storage validation in line with the established Stack invariant without changing valid List allocation or growth behavior.
- Extend the existing storage-boundary test with a deliberately misaligned shallow view. Verify reads reject it, mutations leave all metadata untouched, and `List_asStackAllocatedUArray()` does not expose it.
- No new files, exported symbols, or CMake changes. The existing `list_storage_bounds_test` remains registered; workflow source contracts now require the alignment guard and its regression coverage.

### Validation
- Static preflight: checked the guard executes inside the central validator before callers access elements, `stdint.h` supplies `uintptr_t`, all misaligned-view operations are expected to fail closed, and cleanup retains ownership of the original allocation only.
- No local compiler/CTest run is claimed. Mainline Actions is the verification gate.

---

## 2026-11-07 13:43 CST — Repair PortableStrptime source-contract mapping

- Baseline: main commit `7f7e090420e834d855d423d35e534053874e2c1e`; Actions run #549 failed immediately with zero allocated jobs, before checkout or build.
- The preceding change accidentally inserted a second `PortableStrptime.c` key into the workflow's Python source-contract mapping and displaced the indentation of the following test key. The duplicate dictionary key also shadowed the existing source-contract list.
- Merge the unsupported-directive assertions into the original C-source contract, remove the duplicate key, restore the test key's mapping indentation, and retain the new `%Q`/`%z` regression needles in the existing test contract.
- No C implementation or test code changed in this CI-only repair. CMake registration remains unchanged.

### Validation
- Static preflight requires exactly one source key and one test key, verifies both are indented as sibling mapping entries, confirms the original source contract remains present alongside the new default-branch needles, and confirms all new regression needles remain in the test block.
- Run #549 failed before a job was created; this push triggers the corrected workflow run and is the next build/CTest gate.

---

## 2026-11-07 13:41 CST — Reject unsupported PortableStrptime directives

- Baseline: main commit `5621c6f0e847c743ba6f5dee99341239b2234840`; SkipDBv2 Regression run #548 passed source contracts, configure, the complete build, and all CTest cases.
- Scope: conversion-specifier dispatch in `basekit/PortableStrptime.c` and the existing `portable_strptime_bounds_test`.
- The parser's switch had no default branch. An unsupported directive such as `%Q` or an unimplemented standard directive such as `%z` was silently skipped, allowing an invalid format to return a successful input cursor without consuming the directive's expected data.
- Add an explicit rejection path for unsupported conversion specifiers. Exercise both an unknown directive and an unsupported timezone directive in the existing boundary test; retain all supported directive behavior unchanged.
- Reuse the existing CMake test registration and add a workflow source contract for the rejection branch. No new files or exported declarations.
- Static preflight: confirmed the default branch is inside the format dispatch switch, invalid formats return NULL, both cases are exercised, and the CTest target is already registered. No local compiler/CTest run is claimed; mainline Actions is the verification gate.

---

## 2026-11-07 13:35 CST — Recover absorbing all-zero RandomGen state

- Baseline: main commit `705ea49e1d3943ec002cd147a35b5674e9f0b2e4`; SkipDBv2 Regression run #547 passed source contracts, configure, the complete build, and all CTest cases.
- Scope: MT19937 state recovery in `basekit/RandomGen.c` and the existing `random_gen_state_bounds_test`.
- The Gaussian API already detects an all-zero MT state and reseeds it, but the public integer and floating-point generators did not. An all-zero state is absorbing in MT19937, so `RandomGen_randomInt()` and `RandomGen_randomDouble()` could return a constant zero stream indefinitely after zero-initialization or state corruption.
- Reuse the existing state validator at the initial-index and twist-batch boundaries (`mti == 0` or `mti == N`). This catches a zero-initialized instance before its first output and recovers a zeroed state when the next batch begins, while avoiding a 624-word scan on every generated value. Restore the deterministic default seed `5489`, consistent with the existing Gaussian recovery path.
- Add coverage for integer, floating-point, and twist-boundary recovery. Reuse the registered CTest target; no new files, external symbols, or CMake changes.
- Static preflight: confirmed the helper retains one static definition plus a forward declaration, recovery occurs before indexing/twisting, all three tests compare against the deterministic default-seed sequence, and the existing test remains CMake-registered. No local compiler or CTest run is claimed; mainline Actions is the verification gate.

---

## 2026-11-07 13:33 CST — Repair tagged BStream source-contract boundary

- Baseline: main commit `354d78f6d391a7741408843a1abe14d0f12c65db`; run #546 passed dependency installation but stopped in the XY-C1 integration/source-contract step before CMake configure.
- The new tagged-writer contract used an over-escaped newline string when locating the end of `BStream_writeTaggedUArray_()`; Python searched for a literal backslash-n and therefore produced an empty function slice.
- Use `chr(10) + "}"` as the function boundary, avoiding escape ambiguity. The C implementation and regression cases remain unchanged.
- Static preflight confirms the boundary expression is present exactly once and the stale expression is absent. The next Actions run is the build/CTest gate.

---

## 2026-11-07 13:32 CST — Validate typed storage before tagged BStream array writes

- Baseline: main commit `33e6da33e482f9ca13fb898d6eee55d7183c2d57`; SkipDBv2 Regression run #545 passed XY-C1 source contracts, configure, the complete build, and all CTest regressions.
- Scope: `BStream_writeTaggedUArray_()` in `basekit/BStream.c` and the existing `bstream_tag_boundaries_test`.
- The plain array writer and array readers reject mismatched `itemType`/`itemSize` pairs, but the tagged writer checked only nonzero item size and multiplication bounds. A malformed public `UArray` could therefore be serialized using a byte extent inconsistent with its declared C type; the tagged writer also accepted a stream buffer whose own CTYPE width did not match its byte-sized storage.
- Validate both source and destination stream CTYPE widths before computing serialized byte counts or testing backing-store overlap. Extend the existing layout regression to exercise both malformed source and malformed stream cases and assert the writer leaves the stream untouched.
- No new files, exported declarations, or CMake changes. Extend the existing Actions source contract to scope the width checks to the tagged writer. No local compiler run is claimed; the mainline Actions run is the build/CTest gate.

---

## 2026-11-07 13:31 CST — Stabilize UArray scalar dispatch source contracts

- Baseline: main commit `781ed26360155963e42174e4c60de31ac0c9aaaa`; run #543 failed during the XY-C1 source-contract phase before configure/build. Its new UArray source-contract entries assumed space indentation, while the C switch uses tabs.
- Replace the two formatting-sensitive full-line needles with stable case-label fragments. The behavioral contract remains enforced separately by the Python check for 11 `break;` arms, no early `return;`, and post-dispatch `UArray_changed(self)` in both setters.
- No C implementation or regression test changed. This workflow-only correction triggers another source-contract/build/CTest run.

---

## 2026-11-07 13:29 CST — Correct UArray source-contract newline escapes

- Baseline: main commit `fa29af2957460c923ff7f000ff88144bb4d24cb2`. Preflight of run #543 found that the new Python source-contract searches were double-escaped; they would search for a literal backslash-n instead of the macro/function closing-line boundary.
- Correct both searches to use a single Python newline escape (`\n`). No C implementation or test code changed in this workflow-only repair.
- Static check confirms both targeted escapes were corrected and no double-escaped variants remain. This commit triggers the authoritative rerun of the scalar-write cache fix.

---

## 2026-11-07 13:24 CST — Preserve UArray scalar-write cache invalidation

- Baseline: main commit `6d425107c2712381b7bfc35a85239b75ae9e4ec1`; SkipDBv2 Regression run #542 passed source contracts, configure, and the full build, but 1 of 57 CTest cases failed in `uarray_math_extrema_test`.
- The failure exposed a second defect in the same scalar-write path: `UARRAY_RAWAT_PUT_` used `return;` in every dispatch arm, exiting `UArray_at_putLong_()` / `UArray_at_putDouble_()` before their trailing `UArray_changed(self)`. Same-size writes therefore left cached hashes stale; growth writes masked the defect because resize already invalidated the hash.
- Change the 11 switch arms to `break;`, allowing both setters to run the shared mutation invalidation. Add same-size double and long write assertions, while retaining the int64/uint64 boundary checks that found the issue.
- Tighten the workflow source contract: require switch breaks and verify both setters invalidate after dispatch; correct the layout-validator call-site count diagnostic. Reuse the existing test and CMake registration.
- Validation: run #542 compiled all targets and isolated the failure to this test; the next Actions run is the post-fix build/CTest gate. No local build is claimed.

---

## 2026-11-07 13:21 CST — Align UArray layout-validator source contract

- Baseline: main commit `cbaef56b38fe0c66ae5aae3f6eea72b813359190`; SkipDBv2 Regression run #541 stopped in the XY-C1 source-contract preflight before CMake configure/build/CTest.
- The scalar-write hardening added receiver-layout validation to both `UArray_at_putLong_()` and `UArray_at_putDouble_()`. The workflow still asserted the prior total of one definition plus eight call sites, so its exact occurrence count rejected the intended additional guards.
- Update the existing contract to require one definition plus ten guarded call sites. The validator implementation and all call sites remain unchanged; no source, test, or CMake behavior is altered.
- Static preflight confirms the source has exactly 11 occurrences and 10 call sites, matching the updated contract. The next Actions run is the verification gate.

---

## 2026-11-07 13:16 CST — Guard UArray scalar floating-point writes

- Baseline: main commit `8deffc85c39ccb53aad535984c7568e1bfa5fd41`; SkipDBv2 Regression run #540 passed source contracts, configure, the full SkipDBv2 build, and all CTest regressions.
- Scope: indexed scalar mutation in `basekit/UArray.c`, the existing `uarray_math_extrema_test`, and its Actions source contracts.
- `UArray_at_putDouble_()` previously cast NaN, infinities, and out-of-range doubles directly into integer storage. Those conversions are undefined in C and could corrupt scalar writes. The setter now validates the receiver's complete item layout and checks the truncated-value range before growing or mutating the array.
- Preserve valid C truncation semantics at signed/unsigned limits, including the exact `int64_t` minimum and the final representable `uint64_t) double below 2^64; retain non-finite writes for floating arrays. Apply the same layout guard to the adjacent long setter, and cover append through its existing delegation path.
- Reuse the existing regression executable and CMake registration; extend its source contract. No new source files or exported symbols.

### Validation
- Static preflight: verified the conversion guard runs before `UArray_setSize_()` and before any element assignment; checked each supported integer width, `uintptr_t`, floating-type passthrough, malformed layout rejection, and unchanged hash/data on rejected writes.
- No local compiler or CTest run is claimed. The updated mainline Actions workflow is the verification gate.

---

## 2026-11-07 13:10 CST — Preserve Win32 truncate failure causes

- Baseline: main commit `01cf7aa88f4b2f6a0137e9d4210d85a3a7f887ef`; Actions run #540 passed source contracts, configure, full build, and all CTest regressions.
- Scope: Windows implementation in `basekit/PortableTruncate.c` and its existing Actions source contract.
- Win32 `CreateFileA`, `SetFilePointer`, `SetEndOfFile`, and `CloseHandle` failures previously returned `-1` without setting POSIX `errno`, leaving callers with stale or unrelated error details.
- Map common Win32 file, access, handle, memory, parameter, and disk-full failures to stable errno values; preserve `GetLastError()` before closing the handle and also report close failures.
- The existing source path is unchanged. The Windows-only branch is not locally cross-built; mainline CI checks the error-path contract and Linux build/regressions.

---

## 2026-11-07 13:08 CST — Preserve Date interval precision at large timestamps

- Baseline: main commit `6ea543e8997971d94527d99cb702814652a9e57a`; Actions run #539 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `Date_secondsSince_()` in `basekit/Date.c`, the existing `date_time_init` precision test, and their source contracts.
- The function subtracted two absolute timestamps only after converting each to `double`; above 2^53, adjacent whole seconds or microseconds can collapse to a zero interval even though `Date_compare()` retains exact ordering.
- Compute the signed interval from the integer seconds delta first, splitting the unsigned delta into high/low 32-bit chunks before converting to `double`, then add the fractional-microsecond difference. This avoids signed integer subtraction overflow and preserves small intervals on platforms where `long double` is not wider than `double`.
- Extend the existing precision test for ±1 second and ±1 microsecond at large timestamps. Existing CMake target reused; no new files or exported declarations. No local C build/CTest is claimed; mainline CI is the verification gate.

---

## 2026-11-07 13:06 CST — Align PortableUsleep Windows detection

- Baseline: main commit `ba87ea7a6ca9f8b5a2e08f0316651a58c076316b`; Actions run #538 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `basekit/PortableUsleep.c`, its existing public header, and their workflow source contracts.
- The implementation selected its Windows shim only under `WIN32`, while the header used the project-specific `ON_WINDOWS` guard. Toolchains that provide `_WIN32`/MSVC or MinGW macros without the legacy `WIN32` alias could therefore include a native POSIX header or miss the shim implementation.
- Use one consistent Windows-macro family in the declaration and implementation, preserving the existing high-resolution sleep behavior. Add the header to existing source-contract validation; no new files or CMake targets.
- No local Windows cross-build is claimed; mainline CI verifies Linux integration and the static platform contract.

---

## 2026-11-07 13:03 CST — Correct Python newline escape in helper contract

- Baseline: main commit `dfb9a64b73c4ba0e2958a7542e1becd67038ae34`; Actions run #537 again failed in source-contract preflight, before configure/build/CTest.
- The first contract repair selected the function definition, but encoded the newline escape twice, so Python searched for a literal backslash-n instead of the definition's line break.
- Reduce the escape by one level so the contract search resolves the actual implementation after the forward declaration. No C source, tests, or build targets changed in this repair.
- Mainline CI remains the verification gate.

---

## 2026-11-07 13:02 CST — Match float-range helper definition in source contract

- Baseline: main commit `932f6f2ca0e0813f250af7da96bbd9ffec956663`; Actions run #536 failed during the source-contract preflight before configure/build/CTest.
- Scope: `.github/workflows/skipdbv2-regression.yml` and this handoff log only.
- Adding the required forward declaration for `UArray_integerFloatQuotientIsOutOfRange_()` made the contract's broad signature search stop at the prototype instead of the implementation, falsely reporting that the helper no longer rejected non-finite/out-of-range values.
- Anchor the contract lookup to the function definition's opening brace so declarations and implementation remain unambiguous. The C change and its regression tests are retained; no build-script or test-target changes are required.
- This is a CI-contract repair only. The prior run did not reach compilation or CTest; the new mainline run is the verification gate.

---

## 2026-11-07 13:00 CST — Preflight float-to-integer array fills

- Baseline: main commit `3f005da247d342a2cdf20a19ea49cf8a9ba21782`; SkipDBv2 Regression run #535 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `UArray_setItemsToDouble_()` in `basekit/UArray_math.c`, its existing assignment-transform regression, and matching Actions source contracts.
- Filling an integer-backed array with `INFINITY`, NaN, or a value outside the destination integer range could invoke undefined floating-to-integer conversion during the element loop and partially mutate the array.
- Reuse the existing range preflight used by integer division; reject unsafe values before touching data or its cached hash, while preserving floating-array support for non-finite values.
- Reuse the existing `uarray_math_extrema_test` CMake target; no new files or exported declarations. No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:57 CST — Make UArray size getters NULL-safe

- Baseline: main commit `4c96969dbb2796c2c22b36a73fbafef036f6fe37`; SkipDBv2 Regression run #533 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `UArray_size()` and `UArray_itemSize()` in `basekit/UArray.c`, their existing metadata regression, and the source contracts.
- These public read-only getters unconditionally dereferenced their receiver, unlike the adjacent `UArray_sizeInBytes()` query that already treats a NULL array as zero bytes.
- Return zero for a NULL receiver while retaining the existing values for valid arrays; add regression coverage for both getters and source-contract checks.
- Reuse the existing `uarray_size_in_bytes_test` CMake target; no new files or public declarations. No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:55 CST — Keep empty path append as a no-op

- Baseline: main commit `019b6b855c715814ba8bb6bc0c04f08081943ad5`; SkipDBv2 Regression run #532 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `UArray_appendPath_()` in `basekit/UArray_path.c`, its existing path regression, and the corresponding Actions source contracts.
- Joining a non-empty path with an empty component previously inserted a separator before appending zero bytes, silently changing `/safe` into `/safe/`.
- Return early for a valid empty path component and add a regression asserting that appending `""` preserves the destination byte-for-byte. Reuse the current `uarray_path_null_test` target; no new files or exported declarations.
- No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:53 CST — Prevent UArray item-count ceiling overflow

- Baseline: main commit `0845cf06b9200a2de592ffcb3fd4da1cbdaa7425`; SkipDBv2 Regression run #531 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `UArray_sizeRequiredToContain_()` in `basekit/UArray.c`, its existing byte-size regression, and the matching Actions source contracts.
- The helper computed ceiling division as `(bytes + itemSize - 1) / itemSize`, which can wrap when the byte extent is near `SIZE_MAX`; it also dereferenced a NULL receiver and divided by zero for malformed item metadata.
- Guard the arguments and compute quotient plus a nonzero-remainder flag, avoiding overflowing addition while preserving normal rounding semantics. Extend the existing test with NULL/zero-width cases and a SIZE_MAX boundary without allocating an enormous buffer.
- The existing `uarray_size_in_bytes_test` CMake target is reused; no new files or exported declarations. No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:51 CST — Enforce 32-bit MT state and output width

- Baseline: main commit `3420dd2891a7edf14dd858fc58de4340135d1629`; SkipDBv2 Regression run #530 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `genrand_int32()` in `basekit/RandomGen.c`, its existing state-boundary regression, and the corresponding Actions source contract.
- `RandomGen` stores MT19937 words in `unsigned long`, which is 64-bit on LP64 systems. A corrupted/publicly modified state could therefore leak high bits into the output and violate `RandomGen_randomDouble()`'s `[0,1)` range.
- Mask every twist result and the tempered output to the algorithm's specified 32-bit word width. Add LP64-only regressions covering both output before a twist and state normalization during a twist; standard seeded sequences remain unchanged.
- Reuse the existing `random_gen_state_bounds_test` CMake target; no new files or exported declarations. No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:48 CST — Detect standard Windows macros for JFile path separators

- Baseline: main commit `6ac2b52e2ff75b60f1b513f5f0409b3a7b713837`; SkipDBv2 Regression run #529 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `JFile_fileName()` in `jfile/JFile.c` and its existing Actions source contract.
- The path helper selected backslashes only when `__WIN32__` was defined. MSVC and MinGW commonly define `_WIN32`, `WIN32`, or their compiler-specific macros instead, so `JFile_setLogPath_()` could treat an entire Windows path as the basename and generate a malformed journal path.
- Match the repository's Windows macro family while retaining the POSIX slash branch. Extend the existing source contract to require both separator branches and the platform guard.
- Existing `jfile_journal_test` and CMake target are reused; no new files or exported declarations. No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:46 CST — Route commit-marker writes through EINTR-safe sync

- Baseline: main commit `a6ff958cf1fc9b94f709370dea98552a458b5261`; SkipDBv2 Regression run #528 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `UDBCommitMarker_write()` in `udb/UDBCommitMarker.c` and its existing Actions source contract.
- Marker writes duplicated direct `fsync()`/`F_FULLFSYNC` calls without the `EINTR` retry already provided by `JFile_hardSyncFileDescriptor_()`. A transient signal could therefore turn a recoverable sync interruption into a failed commit-marker durability barrier.
- Reuse the existing shared barrier after the marker byte is flushed, preserving error propagation and the existing platform-specific sync choice. Add a function-scoped source contract ensuring future edits keep this call in the marker-write path.
- The existing `commit_marker_test` and CMake target are reused; no new files or exported declarations. No local C build/CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:40 CST — Remove unreachable legacy snprintf formatter

- Baseline: main commit `e08721f1f5b68b941ae671a24bcbb77a70a1152b`; SkipDBv2 Regression run #527 passed source-contract preflight, configure, full build, and CTest.
- Scope: `basekit/PortableSnprintf.c`.
- This translation unit defined `HAVE_SNPRINTF`, `HAVE_VSNPRINTF`, and `HAVE_C99_VSNPRINTF` unconditionally before the fallback branch, so the roughly 700-line legacy formatter could never be compiled. Normal builds only emitted `dummy_snprintf()`; the standard APIs came from the host C library.
- Remove the unreachable implementation and duplicate setup while preserving the object marker, the optional `TEST_SNPRINTF` libc smoke harness, and the existing source path. CMake uses a recursive basekit source glob, so no build-script or workflow source-contract changes are required.
- No local C build or CTest is claimed; mainline CI remains the verification gate.

---

## 2026-11-07 12:23 CST — Reject absent numeric fields in PortableStrptime directives

- Baseline: main commit `5656f3584628978f366ddafe2a65c3bd37cf0375`; SkipDBv2 Regression run #526 passed source contracts, configure, full build, and CTest.
- Scope: numeric `%M`, `%S`, `%Y`, and `%y` parsing in `basekit/PortableStrptime.c`, using the existing `portable_strptime_bounds` test.
- These directives treated an empty or whitespace-only input field as an optional success and left the corresponding `struct tm` field untouched. A truncated timestamp such as a format ending in `%M` could therefore be reported as parsed despite the required numeric field never appearing.
- Remove the silent-skip branches. Existing digit/range checks now reject missing and whitespace-only fields consistently; add regression coverage for all four directives.
- Reuse the existing CMake/CTest target and workflow contract blocks. No new files and no local build/CTest claim.

---

## 2026-11-07 12:20 CST — Correct Date_fromTime_ source-contract delimiter

- Baseline: main commit `97d3fa3005cf11f0f22983bcac661597e43ed260`; SkipDBv2 Regression run #525 stopped in source-contract preflight.
- The new dynamic contract searched for the end of `Date_fromTime_()` using a Python string containing a literal backslash-plus-`n`, rather than the newline that starts `Date_asTime()`. The source body extraction was therefore empty.
- Correct the delimiter escape so the contract checks the actual function body. Production code and regression test are unchanged.

---

## 2026-11-07 12:19 CST — Preserve exact time_t values in Date_fromTime_

- Baseline: main commit `b2c7ee397792acb267658448ea80ea3a3c79d335`; SkipDBv2 Regression run #523 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `Date_fromTime_()` in `basekit/Date.c` and the existing `date_time_init` precision regression.
- The constructor widened an integral `time_t` to `double` and routed it through `Date_fromSeconds_()`. Beyond the exact-integer range of IEEE-754 double, distinct whole-second timestamps collapse to the same value, silently losing seconds.
- Assign the incoming `time_t` directly to `tv_sec` and reset `tv_usec` to zero. This preserves the exact source value without changing timezone metadata or fractional-second APIs.
- Extend the existing precision test with a representable timestamp at 2^53+1 seconds and add source-contract checks for the direct assignment. Existing CMake target reused; no new files. No local C build/CTest is claimed; mainline CI is the verification gate.

---

## 2026-11-07 12:19 CST — Reject malformed UArray layouts before binary stream writes

- Baseline: main commit `7dfc1d1886069458b6c40e357fd3e9351ed307bb`; SkipDBv2 Regression run #523 passed source contracts, configure, full build, and CTest.
- Scope: `BStream_writeUArray_()` in `basekit/BStream.c`, the existing `bstream_tag_boundaries` regression, and its Actions source contract.
- The read-side array validator already required `itemSize == CTYPE_size(itemType)`, but the write-side preflight checked only multiplication geometry. A forged byte-array descriptor with a larger item width could make `UArray_sizeInBytes()` overstate the valid backing extent, causing the stream writer to read beyond the actual byte storage.
- Reject a mismatched source item width before calculating the serialized payload length. Extend the existing write-validation test with padded backing storage so the regression detects acceptance without itself reading outside the fixture. Require an error and an unchanged stream.
- Reuse the existing CMake/CTest target; no new files or target changes. No local C build or CTest run is claimed; mainline CI is the validation gate.

---

## 2026-11-07 12:17 CST — Invalidate UArray hash after same-size fread

- Baseline: main commit `1e4da38232e3d9c0f6fd28a691d30bbcf42d3e45`; SkipDBv2 Regression run #522 passed source contracts, configure, full build, and all CTest regressions.
- Scope: direct `UArray_fread_()` writes in `basekit/UArray_stream.c` and the existing `uarray_stream_io` test.
- A complete read overwrites every logical item but leaves the array size unchanged. `UArray_setSize_()` is a no-op when the requested size already matches, so the existing path retained the previously cached hash for entirely different contents.
- Remember the pre-read item count and invalidate the hash after a successful nonempty same-size overwrite. Reads that shrink the array continue to use the existing resize/fallback invalidation path; zero-item reads remain unchanged.
- Add a direct full-overwrite regression that seeds a nonzero cached hash and verifies both payload and hash. Update the existing Actions source contract; CMake target and file set are unchanged. No local C build/CTest is claimed; mainline CI is the verification gate.

---

## 2026-11-07 12:15 CST — Correct scalar-division contract guard ordering

- Baseline: main commit `6cec8277387e1fd6f405be5874612e71eac31c21`; SkipDBv2 Regression run #521 stopped in the source-contract preflight.
- The scalar divide implementation and regression compiled neither way because the source-contract verifier searched for the float-type guard starting at the later range-loop declaration, while the guard correctly precedes that loop.
- Correct the verifier to locate the type guard from the start of the scalar function and explicitly require it to precede the scan. Keep the quotient scan and its placement before mutation unchanged.

---

## 2026-11-07 12:14 CST — Preflight scalar floating-to-integer division

- Baseline: main commit `fcf717cb8fbb7e8df06b457ba32bb6e882d8a245`; SkipDBv2 Regression run #520 passed source contracts, configure, full build, and all CTest regressions.
- Scope: `UArray_divideScalarDouble_()` in `basekit/UArray_math.c` and its existing `uarray_math_extrema` regression.
- Scalar division previously assigned each floating quotient directly into an integer element. A finite quotient outside the target integer range, or a non-finite quotient, makes floating-to-integer conversion undefined; processing in-place could also mutate earlier elements before a later element proves invalid.
- Reuse the existing integer-quotient range predicate to scan every integer element before the assignment loop. Reject invalid or non-finite quotients atomically; preserve the existing floating-array path, valid integer division, zero-divisor no-op, and cache invalidation behavior.
- Extend the existing regression with a late out-of-range int8 quotient and a NaN quotient, asserting contents and cached hash remain unchanged. Update the existing Actions source contract; no new files or CMake changes. No local C build/CTest is claimed; mainline CI is the verification gate.

---

## 2026-11-07 12:11 CST — Align UTF embedded-NUL regression source contract

- Baseline: main commit `da34cf94d4f1c0939174658a355fc522b1e8d95b`; SkipDBv2 Regression run #519 stopped in source-contract preflight before configure/build.
- The regression was expanded to cover UTF-16/UTF-32 output, and its failure diagnostic changed accordingly. The workflow's static test-file contract still required the previous UTF-8-only diagnostic, so preflight rejected the correctly updated test.
- Replace the stale needle with the new UTF-16/UTF-32 regression helper and diagnostic. Production code and test behavior are unchanged; no CMake or file-path changes.

---

## 2026-11-07 12:10 CST — Preserve embedded NUL across UTF-16/UTF-32 conversions

- Baseline: main commit `8789c433a53345b74b46d5493de577bf50672397`; SkipDBv2 Regression run #518 passed source contracts, configure, full build, and CTest.
- Scope: `UArray_asUTF16()` / `UArray_asUTF32()` in `basekit/UArray_utf.c`, their existing encoding-conversion regression, and the existing Actions source contract.
- Both conversion paths treated UArray's length-delimited content as a NUL-terminated string after conversion. The post-conversion scan truncated at the first U+0000, silently discarding all following UTF-16/UTF-32 elements even though the converter cursor had already established the exact output length.
- Remove the obsolete NUL-scan helper and retain the converter-derived element count. Extend the existing regression across UTF-8, UTF-16, and UTF-32 inputs to verify both target encodings preserve the complete `A\0B` sequence. Keep source-contract checks aligned with the length-delimited invariant.
- Existing CMake/CTest target reused; no new files or CMake changes. No local C build or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 11:52 CST — Repair UArray bit-count Actions contract placement

- Baseline: main commit `15a34fe809d5b35d7c0cff0aaa8614ca0f301e99`; run #517 failed before configure because a Python ordering assertion had been inserted inside the workflow's source-contract dictionary.
- Remove the misplaced executable assertion from the dictionary while retaining the explicit source guard and regression needles in the existing contract list. The C fix and test remain unchanged.
- No CMake or file-path changes.

---

## 2026-11-07 11:51 CST — Guard UArray bit-count result overflow

- Baseline: main commit `52101f5e858b0d8869aa6e60c6b2822237ff5a23`; SkipDBv2 Regression run #516 passed.
- Scope: `basekit/UArray_math.c` bit-count path, the existing `tests/uarray_bit_access.c` regression, and its existing Actions source contract.
- `UArray_bitCount()` accumulated up to eight set bits per byte into `size_t` without first proving that `byteCount * 8` is representable. A malformed but multiplication-valid UArray geometry could overflow the returned count and drive the scan beyond its representable result range.
- Reject `byteCount > SIZE_MAX / 8` before scanning; add a sentinel-backed regression that reaches the overflowing geometry without allocating or reading a huge buffer. Existing CMake/CTest target reused; no new files.
- No local C build or CTest run is claimed; mainline CI is the validation gate.

---

## 2026-11-07 11:48 CST — Stop futile PHash growth for identical pointer hashes

- Baseline: main commit `514102b8d054386a9f283ed0b163a499f438484b`; SkipDBv2 Regression run #515 passed after the SHash collision guard.
- Scope: `basekit/PHash.c`, the existing `tests/phash_compact.c` fixture, and its existing Actions source contract.
- On 64-bit targets, `PHash_hash()` truncates the transformed pointer to 32 bits. Three distinct pointer values can therefore share the exact hash and are restricted to the same two cuckoo slots at every table size. The old failed-insert path repeatedly grew to the maximum table despite the collision being unresolvable.
- Detect both occupied candidates and verify their full hash matches the incoming pointer before growing. Add a 64-bit regression using distinct pointer values differing only above the retained hash bits; the 32-bit transform is injective and skips this case. Existing CMake/CTest target reused; no new files.
- No local C build or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 11:46 CST — Stop futile SHash growth for unplaceable full-hash collisions

- Baseline: main commit `33c777ccb424b99ff736f8006f3c5fc96c6d3b27`; SkipDBv2 Regression run #514 passed.
- Scope: `basekit/SHash.c`, the existing `tests/shash_compact.c` fixture, and its existing Actions source contract.
- A failed insertion with two resident keys sharing the incoming key's full hash recursively grew the cuckoo table through every size up to (2^{20}), although the two candidate slots are a deterministic function of that full hash and therefore remain the same relative to every table size. The third key can never be placed.
- Detect the two occupied candidate slots and confirm both residents have the identical full hash before calling `SHash_grow()`. This preserves the existing table and its entries while avoiding futile repeated allocations. Extend the constant-hash regression through `SHash_at_put_()`. Existing CMake/CTest target reused; no new files.
- No local C build or CTest run is claimed; mainline CI is the validation gate.

---

## 2026-11-07 11:42 CST — Align Date local-time guard contract with checked conversion

- Baseline: main commit `0bceac25cbceed7a73f156ae0e09807feff196cd`; run #513 reached the Date entry-point contract and rejected its obsolete exact guard string.
- `Date_fromLocalTime_()` now validates the receiver and calendar pointer in a combined guard with the checked conversion. Update the source contract to match the guard's new boundary without weakening its ordering check.
- No C implementation, test, CMake, or file-path changes.

---

## 2026-11-07 11:40 CST — Refresh Date parser source contract for checked conversion

- Baseline: main commit `c030c221549bc98a8d20786302b3fb7f882b83b6`; run #511 failed in the static source-contract step on the old `io_strptime` guard.
- The parser now checks both parse success and `Date_calendarToTime_()` before committing the timestamp. Replace the obsolete exact-line needle and add static assertions for the shared mktime error boundary and its regression case.
- No C implementation, test, CMake, or file-path changes.

---

## 2026-11-07 11:38 CST — Correct Date helper source-contract boundary

- Baseline: main commit `32938d6d2f9c6e41aba802d74f8b916cc2499e67`; run #510 confirmed the preceding Date setter contract mismatch.
- The new helper contract searched for a literal backslash-plus-`n` instead of the newline preceding `Date_new`, leaving its source slice empty. Correct the delimiter so CI checks the actual helper body and its error/commit ordering.
- No C implementation, test, CMake, or file-path changes.

---

## 2026-11-07 11:35 CST — Align Date setter contract with copy-first calendar conversion

- Baseline: main commit `6b41c8ecec634e21d62aca0f03dcf6e1001e15fa`; SkipDBv2 Regression run #510 failed in the Actions source-contract step.
- The Date setter implementation now changes a private `struct tm calendar` copy and commits only after checked conversion, but the static contract still required the removed in-place assignment `tm->tm_year = tmYear;`.
- Update the existing Date source contract to require `calendar.tm_year = tmYear;`. No implementation, test, CMake, or file-path changes.

---

## 2026-11-07 11:30 CST — Commit calendar conversions only after successful mktime

- Baseline: main commit `e9d58845d1eb6a323a59e7c805abd4cb40bb8e3f`; SkipDBv2 Regression run #509 passed.
- Scope: `Date_fromLocalTime_()`, `Date_fromString_format_()`, and all calendar component setters in `basekit/Date.c`, plus the existing `date_time_init` regression.
- These entry points published `mktime()`'s `-1` failure sentinel into the Date even when errno identified an unrepresentable calendar value; component setters could replace a valid prior timestamp with an overflow sentinel.
- Centralize conversion through a checked, copy-first helper that preserves input state on failure, accepts the valid `time_t(-1)` case when errno remains clear, and commits normalized calendar fields and Date timestamps only on success. Add an architecture-aware overflow regression and Actions source contracts. Existing CMake test target is reused; no new files.
- No local C build or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 11:08 CST — Bound Sorting against adversarial median-of-three partitions

- Baseline: main commit `fb8d1667949043a99cb9d26922d8a483db9b8e84`; SkipDBv2 Regression run #508 passed.
- Scope: `Sorting_context_comp_swap_size_type_()` and its existing `sorting_boundaries` regression.
- The three-way median-of-three quicksort avoided duplicate-heavy degeneration and bounded recursive stack depth, but a deterministic even-then-odd permutation still drove repeated highly unbalanced partitions and quadratic comparisons.
- Add an introspective depth budget of twice floor(log2(n)); when exhausted, heapsort the active range. Normal inputs retain the existing sorted fast path, median-of-three partitioning, and duplicate grouping. Add a 4096-element killer-permutation regression with a comparison ceiling; update Actions source/function-order contracts. Existing CMake target is reused; no new files.
- No local C build or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 11:02 CST — Detach List removal targets from shared backing storage

- Baseline: main commit `94974e71a66fdd59836c5088ee5b18de940d715e`; SkipDBv2 Regression run #42330825341 passed.
- Scope: `List_removeItems_()` in `basekit/List_inline.h` and its existing `list_storage_bounds` regression.
- When two List structs shared an allocation, the old path cloned the source for traversal but still removed items from the shared destination buffer. Automatic compaction could reallocate/free that buffer while the other view retained a dangling pointer; without compaction, the source view was silently mutated.
- Detach the destination into an allocation-checked copy before removals, so the source view remains stable through threshold compaction. Extend the existing regression to check retained source contents, destination results, and immediate-compaction behavior for identical backing views. Update static and function-order Actions source contracts. Existing CMake/CTest target is reused; no new files.
- No local C build or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:45 CST — Bound Gaussian rejection and recover unusable RandomGen states

- Baseline: main commit `4c9775873b7b86b6b972bf32def21fbe5ed59228`; SkipDBv2 Regression run #42330602360 passed.
- Scope: `RandomGen_gaussian()` and the existing `random_gen_state_bounds` regression.
- The Gaussian polar sampler used an unbounded rejection loop. Its earlier all-zero-state recovery did not cover a nonzero but degenerate/corrupted MT state; such a state could keep producing points outside the unit circle and stall callers indefinitely.
- Cap consecutive rejected pairs at 128, then restore the deterministic default seed and continue. Normal MT19937 rejection behavior is unchanged unless the state fails to produce a usable pair for the full bound.
- Extend the existing test with a nonzero all-ones state. The test verifies recovery yields the same Gaussian sample and subsequent PRNG state as a freshly default-seeded generator. Update both static and function-order Actions contracts. The existing CMake target is reused; no new files or CMake changes.
- No local C build or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:42 CST — Refresh UArray validator call-count contract

- Baseline: main commit `80651a37a9b4f4017a6bd4d61a63da1974434ccf`; its Actions run failed in the source-contract step before configuration/build.
- The new `UArray_at_putAll_()` preflight adds two valid calls to `UArray_hasValidItemLayout_()`. The existing Actions contract still hard-coded the previous total of seven (one definition plus six call sites), so the refactor was rejected despite both changed-file needle sets passing.
- Update the in-place source contract to nine occurrences (one definition plus eight guarded call sites). No source, test, CMake, or file-path changes.

---

## 2026-11-07 10:37 CST — Make UArray insertion preflight and alias handling transactional

- Baseline: main commit `81ddf68ab344420dfb6dd28d6682c773196ac589`; SkipDBv2 Regression run #504 passed.
- Scope: `UArray_at_putAll_()` and the existing `uarray_stack_resize` regression.
- The insertion path checked only nonzero widths and multiplication bounds. Mismatched CTYPE/itemSize metadata could therefore pass preflight, resize the destination, and move its tail before `UArray_copyItems_()` rejected the source or destination. Validate both complete item layouts before any mutation.
- The overlap calculation excluded the allocated trailing NUL. A borrowed view beginning exactly at `self->data + selfBytes` could be overwritten by the tail move or invalidated by reallocation. Include this one-past-logical-end byte in overlap detection and snapshot it before resizing.
- Extend the existing regression for malformed source/destination layouts and an insertion sourced from the owner's terminator. Update the Actions source contracts. The existing CMake target already builds the test; no new files or CMake changes.
- No local C build or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:18 CST — Preserve malloc-compatible alignment in tracked allocations

- Baseline: main commit `39ef3e80488b4c0876028f07a9635e6f41c58c50`; SkipDBv2 Regression run #503 passed.
- Scope: private `MemoryBlock` payload layout in `basekit/Common.c` and the existing `common_alloc` regression.
- `MemoryBlockToPtr()` returns the byte immediately after the tracking header. The prior padding left `sizeof(MemoryBlock)` at 56 bytes on common 64-bit ABIs, so a `calloc()`-aligned base could yield an 8-byte-offset payload and violate `malloc()` alignment for `long double` and other fundamental types in `IO_CHECK_ALLOC` builds.
- Replace byte padding with an alignment-bearing union member, making the private header's size a multiple of the strongest included fundamental alignment. Extend the existing regression to store a `long double` and check both allocation and reallocation alignment. Update the Actions source contracts; the existing CMake target already compiles `Common.c` with `IO_CHECK_ALLOC`. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:10 CST — Preserve PID ownership when free-list insertion fails

- Baseline: main commit `b967358bf153983474948e930bf94ed9f033eece`; SkipDBv2 Regression run #502 passed.
- Scope: PID reservation and reusable-hole transitions in `udb/UDBIndexFreeList.c`.
- `UDBIndex_releasePid_()` removed the reservation before attempting a potentially allocating free-list insertion. If the list could not grow, the function returned failure after losing both ownership states. Hole normalization also removed existing entries before allocating their replacement.
- Secure/deduplicate an existing hole without allocation, insert before unreserving, and leave the reservation intact if insertion fails. Add a focused failed-insertion regression by temporarily making the existing list fail its storage-validity guard, plus source contracts enforcing the handoff order. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:08 CST — Reject PID mappings inside the data-file header

- Baseline: main commit `8d0258b9f2a5d4a793705ab918e8deaa9e1473d4`; SkipDBv2 Regression run #501 passed.
- Scope: `UDBIndex_setPos_forPid_()` in `udb/UDBIndexMap.c`.
- The setter rejected offsets beyond `LONG_MAX` but accepted nonzero offsets smaller than `sizeof(PID_TYPE)`. Those bytes are occupied by the records file's leading `firstEmptyPos` field, so an invalid mapping could overwrite metadata and release a PID reservation.
- Reject nonzero record offsets before the data header while preserving zero as the unmapped sentinel. Extend the existing index-layer regression to assert both reservations remain intact after rejection; add source-contract checks. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:06 CST — Reject record creation with invalid end offsets

- Baseline: main commit `2ed8d3fb51c749fc7828fb15f652b44001719c28`; SkipDBv2 Regression run #500 passed.
- Scope: `UDBRecords_newRecord()` in `udb/UDBRecordsCursor.c`.
- The helper checked that the record stream was open but not that its cached end offset was valid. When `maxPos < 0`, `UDBRecord_setPosToEnd()` correctly declined to update the cursor, yet `UDBRecords_newRecord()` still returned the shared record object at its previous position as though new-record creation succeeded.
- Reject invalid end metadata before returning the cursor. Extend the existing record-storage regression to verify that a rejected request preserves the old cursor and that normal EOF creation still succeeds. Add source-contract checks; existing CMake/CTest wiring is unchanged.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:04 CST — Reject tagged pointers wider than intptr_t

- Baseline: main commit `55a299d93bd842bb5f6404cba0f708dd0e7a4abf`; SkipDBv2 Regression run #499 passed.
- Scope: tagged pointer decoding in `basekit/BStream.c`.
- The 8-byte pointer-tag path cast an `int64_t` directly to `intptr_t`. On 32-bit builds, values outside the platform's signed pointer-width range were silently truncated, producing a different pointer value without setting the stream error.
- Decode into a temporary, preserve existing read errors, and reject out-of-range values before the cast. Add an architecture-sensitive regression to the existing BStream boundary test and enforce the guard in the Actions source contract. Existing CMake/CTest target is unchanged; no new files.
- No local compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 10:02 CST — Preserve cached headers when the record stream is closed

- Baseline: main commit `faa375bd8bf8d92ae35284fbf2d5fceff25978fc`; SkipDBv2 Regression run #498 passed.
- Scope: `UDBRecord_readHeader()` in `udb/UDBRecordRead.c`.
- The reader checked the cached file length but not whether the underlying `FILE *` was open. After close, a read failure was classified as a corrupt header and zeroed the caller's cached metadata while also seeking the closed `JFile` cursor.
- Reject closed streams before any seek or header mutation. Add a closed/reopened stream regression to the existing record-storage test and enforce the descriptor guard in the Actions source contract. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:59 CST — Align compaction contract with monotonic timing

- Baseline: main commit `00ff2c40036da65f6010e59c4764fb6fc1a98b58`; SkipDBv2 Regression run #497 failed in the XY-C1 source-contract step.
- The compaction implementation had switched from `gettimeofday()` to `clock_gettime(CLOCK_MONOTONIC)`, but the pre-existing contract still required the removed wall-clock call.
- Remove the stale assertion; keep both monotonic-clock assertions. No C-source changes.

---

## 2026-11-07 09:57 CST — Use monotonic time for compaction budgets

- Baseline: main commit `8b5e96b65fe334202e265daf0a46f3a5368db602`; SkipDBv2 Regression run #496 passed.
- Scope: time-sliced compaction scheduling in `udb/UDBCompact.c`.
- The budget used `gettimeofday()`, so wall-clock corrections could abruptly end a time slice even when the requested budget had not elapsed. The negative-elapsed guard prevented overruns but could not prevent premature stops.
- Measure the operation budget with `CLOCK_MONOTONIC` and nanosecond precision; retain the existing fail-safe behavior if clock reads fail. Extend the existing source contract. No new files or CMake changes.
- No local compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:55 CST — Retry interrupted commit-marker directory syncs

- Baseline: main commit `a14e65525c9c1482ba377000d137de3743c46328`; SkipDBv2 Regression run #495 passed.
- Scope: `UDBCommitMarker_syncParentDirectory()` in `udb/UDBCommitMarker.c`.
- The commit-marker durability path called `fsync()` directly, unlike the shared persistence barrier which retries `EINTR`. A harmless signal interruption therefore aborted database startup/commit instead of completing the required directory sync.
- Route the directory descriptor through `JFile_hardSyncFileDescriptor_()` so both marker-directory and journal/file durability barriers share the same platform-specific sync and interruption handling. The existing `commit_marker_test` still covers directory syncing; Actions now enforces the shared helper call. No new files.
- No local compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:52 CST — Reject PID lookups against a closed index file

- Baseline: main commit `8047b18c8ad0c2ae6f04144b54b2ece5ea5d272c`; SkipDBv2 Regression run #494 passed.
- Scope: `UDBIndex_posForPid_()` in `udb/UDBIndexMap.c`.
- The lookup validated cached size and the `JFile` object but not its open data stream. After close, a lookup returned the same zero used for an unmapped PID and changed the cached seek position before the lower-level read rejected the closed stream.
- Require an open index file before computing or seeking a slot. Extend the existing index-layer test to prove closed-file lookups preserve cached position/size, and make the Actions source contract enforce the descriptor guard. Existing CMake/CTest target is unchanged; no new files.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:50 CST — Register database cursors at construction

- Baseline: main commit `f75050a27a81195edda38caf37d537d61201f0b5`; SkipDBv2 Regression run #493 passed.
- Scope: SkipDB cursor ownership and registration lifecycle.
- The exported `SkipDBCursor_newWithDB_` assigned an owning database pointer without registering the cursor in the owner's cursor list. Direct constructor callers could therefore be missed by cache protection and database teardown.
- Route binding through `SkipDBCursor_sdb_`, reject construction when owner registration fails, and remove the factory's duplicate list append. Extend the existing core-layer regression and CI source contracts; its CMake/CTest target already exists. No new files.
- No local compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:49 CST — Return the durability barrier result

- Baseline: main commit `b250e2433620df062a002b5a1be6cdf24e11215b`; SkipDBv2 Regression run #492 passed.
- Scope: `JFile_hardSyncFileDescriptor_()` in `jfile/JFileSync.c`.
- The helper retried interrupted `fsync()`/`F_FULLFSYNC` calls but fell off the end of a non-void function instead of returning the final status. Callers relied on that undefined return value to decide whether to close persistence streams and abort a transaction.
- Return `result` explicitly, add a valid-descriptor regression to the existing journal test, and enforce function-scoped return ordering in the Actions contract. Existing test target and headers already cover the helper; no new files.
- No local C compilation or CTest run is claimed; mainline CI is the validation gate.

---

## 2026-11-07 09:47 CST — Preserve absolute IPC deadlines across EINTR

- Baseline: main commit `1f15eb00a065392db5904d7b1032fc385b485e70`; SkipDBv2 Regression run #491 passed.
- Scope: exact socket I/O retries in `mgr/ipc_transport.c`.
- Direct `recv()` and `send()` interruptions retried immediately without rechecking the monotonic deadline. A continuous signal stream could therefore keep a caller in the retry loop beyond its configured timeout even though poll-based retries already used an absolute deadline.
- Recheck monotonic time after EINTR, return the existing clock-failure code if the clock cannot be read, and stop retrying when the deadline has expired. Strengthen the existing Actions source contract for both read and write paths; existing IPC transport CTest registration remains unchanged. No new files.
- No local compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:40 CST — Reject missing DBAPI list callbacks before parsing

- Baseline: main commit `e4abcfeb795752e0deb8738e821a3048a11d25f2`; SkipDBv2 Regression run #489 passed.
- Scope: `mgr/dbapi_result.c` list-result argument validation and the existing `dbapi_protocol.c` regression.
- The list parser dereferenced its callback for the first item without checking whether the function pointer was NULL; malformed internal calls could crash the CLI instead of returning a normal argument error.
- Reject NULL client, prefix, or callback before consuming any response bytes. Add a focused NULL-callback regression and enforce both the guard and test in the Actions source contract. The existing CMake/CTest target already covers this test file; no new files.
- No local C compilation or CTest run is claimed; CI remains the validation gate.

---

## 2026-11-07 09:35 CST — Harden the legacy array-heap container

- Baseline: main commit `50eb8be552cd66893429b25131c3f0bc6dd435e5`; SkipDBv2 Regression run #488 passed.
- Scope: `mgr/array-heap.c` and its existing public header.
- The container wrote before ensuring capacity, assigned `realloc()` directly to the owning pointer, used an invalid `-1` pointer-allocation check, stored pointers through `size_t`, and lacked null/state/overflow guards.
- Store elements as `void *`, grow before a full-capacity write, use a temporary realloc result, reject invalid capacities and integer overflow, and make cleanup null-safe/idempotent. Add a focused regression target and Actions source-contract checks; no change to active daemon behavior or new production files.
- No local compilation or CTest run is claimed; CI is the validation gate.

---

## 2026-11-07 09:33 CST — Report deterministic errors for invalid IPC socket paths

- Baseline: main commit `fbcb6aca8b763677dc7dde1ba71602a4a3625f3d`; SkipDBv2 Regression run #487 passed.
- Scope: path validation in `skipdb_ipc_connect_unix()` in `mgr/ipc_transport.c`.
- Null and empty socket paths, and paths too long for `sockaddr_un.sun_path`, previously returned `-1` without setting `errno`, leaving callers with stale/unrelated diagnostics.
- Report `EINVAL` for null/empty paths and `ENAMETOOLONG` for overlong paths. Tighten the existing Unix-socket transport regression and the Actions source contract. Existing CMake/CTest registration is unchanged; no new files.
- No local compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:32 CST — Preserve platform timezone declarations in DST regression

- Baseline: main commit `fde8f11122c0c2bb1a909feaa646c169c281e421`; Regression run #486 failed while compiling `date_time_init_test`, before tests ran.
- The test-only feature macro `_POSIX_C_SOURCE` hid glibc's non-POSIX `struct timezone` declaration, breaking existing Date test declarations. Replace it with `_DEFAULT_SOURCE` so the DST fixture exposes `setenv/tzset` while retaining the timezone compatibility types.
- Production Date logic and workflow contracts are unchanged; no new files.

---

## 2026-11-07 09:30 CST — Resolve DST when parsing local Date strings

- Baseline: main commit `8e5d94a562ea0cd8e554fc4c7584eda6aad7fb16`; SkipDBv2 Regression run #485 passed.
- Scope: `EmptyTM()` and `Date_fromString_format_()` in `basekit/Date.c`.
- The parser seeded its `struct tm` from local midnight on January 1, 1970, then reset calendar fields but retained that template's `tm_isdst`. On a DST-observing timezone, parsing a summer local time could force standard time and shift the resulting instant by an hour.
- Set `tm_isdst = -1` in the parsing template so `mktime()` resolves the correct seasonal offset for the parsed date. Extend the existing date test with an isolated POSIX DST timezone regression and update the workflow source contract. Existing CMake/CTest registration is unchanged; no new files.
- No local C compilation or CTest run is claimed; mainline CI remains the validation gate.

---

## 2026-11-07 09:25 CST — Recover Gaussian sampling from a degenerate RandomGen state

- Baseline: main commit `99ec4482a505c627e05f900d8f8eb84dc7b4437d`; SkipDBv2 Regression run #484 passed.
- Scope: `RandomGen_gaussian()` in `basekit/RandomGen.c`.
- A zero-initialized/corrupted but index-in-range MT state produces only zero random values, so the Gaussian rejection loop can reject forever because its radius remains outside the unit circle.
- Detect an all-zero state before sampling and restore the deterministic fallback seed. Extend the existing state-bounds test to compare its result with a freshly seeded generator and make the Actions source contract enforce recovery before rejection sampling. The CMake test target already exists; no new files.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 09:23 CST — Include active DST bias in Windows timezone offsets

- Baseline: main commit `737d63b496a45e427cf64f339aab7b408a6638e5`; SkipDBv2 Regression run #483 passed.
- Scope: Windows adapter in `basekit/PortableGettimeofday.c`.
- The adapter reported only `TIME_ZONE_INFORMATION.Bias`, omitting `StandardBias` or `DaylightBias` when Windows reported the current seasonal mode. Callers requesting `struct timezone` therefore received the base-zone offset instead of the active local offset during adjusted time.
- Apply the bias corresponding to `TIME_ZONE_ID_STANDARD` or `TIME_ZONE_ID_DAYLIGHT`, retaining the base bias for `TIME_ZONE_ID_UNKNOWN`. Extend the existing Actions source contract; no new files or test/CMake changes. The Windows branch cannot be executed by the Linux runner.
- No local Windows or C compilation is claimed; mainline CI remains the available validation gate.

---

## 2026-11-07 09:22 CST — Repair Datum source-contract delimiter

- Baseline: main commit `957c317e258783144fa0b2b647254f12167dfd46`; SkipDBv2 Regression run #482 failed in source-contract preflight before CMake configuration or compilation.
- The new `Datum_FromUArray_()` guard-order contract had the same double-escaped Python newline delimiter issue as a previous contract. Its function-body extraction returned empty and incorrectly failed the preflight.
- Correct the workflow delimiter to the Python newline escape; the production guard and regression assertions remain unchanged. No new files.

---

## 2026-11-07 09:20 CST — Reject malformed UArray layouts in Datum conversion

- Baseline: main commit `e6d63bc90e0d8a99b7e34c8926f02002be1b53f8`; SkipDBv2 Regression run #481 passed.
- Scope: `Datum_FromUArray_()` in `basekit/Datum.c`.
- The conversion checked multiplication overflow and the backing pointer, but not whether `itemSize` matched the declared `itemType`. A corrupted UArray could therefore be exposed as an incorrectly sized raw Datum byte span.
- Validate the declared CTYPE width before returning `UArray_bytes()`. Extend the existing Datum boundary test to preserve valid conversions and reject mismatched item widths, and add a workflow source contract for guard ordering. The CMake/CTest test is already registered; no new files.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 09:17 CST — Repair IPC parser source-contract delimiter

- Baseline: main commit `45e1a75f8eea3af4598cd05c3e6bbb9bfd581bae`; SkipDBv2 Regression run #480 failed during source-contract preflight, before configuration or compilation.
- The new IPC parser guard-order contract searched for a literal backslash-plus-\n sequence because its function-boundary delimiter was escaped twice. The contract therefore failed to extract the function body even though the parser's guard order was correct.
- Correct the workflow delimiter to Python's newline escape so the preflight checks the intended function body. This is a CI-only correction; no new files and no C behavior changes.

---

## 2026-11-07 09:16 CST — Clear IPC envelope views on invalid input

- Baseline: main commit `27ec1a6a99d3b91adcc5ed9bad1e7dc50f30e789`; SkipDBv2 Regression run #479 passed.
- Scope: `skipdb_ipc_parse_envelope()` in `mgr/ipc_protocol.c`.
- The parser returned immediately for a null body or zero-length frame before clearing a non-null output structure. Callers that reused a frame view could retain pointers and lengths from a prior successful parse after a failed parse.
- Reject a null output pointer first, clear valid output storage next, and only then reject null/empty input. Add regression coverage for both invalid-input paths and update the Actions source contract to enforce the guard order. The existing CMake/CTest registration remains unchanged.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 09:12 CST — Track realloc growth in allocator high-water accounting

- Baseline: main commit `f0cb6e3992ac99efff7d320aeeaba990750e7b46`; SkipDBv2 Regression run #478 passed.
- Scope: tracked allocation resize in `basekit/Common.c`.
- `MemoryBlock_newWithSize_file_line_()` updated `maxAllocatedBytes`, but successful `MemoryBlock_reallocToSize_()` only changed the current byte total. A realloc that grew the live heap could therefore leave the advertised peak-memory statistic stale until a later allocation happened to exceed it.
- Update the high-water mark after the realloc succeeds and the current total is committed. Extend the existing `common_alloc.c` regression and Actions source contract to assert that growing 16 bytes to 32 immediately raises the peak to 32. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 09:04 CST — Reject malformed UArray stack-range layouts

- Baseline: main commit `d50b95ba0f2abd1d4262c0125d365c3b9d9e9579`; SkipDBv2 Regression run #477 passed.
- Scope: `UArray_stackRange()` in `basekit/UArray.c`.
- The stack-range constructor checked only `itemSize` and count geometry. A malformed public UArray whose `itemType` width disagreed with `itemSize` could still produce a typed view over invalid storage, allowing subsequent typed access beyond the backing bytes.
- Reuse `UArray_hasValidItemLayout_()` before offset arithmetic; normalize malformed input to a safe empty byte-array view while preserving out-of-range behavior for otherwise valid arrays. Add malformed-layout and valid-slice regression cases to the existing test, and update the Actions source contract. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 08:47 CST — Repair UArray layout-contract call count

- Baseline: main commit `3e71d7304d1fb52a1e09c9ea31fe390779f39d1a`; SkipDBv2 Regression run #476 stopped at source-contract validation before configure/build.
- The `UArray_sort()` and `UArray_sortBy_()` layout guards correctly added two calls to `UArray_hasValidItemLayout_()`, but the existing exact-count contract still expected one definition plus three earlier callers.
- Update the contract to expect one definition and five guarded callers (six total occurrences). No additional C or test changes, new files, or CMake changes.

---

## 2026-11-07 08:46 CST — Validate UArray layout before sorting

- Baseline: main commit `2b2df6aa6252c894b66d79f6c3d6d6e250e20277`; SkipDBv2 Regression run #475 passed source contracts, configure, full build, and all regression tests.
- Scope: `UArray_sort()` and `UArray_sortBy_()` in `basekit/UArray.c`.
- Both sort entry points checked only for a null buffer, not whether `itemType` and `itemSize` agreed. A malformed public UArray struct could make `qsort()` use an element width larger than its declared backing layout, allowing out-of-bounds memory access and invalidating the cached hash.
- Reuse `UArray_hasValidItemLayout_()` before hashing or sorting; extend the existing malformed-layout regression and Actions source contracts. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 08:43 CST — Retry interrupted durability barriers

- Baseline: main commit `73b2c1b693f87abfd3d6d3e9762f7ca39a310c5a`; SkipDBv2 Regression run #474 passed source contracts, configure, full build, and all regression tests.
- Scope: `JFile_hardSyncFileDescriptor_()` in `jfile/JFileSync.c`.
- `fsync()` and `F_FULLFSYNC` may return `EINTR`. The shared helper treated this transient interruption as a fatal persistence error, causing callers to close an otherwise usable data or journal stream.
- Retry only interrupted durability calls; preserve existing error propagation for all other failures. Add the retry invariant to the existing Actions source contract. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 07:56 CST — Stop safe reverse iteration after cursor invalidation

- Baseline: main commit `fd7a0b31a8ffbdc6815bb95065f0880653109fea`; SkipDBv2 Regression run #473 passed integration contracts and the full build but failed `list_storage_bounds_test`.
- Scope: `LIST_SAFEREVERSEFOREACH` in `basekit/List_inline.h`.
- When a callback reduced the list to one slot, the reverse cursor still referred past the new logical end. Clamping that cursor to the new size caused another callback invocation at index zero, contrary to the existing mutation regression. Stop traversal when the cursor is beyond the current size instead of remapping it to an unrelated slot; keep per-iteration storage validation.
- Update the existing Actions source contract to require the early-stop guard. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 07:31 CST — Revalidate storage during safe reverse iteration

- Baseline: main commit `4c50788e432ded69288ad191ffe7312e1c4ca35d`; SkipDBv2 Regression run #472 passed integration contracts, configure, full build, and all 56 tests.
- Scope: `LIST_SAFEREVERSEFOREACH` in `basekit/List_inline.h`.
- The reverse-safe macro validated storage only when iteration started. If callback code made the List metadata inconsistent while retaining its struct (for example, reducing `memSize` below `size`), subsequent iterations still ran; `List_size()` read unchecked metadata and `List_at_()` returned NULL repeatedly instead of terminating.
- Revalidate the storage invariant on every loop pass and clamp against the validated struct's `size`. Add a callback-mutation regression in the existing List bounds test and update the two existing Actions source contracts. No new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 07:29 CST — Bound JFile reads to signed seek offsets

- Baseline: main commit `9111b2a76c306f5358219bcc1dcd21a4184690e4`; SkipDBv2 Regression run #471 passed source contracts, configure, full build, and all 56 tests.
- Scope: low-level read bounds in `jfile/JFile.c`.
- `JFile_fwrite()` rejects byte counts above `LONG_MAX` and spans where `pos + total` exceeds the signed seek range. `JFile_fread()` checked only multiplication overflow and a negative starting position, then called `fseek/fread` with an impossible span; this could move the stream beyond the representable logical cursor and leave `self->pos` stale when `ftell()` failed.
- Apply the same checked total-span limits before touching the stream. Extend the existing journal regression to verify both an oversized request and a start-plus-length overflow are rejected without changing stream or logical position. Update source contracts only; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:27 CST — Repair next-PID preview source contract

- Baseline: main commit `50ab4c3432a431a5f761d1aa0e46506ff23e44cb`; SkipDBv2 Regression run #470 failed at the integration-contract step before configure/build.
- The assertion text `next-PID preview returned a slot beyond LONG_MAX` was incorrectly attached to the production `UDBIndexFreeList.c` contract even though it is emitted by `tests/udb_index_layers.c`.
- Remove it from the production contract; keep the existing test-file assertion. No C source or test behavior changed.

---

## 2026-11-07 07:26 CST — Keep PID preview within the allocatable index range

- Baseline: main commit `4ac85843f6bbd9b3a5ac150258c88c3b1c187eee`; SkipDBv2 Regression run #469 passed integration contracts, configure, full build, and all 56 tests.
- Scope: PID availability preview in `udb/UDBIndexFreeList.c`.
- The allocator now rejects a PID whose fixed-width slot would extend beyond `LONG_MAX`, but `UDBIndex_nextPid()` still returned that same unpersistable PID from the current index EOF (and returned a free-list top without validating its slot). Callers could therefore be told an identifier is available that allocation must reject.
- Reuse the shared full-slot offset validator for both the free-list and EOF preview paths. Extend the existing index-layer regression with a synthetic near-limit extent and assert preview and allocation both return the failure sentinel without reservations. Update source contracts; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:24 CST — Make failed initial metadata writes safe to unwind

- Baseline: main commit `760546b81491bd847a08347b41cd6c295ff12e28`; SkipDBv2 Regression run #468 passed integration contracts, configure, full build, and all 56 tests.
- Scope: empty-file startup cleanup in `udb/UDBRecordsStorage.c`.
- The failure branch called `fclose(self->file->log)` unconditionally after a failed journal write. `JFile_fwrite()` may already close and null the journal stream when it cannot roll back a partial journal record, making that cleanup call undefined behavior and leaving the data stream open.
- Use the existing null-safe `JFile_close()` lifecycle routine to unwind both streams on this failure path. Update the source contract; no new files, public symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:22 CST — Bound allocated PID slots to the seekable file range

- Baseline: main commit `7f9303f8488d00fabea1b5253e2ef89df0d7c23f`; SkipDBv2 Regression run #467 passed integration contracts, configure, full build, and all 56 tests.
- Scope: fixed-width PID-slot mapping/allocation in `udb/UDBIndexMap.c` and `udb/UDBIndexFreeList.c`.
- Offset validation checked only whether a slot's starting byte was at or below `LONG_MAX`; the final slot could still extend beyond the signed range accepted by `JFile_fwrite()`. The allocator could consequently hand out a PID that could never be persisted.
- Make the shared offset validator require the entire entry to fit, expose it through the existing internal index header, and reject both newly allocated and reused PIDs whose slot is not seekable. Extend the existing index-layer regression by setting a synthetic near-limit max PID and asserting allocation fails without creating a reservation. Update Actions contracts; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:18 CST — Reject reserved metadata offsets in the record reader

- Baseline: main commit `c45f5e180859c4fbfb98602ebd9378b2588a6e77`; SkipDBv2 Regression run #466 passed integration contracts, configure, full build, and all 56 tests.
- Scope: the shared physical-extent predicate in `udb/UDBRecordRead.c`.
- The high-level record cursor, writer, removal, and relocation paths already reserve offset zero for the first-empty pointer, but the low-level header reader only checked file bounds. Crafted metadata bytes that matched a valid `UDBRecordHeader` could still be accepted as a physical record by a direct `UDBRecord_readHeader()` caller.
- Reject offsets below the first physical-record boundary and offsets that cannot be represented by the signed file-seek API. Add a regression that temporarily places a valid EMPTY header over the metadata bytes, verifies the reader rejects it, and restores the original bytes before continuing. Update existing source contracts only; no new files, exported symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:14 CST — Make UDB journal-path updates atomic

- Baseline: main commit `4891eff043e3d60d0d93254fb467917098340348`; SkipDBv2 Regression run #465 passed integration contracts, configure, full build, and all 56 tests.
- Scope: journal-path configuration in `udb/UDBLifecycle.c`.
- `UDB_setLogPath_()` updated the index and record journals sequentially. If deriving the second path failed (for example, allocation failure or an invalid basename), the first path could already have changed, leaving a split pair.
- Stage both derived paths using temporary `JFile` structs, then swap both only after both allocations/validations succeed. Add a regression that makes the second basename invalid and verifies neither live path changes. Update the existing Actions contracts; no new files, public symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:12 CST — Repair append-only regression source contract

- Baseline: main commit `df418c95d15397aeaf521d49b9b25b2c269c6733`; SkipDBv2 Regression run #464 stopped at the source-contract step before configure/build.
- The new runtime diagnostic `record save accepted a non-append position` was accidentally registered as a required string in the production `UDBRecordWrite.c` contract, even though it correctly belongs to the existing `udb_record_storage.c` test.
- Move the string assertion to the test-file contract and remove it from the production-source contract. No C source or runtime-test logic changed in this repair.

---

## 2026-11-07 07:11 CST — Enforce append-only record creation in journal mode

- Baseline: main commit `9cf85ee031e186a136a22d7bce580778c625de26`; SkipDBv2 Regression run #463 passed source contracts, full build, and all 56 regression tests.
- Scope: `UDBRecord_saveWithDatum_()` in `udb/UDBRecordWrite.c`.
- The non-journaling path already required a new record to start at EOF, but the journaling path skipped that same invariant. A caller holding a cached live-record position could therefore overwrite its persisted header/payload without updating the PID index or retiring the old row through the replacement path.
- Enforce the EOF precondition before either write mode and test that an existing record position is rejected without changing journal size, logical file extent, or the record cursor. Update the existing Actions source contract; no new files, public symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:08 CST — Reject record-header writes beyond the data extent

- Baseline: main commit `357e616178b2da7117f40784b4a896ffbf054ead`; SkipDBv2 Regression run #462 passed source contracts, full build, and all 56 regression tests.
- Scope: physical bounds validation in `udb/UDBRecordWrite.c`.
- `UDBRecord_saveHeader()` rejected metadata offset zero but accepted any larger PID-sized offset up to `LONG_MAX`. In journaled mode, a caller-supplied position beyond EOF could extend the predicted data extent and commit a sparse gap or corrupt later record traversal.
- Reject positions beyond the current logical file end and reject existing-file positions whose full header would cross EOF; retain the exact-EOF case required for valid appends and guard signed-offset overflow. Extend the existing storage regression and Actions source contracts; no new files, public symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:05 CST — Guard UDB diagnostic entry points

- Baseline: main commit `1f0dccea4366ab9f753f48a2ce56a8bf55f6c569`; SkipDBv2 Regression run #461 passed the full build and all 56 regression tests.
- Scope: public diagnostic helpers in `udb/UDB.c`.
- `UDB_show()` and `UDB_showIndex()` dereferenced the database facade unconditionally, so a NULL or zero-initialized database caused a crash despite the rest of the public API being defensive.
- Return without side effects when the facade or its required subobject is absent. Extend the existing record-API regression and workflow source contracts; no new files, exported symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 07:03 CST — Fix alias-sensitive record-move regression

- Baseline: main commit `01b7d79e6ce8fcd189aa7a8009a80b9a7e720387`; SkipDBv2 Regression run #460 built successfully but failed `udb_record_storage_test`.
- The regression captured two `UDB_at_()` results before checking either one, but both Datum payload pointers refer to the shared records cursor buffer. Reading the destination replaced the source payload before it was asserted.
- Check the source bytes before the next cursor read, then read and validate the destination. The production change and its source contract remain unchanged; no new files or CMake changes.
- CI retry required before further refactoring.

---

## 2026-11-07 07:01 CST — Repair record-storage regression build

- Baseline: main commit `8617adcdbb6704fc3b6933474cf6c665e4a2d027`; SkipDBv2 Regression run #459 failed during compilation because the new record-move regression referenced an undefined `has_value()` helper in `udb_record_storage.c`.
- Replace those helper calls with direct `Datum` length and byte comparisons. Keep the occupied-destination test intact and add workflow source-contract checks for the inline assertions. No production behavior changed in this repair; no new files or CMake changes.
- CI retry is required before further refactoring.

---

## 2026-11-07 07:00 CST — Refuse UDB record moves onto occupied extents

- Baseline: main commit `234cd05b9fd81b71f4e6395deefca51b3775feaf`; SkipDBv2 Regression run #458 passed source contracts, configure, complete build, and all regression tests.
- Scope: record relocation validation in `udb/UDBRecordMove.c`.
- Bounds checks alone allowed `UDBRecord_moveToPos_()` to overwrite a live record if the requested target happened to be inside the file and large enough, even though the function is used by the compactor to move live rows into free extents. It also accepted header-shaped payload bytes as a potential source position.
- Walk the validated physical record chain to resolve exact source/target boundaries. Permit a different target only when its persisted record is an empty extent large enough for the source; same-position moves remain harmless no-ops. Extend the existing record-storage regression with a live-destination collision case and update Actions source contracts. No new files, public symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:55 CST — Validate both UDB journals before arming commit recovery

- Baseline: main commit `a3e08fa14a07dd80fc409318f6899b3e5ebdb5a1`; SkipDBv2 Regression run #457 passed source contracts, configure, complete build, and all regression tests.
- Scope: the two-file commit gate in `udb/UDBTransaction.c`.
- The transaction path synced and finalized both journals before persisting the recovery marker, but did not run the existing strict journal parser against both prepared logs. A malformed interior journal record could therefore be declared recoverable before apply/replay later rejected it.
- Validate the complete index and record journals after prepare and before writing marker 1. Add a lifecycle regression that corrupts a persisted journal record while preserving its tail terminator, verifies the commit is rejected, and confirms the preceding committed value survives reopen while the failed write stays absent. Update the existing Actions source/runtime contracts only; no new files, exported symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:52 CST — Make UDBRecord metadata accessors NULL-safe

- Baseline: main commit `57be964b2e14e224d8bf677c986c0a91b7b85dd4`; SkipDBv2 Regression run #455 passed source contracts, configure, full build, and all regression tests.
- Scope: record identity and metadata accessors in `udb/UDBRecord.c`.
- Several public getters, setters, `UDBRecord_isEmpty()`, and `UDBRecord_show()` dereferenced a NULL record despite the surrounding record API already providing NULL-safe allocation/data/size helpers. `UDBRecord_setPosToEnd()` also cast a failed `JFile_setPositionToEnd()` result of -1 into the unsigned position field.
- Make metadata accessors NULL-safe with zero/no-op sentinel behavior. Only publish an end position when the file reports a nonnegative offset, so a missing file cannot corrupt the in-memory cursor. Extend the existing record-storage regression and its Actions source contract; no new files, exported symbols, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 06:50 CST — Honor UDB compaction time slices

- Baseline: main commit `12c784c219925e43db8727aa615b14bf4a17b7f1`; SkipDBv2 Regression run #454 passed source contracts, configure, full build, and all regression tests.
- Scope: compaction scheduling in `udb/UDBCompact.c`.
- `UDB_compactStepFor_(self, maxSeconds)` ignored its time budget and performed at most one physical compaction operation; the public `UDB_compact()` consequently re-entered the transaction machinery for every tiny step instead of allowing a bounded slice to make multiple units of progress.
- Isolate the existing atomic operation as `UDB_compactStepOnce_()`; let positive budgets repeat whole operations until the elapsed wall-clock slice expires or progress ends. Nonpositive budgets still perform at most one operation. Clock-read failures fall back to one step, backward clock adjustments do not extend a slice, and the integer operation count saturates rather than overflowing. Extend the existing compaction regression to verify that one positive time slice merges the adjacent hole and truncates the trailing extent. Update existing Actions contracts; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:46 CST — Keep Stack mark chains valid when ordinary pop reaches a mark

- Baseline: main commit `00ec98bd155b3df120d76d7af2311949736b33c8`; SkipDBv2 Regression run #453 passed source contracts, configure, full build, and all regression tests.
- Scope: mark lifecycle in `basekit/Stack_inline.h`.
- `Stack_pop()` removed the sentinel slot for a mark without rewinding `lastMark`. Once ordinary pops reached a mark, the mark index could exceed the new stack depth; later push/pop operations were then rejected by the mark-chain validator, leaving a live stack unusable.
- Validate the mark chain before popping and, when the removed top slot is the current mark, restore its recorded predecessor before decrementing the top pointer. Add a nested-mark regression proving normal pop can remove inner and outer mark slots and the stack remains usable afterward. Update existing Actions contracts only; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:36 CST — Snapshot aliased List removal inputs before mutation

- Baseline: main commit `e3cdfd63b94ac76c02aa8b51c58656c1d61dd48e`; SkipDBv2 Regression run #452 passed source contracts, configure, complete build, and all regression tests.
- Scope: the inline `List_removeItems_()` operation in `basekit/List_inline.h`.
- The self-removal guard handled only identical `List *` addresses. Distinct structs that shared `items` caused the removal loop to traverse an array while `List_remove_()` shifted its contents, so items were skipped; large lists could also compact/reallocate the source allocation during traversal.
- Clone the source pointer sequence before mutating a destination that shares its backing array. Allocation failure leaves the destination untouched; non-aliased calls retain the existing no-copy path. Add regression cases for a shorter shallow view and two identical views. Update existing Actions source/test contracts; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:33 CST — Advance UDB cursors using validated persisted extents

- Baseline: main commit `2e76f4feed899064ae9271d3f8624977510a9058`; SkipDBv2 Regression run #451 passed source contracts, configure, complete build, and all regression tests.
- Scope: shared sequential/relative record-offset helper in `udb/UDBRecordsCursor.c`.
- `UDBRecords_nextPosition_()` calculated the next row from the public `UDBRecord.header.size` cache. If a caller or prior failed operation left that cache stale, iteration could jump into payload bytes and treat a valid database as a corrupt chain.
- Reload and validate the physical header before deriving the next offset; retain the refreshed header and restore JFile's logical cursor on success. If the on-disk chain is invalid, close the streams rather than continuing from stale metadata. Add a regression that poisons the shared cursor's cached PID/size and requires EOF to be reached normally with the database stream still open. Update existing Actions contracts only; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:31 CST — Prevent UDB header writes from clobbering free-list metadata

- Baseline: main commit `757ac6a408b739653089ac3943199e5b14f18e2e`; SkipDBv2 Regression run #450 passed source contracts, configure, full build, and all regression tests.
- Scope: the standalone record-header writer in `udb/UDBRecordWrite.c`.
- `UDBRecord_saveHeader()` was exported and callable with a record position of zero; it wrote a full `UDBRecordHeader` at byte zero even though that region stores the database's first-empty pointer. That could silently corrupt allocator/compactor metadata.
- Reject reserved offsets below `sizeof(PID_TYPE)` and require an open data stream with a valid logical file bound before writing. Add a regression that calls the writer at offset zero and verifies it returns failure without appending any journal bytes. Update existing Actions contracts; no new files or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:27 CST — Validate persisted extents before relocating UDB records

- Baseline: main commit `1f2ff69566d1e022ea41f502080aeaceced68ddd`; SkipDBv2 Regression run #449 passed source contracts, configure, full build, and all regression tests.
- Scope: physical record relocation in `udb/UDBRecordMove.c`.
- `UDBRecord_moveToPos_()` derived the copied byte count from the caller's cached `header.size` and validated offsets against that count without re-reading the persisted header. A stale/small cache could therefore authorize a partial record copy into an overlapping destination.
- Re-read and validate the source header before deriving the relocation extent. On rejected moves, restore the caller's cached header and JFile cursor bounds. Extend the existing record-storage regression with a stale-size move whose actual extent does not fit the destination; discard any uncommitted journal fixture, then assert the persisted header and payload remain intact. Update existing Actions contracts. No new files, public declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:25 CST — Reject UDB record cursors inside reserved metadata

- Baseline: main commit `0d1b2116c72fe0d62da22881da113e9b52938a37`; SkipDBv2 Regression run #448 passed source contracts, configure, full build, and all regression tests.
- Scope: physical record offset validation in `udb/UDBRecordsCursor.c`.
- `UDBRecords_recordAtPos_()` checked the upper file bound but did not reject offsets below `sizeof(PID_TYPE)`, even though byte zero stores the first-empty free-list pointer, not a row. A specially shaped metadata/header byte sequence could therefore be accepted as a fabricated record.
- Reject reserved metadata offsets before moving the shared cursor or reading a header. Extend `udb_record_storage` with a crafted on-disk fixture that would parse as a valid EMPTY row at offset zero without the guard; restore the original header/metadata before continuing. Update existing Actions source/runtime contracts. No new files, declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:21 CST — Refresh persisted UDB headers before payload reads

- Baseline: main commit `a24d5684a0357bb4e4b8584e3e58efdeb0a9d0f2`; SkipDBv2 Regression run #447 passed source contracts, configure, complete build, and all regression tests.
- Scope: payload-read bounds in `udb/UDBRecordRead.c`.
- `UDBRecord_readDatum()` used the caller's cached `header.size` to validate and size the payload buffer without re-reading the header. A stale/small cached size could silently return a truncated payload; a forged larger size could cause a valid persisted value to be rejected.
- Refresh and validate the persisted header first, then derive the payload extent from that header. On failure, restore the previous cached header and JFile cursor bounds; clear a partially prepared payload buffer when a read fails. Extend the existing `udb_record_storage` test by poisoning cached PID/size and requiring the complete original binary payload and correct header metadata. Update existing Actions contracts. No new files, public declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:19 CST — Align UDB cursor-restoration contract with stale-header rejection

- Baseline: main commit `c0fdb6b536c4f22ed1b5e4731e7c5fa20a50c50d`; SkipDBv2 Regression run #446 stopped in the XY-C1 source-contract step before configure/build because the existing contract expected five references to `UDBRecord_restoreFileState_()`.
- The stale-header tombstone fix adds one deliberate failure-path cursor restoration after `UDBRecord_readHeader()` rejects an invalid persisted header. The implementation now has six references including the helper definition; update the existing exact-count contract from five to six.
- Workflow contract only; the C fix and regression test remain unchanged. No new files, declarations, or CMake changes.
- Compile and runtime regression gates remain pending until the next mainline run.

---

## 2026-11-07 06:17 CST — Preserve persisted metadata when removing stale UDB records

- Baseline: main commit `d29026875cd3b3180418c5b4975389c145f3b2f5`; SkipDBv2 Regression run #445 passed source contracts, configure, full build, and all regression tests.
- Scope: record tombstoning in `udb/UDBRecordWrite.c`.
- `UDBRecord_remove()` trusted the caller's cached `header.pid` and `header.size` and rewrote the entire header while intending to change only the EMPTY tag. A stale or altered cache could therefore replace valid on-disk identity/extent fields with incorrect values during removal.
- Reload and validate the persisted header before toggling the tag; retain the caller's cached header and JFile cursor bounds if validation fails, and restore the freshly-read header/cursor on write failure. Extend the existing `udb_record_storage` regression by deliberately zeroing the cached PID/size before removal and asserting that the committed tombstone retains the persisted PID and extent. Update existing Actions source/runtime contracts. No new files, declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:06 CST — Reject cross-file UDB record cursor and removal operations

- Baseline: main commit `eb143b3e28ded87da1b1959263139bc8509567c3`; SkipDBv2 Regression run #444 passed source contracts, configure, full build, and all regression tests.
- Scope: physical record cursor ownership in `udb/UDBRecordsCursor.c`.
- `UDBRecords_recordAfter_()` accepted a record belonging to another `JFile`, then interpreted its offset against the receiver's record file. A foreign row ending inside a local record could be mistaken for a truncated header and close the wrong database. `UDBRecords_removeRecord_()` likewise tombstoned the supplied record through its own foreign file handle, then updated the receiver's free-list metadata with that foreign offset.
- Require `record->file == self->file` before either operation; leave foreign records and both databases untouched on mismatch. Extend the existing `udb_record_storage_test` with two independent UDB files, a deliberately short foreign row, a wrong-store traversal check, and a write-enabled foreign tombstone check. Update the existing Actions contracts. No new files, declarations, headers, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 06:01 CST — Keep SkipDB cursor ownership registration consistent

- Baseline: main commit `d0becbde4bb4d19cf501b985f2c7f08dbf56b652`; SkipDBv2 Regression run #443 passed source contracts, configure, complete build, and all regression tests.
- Scope: cursor-to-database ownership transitions in `skipdb/SkipDBCursor.c`.
- `SkipDBCursor_sdb_()` previously replaced only the raw database pointer. A cursor already registered in the old database remained in its cursor list, while deallocation later tried to remove it from the new database. This could leave a freed cursor pointer in the old database's cache-protection list; the cursor also retained a record pointer and traversal counters from the old database.
- Register the destination first (fail without changing ownership if the destination is closed/unavailable), then remove the old registration and reset position/directional history. Detaching to NULL removes the old registration. Add lifecycle assertions to the existing `skipdb_core_layers_test`; update existing Actions contracts. No new files, declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 05:57 CST — Reject foreign cursors in SkipDB prefix iteration

- Baseline: main commit `5e7127ccdf802a4cbe61bd8c5bec83c9ed6a343b`; SkipDBv2 Regression run #442 passed source contracts, configure, complete build, and all regression tests.
- Scope: prefix-bound cursor iteration in `skipdb/SkipDBPrefix.c`.
- `SkipDB_list_next(self, key, cursor)` checked that both pointers were non-NULL but advanced the cursor without verifying that it belonged to `self`. Passing a cursor created by another open SkipDB silently traversed and returned records from the wrong database.
- Require `cursor->sdb == self` before stepping; mismatched cursors now return NULL without changing their position. Extend the existing `skipdb_core_layers_test` using two open databases, verifying a foreign cursor remains positioned on its original record. Update the existing workflow source/runtime contracts. No new files, declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 05:55 CST — Detach shallow aliases in List_copy_

- Baseline: main commit `a60d663d68d045d48f532205ee4a272a7c3a23ee`; SkipDBv2 Regression run #441 passed source contracts, configure, full build, and regression tests.
- Scope: List ownership/copy semantics in `basekit/List.c`.
- `List_copy_()` accepted two distinct structs that shared the same `items` allocation, then copied/truncated in place. The destination remained aliased to the source, so subsequent destination writes could silently mutate the source view; reallocating the aliased buffer would also risk invalidating the source pointer.
- Detect identical non-NULL backing pointers and allocate a separate destination buffer before publishing the copy. Preserve destination capacity where possible, zero-initialize spare slots, and leave the shared source allocation untouched on allocation failure or success.
- Extend the existing `list_storage_bounds_test` to verify detachment, element preservation, independent subsequent writes, and safe cleanup. Add source/runtime contract markers to the existing Actions workflow. No new files, declarations, or CMake changes.
- No local C compilation or CTest run is claimed; the mainline CI run is the compile/runtime gate.

---

## 2026-11-07 05:56 CST — Make MainArgs accessors NULL- and bounds-safe

- Baseline: main commit `fb74b42d610c21f12577cf7aa2348dd175b6072b`; SkipDBv2 Regression run #440 passed source contracts, configure, full compilation, and CTest after the SkipDB link-level contract repair.
- Scope: the two public read accessors in `basekit/MainArgs`.
- The public `MainArgs_argCount` and `MainArgs_argAt_` macros dereferenced the receiver directly; indexed access also trusted both the vector and caller-supplied index. NULL receivers, inconsistent public structs, negative indexes, or indexes at/above argc could therefore trigger invalid memory access.
- Route the existing macros through small exported checked accessors. The count accessor returns zero for NULL/negative/inconsistent state; indexed access returns NULL for NULL/empty/inconsistent state and any out-of-range index. Existing macro call syntax remains source-compatible, and each argument expression is evaluated once.
- Extend the existing lifecycle regression for NULL, malformed receiver, negative and upper-bound indexes, and normal access after replacement. Extend the existing Actions source/dynamic contracts. CMake already compiles the basekit source glob and registers `main_args_lifecycle_test`; no build-file or new-file changes.
- No local C compile or CTest run is claimed; the new mainline run is the validation gate.

---

## 2026-11-07 05:50 CST — Align SkipDB level-validation source contract

- Baseline: main commit `2748369d79f586aa0132b9fe23ae284834305953`; SkipDBv2 Regression run #439 stopped in the pre-build source-contract step.
- The new search guards correctly reuse `SkipDBRecord_hasValidLevel_()` at the initial node and each resolved forward target. The workflow still asserted the previous exact reference count of six, so its contract rejected the intentional two new calls before configure/build/CTest.
- Update the existing source contract to expect eight references and describe the shared search/link validation requirement. No C source or test behavior changed in this CI-only correction.
- The C compile and runtime regression gates remain pending on the next mainline run.

---

## 2026-11-07 05:49 CST — Reject invalid record levels before SkipDB search traversal

- Baseline: main commit `8b616b6543e11d850a48d4963a800cd25b2653b2`; SkipDBv2 Regression run #438 passed the XY-C1 source contracts, configure, complete build, and regression tests.
- Scope: skip-list search traversal in `skipdb/SkipDBRecordLinks.c`.
- `SkipDBRecord_find_quick_()` previously calculated the initial level before validating the root record. A negative/corrupt level could silently turn a lookup into a false “not found”; extreme negative values also make the subtraction unsafe. A resolved forward node's level was likewise trusted before resetting the traversal level.
- Validate the starting record before deriving its level, and validate every non-root forward target (including its pointer array and nonzero level) before comparing or descending. On invalid metadata, clear the update path and close UDB rather than returning a plausible miss.
- Extend the existing `skipdb_core_layers_test` with a negative-root-level corruption fixture and verify clean reopen/data recovery. Add source/runtime assertions to the existing Actions workflow. Reuse the registered test target; no new files, declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile and runtime gate.

---

## 2026-11-07 05:26 CST — Reject embedded NUL bytes in UArray file paths

- Baseline: main commit `cf06b1b3f34dbc46672a4cec09715e3326e97a20`; SkipDBv2 Regression run #437 passed the complete configure, build, and CTest sequence after the preceding UDB correction.
- Scope: file-path handling in `basekit/UArray_stream.c`.
- UArray text is length-delimited and `UArray_asUTF8()` preserves embedded U+0000. Passing that buffer to `fopen()` through `UArray_asCString()` silently truncates the path at its first NUL. On the write path this can truncate an unintended prefix file.
- Centralize path preflight: validate the source UArray layout, convert to UTF-8, validate the converted byte layout, reject NUL bytes within the logical path extent, and only then pass the C string to `fopen()`. Apply the same rule to reads and writes; validate the read destination before opening.
- Extend the existing `uarray_stream_io_test` to verify embedded-NUL paths are rejected for both read and write and that the prefix file remains unchanged. Add source and runtime regression contracts in the existing Actions workflow. Reuse the registered CTest target; no new files, APIs, or CMake changes.
- No local C compilation or CTest run is claimed; the existing mainline CI remains the compile and runtime gate.

---

## 2026-11-07 05:22 CST — Restore nonzero UDB PID mapping writes

- Baseline: main commit `b8a59323c4a51005047e1a0b952c1cef1c5c1e2e` (`fix(udb): reject mappings for unreserved PIDs`). The preceding SkipDBv2 Regression run #435 passed against commit `08a2873b6a0c72db2f00b4bf19b7a60e586a4f4c`; no CI result for the b8a5932 revision was recorded when this correction was prepared.
- Scope: write control flow in `udb/UDBIndexMap.c`, specifically `UDBIndex_setPos_forPid_()`.
- Moving the existing-mapping/reservation guard ahead of the write left a stale outer `if (pos == 0)` around the write path. Nonzero PID assignments therefore skipped the index write and reached the end of the function without returning a result, breaking inserts and replacements.
- Remove the stale outer conditional so both zero (release) and nonzero (assign/replace) operations perform the validated fixed-width write. Preserve the post-write release/unreserve/forget handling. The existing `udb_index_layers` regression already covers fresh PID assignment and replacement; the workflow source contract already checks that the ownership guard precedes the write.
- No new files, declarations, public API, CMake changes, or test-target changes. No local C compilation or CTest run is claimed; the pushed source correction triggers the existing mainline regression workflow.

---

## 2026-11-07 05:15 CST — Reject UDB index mappings for unreserved PIDs

- Baseline: main commit `08a2873b6a0c72db2f00b4bf19b7a60e586a4f4c`; SkipDBv2 Regression run #435 completed successfully.
- Scope: PID mapping ownership in `udb/UDBIndexMap.c`.
- `UDBIndex_setPos_forPid_()` checked that a PID already existed or was reserved only when clearing a mapping. A direct nonzero update could therefore manufacture a mapping for an unallocated PID, extend the index with sparse slots, and advance the next-PID calculation.
- Apply the existing-mapping-or-active-reservation check before either mapping direction. Existing PID relocation/replacement remains valid, while an unreserved unmapped PID is rejected before the journal is touched.
- Extend the existing `udb_index_layers` regression with sparse-slot rejection and replacement of a committed PID after its reservation has been cleared. Add an Actions source-contract ordering check proving the ownership guard precedes the index write. Reuse the existing CTest target; no new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile and runtime gate.

---

## 2026-11-07 05:01 CST — Make first UDB commit-marker creation directory-durable

- Baseline: main commit `bfd9e71c5bf7d11b73c798dac96becd5ddb94b77`; SkipDBv2 Regression run #434 completed successfully after the cache-age traversal correction.
- Scope: first-open crash ordering in `udb/UDBRecovery.c`.
- On a brand-new database, the marker file contents were fsynced before index and record artifacts were created, but the marker's parent directory entry was not fsynced until the very end of startup. A power loss in between could leave durable database artifacts with no durable marker name; the next open correctly refuses that ambiguous state.
- Immediately fsync the commit-marker parent directory after its initial clean-marker write and before opening either database artifact. Retain the existing end-of-open directory syncs for the data and journal directories.
- Strengthen the existing Actions source contract with an ordering assertion proving marker write, marker-directory sync, and first artifact open remain in that order. No new API, file, or CTest target.
- No local C compilation or crash-injection test is claimed; mainline CI is the compile and integration gate.

---

## 2026-11-07 04:58 CST — Bound SkipDB cache-age traversal and recover cycles

- Baseline: main commit `41f6250563ec6d97b7de6562395a8749c4aade97`; SkipDBv2 Regression run #433 completed successfully after the UArray path-boundary correction.
- Scope: cache age-list integrity and cleanup in `skipdb/SkipDBCache.c`, exercised by the existing `skipdb_core_layers` regression.
- Cache eviction walked the `olderRecord` chain in several phases without a traversal bound or reciprocal-link check. A cycle could therefore hang a transaction commit indefinitely; cleanup also assumed the chain was acyclic.
- Add a private preflight that verifies the age-list count and every `youngerRecord` back-link before eviction mutates marks, links, or records. Invalid state now aborts the active UDB transaction rather than partially evicting cache entries.
- Before full cache teardown, use cycle detection to cut an older-link cycle, rebuild reverse links, and recompute the cache count so close/reopen cleanup can release the cache without looping.
- Extend the existing core-layer test with a cyclic age-list fixture: commit must fail closed, and reopening must repair and reload persisted records. Existing CMake/CTest target is reused; no new files or public API.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 04:49 CST — Normalize trailing separators in UArray path boundaries

- Baseline: main commit `95a33bd84b0749119122337db0e963c9ef5b81d4`; SkipDBv2 Regression run #432 passed source contracts, configure, full build, and all CTest cases.
- Scope: path-component boundary handling in `basekit/UArray_path.c` and its existing `uarray_path_null` regression.
- A trailing separator run was removed only from the temporary search view. The resulting start offset was then reused with the original array size, so last-component, extension, and filename extraction included trailing separators; component removal/clipping could also leave inconsistent suffixes or treat a separator-only root as a component.
- Add one private typed-array boundary helper that computes both the component start and end after trimming trailing separators. Reuse it across component lookup, extension parsing, filename extraction, removal, and clipping while retaining the existing CTYPE, width, overflow, and backing-storage validation.
- Extend the existing CTest regression and static/dynamic Actions contracts for trailing separators and root-only paths. The current CMake test target already covers this file; no new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 03:46 CST — Repair UArray reverse range contract boundary

- CI run #431 stopped in the XY-C1 source-contract check before configure because the `UArray_reverse()` contract could not locate its end boundary.
- The workflow used `/*\\n#define` as a literal backslash-n marker, but the C source has a real newline before the following comment. The empty extracted function body therefore failed the otherwise-correct CTYPE-layout guard check.
- Bound the reverse-function contract at `chr(10) + "/*"`, preserving the existing guard/order assertions while matching the source layout. No C implementation, test, or CMake behavior changed.

---
## 2026-11-07 03:40 CST — Refresh stale UArray string source contract

- CI run #430 stopped in the XY-C1 source-contract check because the `UArray_string.c` manifest still required the old inline `self->size / self->itemSize` check in `UArray_splitInputsAreValid_()`.
- The implementation now delegates that geometry check to `UArray_hasValidStringItemLayout_()`; the contract is updated to require the actual shared-validator guard rather than a removed duplicate expression.
- No C implementation or test behavior changed in this CI repair.

---

## 2026-11-07 03:39 CST — Guard UArray reverse/swap byte offsets with CTYPE layout checks

- Baseline: main commit `e9a1c4b34cd0f54c710b3a303ad72917073c26a6`; SkipDBv2 Regression run #429 passed source contracts, configure, full build, and all CTest cases.
- Scope: `UArray_reverse()` and `UArray_swapIndex_withIndex_()` in `basekit/UArray_string.c`.
- Both mutators calculated byte offsets from `itemSize` after checking only that it was nonzero and below the temporary union size. A forged width larger than the declared CTYPE width could therefore make them access beyond the actual array allocation.
- Reuse `UArray_hasValidStringItemLayout_()` before any per-item address calculation, retaining each function's existing index and temporary-buffer bounds checks.
- Extend the existing `uarray_reverse_item_byte_order` CTest with valid item-order/hash behavior and malformed width/type no-op regressions. Strengthen static and dynamic Actions contracts. No new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 03:36 CST — Validate CTYPE geometry before UArray byte translation

- Baseline: main commit `129c44ba2398f061a2ee9573d6a65493ec62e7af`; SkipDBv2 Regression run #428 passed source contracts, configure, full build, and all CTest cases.
- Scope: `UArray_translate()` in `basekit/UArray_string.c`.
- The byte translation routine checked only `itemSize == 1`. A malformed array declaring a wider CTYPE while advertising a one-byte item width passed validation and was then treated as a byte string, allowing the routine to mutate storage under inconsistent element metadata.
- Reuse the module's existing private string-layout validator for receiver, source map, and target map, then explicitly require one-byte item widths. All three layouts now pass CTYPE/width/overflow/backing-storage checks before the map is read or receiver mutation begins.
- Extend the registered `uarray_translate_validation` CTest with mismatched receiver/source/target CTYPE regressions and no-op/hash-preservation assertions. Strengthen static and dynamic Actions contracts. No new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 03:33 CST — Repair division-contract Python block placement

- CI run #427 failed before configure because the newly added floating-quotient assertions were inserted before the existing division-contract `if (...):` block had closed.
- Move the additional checks after that block's existing `raise SystemExit`; no C source or regression behavior changed in this repair.
- The corrected Actions script now parses the full original division contract before evaluating the new float32/float64 range, non-finite, and test-coverage assertions.

---

## 2026-11-07 03:29 CST — Preflight floating quotients before integer UArray division

- Baseline: main commit `147a8f9474f543b370fb842cb2140f2b5e6891fc`; SkipDBv2 Regression run #426 passed source contracts, configure, full build, and all CTest cases.
- Scope: integer-receiver elementwise division in `UArray_math.c`.
- The existing preflight rejected zero divisors and signed-minimum divided by `-1`, but it skipped range analysis whenever the divisor array used a floating CTYPE. A finite or non-finite floating quotient could therefore reach the generic compound assignment and be converted to an out-of-range integer, which is undefined behavior.
- Evaluate float32/float64 divisors using the corresponding arithmetic precision, then reject non-finite or unrepresentable receiver results before any element is mutated. Keep valid mixed integer/floating division and existing integer divisor behavior.
- Extend the registered `uarray_math_extrema` CTest with valid mixed float64, float32 overflow, and NaN atomicity cases; strengthen static and dynamic Actions source contracts. No new files, public declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 03:24 CST — Validate UArray C-stream I/O against CTYPE widths

- Baseline: main commit `7c915cb673f64a3b9bf1db73c05eaf8982f40440`; SkipDBv2 Regression run #425 passed source contracts, configure, full build, and all CTest cases.
- Scope: typed-layout preflight shared by `UArray_stream.c` read and write entry points.
- Stream operations previously trusted `itemSize` independently of `itemType`. A forged width could make reads/writes use a byte extent inconsistent with the typed array, and the line reader could consume input before its later chunk-width check rejected the array.
- Add one private layout validator, explicitly reference the existing `CTYPE_size()` symbol, and reject invalid widths, overflowing extents, or missing storage before any stream side effect. Apply it to item reads, bulk reads, line reads, and write paths.
- Extend the existing `uarray_stream_io` regression to cover all affected C-stream APIs and prove malformed file writes do not truncate an existing destination. Strengthen Actions source/dynamic contracts. Reuse existing CTest target; no new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 03:20 CST — Accept space-padded %e, %k and %l fields

- Baseline: main commit `808cc1e380363c1aa19e9e6d40926c45268d0490`; SkipDBv2 Regression run #424 passed source contracts, configure, full build, and all CTest cases.
- Scope: numeric field parsing for `%e`, `%k`, and `%l` in `PortableStrptime.c`.
- These directives were recognized but rejected their specified single-column space-padded values because the parser required the first input byte to be a digit. This also made direct parsing of common space-padded day/hour fields fail.
- Consume one optional literal space for the three space-padded directives before the existing digit and range checks. Keep `%d`, `%H`, and `%I` strict and preserve all existing range validation.
- Extend the existing `portable_strptime_bounds` test with standalone padded day/hour cases and the built-in composite `%C` format. Strengthen static and dynamic Actions source contracts. Reuse the registered CTest target; no new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 03:15 CST — Validate BStream array layouts before overlap and decode

- Baseline: main commit `4add9d4ddd291f39e39321d6487e0d4520d3de56`; SkipDBv2 Regression run #423 passed source contracts, configure, full build, and all CTest cases.
- Scope: the shared `BStream_canReadArray_()` preflight used by plain and tagged array reads.
- The preflight trusted nonzero `itemSize` values without checking them against the declared `CTYPE`. Its backing-store overlap calculation therefore used a potentially false destination extent, and a malformed stream buffer could be consumed as a byte array despite inconsistent element metadata.
- Declare the existing internal `CTYPE_size()` symbol explicitly and reject stream/destination layouts whose stored width differs from the type width before overlap checks or cursor movement.
- Extend the existing `bstream_tag_boundaries` regression and strict Actions source/dynamic contracts. Reuse the registered CTest target; no new files, public API, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 03:09 CST — Reject malformed UArray element layouts before typed copies

- Baseline: main commit `168041dc8810aa368222e6b26bc62749a3b3bde7`; SkipDBv2 Regression run #422 passed source contracts, configure, full build, and all CTest cases.
- Scope: `UArray_copy_()` and `UArray_copyItems_()` in `basekit/UArray.c`.
- The typed-copy path accepted a nonzero `itemSize` that disagreed with `CTYPE_size(itemType)`. When the declared C type differed from the destination, `DUARRAY_OP` indexed using the C type width and could read beyond malformed source storage or write beyond malformed destination storage.
- Add one private layout validator and run it before `UArray_copy_()` changes destination metadata and before `UArray_copyItems_()` performs typed access. Reject invalid CTYPE widths, overflowing extents, and missing non-empty storage.
- Extend the existing `uarray_stack_resize` regression and strict Actions source/dynamic contracts. Reuse the registered CTest target; no new files, public declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 02:51 CST — Preserve Stack source storage when copying into a shallow alias

- Baseline: main commit `bc5f3ad53f9448955e1f441d79e793424318a932`; SkipDBv2 Regression run #421 passed source contracts, configure, full build, and all CTest cases.
- Scope: `Stack_copyState_()` and the existing `stack_lifecycle` regression.
- A distinct `Stack` struct can shallow-alias the source's `items` allocation. Copying correctly creates and publishes independent storage, but the destination cleanup then unconditionally frees its old `items` pointer—which still belongs to the source. This leaves the source with dangling `items` / `top` / `memEnd` pointers.
- Preserve the shared allocation when `oldItems == other->items`; free only a genuinely separate previous destination allocation. Add a regression that copies into a shallow struct alias and verifies both stacks retain independent, readable contents.
- Extend the existing Stack source/test contracts. Reuse the registered `stack_lifecycle_test` target; no new files, public declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 02:48 CST — Preserve SHash source storage when copying into a shallow alias

- Baseline: main commit `fb2ebb56caec60e14ea85e618c4425bdcb87ecb5`; SkipDBv2 Regression run #420 passed the source contracts, configure, full build, and CTest suite.
- Scope: `SHash_copy_()` in the cuckoo-hash module and the existing `shash_compact` regression.
- A distinct `SHash` struct can be a shallow alias of another table. The copy routine correctly allocated and published an independent destination table, but unconditionally freed the destination's previous `records` pointer—which was still owned by the source alias. The source then retained a dangling pointer and could use freed storage or double-free it.
- Match the established PHash ownership rule: release the old destination allocation only when it differs from the source's backing pointer. Add a regression that shallow-copies the struct, deep-copies into the alias, and verifies both source and destination can still look up the same key/value.
- Extend the existing SHash source/test contracts. Reuse the registered `shash_compact_test` CMake target; no new files, public declarations, or CMake changes.
- No local C compilation or CTest run is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 02:45 CST — Make List append source-contract boundary unambiguous

- Baseline: main commit `c4b25098b7157ca352af1bd85df0d452238f2976`; the List shared-storage implementation and regression are awaiting the first CI result.
- The newly added ordered-source contract used a double-escaped newline pattern, which would search for a literal backslash sequence instead of the end-of-function newline. Resolve the boundary with `chr(10) + "}"`, matching the existing robust workflow-contract convention.
- Workflow contract only; no C implementation, regression logic, CMake target, or source path changed.

---

## 2026-11-07 02:43 CST — Detach aliased List storage before sequence append

- Baseline: main commit `462c45fe8f953a87ce6ac9d991ce154dd351229b`; SkipDBv2 Regression run #418 passed.
- Scope: `List_appendSeq_()` in `basekit/List_inline.h` and the existing `list_storage_bounds` regression.
- Distinct `List` structs can expose the same backing `items` allocation. Appending one view to another in place can overwrite source elements when the logical views overlap; growing the destination can also invalidate the source view through `realloc()`.
- Detect shared backing storage and copy both logical views into a new zero-initialized destination allocation before publishing it. Keep the original allocation and source view intact, so the two lists have independent storage and can be freed independently. Add a regression with an overlapping one-element destination view and a three-element source, then strengthen the existing static/dynamic Actions contracts.
- Reuse the registered `list_storage_bounds_test` CMake target; no new files, API changes, or build-list updates.
- No local C compilation or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 02:39 CST — Preserve PHash source storage across shallow-alias copies

- Baseline: main commit `593f91324945dd087a2411a02c0f683178e80629`; SkipDBv2 Regression run #417 passed.
- Scope: `PHash_copy_()` and the existing `phash_compact` regression.
- `PHash_copy_()` deep-copied the source records, published the new destination buffer, then unconditionally freed the destination's previous buffer. When two distinct public `PHash` structs were shallow aliases of the same record allocation, this freed the source table's live storage and left it dangling.
- Keep the destination's normal deep-copy behavior, but release its previous buffer only when it is not the source's shared allocation. Add a regression using a shallow struct alias and verify both tables remain readable; extend existing static/dynamic Actions contracts. Reuse the registered CMake target; no new files or public API changes.
- No local C build or CTest run is claimed; mainline CI is the compile/runtime gate.

---
## 2026-11-07 02:35 CST — Preserve full-width UArray floating-format output

- Baseline: main commit `2d320cb4eb92b567a075957e98ff29d4a3cd2080`; SkipDBv2 Regression run #416 passed.
- Scope: the `%f` branch in `basekit/UArray_format.c` and the existing formatter regression.
- The formatter used a fixed 100-byte buffer and ignored `snprintf()`'s required output length. Large finite doubles such as `DBL_MAX` therefore produced silently truncated text even though the formatted output is valid and longer than the buffer.
- Query the required length with C99 `snprintf(NULL, 0, ...)`, allocate exactly the required storage, verify the second formatting pass, append only a complete result, and free the temporary buffer. Add a `DBL_MAX` regression and strengthen static/dynamic Actions contracts. Reuse the registered CMake target; no new files or API changes.
- No local C build or CTest run is claimed; mainline CI is the compile/runtime gate.

---
## 2026-11-07 02:32 CST — Update Date_compare ordered contract for exact field comparison

- Baseline: main commit `64aea53ab55aa4d3bfc8e3c7db578a5a91e6bd2f`; SkipDBv2 Regression run #415 failed in the XY-C1 integration/source-contract step after the Date needles were correctly moved into the Date block.
- The existing ordered Date entry-point contract still required `Date_asSeconds(self)` as the operation in `Date_compare()`. That expectation contradicted the new exact `tv_sec`/`tv_usec` comparison; the dedicated Date check already verifies both fields and rejects floating conversion.
- Change only the Date_compare operation needle to the exact seconds-field comparison. This preserves the NULL-guard ordering contract and removes the stale double-conversion requirement.

---
## 2026-11-07 02:30 CST — Correct Date source-contract ownership after CI rejection

- Baseline: main commit `42a4d5bead1f05a989d71890cb4a58e39a161d06`; SkipDBv2 Regression run #414 stopped in the XY-C1 integration source-contract check before configure/build.
- The Date comparison needles were inserted after the first repository-wide NULL-comparison match, which belonged to `BStreamTag.c`, not `Date.c`. The contract checker therefore correctly reported missing Date-only text in BStreamTag.
- Move those four needles into the existing Date.c contract block and restore the BStreamTag block. No C source or test logic changed; this is an in-place workflow-contract correction.

---
## 2026-11-07 02:26 CST — Compare Date timestamps without double precision loss

- Baseline: main commit `8909b11b2b2a705fbcc88d1f5b44f1c37a0d9f6a`; SkipDBv2 Regression run #413 passed.
- Scope: `Date_compare()` in `basekit/Date.c` and the existing `date_time_init` regression.
- The comparator converted both timestamps to `double` through `Date_asSeconds()`. Above 2^53 seconds, adjacent whole seconds can compare equal; microsecond differences at the same large second are also lost.
- Compare `tv_sec` first and `tv_usec` second, returning the existing -1/0/1 convention without floating-point conversion. Add guarded regression coverage for adjacent large seconds and adjacent microseconds, plus static/dynamic Actions contracts. Reuse the registered CMake target; no new files or public API changes.
- No local C build or CTest run is claimed; mainline CI is the compile/runtime gate.

---
## 2026-11-07 02:24 CST — Repair UArray distance return after scaled accumulation

- Baseline: main commit `4b32dcbbf6c275d3a7bd13e6bdc4f03e4651c9fe`; SkipDBv2 Regression run #412 failed in `uarray_distance_precision_test`.
- CI showed the new `hypot()` accumulation was still passed through the old final `sqrt(sum)`, producing `sqrt(distance)` and breaking the existing float32 distance assertion. The compile and all earlier contracts passed.
- Return the accumulated norm directly in both typed branches. Tighten the ordered Actions contract to require one direct `return sum;` per branch and forbid the obsolete second square root. This is an in-place correction of the same UArray distance change; no new files or test-target changes.

---
## 2026-11-07 02:20 CST — Prevent overflow in UArray Euclidean distance accumulation

- Baseline: main commit `c653b77949390ae6f4155d49bfa7a36f3eceeb7f`; SkipDBv2 Regression run #411 passed.
- Scope: `UArray_distanceTo_()` in `basekit/UArray_math.c` and its existing `uarray_distance_precision` regression.
- The implementation accumulated `d * d` before taking the square root. A finite and representable distance such as two float64 deltas of `1e200` overflowed the intermediate squares to infinity, incorrectly returning infinity.
- Accumulate each component with `hypot(sum, (double)d)` in both float32 and float64 branches. This performs scaled norm accumulation without avoidable intermediate square overflow. Add the two-component `1e200` regression and strengthen static/dynamic Actions contracts. Reuse the registered CMake target; no new files, public API, or build registration.
- No local C build or CTest run is claimed; mainline CI is the compile/runtime gate.

---
## 2026-11-07 02:22 CST — Validate UArray split element layouts before scanning

- Baseline: main commit `016c5efa42077873e1c4706768d4d5d94349110c`; SkipDBv2 Regression run #410 passed.
- Scope: the private input preflight in `basekit/UArray_string.c` and the existing `uarray_split_delimiters` regression.
- The split preflight checked byte extents and the pointer-container geometry but did not verify that the source or each non-empty delimiter's `itemSize` matched its declared `CTYPE`. The downstream search routine rejected such layouts, but split silently treated malformed delimiters as non-matches and could return a full copy of malformed source data as a valid result.
- Reuse `UArray_hasValidStringItemLayout_()` to validate source and non-empty delimiter storage before allocating results or scanning. Preserve the existing NULL/empty delimiter skip behavior. Add regressions for mismatched source metadata, mismatched delimiter metadata, and missing delimiter storage; update static and dynamic Actions contracts. Reuse the registered CMake target; no new files, public API, or build registration.
- No local C build or CTest run is claimed; mainline CI remains the compile/runtime gate.

---
## 2026-11-07 02:18 CST — Preflight UArray file writes before destructive open

- Baseline: main commit `468b9e1337d0957a06b1967d69c8e68f89cbb015`; SkipDBv2 Regression run #409 passed.
- Scope: the file-write lifecycle in `basekit/UArray_stream.c` and the existing `uarray_stream_io` regression.
- `UArray_writeToFilePath_()` opened the destination with `"wb"` before the lower-level writer validated the source. A malformed non-empty UArray with a NULL data pointer therefore caused an error only after the existing destination had already been truncated. The stream writer also performed its LONG_MAX return-range check after writing.
- Add one private source-storage preflight covering byte-extent overflow, NULL backing storage, and the representable `long` result count. Apply it before stream writes and, critically, before path conversion/open. Extend the existing test to prove an invalid source leaves an existing file's contents untouched, and strengthen static/ordered Actions contracts. Reuse the registered CMake target; no new files, API changes, or build registration.
- No local C build or CTest run is claimed; the pushed SkipDBv2 Regression workflow is the compile/runtime gate.

---

## 2026-11-07 02:11 CST — Avoid narrowing calendar years in Date_isLeapYear

- Baseline: main commit `e3b06a6c47362548e11dd26de797ed373f04eb5d`; SkipDBv2 Regression run #408 passed.
- Scope: the calendar-year arithmetic in `Date_isLeapYear()` and its existing `date_time_init` regression.
- The function narrowed `Date_year()` from `long` to `int` before applying the Gregorian 4/100/400 rule. A valid future year above `INT_MAX` could therefore be reinterpreted implementation-dependently and produce the wrong leap-year result.
- Read `tm_year` through the existing NULL-safe local-time helper, widen it to `long long` before adding 1900, and apply the Gregorian divisibility test without narrowing. Add an existing-target regression for a representable year above `INT_MAX` divisible by 400, and extend static/dynamic source contracts. No new files, public API, or CMake changes.
- No local C build or CTest run is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 02:09 CST — Preserve required directives across input whitespace in PortableStrptime

- Baseline: main commit `8a32c7174ecbaac13ac36fd7fea51bef51dfee4a`; SkipDBv2 Regression run #407 passed.
- Scope: the private whitespace shortcut in `basekit/PortableStrptime.c` and its existing parser-boundary regression.
- The shortcut advanced the format cursor to the next whitespace whenever the next input byte was whitespace. For a format such as `%Y-%m-%d`, input like `2024-11 x` could therefore skip the required `-%d` tail and return success after parsing only the year and month.
- Remove that shortcut and rely on the parser's existing format-whitespace handling. Add regressions for unexpected whitespace after the year/month and for valid whitespace-separated date fields. Update the existing static/dynamic Actions source contracts. Reuse the existing CMake target; no new files, declarations, or public API.
- No local C build or CTest run is claimed; the pushed SkipDBv2 Regression workflow is the compile/runtime gate.

---

## 2026-11-07 02:04 CST — Preserve embedded NUL bytes in UArray UTF-8 conversion

- Baseline: main commit `277b727ee584f02871f2ab42a7672a3dcbecf084`; latest mainline SkipDBv2 Regression run #406 passed.
- Scope: `UArray_asUTF8()` in `basekit/UArray_utf.c` and the existing `uarray_encoding_conversion` regression.
- The conversion used `strlen()` to infer the output length. UArray is length-delimited and can legally contain U+0000, so UTF-8 copies and UTF-16/UTF-32 conversions silently discarded every byte after the first embedded NUL.
- Derive the UTF-8 result length from the original UArray size for ASCII/UTF-8 copies, and from the converter's final target cursor for UTF-16/UTF-32 input. Add regression coverage for all three source encodings and extend the existing static and ordered Actions source contracts. The existing CMake/CTest target is reused; no new files, API changes, or build registrations.
- No local C build is claimed; the pushed mainline CI is the compile/runtime gate.

---

## 2026-11-07 01:50 CST — Consume Windows dynamic-loader errors on retrieval

- Baseline: main commit `ab59b37cf842f8aec8f553e4c7dfd7f1c1a5351d`; SkipDBv2 Regression run #405 passed.
- Scope: the Windows `dlerror()` compatibility shim in `basekit/DynLib.c`.
- The shim formatted `GetLastError()` but left the thread's last-error value set. Unlike POSIX `dlerror()`, repeated retrievals therefore returned the same stale loader failure; `DynLib_updateError()` could not observe the consumed state and clear the stored error consistently across platforms.
- Clear the Win32 last-error state after capturing either a formatted message or the fallback message. Extend the existing lifecycle test to verify that a second update consumes/clears the prior loader error. No public API, new files, or CMake changes.
- No local Windows build is claimed; Linux CI verifies shared lifecycle behavior, while the Windows-only shim is guarded by source contracts.

---

## 2026-11-07 01:46 CST — Bound Date calendar-year conversion before narrowing

- Baseline: main commit `744afb9016bafa4757b904862573a409a60dde39`; SkipDBv2 Regression run #404 passed after the Stack cursor contract correction.
- Scope: `Date_setYear_` calendar-year conversion.
- The setter subtracted 1900 from a caller-supplied `long` and assigned the result to `struct tm.tm_year` (`int`) without checking either the subtraction or narrowing range. Extreme inputs could trigger signed overflow or implementation-defined truncation.
- Add one private checked conversion helper and reject values outside `INT_MIN..INT_MAX` before calling `localtime` or mutating the Date. Extend the existing Date time-initialization regression for a normal year, `LONG_MIN`, and (where representable) `LONG_MAX`. Reuse the existing CMake target; no new files or public API.
- No local C build is claimed; CI remains the compile/runtime gate.

---

## 2026-11-07 01:43 CST — Align Stack cursor-reader contract with shared validation

- Baseline: main commit `5f7147b8942325d252e6f4282d4317cdd6175978`; run #403 passed the static source-needle list but failed the later ordered Stack reader check.
- Update that check to require each reader to call `Stack_hasValidStorage_`, which now owns the NULL-end and one-past-end checks centrally. This preserves the stronger centralized invariant instead of requiring duplicated local guards.

---

## 2026-11-07 01:40 CST — Align Stack cursor source contract with the validator declaration

- Baseline: main commit `77b1e29d7288a88e294199588d7b6132f236f2a3`; run #402 failed only during the XY-C1 static source-contract check.
- Correct the Stack inline-header contract to match the actual grouped `uintptr_t` declaration. The implementation and test logic are unchanged; this is a CI-contract-only correction.

---

## 2026-11-07 01:37 CST — Validate Stack cursor geometry without undefined pointer arithmetic

- Baseline: main commit `1538ea8f5f5a172b5733abd4fa4ad167128b270d`; SkipDBv2 Regression run #401 passed.
- Scope: the Stack storage/cursor invariant in `basekit/Stack.c` and its inline API.
- The validity checks compared and subtracted caller-visible `items`, `top`, and `memEnd` pointers before proving they belonged to one array. A corrupted or misaligned cursor can make those relational operations undefined before the guard has a chance to reject it. The same range logic was duplicated across push/pop/accessor paths.
- Centralize storage validation using `uintptr_t` address ordering and pointer-slot alignment before any subtraction or indexing; share the mark-chain validator with clone/resize and mark mutation paths. Keep the existing stack layout and public API. Copy replacement now frees the old backing store only when its storage geometry is valid, avoiding an invalid free on a malformed destination.
- Add a malformed-cursor regression that exercises accessors, mutation, clone, memory sizing, and copy-source rejection. Update the existing Actions source contracts. No new files, public API, or CMake targets.
- No local C build is claimed; CI remains the compile/runtime gate.

---

## 2026-11-07 01:23 CST — Scope UArray path extensions to the basename

- Baseline: main commit `21459851297d0f71e75507a639e7b2b92fb83048`; mainline regression run #399 passed.
- Scope: `UArray_findPathExtension()` and the existing `uarray_path_null` boundary test.
- The implementation searched for the last dot anywhere in the path. A dot in a parent directory (for example, `dir.with.dot/file`) was incorrectly returned as the file extension; `UArray_fileName()` then returned NULL for a valid filename. A leading dot in a dotfile was also mistaken for its extension, producing an empty filename.
- Resolve the basename offset first and accept a dot only when it occurs after the basename's first character. This preserves ordinary multi-part extensions while treating parent-directory dots and leading dotfiles as extension-free. Add both cases to the existing test and strengthen static/dynamic Actions contracts. Existing CMake registration is reused; no new files.

---

## 2026-11-07 01:34 CST — Validate SHash geometry before table access

- Baseline: main commit `11433b678edbbeb9420ce79f8c9ee07e58c64a44`; SkipDBv2 Regression run #399 passed.
- Scope: the callback-driven string-key cuckoo hash table in `basekit/SHash.c` and its existing inline API.
- As with the pointer-key table, grow and compact checked their capacity metadata, but hot-path lookup, insert, delete, cleanup, enumeration, and copy/clone did not share that invariant. A damaged `tableSize` or `mask` could route the custom hash callback into an out-of-bounds slot; copy could also trust an invalid record extent.
- Add a shared geometry validator without requiring optional equality/hash callbacks, preserving the documented ability to create an empty SHash before configuring its callbacks. Gate all indexed/bulk operations and both sides of copy; retain callback-specific checks in operations that need them. Make the table initializer file-local.
- Extend the existing `shash_compact` test across five malformed geometry states, including a four-record allocation advertising a maximum-sized table, and add matching Actions source contracts. No new files, public API, or CMake targets.
- No local C build is claimed; CI remains the compile/runtime gate.

---

## 2026-11-07 01:31 CST — Validate PHash geometry before hot-path access

- Baseline: main commit `f96e49e290d73172737eb61b7ec92071c5aeca65`; SkipDBv2 Regression run #398 passed.
- Scope: the pointer-key cuckoo hash table in `basekit/PHash.c` and its existing inline API.
- Grow and compact already rejected inconsistent table geometry, but lookup, insertion, deletion, cleanup, enumeration, copy, and clone could still trust public `tableSize`/`mask` fields. Corrupted or partially initialized metadata could therefore index beyond the backing allocation or make copy/clone read an oversized record span.
- Add one shared geometry validator (bounded log2, exact power-of-two size, matching mask, non-NULL storage, and a representable key count) and apply it before indexed access or bulk traversal. Guard both source and destination for copy; keep cuckoo placement, hashing, valid-table behavior, and capacity policy unchanged. Make the table initializer file-local.
- Extend the existing `phash_compact` regression across five malformed geometry states, including a four-record allocation advertising a maximum-sized table. Strengthen existing Actions source contracts. No new files, public API, or CMake targets.
- No local C build is claimed; CI remains the compile/runtime gate.

---

## 2026-11-07 01:20 CST — Bound Windows PortableUsleep busy-waiting and clock failures

- Baseline: main commit `e3a4a6becfbf9711c1b0ec459859e78e01eca94a`; latest mainline regression run #397 passed.
- Scope: the Windows `usleep()` compatibility implementation in `basekit/PortableUsleep.c`.
- The previous implementation spun continuously for the entire requested interval, shared an unsynchronized static performance-counter frequency across threads, and ignored failures from both QueryPerformance APIs. Those failures could use uninitialized counters or a zero frequency and return too early.
- Query the clock per call, return immediately for zero delay, validate frequency/start counter, and use a rounded-up `Sleep()` fallback if the high-resolution clock fails or the tick conversion is out of range. For normal waits, let the OS sleep for all but the final <2 ms and spin only for the remaining fraction; if a later counter read fails, sleep the maximum residual interval.
- Add strict static and ordered dynamic Actions source contracts; the existing recursive basekit source glob already includes this translation unit. No new files. CI does not execute the Windows-only branch natively, so platform runtime behavior remains to be verified by a Windows build.

---

## 2026-11-07 01:17 CST — Make allocator diagnostics type-safe and failure-safe

- Baseline: main commit `eb4a5e0c50e15b6d18a82ce38a915cae820f0c96`; SkipDBv2 Regression run #396 passed.
- Scope: the `IO_CHECK_ALLOC` diagnostics inside `basekit/Common.c`.
- Active reporting paths passed `size_t` and `long` counters to `%i`, which expects `int` and invokes undefined behavior while printing allocation/leak data. Leak reporting also dereferenced the lazily allocated tracker head without handling allocation failure, precisely when diagnostic memory may be unavailable.
- Match format specifiers to `size_t`, `long`, and `int`; safely render absent source-file metadata; handle a NULL tracker sentinel; and test live-block reporting without changing allocator accounting. Extend existing static and dynamic source contracts. No new files or build targets.
- CI is the compile/runtime gate; no local C build is claimed.

---

## 2026-11-07 01:17 CST — Protect UDB free-list metadata from record relocation

- Baseline: main commit `8a6fc2639fcd036790b02eae56cadb2c2935883d`; SkipDBv2 Regression run #395 passed source contracts, full build, and all regression tests.
- Scope: `UDBRecord_moveToPos_()` in the physical-record relocation module.
- The bounds checks validated file extents but allowed a caller to relocate a record to offset zero, which stores the persisted first-empty/free-list pointer, or to another byte inside that metadata header. This could overwrite allocator metadata and corrupt subsequent record reuse.
- Reject source and destination positions before `sizeof(PID_TYPE)` before allocating or writing. Extend the existing record-storage regression to verify a move to offset zero is rejected and the source header remains intact. Update existing source contracts; no new files or CMake changes. No local C build is claimed.

---

## 2026-11-07 01:14 CST — Reject UDB tombstones outside complete record extents

- Baseline: main commit `0f3fabcc1b86082f175c10b8904a9ba219c6765f`; SkipDBv2 Regression run #394 passed source-contract checks, the full build, and regression tests.
- Scope: `UDBRecord_remove()` in the existing record-write module.
- Removal previously wrote an EMPTY header after only checking record/file pointers. A stale position at EOF minus one byte could extend the data file because the path did not validate the stream or physical record extent.
- Reject closed streams, invalid file bounds, the reserved metadata header position, positions beyond EOF, and positions lacking a complete record header before changing the tag or writing. Extend the existing `udb_record_storage` regression to assert that a truncated trailing header cannot be removed or extend the file. Update existing workflow source contracts; no new files or CMake target changes. No local C build is claimed.

---

## 2026-11-07 01:11 CST — Match PortableTruncate's seek-error check after the call

- Baseline: main commit `5a089cd67da91f02f4f6ca8179bdf3cbed26311b`; SkipDBv2 Regression run #393 reached the source-contract script but failed because `find("INVALID_SET_FILE_POINTER")` matched the explanatory comment before the Win32 seek call.
- Minimal in-place fix: search for the sentinel starting at the `SetFilePointer` call so the ordering contract validates the actual failure check. No source or build target changes.

---

## 2026-11-07 01:10 CST — Correct all PortableTruncate contract block indentation

- The #392 run still stopped in Python parsing: the inserted contract block had inconsistent indentation relative to the surrounding source-contract checks.
- Replace the whole block and align the list body, continued condition, and error path with the neighboring checks.

---

## 2026-11-07 01:10 CST — Correct remaining PortableTruncate contract indentation

- Baseline: main commit `26ce3f2a481a27d27f89996ae5230af3a272b48e`; SkipDBv2 Regression run #392 still failed before configure because `truncate_source` remained over-indented in the workflow's embedded Python script.
- Minimal in-place fix: normalize the entire inserted PortableTruncate contract block to the same indentation as its surrounding checks. No source or build target changes.

---

## 2026-11-07 01:09 CST — Repair the PortableTruncate workflow-contract indentation

- The #391 run failed before configuration because the newly inserted Python source-contract block began one indentation level too far left.
- Align the first statement with the existing in-script checks; no source behavior changed.

---

## 2026-11-07 01:08 CST — Correct the Windows PortableTruncate adapter

- Baseline: main commit `5e9e6dce2ef3eb3c0247bb5306f2807ad11d1abb`; SkipDBv2 Regression run #390 passed.
- Scope: `basekit/PortableTruncate.c` Windows implementation only.
- The old `CreateFile` call passed zero as the sharing mode and passed `FILE_SHARE_READ | FILE_SHARE_WRITE` in the file-attributes slot. It also accepted a NULL path/negative offset and compared against a magic seek-error constant.
- Validate arguments before the Win32 call, explicitly use the narrow-character API for the declared `const char *` path, put share flags and `FILE_ATTRIBUTE_NORMAL` in their correct parameters, and use `INVALID_SET_FILE_POINTER`. No new files or CMake target changes.
- Linux CI can build the portable translation unit and checks the Windows-branch source contract; this runner does not execute a native Windows build.

---

## 2026-11-07 01:07 CST — Abort SkipDB commits when synchronization cannot prepare

- Baseline: main commit `f56af6f1993cc7dc71a3d6196124c716d4a70837`; SkipDBv2 Regression run #389 passed source contracts, full build, and all CTest cases.
- Scope: `SkipDB_sync()` / `SkipDB_commitTransaction()` failure gating.
- Previously, missing synchronization state, a missing root record, or failure to reserve the root PID could return from `SkipDB_sync()` while UDB remained open; `SkipDB_commitTransaction()` would then still commit the UDB transaction. Pending dirty records could be left unpersisted while the caller observed a successful/open database.
- Treat these preparation failures as transaction aborts by closing UDB before returning, so the existing commit wrapper cannot proceed. Extend the existing `skipdb_core_layers` regression to verify a missing-root preparation failure closes the database instead of committing. Update existing source contracts; the existing CMake/CTest target is unchanged. No local C build is claimed.

---

## 2026-11-07 01:01 CST — Make UDBRecord clones independent and failure-safe

- Baseline: main commit `47f7566f0c8fe929b42c2fddf5b0de474a42d249`; SkipDBv2 Regression run #388 passed source contracts, the full build, and all CTest cases.
- Scope: `UDBRecord_clone()` and the existing `udb_record_storage` regression.
- The old clone copied the struct byte-for-byte: a NULL source returned uninitialized allocated state, while a populated source shared its owned `UArray` with the original. Freeing both could double-free the payload; the struct allocation also bypassed the record's normal `calloc/free` ownership pairing.
- Allocate clones through `UDBRecord_new()`, copy file/position/header metadata, and deep-clone owned payload storage with failure cleanup. Test NULL input, metadata and payload preservation, non-aliasing, and source lifetime after clone destruction. Update existing workflow source contracts; no new files or CMake registrations. No local C build is claimed.

---

## 2026-11-07 00:59 CST — Restore string declarations for ConvertUTF range regression

- Baseline: main commit `e9c77fba669c6a57bff04f78b0be7584339595b8`; regression run #387 failed during test compilation.
- The new reversed-cursor checks use `memset()`; the existing test did not include `<string.h>`, and the strict build rejected its implicit declaration.
- Add the standard header and assert its presence in the existing Actions source contract. No runtime logic or files changed.

---

## 2026-11-07 00:56 CST — Reject reversed ConvertUTF cursor ranges before conversion

- Baseline: main commit `1d563eb10c90c1f392a99800985b163271284a56`; SkipDBv2 Regression run #386 passed.
- Scope: the six public UTF-8/UTF-16/UTF-32 buffer converters and their existing `convert_utf8_bounds` regression.
- The converters rejected NULL handles but did not reject a source cursor positioned after its end, silently reporting success for a malformed range. The UTF-16/UTF-32-to-UTF-8 paths subtract the target cursor from its end and cast the result to `size_t`; a reversed target range could wrap to a huge capacity and write past the intended output span.
- After NULL validation, reject source or target cursors that are past their end before any cursor dereference, pointer subtraction, or byte write. Preserve equal start/end pointers as valid empty ranges; document the contract.
- Extend the existing test to cover reversed source and target ranges for all six converters, verifying `sourceIllegal`, unchanged cursors, and untouched sentinel buffers. Strengthen static and dynamic Actions source contracts; reuse existing CMake registration. No new files.
- No local C build is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:53 CST — Validate UArray stream append item geometry

- Baseline: main commit `ed0412e0c9955e51fd11451725021d4aaed3087c`; SkipDBv2 Regression run #385 passed the source contracts, full build, and all CTest cases.
- Scope: `UArray_readFromCStream_()` and `UArray_readNumberOfItems_fromCStream_()` in the existing stream-I/O module.
- Both paths could accept a caller-mutated item count whose byte extent overflows `size_t`; stream reads could also try to append items beyond the representable byte extent.
- Reject malformed existing storage before consuming input and reject actual read counts that exceed the remaining representable item capacity before appending. Extend the existing stream regression to cover pre-read rejection and append-boundary failures for 16-bit items.
- Update the existing workflow source contracts. Reuse the existing CMake/CTest target; no new files or registrations. No local C build is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:50 CST — Reject out-of-range BStream tag fields before bit-field truncation

- Baseline: main commit `d10391ceb2066d8bd0ed9d92b8e0846b3fef8654`; SkipDBv2 Regression run #384 passed.
- Scope: the tagged-wire header writer `BStream_writeTag()` and its existing `bstream_tag_boundaries` regression.
- The previous round-trip check compared values only after assigning them to 1/2/5-bit fields. Inputs outside those widths had already been truncated, so invalid type, byte-count, or array-flag values could pass the check and emit a different tag than requested.
- Validate the original unsigned arguments before bit-field assignment, fail through the existing stream error channel, and emit no tag on invalid input. Preserve the maximum valid field combination (`0xff`).
- Extend the current C test for each invalid dimension and the valid width boundary. Add static and ordered dynamic source-contract checks; reuse the current CMake/CTest registration. No new files.
- No local C build is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:19 CST — Stop Datum detection when callbacks invalidate the source span

- Baseline: main commit `0c1c7c54764563f7209944e3b0d3c3a358229e57`; SkipDBv2 Regression run #383 passed source contracts, full build, and all CTest cases.
- Scope: `Datum_detect_with_()` and its existing `datum_boundaries` test.
- The detector snapshots the input pointer and length before invoking user callbacks. A callback that replaces or resizes the Datum could leave subsequent suffix scans reading the stale allocation.
- After every callback, compare the original Datum pointer and extent with its current state; stop with `-1` before reusing stale storage if either changed. Document this callback mutation rule in the public header.
- Extend the existing boundary test with a callback that changes both pointer and size while reporting a match; require one callback invocation and an invalidated-search result. Update workflow source contracts. Reuse existing CMake/CTest target; no new files or registrations.
- No local C build is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:18 CST — Avoid quadratic Sorting partitions for duplicate-heavy inputs

- Baseline: main commit `83a74f0b2a722da05d9395598910b94d2f9c06d8`; SkipDBv2 Regression run #382 passed source contracts, full build, and CTest.
- Scope: `Sorting_quickSortRearrange_()` and the existing `sorting_boundaries` regression.
- The median-of-three two-way partition still produced a one-element pivot band when most values were equal. Large duplicate-heavy arrays therefore degraded to quadratic comparisons although reverse-ordered inputs were already protected.
- Keep the pivot at the high endpoint while making two bounded passes: segregate smaller values left, then gather equal values next to the pivot. Return the inclusive equal band and recurse/iterate only over strict lower and upper partitions. Preserve median-of-three selection, callback index bounds, and smaller-side recursion.
- Extend the existing test with a 4,096-element mostly-equal but deliberately unsorted input and a comparison-count ceiling. Update static and dynamic workflow source contracts; reuse the existing CMake target and add no files.
- No local C build is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-07 00:17 CST — Fix PortableStrlcpy regression include order

- CI #381 passed source contracts and CMake configuration, then failed compiling the new test. The test included `PortableStrlcpy.h` before `string.h`; its legacy `strlcpy` macro consequently rewrote the system header declaration.
- Include standard C headers before the portability header, matching the order already used by `PortableStrlcpy.c`. No implementation or API behavior changed.

---

## 2026-11-07 00:16 CST — Make PortableStrlcpy reject invalid pointers safely

- Baseline: main commit `31b43e70a4716e9bcc236b6d2567cd9af0820004`; SkipDBv2 Regression run #380 passed source contracts, full build, and CTest.
- Scope: the existing `PortableStrlcpy()` implementation and one focused regression target.
- The function dereferenced `src` and `dest` without validation. A NULL source could be dereferenced even when the destination was valid; a NULL destination with nonzero capacity also caused a write through NULL.
- Reject NULL sources without modifying the destination. Permit a NULL destination only for a zero-capacity length query, returning the source length. Preserve normal copy, truncation, NUL-termination, and size-zero behavior.
- Add a dedicated regression for full/exact/truncated/empty/zero-size copies and NULL argument combinations. Register it in CMake and the workflow source contracts. The basekit source is already covered by the existing source glob; no new production symbol or source file is introduced.
- No local C build is claimed; CI is the compile/runtime gate.

---

## 2026-11-07 00:15 CST — Correct Date workflow function-boundary delimiter

- CI #379 reached the updated Date contract but still stopped before configure/build. Its Python function-boundary lookup contained a double-escaped newline, so it searched for the literal characters `\\n` instead of the source newline and extracted an empty function body.
- Reduce the delimiter to the workflow's normal single newline escape. The integer-order assertions and regression checks remain unchanged; no C source or test logic changed.

---

## 2026-11-07 00:14 CST — Repair Date timezone workflow contract

- CI #378 stopped in the pre-build source-contract step. The workflow's older Date-specific ordering check still required the replaced `double` conversion and `Date_splitSeconds_()` call, so it rejected the new checked-integer implementation before CMake or compilation.
- Replace that stale assertion with ordered checks for normalized microseconds, exact integer offset arithmetic, overflow/range validation, `time_t` representability, and final state commit. Add the high-timestamp and malformed-microsecond regression markers to the dynamic test contract.
- No production C or regression behavior changed in this contract-only repair; CMake registration remains unchanged. The next CI run is the build/runtime gate.

---

## 2026-11-07 00:13 CST — Preserve exact Date timezone shifts

- Baseline: main commit `31c2db01c222ecc73281149967cbb938557d90f7`; SkipDBv2 Regression run #377 passed the source contracts, full build, and all CTest cases.
- Scope: `Date_convertToTimeZone_()` and the existing `date_time_init` regression.
- The timezone shift passed the whole timestamp through `double`, which loses whole-second precision above 2^53. It also normalized malformed microsecond fields instead of rejecting an invalid Date state.
- Compute the timezone delta and new seconds with checked integer arithmetic, retain the existing `long` timestamp range, verify the `time_t` conversion before commit, and preserve valid microseconds unchanged. Reject malformed microseconds without modifying the Date.
- Extend the existing test with a timestamp above double's exact-integer range and malformed-microsecond atomicity. Reuse the existing CMake target; update the workflow source contracts. No new files or public declarations.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:08 CST — Size Stack callback fixture for actual initial capacity

- CI #376 passed source contracts, configuration, and all compilation, then failed before invoking callback traversal: the new test capped its fixture at 128 entries while the production Stack starts with 512 slots, so the test never reached the reallocation boundary.
- Increase the fixture/recording capacity to 1024, allowing both traversal cases to fill the existing 512-slot Stack and force the callback's next push through `Stack_resize()`. Add the capacity to the workflow test contract.
- Production Stack implementation unchanged; no CMake target changes.

---

## 2026-11-07 00:07 CST — Align Stack callback source contract with snapshot validation

- CI #375 stopped in the workflow source-contract stage before configure/build. The pre-existing generic cursor-reader assertion required inline raw-pointer guards in `Stack_do_()` and `Stack_doUntilMark_()`; these two functions now delegate storage/mark validation to `Stack_clone()` before accessing any cursor.
- Remove only these two callback traversal functions from the raw cursor-reader guard list. Keep the existing pointer-bound assertions for `Stack_totalSize()`, `Stack_popMarkPoint_()`, and `Stack_at_()`; the new dedicated snapshot-lifecycle contract continues to verify clone, callback traversal, and release ordering.
- No C implementation or regression test changed.

---

## 2026-11-07 00:25 CST — Isolate Stack callback traversal from source mutation

- Baseline: main commit `30faa535d7118785000b241d2731ff0c73974342`; SkipDBv2 Regression run #374 completed successfully.
- Scope: inline `Stack_do_()` and `Stack_doUntilMark_()` callback traversal.
- Both functions retained raw pointers into the live Stack allocation while invoking user callbacks. A callback can push onto the same Stack and trigger `Stack_resize()`, invalidating the iterator's backing pointers; mark traversal has the same exposure.
- Clone and validate the Stack before callback iteration, then traverse the private snapshot in the existing reverse order while preserving mark-chain semantics. Release the snapshot after iteration. This gives callbacks a stable traversal view while allowing them to mutate the original stack.
- Extend the existing `stack_lifecycle_test` with forced backing-store growth from callbacks for both full traversal and mark-bounded traversal. Strengthen Stack_inline source contracts; reuse the current CMake target and add no files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:16 CST — Fix List map/select workflow function-boundary lookup

- CI #373 stopped in the source-contract stage before configure/build. The new contract searched for a literal backslash-`n` sequence because the Python source contained a double-escaped newline, so it could not find the end of either C function.
- Use `chr(10) + "}"` to locate the closing function boundary without escape ambiguity. No C implementation or regression behavior changed.

---

## 2026-11-07 00:12 CST — Bound List_map_ and List_select_ to their source snapshot

- Baseline: main commit `124a86966bda32d179e30bbbec8b6e994ee29aa0`; SkipDBv2 Regression run #372 completed successfully.
- Scope: callback-driven construction in `List_map_()` and `List_select_()`.
- Both loops used the live `self->size` as their traversal bound. A callback that appended to its source could silently extend the current operation and, if it continued appending, make mapping/selection unbounded.
- Capture the original source size and validate storage plus unchanged size before and after each callback. If user code structurally resizes the source, free the partial result and return `NULL`. Preserve NULL-source empty-list behavior and the existing selected-slot value semantics.
- Extend `list_storage_bounds` with one-shot append callbacks for map/select and strengthen their source contracts. Reuse the existing CMake target; no new files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-07 00:02 CST — Make List filter reallocation fixture deterministic

- CI #371 isolated the failure to the test's pointer-movement precondition: the filter fixture allocated its second List before the realloc blocker, so the allocator could still extend the first List's backing block in place.
- Place the blocker immediately after the filter List's backing allocation, then create the shrink fixture. Simplify the callback to return a truthy item for each entry so this case tests only callback-time growth and filtering completion; the separate shrink test continues to cover size mutation.
- Production `List_removeTrueFor_()` remains unchanged in this test-only correction.

---

## 2026-11-06 23:56 CST — Isolate List predicate-filter regression failure

- CI #370 passed source contracts, configure, and full compilation; `list_storage_bounds_test` failed in the new callback-reallocation assertion.
- Split the aggregate check into separate backing-pointer movement, callback-count, and filtered-result assertions so the next run identifies the exact failing invariant. This is test-only diagnosis; production filtering logic is unchanged.

---

## 2026-11-06 23:53 CST — Bound List callback traversals to live storage

- Baseline: main commit `7fb99ff23c9af7448cb594c82667df83431a61ff`; SkipDBv2 Regression run #369 completed successfully.
- Scope: `List_do_()`, `List_do_with_()`, and `List_detect_()` callback traversal.
- These functions used `LIST_FOREACH`, which snapshots the initial item count but does not revalidate the list's live size/capacity after user callbacks. A callback that reduces the logical capacity could leave later iterations reading slots outside the current List storage contract.
- Switch these callback consumers to the existing `LIST_SAFEFOREACH` iterator, which revalidates storage and the live size on every iteration. Keep callback signatures, null-item handling, and return behavior unchanged.
- Extend the existing `list_storage_bounds` test and workflow source contracts with callbacks that reduce declared capacity during traversal. Reuse the current CMake target; no new files or target changes.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:47 CST — Make List_removeTrueFor_ callback-reallocation safe

- Baseline: main commit `371f54aa525e38a0fb311e435ad15facf5ef22e1`; SkipDBv2 Regression run #368 completed successfully.
- Scope: the in-place predicate-removal loop in `List_inline.h`.
- The filter cached `self->items` across user callbacks. A predicate that grows the list allocation can move storage, leaving subsequent reads, compaction writes, and tail clearing pointed at the old block. A callback that changes list size can also invalidate the captured traversal bound. The API returns `int` while the number removed is `size_t`, so very large results previously narrowed without bounds.
- Revalidate storage and the captured size before and after each callback, use the current `self->items` for every access, and abort if callback code structurally resizes the list. Preserve existing null-item filtering semantics and clamp the removed count to `INT_MAX`.
- Extend the existing `list_storage_bounds` regression with callback-driven reallocation and shrink cases. Strengthen inline source contracts; reuse the existing CMake target and add no files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:42 CST — Fix List_mapInPlace_ workflow source loading

- CI #367 passed checkout and dependency installation, then stopped in the source-contract script before configure/build. The new List mapping contract referenced `list_source` before the workflow's later List ownership checks initialized that variable.
- Load the existing `List.c` source immediately before the new mapping contract. No C implementation, test behavior, CMake target, or source path changed.

---

## 2026-11-06 23:35 CST — Make List_mapInPlace_ callback-reallocation safe

- Baseline: main commit `9b0eda48f06ab95f85f13418b7501690f0d3259e`; SkipDBv2 Regression run #366 completed successfully.
- Scope: the in-place list mapping callback loop in `basekit/List.c`.
- The function cached `self->items` before calling user code. A callback can call `List_preallocateToSize_()`, moving the allocation; the subsequent mapped-value write then targets the stale pointer. A callback that changes list size could also make the current index invalid or change its meaning before the write.
- Replace the macro loop with an explicit bounded loop. Revalidate storage and the original size before and after each callback, and resolve `self->items[i]` only after the callback returns. Capacity-only reallocation remains supported; structural size changes stop the mapping safely.
- Extend the existing `list_storage_bounds` test with forced backing-store growth during callback and callback-driven shrink coverage. Tighten List.c and test source contracts; reuse the existing CMake target and add no files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:30 CST — Fix UArray negate helper source-contract lookup

- CI #365 stopped in the workflow source-contract stage before configure/build. The negate boundary-helper lookup depended on an incorrectly escaped newline and therefore failed to locate the definition.
- Search for the helper signature starting at the `UArray_negate()` implementation. The prototype remains earlier in the file, so this unambiguously selects the later implementation without newline escaping.
- No C logic or regression test changed.

---

## 2026-11-06 23:27 CST — Guard UArray_negate against invalid storage and signed overflow

- Baseline: main commit `619e1fa7e8087a20843a6b0b0987dd9f67ca19c3`; SkipDBv2 Regression run #364 completed successfully.
- Scope: the single-array `UArray_negate()` mutation.
- The function dereferenced its receiver through `UArray_isSignedType()` before validating storage, modified a const-qualified receiver through the assignment macro, and negating a signed minimum value invoked undefined behavior.
- Validate storage first, retain the legacy unsupported-type error for valid unsigned receivers, preflight all signed integer widths for their minimum value before any write, and invalidate the receiver hash after successful mutation. Keep the existing public signature unchanged.
- Extend the existing `uarray_math_extrema` regression and workflow source contracts for integer/floating results, INT8/16/32/64 minimum atomicity, NULL/malformed storage, overflow geometry, and hash lifecycle. Reuse the existing CMake target; no new files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:23 CST — Guard UArray scalar assignment transforms

- Baseline: main commit `a4b40e9d93835c1bb75b09f07bd01a3859bc7496`; SkipDBv2 Regression run #363 completed successfully.
- Scope: `UArray_round()`, `UArray_clear()`, `UArray_setItemsToLong_()`, `UArray_setItemsToDouble_()`, and `UArray_rangeFill()`, the shared single-array assignment-transform family.
- These functions sent unvalidated metadata directly to `UARRAY_FOREACHASSIGN`, which has no pointer-sized CTYPE branch, and none invalidated the cached hash after mutation.
- Move the existing numeric-storage validator declaration ahead of the assignment family, reject malformed/pointer-sized layouts and empty arrays before dispatch, and clear the hash once after each nonempty transform. Preserve each operation's existing conversion and rounding rules.
- Extend the existing `uarray_math_extrema` regression and workflow contracts for valid transforms, NULL/empty/malformed storage, overflow geometry, pointer-sized arrays, and hash lifecycle. Reuse the existing CMake target; no new files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:20 CST — Harden UArray logical binary mutations

- Baseline: main commit `83d13e501b2cade038310352f82b384ba216ea19`; SkipDBv2 Regression run #362 completed successfully.
- Scope: `UArray_logicalOr_()` and `UArray_logicalAnd_()`, including their shared dispatch macro.
- The functions dispatched directly into typed storage without checking either operand. The receiver and operand dispatch tables support different CTYPE subsets, so unsupported metadata could reach the fatal unsupported-type path. The shared logic macro also omitted the early return used by the generic dispatcher and did not invalidate the receiver hash.
- Validate both numeric layouts and the exact receiver/operand CTYPE sets before dispatch. Make the shared macro preserve empty-prefix no-op behavior, invalidate the hash after nonempty writes, and return before the unsupported fallback.
- Extend the existing `uarray_math_extrema` regression and workflow source contracts for OR/AND prefix semantics, supported uintptr operands, malformed/unsupported inputs, overflow geometry, and hash lifecycle. Reuse the existing CMake target; no new files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:18 CST — Repair UArray pairwise extrema hash invalidation

- CI #361 passed source contracts, configure, and compilation, then exposed a regression assertion: pairwise maximum changed values but retained the receiver's cached hash.
- Root cause: `UArray_at_putDouble_()` calls a type-dispatch macro whose branch returns before the setter's trailing `UArray_changed()`. Invalidate the hash once after each nonempty Max/Min loop; keep empty prefix operations as no-ops.
- Tighten the workflow contract to require the explicit post-loop invalidation. The existing regression already checks both mutations and is reused; no new files or CMake changes.

---

## 2026-11-06 23:15 CST — Guard UArray pairwise extrema mutations

- Baseline: main commit `bcb8cc4dea53e44db2a8d955f3644ddc04ea8c31`; SkipDBv2 Regression run #360 completed successfully after the elementwise-division source-contract correction.
- Scope: `UArray_Max()` and `UArray_Min()` in the existing basekit math module.
- Both functions read receiver and operand elements without checking either array's type/width/storage geometry. Malformed metadata could reach typed reads, and pointer-sized values were accepted as numeric extrema. Writes used the bounded setter, but its implicit hash invalidation did not protect the unsafe reads.
- Preflight both operands with the shared numeric-storage validator, reject pointer-sized elements, preserve the common-prefix result semantics, and retain the existing setter-based write/hash lifecycle.
- Extend the existing `uarray_math_extrema` regression and workflow source contract for mixed-width prefix results, NULL/empty/malformed storage, overflow geometry, pointer-sized types, and hash invalidation. Reuse the existing CMake target; no new files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:09 CST — Fix UArray division source-contract helper lookup

- CI #359 stopped in the workflow source-contract stage before configure/build: the helper-definition search encoded a literal backslash-n instead of matching the C source newline.
- Anchor the search after `UArray_divide_()` using the helper signature alone. This excludes the forward declaration and finds the implementation without relying on newline escaping.
- No C implementation or regression behavior changed; the existing workflow and test target remain in place.

---

## 2026-11-06 23:06 CST — Guard UArray elementwise division

- Baseline: main commit `a16642520ed3b77c25ff4dc1c196cbeb9a63c4ef`; SkipDBv2 Regression run #358 completed successfully.
- Scope: `UArray_divide_()` and its existing elementwise division regression.
- The division path dereferenced both operands before validating them, trusted storage geometry in the float32 fast path and generic dispatcher, and never invalidated the receiver hash. Integer zero divisors and signed minimum divided by negative one could invoke undefined behavior; a later zero divisor could also leave earlier elements already modified.
- Validate both numeric layouts, reject pointer-sized operands, retain the common-prefix behavior, and preflight the entire affected range before integer division. Treat integer zero division and signed MIN/-1 as an all-or-nothing no-op; preserve IEEE floating-point division-by-zero behavior. Invalidate the hash on both the float32 fast path and generic path.
- Extend the existing `uarray_math_extrema` test and workflow source contracts for mixed-width results, both floating paths, atomic zero handling, signed overflow, malformed inputs, and hash lifecycle. Reuse the existing CMake target; no new files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:01 CST — Guard UArray unary math transforms

- Baseline: main commit `43f04fa30e56dbc3d481b98e4bc41fc9d2ba3c39`; SkipDBv2 Regression run #357 completed successfully.
- Scope: the generated unary math family in `basekit/UArray_math.c` (trigonometric, hyperbolic, exponential, logarithmic, square-root, rounding, and absolute-value transforms).
- The shared `UARRAY_DOP` macro iterated unvalidated array metadata, silently omitted the pointer-sized type only because the assignment switch had no matching case, and never invalidated the receiver's cached hash after writing values.
- Validate numeric storage before dispatch, explicitly reject `CTYPE_uintptr_t`, leave empty arrays untouched, and clear the hash after each nonempty transform. Preserve the existing scalar math and conversion behavior for valid numeric inputs.
- Extend the existing `uarray_math_extrema` test with the complete generated transform family, NULL/empty/malformed storage, pointer-sized arrays, and hash lifecycle. Tighten the workflow source contracts; reuse the existing CMake target and add no files.
- No local C compilation or CTest result is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 23:00 CST — Guard UArray scalar arithmetic mutations

- Baseline: main commit `928c5c1c4812b05cefc9a99d1c6f9b2e221916e3`; SkipDBv2 Regression run #356 passed all 55 CTest cases after the elementwise arithmetic cache fix.
- Scope: `UArray_addScalarDouble_()`, `UArray_subtractScalarDouble_()`, `UArray_multiplyScalarDouble_()`, and `UArray_divideScalarDouble_()`.
- The four functions fed unvalidated array storage to the assignment macro and modified values without clearing the cached hash. The assignment macro does not dispatch `CTYPE_uintptr_t`, although the shared numeric-layout validator recognizes it; scalar division by zero also reached integer division, which is undefined behavior.
- Validate storage before iteration, explicitly exclude pointer-sized values from the numeric assignment path, invalidate the hash after nonempty mutation, and treat a zero divisor as a no-op. Preserve existing scalar arithmetic and integer-conversion behavior for nonzero operands.
- Extend the existing `uarray_math_extrema` regression and workflow source contract for all four scalar operations, NULL/empty/malformed storage, pointer-sized types, divide-by-zero, and hash lifecycle. Reuse the existing CMake target; no new files or CMake changes.
- No local C compilation or CTest result is claimed; mainline CI remains the build/runtime gate.

---
## 2026-11-06 22:57 CST — Fix UArray generic arithmetic cache invalidation

- CI #355 built successfully but `uarray_math_extrema_test` failed in the first generic elementwise add assertion: `UARRAY_BASICOP_TYPES` contains an early `return`, so the post-dispatch `UArray_changed(self)` was unreachable for generic type combinations. The float32 fast path did not have this issue.
- Move cache invalidation into the generic branch immediately before the dispatch macro (which returns after performing the operation), while keeping post-operation invalidation for the float32 path. Extend the workflow contract to assert the different but necessary orderings for both paths.
- Keep the regression test and existing CMake target unchanged. No new files or unrelated C changes.

---
## 2026-11-06 22:56 CST — Fix UArray math source-contract search after forward declaration

- CI #354 failed in the Python source-contract step before configure/build because the existing dot-product contract found the newly added elementwise validator forward declaration instead of the later function definition.
- Narrow the search for the validator definition to begin after `UArray_numericItemSize_()`. This keeps the existing dot-product contract anchored to the implementation while separately checking the new forward declaration in the elementwise contract.
- No C source or runtime behavior changed; this is an in-place workflow-contract correction for the preceding UArray elementwise arithmetic commit.

---
## 2026-11-06 22:53 CST — Validate UArray elementwise arithmetic mutations

- Baseline: main commit `db1d474c78cacab4968141bfb1acca67524609dc`; SkipDBv2 Regression run #353 passed all 55 CTest cases, including the index-compaction guards.
- Scope: the three related elementwise operations `UArray_add_()`, `UArray_subtract_()`, and `UArray_multiply_()`. Division remains out of scope because integer divide-by-zero policy requires separate treatment.
- These functions inspected CTYPE and cast operand storage before validating either array. Their float32 fast paths also trusted a helper-derived count, and both fast/generic paths mutated contents without invalidating the receiver's cached hash.
- Add a file-local validator declaration; preflight both operands for supported numeric types, matching item widths, safe geometry, and backing bytes; derive the common item count only after validation; keep the zero-length no-op; and invalidate the hash after a successful nonempty operation.
- Extend the existing `uarray_math_extrema` regression and workflow source contract for mixed-width prefix behavior, float32 fast-path behavior, NULL/malformed storage, unsupported types, overflow, zero-length arrays, and hash invalidation. Reuse the existing CMake target; no new files or CMake changes.
- No local C compilation or CTest result is claimed; mainline CI remains the build/runtime gate.

---
## 2026-11-06 22:47 CST — Guard UArray index compaction

- Baseline: main commit `c7338c3aef367699fed7fea6b61acd45306e9b6f`; SkipDBv2 Regression run #352 passed all 55 CTest cases, including the bitwise mutation guards.
- Scope: `UArray_removeOddIndexes()` and `UArray_removeEvenIndexes()` in the existing basekit math module.
- Both routines dereferenced the receiver and formed byte offsets from unchecked `itemSize` and `size`; NULL data and overflowing geometry could reach `memcpy`. They also changed bytes before shrinking, so a failed shrink allocation could leave a stale cached hash.
- Validate the backing byte extent before pointer arithmetic, derive item count from the validated extent, and invalidate the hash immediately after in-place compaction (before the potentially failing shrink).
- Extend the existing `uarray_stack_resize` regression and workflow source contract for retained-item order, hash state, NULL, zero-width, and overflowing storage. Reuse the existing CMake target; no new files or CMake changes.
- No local C compilation or CTest result is claimed; mainline CI remains the build/runtime gate.

---
## 2026-11-06 22:42 CST — Harden UArray bitwise mutation storage and hash lifecycle

- Baseline: main commit `77fabdf63c62e130f59dfc792361568d13957024`; SkipDBv2 Regression run #351 passed all 55 CTest cases.
- Scope: the five byte-oriented mutators in `basekit/UArray_math.c`: OR, AND, XOR, NOT, and set-all-bits.
- The binary operations multiplied array dimensions without overflow checks, unary operations trusted unchecked byte counts, and all five changed data without invalidating the cached hash. NULL or malformed storage could be dereferenced or addressed out of bounds.
- Reuse the existing byte-storage validator before every read/write, compute binary overlap from validated byte counts, and invalidate the hash only after a nonempty mutation. Add a file-local forward declaration because the existing validator is defined below these functions.
- Extend the existing `uarray_bit_access` regression and workflow source contract for all five mutators, hash invalidation, NULL inputs, missing storage, zero item width, and overflow geometry. Reuse the existing CMake target; no new files or CMake changes.
- No local C compilation or CTest result is claimed; mainline CI remains the build/runtime gate.

---
## 2026-11-06 22:38 CST — Validate UArray extrema item layouts

- Baseline: main commit `2b18f599e03ef3266ddc3f545bfbb34db16d8fdf`; SkipDBv2 Regression run #350 passed all 55 CTest cases after hardening the aggregate reductions.
- Scope: `UArray_maxAsDouble()` and `UArray_minAsDouble()` in the existing basekit math module.
- The extrema functions checked non-NULL storage and multiplication overflow but did not verify that `itemSize` matched `itemType`. Their typed `UArray_rawDoubleAt_()` reads could therefore overrun a malformed byte allocation.
- Reuse `UArray_hasValidNumericStorage_()` before the first typed read and retain the existing empty-array result of zero. Extend the existing extrema regression and workflow source contract for mismatched widths and types; no new files or CMake changes.
- No local C compilation or CTest result is claimed; mainline CI remains the build/runtime gate.

---
## 2026-11-06 22:33 CST — Harden UArray statistical reductions

- Baseline: main commit `cd4a796114f40a6ec026593b110f994f863157c6`; SkipDBv2 Regression run #349 passed all 55 CTest cases, including the scale-safe normalization change.
- Scope: the four related UArray aggregate operations in `basekit/UArray_math.c`: sum, product, arithmetic mean, and arithmetic mean square.
- These routines could iterate malformed storage, divide by zero for empty means, and dereference a failed clone in mean-square calculation. The clone also made a read-only statistic unnecessarily allocation-dependent.
- Validate numeric type/width, overflow-safe geometry, and backing storage before iteration. Define empty means as zero while preserving the empty product identity of one. Compute mean square directly in double precision without cloning or mutating the source.
- Extend the existing `uarray_math_extrema` regression and workflow source contract for positive/negative inputs, empty/NULL arrays, malformed storage, unsupported types, and source-hash preservation. Reuse the existing CMake target; no new files or CMake changes.
- No local C compilation or CTest result is claimed; mainline CI remains the build/runtime gate.

---
## 2026-11-06 22:30 CST — Make UArray normalization scale-safe and allocation-free

- Baseline: main commit `3ae2e440aa62e4f16c2f77b5277a737cff03ea18`; SkipDBv2 Regression run #348 passed all 55 CTest cases.
- Scope: `UArray_normalize()` in the existing basekit math module and its `uarray_math_extrema` regression.
- The previous implementation cloned and squared the array without checking allocation failure, then divided by a zero, underflowed, or overflowed norm. A failed clone could be dereferenced; zero vectors and extreme finite values could be corrupted.
- Replace the temporary clone with a three-pass scaled norm: validate numeric storage and supported type, reject non-finite values before mutation, scale by the largest absolute element, accumulate bounded ratios, then normalize in place and invalidate the cached hash. Empty/zero, malformed, pointer-sized, and non-finite inputs remain unchanged.
- Extend the existing test and workflow source contract for normal vectors, zero vectors, DBL_MAX/DBL_MIN ranges, non-finite input, malformed storage, and hash invalidation. Reuse the existing CMake/CTest target; no new files or build-list changes.
- No local C compilation or CTest result is claimed; the next mainline CI run is the compile/runtime gate.

---
## 2026-11-06 22:27 CST — Validate UArray UTF-8 multibyte iteration layout

- Mainline CI #347 passed after the UArray cross-product storage guard.
- Scope: UArray_isMultibyte() and the existing uarray_encoding_conversion regression.
- The UTF-8 path checked itemSize == 1 but iterated through UARRAY_INTFOREACH, which dispatches by itemType. A malformed wide itemType paired with byte-sized metadata could therefore read beyond the backing allocation.
- Require a valid UTF-8 byte layout and a uint8_t/int8_t item type, then scan the validated data bytes directly. Add a malformed wide-type fixture and enforce the raw-byte iteration in the workflow source contract; no new files or CMake changes.
- No local C compilation is claimed; mainline CI remains the build/runtime gate.

---

## 2026-11-06 22:22 CST — Guard UArray cross-product storage

- Mainline CI #346 passed after UArray path storage validation.
- Scope: UArray_crossProduct_() and the existing uarray_distance_precision regression.
- The cross-product implementation checked only itemType and logical size before casting both data pointers to float32_t. It did not reject NULL backing storage, a mismatched itemSize, or overflowing array geometry before reading the first three elements.
- Reuse UArray_hasValidNumericStorage_() for both operands, retain float32-only and minimum-three-element behavior, and add valid cross-product plus malformed storage cases. Extend the existing workflow source contract; no new files or CMake target changes.
- No local C compilation is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 22:15 CST — Validate UArray path storage geometry

- Mainline CI #345 passed after the hex-byte conversion fix.
- Scope: UArray path append, component lookup/mutation, extension handling, and OS-path conversion.
- These entry points accepted non-NULL arrays without verifying itemType/itemSize consistency, overflow-safe geometry, or that returned long offsets could represent the array length. A malformed wide-type view over byte storage could reach typed separator scans.
- Add one local path-storage validator for the supported numeric CTYPE set, reject lengths above LONG_MAX, and preflight both append operands. Extend the existing path regression and workflow source contract; reuse its CMake target without adding files.
- No local C compilation is claimed; mainline CI remains the build/runtime gate.

---

## 2026-11-06 22:12 CST — Sync UArray format regression contract

- CI #344 stopped during firmware integration contract validation, before configure/build: the existing test-success banner was intentionally expanded for the hex-byte case, but its old source-contract needle remained.
- Update the workflow expectation to the new banner. No C source or runtime behavior changed in this correction.

---

## 2026-11-06 22:10 CST — Validate UArray hex conversion byte layout

- Mainline CI #343 passed with the dot-product input validation.
- Scope: UArray_asNewHexStringUArray() and the existing uarray_format_trailing_percent regression.
- The hex converter checked itemSize == 1 but then fetched each element through UArray_longAt_(), whose typed dispatch follows itemType. A corrupted wide itemType paired with one-byte storage could read past the allocation.
- Restrict this byte-string API to uint8_t/int8_t items and read the raw byte directly. Extend the existing format regression and workflow source contract; no new files or CMake changes.
- No local C compilation is claimed; mainline CI is the compile/runtime gate.

---

## 2026-11-06 22:05 CST — Validate UArray dot-product input geometry

- Mainline CI #342 passed with the UArray string-iteration layout checks.
- Scope: UArray_dotProduct_() and the existing uarray_math_extrema regression.
- The generic typed dot-product dispatcher read both arrays without checking NULL inputs, supported item types, item-width consistency, overflow-safe geometry, or backing storage. Malformed metadata could make the typed loop read beyond the allocation.
- Add one local numeric-layout validator for all types dispatched by DUARRAY_OP; reject invalid operands before dispatch while preserving mixed-width and unequal-length valid dot products. Extend the existing regression and workflow source contract; no CMake or file-list changes.
- No local C compilation is claimed; mainline CI remains the compile/runtime gate.

---

## 2026-11-06 22:02 CST — Validate UArray string iteration item layouts

- Mainline CI #341 passed after the BStream attachment guard repair.
- Scope: UArray_lstrip_(), UArray_rstrip_(), UArray_escape(), and the existing uarray_unescape_bounds regression.
- The strip and escape paths checked for non-NULL data but did not verify that itemSize matched itemType. Typed iteration could therefore read wider numeric elements from undersized backing storage.
- Reuse UArray_hasValidStringItemLayout_() before those typed reads, validating both the receiver and trim set. Add malformed receiver/trim-set cases and retain a valid strip round trip; extend existing workflow source contracts without adding files or changing CMake registration.
- No local C compilation is claimed; the mainline CI run remains the build/runtime gate.

---

## 2026-11-06 21:57 CST — Cover empty BStream views in the ownership guard

- CI #340 compiled all targets and passed the workflow source contracts, but the new BStream regression exposed a zero-length tail-view blind spot: the byte-write overlap helper intentionally ignores zero-byte sources.
- Add a dedicated UArray-view overlap predicate for attachment. It rejects pointers anywhere inside the currently owned byte allocation, including an empty view at the trailing NUL, while retaining the byte-write helper's zero-length behavior.
- Keep the regression for exact, interior, and trailing-NUL aliases. Extend the workflow contract to verify the empty-view boundary rule and that attachment preflight occurs before freeing the old owner.
- The failure was confined to the new regression; the other 54 CTest cases passed.

---

## 2026-11-06 21:49 CST — Prevent BStream owned-buffer use-after-free during attachment

- Baseline: main `bf6ae8c72896324cc5418bf241ba887c26fbe142`; SkipDBv2 Regression #339 passed.
- Scope: `BStream_setUArray_()` and the existing `bstream_tag_boundaries` regression.
- A distinct incoming `UArray` can borrow all or part of the stream's currently owned byte buffer. Replacing the stream then freed the old `UArray`, leaving the new stream's borrowed view dangling.
- Before releasing an owned stream array, reject incoming views whose data overlaps the old byte range, including its trailing NUL. Preserve the current stream, ownership flag, cursor, and bytes on rejection; continue accepting unrelated external arrays as non-owning attachments.
- Add exact-buffer, interior-buffer, and tail-terminator regressions. Extend the existing workflow source contract to enforce preflight ordering; the existing CMake/CTest target is reused.
- No local C compilation is claimed; the next mainline CI run is the compile/runtime gate.

---

## 2026-11-06 21:14 CST — Distinguish invalid UArray line-reader input from EOF

- Baseline: main `bfb07b7a6a5780aabf95ca6c2c969dcb8470d8fa`; SkipDBv2 Regression #338 passed.
- Scope: `UArray_readLineFromCStream_()`, its public return contract, and the existing `uarray_stream_io` test.
- Invalid destination, stream, item width, or backing storage previously returned `0`, making invalid calls indistinguishable from clean EOF. The shared stream API comment already specifies `-1` for errors.
- Return `-1` for invalid arguments before reading any bytes. Document the three-way result: 1 for a line, 0 for clean EOF, and -1 for invalid input or I/O failure.
- Extend the existing test with NULL destination/stream, non-byte destination, and malformed-storage cases; assert the input stream position remains unchanged. Tighten the existing workflow source contract to verify the guard returns before the first `fgetc()`. Existing CMake/CTest registration is reused.
- No local C compilation is claimed; mainline CI #339 is the compile/runtime gate.

---

## 2026-11-06 21:10 CST — Make Date timezone conversion overflow-safe and atomic

- Baseline: main `abedf58111aadce321924ee96eb277aa67fb6d66`; SkipDBv2 Regression #337 passed.
- Scope: `Date_convertToTimeZone_()` and the existing `date_time_init` regression.
- The timezone delta previously subtracted and multiplied two `int` offsets before converting to `double`, allowing signed overflow. It also updated the timezone even when `Date_fromSeconds_()` rejected an unrepresentable adjusted timestamp.
- Compute the offset delta in floating point, validate normalized seconds and `time_t` representability first, then publish timeval and timezone together. Failed conversions now leave the complete Date state unchanged.
- Extend the existing test for a normal conversion, an out-of-range transition with full-state preservation, and extreme offset arithmetic when the host time types can represent the result. Extend the workflow's Date source contract; existing CMake/CTest registration is reused.
- No local C compilation is claimed; mainline CI #338 is the compile/runtime gate.

---

## 2026-11-06 21:05 CST — Validate ConvertUTF buffers before dereference

- Baseline: main `09fa2efecd93be008e154fe6783084daff892499`; SkipDBv2 Regression #336 passed.
- Scope: the six UTF-8/UTF-16/UTF-32 conversion entry points and the existing `convert_utf8_bounds` test.
- Each converter now rejects NULL cursor handles, NULL cursor values, and NULL end pointers with `sourceIllegal` before dereferencing or comparing them. Invalid calls leave cursors and output unchanged. The public header now documents the non-NULL requirement and representation of empty ranges.
- Added 36 invalid-argument regression calls covering all six argument categories in all six conversion paths, with cursor/output preservation checks.
- Extended the GitHub Actions source contract to verify guard ordering in all six functions, the public contract, and the regression matrix. Existing CMake/CTest registration is reused.
- No local compilation is claimed; mainline CI #337 is the compile/runtime gate.

---

## 2026-11-06 20:58 CST — Restore the declared Datum detector API

### Baseline
- Main commit `4c055add539d0dc8c1351459362a78ae02502f1b`; SkipDBv2 Regression run #335 (`42220283821`) passed source contracts, CMake configuration, all target builds, and the complete CTest regression step.
- Scope: the existing `Datum_detect_with_()` API in `Datum.c` / `Datum.h` and its existing boundary regression.

### Defect and fix
- `Datum.h` exported `Datum_detect_with_()`, but `Datum.c` contained no linked definition; only a commented legacy detector sketch remained. Callers using the declared API would fail at link time.
- Restore the detector as a first-match scan over successive Datum suffixes. Callback result `1` returns the byte offset, `-1` stops with no match, and other results continue. Reject NULL callbacks, malformed storage, and spans too large for the public `int` result before invoking callbacks; keep the original search extent stable.

### Regression and contracts
- Extend the existing `datum_boundaries` target with first-match/suffix, full miss, early-stop, empty input, NULL/malformed input, NULL callback, and unrepresentable-size cases.
- Extend the existing GitHub Actions Datum source contract. Reuse the already-registered CMake/CTest target; no new files or build-list changes.

### Validation
- Static review confirms every returned offset fits `int`, each callback receives the remaining suffix beginning at the current byte, and invalid input cannot reach the callback.
- No local C compilation is claimed. Mainline CI run #336 is required to compile the restored definition and execute the regression.

---

## 2026-11-06 20:45 CST — Avoid quadratic quicksort partitions on reverse-ordered input

### Baseline
- Main commit `a4d9fa22f6ab6f66e5430ca9c6af8611ebfc6043`; SkipDBv2 Regression run #334 (`42215666889`) passed integration contracts, CMake configuration, all target builds, and the complete CTest regression step.
- Scope: the pivot selection in `basekit/Sorting.c` and the existing sorting boundary regression.

### Defect and fix
- The partitioner always used the first item as its pivot. On a 4,096-item reverse-ordered input this produced highly unbalanced partitions and quadratic comparison growth, despite the existing smaller-side recursion guard.
- Select the median of the first, middle, and last values before partitioning. Keep the existing callback API, endpoint partition mechanics, int-index boundary, and bounded-recursion strategy unchanged.

### Regression and contracts
- Extend `sorting_boundaries.c` with a comparison budget for the existing 4,096-item reverse-order fixture. It must sort correctly in fewer than `LARGE_SORT_COUNT * 32` comparisons.
- Update the existing Sorting source contracts to require median-of-three sampling and pivot installation. Reuse `sorting_boundaries_test`; no new files, public declarations, or CMake changes.

### Validation
- Static review confirms the midpoint calculation cannot exceed `ub`, all sampled indexes are within the already-validated `INT_MAX` range, and the pivot is installed before the partition scan.
- No local C compilation or CTest result is claimed. Mainline CI will compile and execute the new comparison-bound regression.

---

## 2026-11-06 20:05 CST — Preserve BStream writes sourced from the stream tail

### Baseline
- Main commit `61fea483ce68ed18fd12eb33dac011e352e7559d`; SkipDBv2 Regression run #333 passed.
- Scope: byte and array writer alias preflight in `BStream.c`.

### Defect and fix
- Existing overlap checks excluded the stream's trailing NUL byte and skipped empty streams. Plain array and tagged writers add framing bytes before copying the payload, so a source view of that terminator could be invalidated by reallocation before it was read.
- Centralize stream/source overlap detection and include the terminator position. Snapshot overlapping raw appends before growing the backing array, and snapshot UArray sources before writing their length prefix.

### Regression and contracts
- Extend the existing `bstream_tag_boundaries` test with direct append, empty-stream append, plain-array, tagged-data, and tagged-UArray tail-source cases.
- Update existing BStream source contracts. Reuse the registered CTest target; no new files or CMake changes.

### Validation
- Static review confirms stream-backed payload bytes are copied before an append or framing write can reallocate the stream buffer. Mainline CI run will compile and execute the regression suite.

---

## 2026-11-06 19:59 CST — Normalize two-digit years in PortableStrptime

### Baseline
- Main commit `3a32fa0d50df1617746d1ba73c3b8d4c07b91391`; SkipDBv2 Regression run #332 passed.
- Scope: the `%y` conversion in `io_strptime()` and the existing parser-boundary regression.

### Defect and fix
- `%y` assigned the parsed value directly to `tm_year`, so years `00`–`68` became 1900–1968 instead of 2000–2068. A value such as `24` therefore represented 1924 instead of 2024.
- Apply the POSIX two-digit year window: `00`–`68` map to 2000–2068; `69`–`99` map to 1969–1999. Leave the four-digit `%Y` branch unchanged.

### Regression and contracts
- Add boundary checks for `00`, `68`, `69`, and `99` in `portable_strptime_bounds`.
- Update existing workflow source contracts. Reuse the current CTest target; no new files or build-list changes.

### Validation
- Static inspection confirms the parser stores years-since-1900 in `tm_year` and preserves its consumed-input return contract. Mainline CI will verify compilation and all regressions.

---

## 2026-11-06 19:49 CST — Preserve Date state when parsing fails

### Baseline
- Main commit `a5178903411a2bbca35a85c33d733e98a20ad530`; SkipDBv2 Regression run #331 passed.
- Scope: `Date_fromString_format_()` and its existing time-initialization regression.

### Defect and fix
- The parser return value was ignored. Malformed/truncated input could partially update `struct tm`, after which `mktime()` overwrote the destination with a date derived from incomplete fields.
- Return without changing the destination when `io_strptime()` reports failure; successful parsing still follows the existing conversion path.

### Regression and contracts
- Add a truncated-date case that verifies both seconds and microseconds remain unchanged after parse failure.
- Update existing Date source/test contracts. Reuse the existing CTest target; no new files or build-list changes.

### Validation
- Static review confirms `mktime()` is reached only on successful parsing. CI will verify compilation and the complete regression suite.

---

## 2026-11-06 19:40 CST — Reject negative Stack mark metadata

- Main baseline `7630dd2295d0a16c6e4a822265ef72dd6951c673`; SkipDBv2 Regression run #330 passed.
- `Stack_isValid_()` validated only positive mark-chain nodes, allowing a negative `lastMark` to pass as an empty chain. Reject the invalid sentinel in the shared validator used by clone, compact, and memory-size queries.
- Extend `stack_lifecycle` to assert negative mark metadata is rejected without changing the stack state, and update existing source contracts. No new files or build target changes.

---

## 2026-11-06 19:37 CST — Preserve the existing byte-size regression contract

- CI run #329 stopped in the integration-contract stage because the byte-access test combined the pre-existing byte-size overflow assertion with the new accessor assertion, removing the exact diagnostic text that the existing contract requires.
- Split the checks so `UArray_sizeInBytes()` retains its established regression message and the new `UArray_byteAt_()` overflow guard has its own assertion. No implementation changes.
- In-place contract repair; reuse the existing CTest target, with no new files or build-list edits.

---

## 2026-11-06 19:36 CST — Bound UArray raw byte access to validated storage

### Baseline
- Main commit `8bcb3ff385d79aaa243c3d8d1a3f000129192e12`; SkipDBv2 Regression run #328 passed.
- Scope: `UArray_byteAt_()` and the existing byte-size regression.

### Defect and fix
- The byte accessor dereferenced a NULL receiver or missing backing pointer and compared a byte index against item count rather than total byte count. Typed arrays could therefore not access their full raw byte extent.
- Reuse the existing bit-storage validator for NULL, item-width, overflow, and backing-storage checks, and bound the index against the validated byte count.

### Regression and contracts
- Extend `uarray_size_in_bytes` to cover NULL/empty input, typed-array byte offsets beyond item count, the byte boundary, missing storage, zero item width, and overflowing geometry.
- Update the existing GitHub Actions source contracts. Reuse the existing CMake/CTest target; no new files or public declarations.

### Validation
- Static inspection confirms the raw byte read only happens after storage validation and an in-range byte index. Mainline CI will validate compilation, contracts, and all regressions.

---

## 2026-11-06 19:34 CST — Repair UArray distance precision source contract

- CI run #327 stopped in the source-contract stage before compilation: the check still searched for the old `else if` float64 branch after `UArray_distanceTo_()` was changed to validate both vectors up front and use explicit typed branches.
- Update the existing assertion to locate the float64 branch by its current `if (self->itemType == CTYPE_float64_t)` form. Keep the check that requires a `float64_t` intermediate and rejects a `float32_t` intermediate.
- Contract-only in-place repair; no C implementation, regression, build target, or file list changes. The next CI run must execute the full regression pipeline.

---

## 2026-11-06 19:32 CST — Validate UArray distance vector storage

### Baseline
- Main commit `a2e9998aa0a1ef50863b81e1bcbfc133cbbf75d4`; SkipDBv2 Regression run #326 passed.
- Scope: `UArray_distanceTo_()` and its existing precision regression.

### Defect and fix
- The function dereferenced both receivers before checking for NULL. Matching floating-point `itemType` alone also allowed inconsistent `itemSize` metadata or missing backing storage to reach typed reads.
- Add a private validator for float32/float64 vectors, require exact C-type width, representable item-byte geometry, and backing storage before reading. Preserve the existing zero result for unequal lengths, unsupported types, and invalid inputs; use const input pointers.

### Regression and contracts
- Extend `uarray_distance_precision` with NULL receiver, missing-storage, malformed-width, overflowing-geometry, and non-floating-type cases.
- Update the existing GitHub Actions source contracts. Reuse the current CMake/CTest target; no new files or public declarations.

### Validation
- Static review confirms distance loops execute only after both arrays pass validation and their type and element counts match. Existing float32, tiny float64, large float64, and unequal-length behavior remains covered.
- Mainline CI run #327 will compile the change and execute the full regression suite.

---

## 2026-11-06 19:30 CST — Validate UArray storage before unescaping

### Baseline
- Main commit `585016b8ad87e14abe9fba6d4d8d24b8c9872bd2`; SkipDBv2 Regression run #325 passed.
- Scope: `UArray_unescape()` and its existing bounds regression.

### Defect and fix
- The unescape loop read values according to `itemType` but validated only the backing pointer. When a caller supplied inconsistent public metadata (for example, a two-item 64-bit array whose `itemSize` was changed to one byte), it could read and rewrite storage using a different stride, then shrink the logical array.
- Validate every supported CTYPE's expected width, require `itemSize` to match that width, check the size multiplication, and reject missing backing storage before the first typed read.

### Regression and contracts
- Extend `uarray_unescape_bounds` with a deliberately malformed two-element 64-bit fixture containing a backslash escape. Verify unescape leaves all bytes, count, and malformed metadata unchanged, then restore the original item width before freeing it.
- Update the existing GitHub Actions source contracts; reuse the current CMake/CTest target without new files or public declarations.

### Validation
- Static checks confirm all CTYPE variants used by the typed array macros are handled, unsupported enum values are rejected, and the validator executes before any read/write or size change.
- Mainline CI will compile and execute the regression suite.

---

## 2026-11-06 19:28 CST — Validate UArray numeric encoding item layout

### Baseline
- Main commit `4687d9b8c0efdd09d21e188720f38de5a013ee76`; SkipDBv2 Regression run #324 passed.
- Scope: `UArray_asNumberArrayString()`, used by numeric-to-text encoding conversion.

### Defect and fix
- The numeric formatter checked only for missing data when the item count was nonzero. It then dispatched on `itemType` and read elements using the C type's width, even when the public `UArray` metadata declared a different `itemSize`. A malformed array could therefore read beyond the declared storage and still be accepted for conversion.
- Add an explicit supported-type-to-width check and require the stored item width to match it, the item-count multiplication to remain representable, and nonempty storage to have a backing pointer before entering `UARRAY_INTFOREACH`.

### Regression and contracts
- Extend `uarray_encoding_conversion` with a numeric array whose 64-bit item type has deliberately mismatched 8-bit item metadata. Conversion must fail without changing its bytes or metadata; restore the original width before cleanup.
- Extend the existing GitHub Actions source contracts. Reuse the existing CMake/CTest target; no new files, exported declarations, or build-list edits.

### Validation
- Static review covers every CTYPE branch consumed by `UARRAY_INTFOREACH`; unsupported type identifiers are rejected instead of silently yielding a partial/empty number string.
- Mainline CI will compile the C changes and run the source-contract and regression suites.

---

## 2026-11-06 19:24 CST — Align bit-storage CI contract with the stricter size bound

- CI run #323 stopped in `Verify XY-C1 firmware integration contracts` before compilation. The existing detailed assertion still required the older `SIZE_MAX / itemSize` boundary after the shared bit-storage validator was tightened to reserve the UArray terminator byte.
- Update only the assertion to require the stricter `(SIZE_MAX - 1) / itemSize` guard. The implementation and regression coverage remain unchanged.
- This is a contract-only repair; the next mainline run will continue through compilation and CTest.

---

## 2026-11-06 19:23 CST — Validate UArray bit summaries against malformed storage

### Baseline
- Main commit `0dd7fac651e21a050aa6bd774f1f355f65073787`; SkipDBv2 Regression run #322 passed.
- Scope: `UArray_asBits()`, `UArray_bitCount()`, and shared bit-storage validation.

### Defect and fix
- Both bit-summary readers derived a byte count without consistently validating the receiver's backing pointer. A nonempty `UArray` with `data == NULL` therefore reached a dereference; `UArray_asBits()` also appended each output bit separately and did not verify allocation success.
- Reuse the shared storage validator for both readers, reject unrepresentable byte/bit counts, allocate the complete bit string once, verify the resize succeeded, and then emit the same LSB-first textual representation directly. Invalid storage now returns `NULL` from `UArray_asBits()` and zero from `UArray_bitCount()`.

### Regression and contracts
- Extend `uarray_bit_access` with valid bit-string/count checks, NULL receiver checks, and malformed-storage cases for both readers. Keep cleanup safe by restoring the original array metadata before freeing allocations.
- Update existing GitHub Actions source contracts. Reuse the current CMake/CTest target; no new files or exported declarations.

### Validation
- Static review confirms the output-size multiplication and terminator-byte constraint are checked before allocation, and the new direct-fill loop only executes after the target array size and backing pointer are verified.
- Mainline CI will compile the changes and run the updated regression suite.

---

## 2026-11-06 19:19 CST — Make UArray index duplication failure-safe

### Baseline
- Main commit `fdb9580b86ae5a3f7926b0b427fb1db8675a83da`; SkipDBv2 Regression run #321 passed.
- Scope: `UArray_duplicateIndexes()` and its existing resize regression.

### Defect and fix
- The routine doubled the item count without checking integer overflow, ignored the fact that `UArray_setSize_()` reports allocation/validation failure by leaving the original size unchanged, and then unconditionally wrote into the presumed expanded buffer. Invalid geometry or allocation failure could therefore lead to out-of-bounds reads/writes.
- Validate the receiver, backing storage, item width, current and doubled byte geometry, and item-count doubling before resizing. After resizing, verify both the new size and backing pointer before copying each item backward into its two destination slots.

### Regression and contracts
- Extend `uarray_stack_resize` with normal duplication-order/source-preservation checks, NULL receiver coverage, and synthetic item-count/item-byte overflow geometries. Restore malformed fixture metadata before cleanup.
- Update the workflow source contracts for the function's bounds and post-resize check. Reuse the existing CMake/CTest target; no new files or exported declarations.

### Validation
- Static review confirms all arithmetic checks precede pointer arithmetic, and the copy loop is unreachable unless the target resize actually succeeded.
- Mainline CI will compile the changes and run the source-contract and regression suites.

---

## 2026-11-06 19:07 CST — Reject malformed UArray bounded-read destinations

### Baseline
- Main commit `4a27c1c779f9b4974bd93c3509a5b3e5241049ad`; SkipDBv2 Regression run #320 passed.
- Scope: `UArray_readNumberOfItems_fromCStream_()` and its existing stream-I/O test.

### Defect and fix
- The bounded-read entry point validated the stream, item size, and arithmetic bounds but did not reject a destination whose logical size was nonzero while its data pointer was NULL. The neighboring streaming reader already rejects this malformed state.
- Reject the malformed destination before allocating the temporary buffer or consuming input.

### Regression and contracts
- Extend `uarray_stream_io` with a malformed destination fixture. It verifies failure, unchanged destination metadata, and that the input stream position remains at the start; the original allocation is restored before cleanup.
- Update the existing GitHub Actions source contract for the exact guard and test case. Reuse the existing CMake/CTest target; no new files or exported declarations.

### Validation
- Static review confirms the guard runs before buffer allocation, `fread`, or `UArray_append_()`.
- Mainline CI will validate the source contract, compile the test, and run the regression suite.

---

## 2026-11-06 19:05 CST — Reject hard-linked JFile data and journal files

### Baseline
- Main commit `6aeb298670a0891b31ba9241623a9d96caedda6e`; SkipDBv2 Regression run #319 passed.
- Scope: `JFile_openManagedFile_()` and the existing journal recovery regression.

### Defect and fix
- JFile already refused symlinks and verified file type, ownership, and inode identity, but it still accepted regular files with multiple hard links. This left the data-file and journal paths aliasable despite the new UDB commit-marker checks.
- Require the opened inode's link count to equal one in the shared managed-file verifier. The same check now applies to existing data files and journals, as well as newly created files.

### Regression and contracts
- Extend `jfile_journal` to verify that opening an aliased data file and opening a normal data path with an aliased journal both fail closed. Retain the existing symlink and journal-recovery cases.
- Update GitHub Actions source contracts; reuse the existing CMake/CTest target with no new files or exported declarations.

### Validation
- Static review confirms the opened descriptor is checked with `fstat` before it is wrapped as a `FILE *`, and failure follows the existing cleanup path.
- Mainline CI will compile the modified persistence code and run the source-contract and regression suites.

---

## 2026-11-06 19:03 CST — Reject hard-linked UDB commit markers and journals

### Baseline
- Main commit `56496d9e8e8ff2e143cfa890829eee423010a589`; SkipDBv2 Regression run #318 passed.
- Scope: UDB commit-marker opened-file validation and its existing regression fixture.

### Defect and fix
- UDB's secure-open helper verified regular-file type, ownership, and inode stability but accepted files with multiple hard links. A second path could therefore alias the commit marker or a recovery journal and undermine the path-based persistence trust boundary.
- Require the opened inode's link count to be exactly one. The same verifier protects both marker opens and journal validation.

### Regression and contracts
- Extend `commit_marker_test` to create hard-linked marker and journal fixtures and verify both aliases are rejected with the expected marker-open error or invalid-journal result.
- Update existing GitHub Actions source contracts. The existing CMake/CTest target is reused; no new files or exported declarations.

### Validation
- Static review confirms hard-link rejection occurs after `fstat` and before changing file permissions or exposing a `FILE *`.
- Mainline CI will execute the updated source contracts, compile the C changes, and run the existing regression suite.

---

## 2026-11-06 19:00 CST — Roll back failed PHash cuckoo kick chains

### Baseline
- Main commit `ad8abec4603f445e0271d19ade1fe9f21c3f80fc`; SkipDBv2 Regression run #317 passed.
- Scope: `PHash_cuckoo_()`, its kick-loop bound, and the existing pointer-hash regression fixture.

### Defect and fix
- An exhausted pointer-hash cuckoo chain returned failure after swapping records in-place, leaving the live table partially rewritten if growth could not complete.
- Bound the kick chain to 20 iterations, matching SHash, and record the previous value of each touched slot before swapping. On failure, restore all touched slots in reverse order, the incoming record, and the balance bit before returning `NULL`.

### Regression and contracts
- Extend the existing `phash_compact_test` with dynamically selected pointer keys that share both candidate slots, forcing kick-chain exhaustion deterministically. Verify slot-for-slot table identity, existing key/value mappings, incoming record, count, and balance.
- Update workflow source contracts for the loop bound, rollback ordering, and regression. Reuse the existing CMake/CTest target; no new files or exported declarations.

### Validation
- Static checks confirm at most 41 swaps can occur (one balancing kick plus two swaps per each of 20 iterations), and all three swap sites save prior slot contents.
- Mainline CI will validate the source contracts, compilation, and full CTest suite.

---

## 2026-11-06 18:57 CST — Correct SHash loop-bound contract header path

- CI run #316 stopped in the source-contract phase before configure/build: `SHash_maxLoops` is declared in `SHash.h`, but the new check read `SHash_inline.h`.
- Point the source contract at the declaration owner. The SHash rollback implementation and regression fixture are unchanged.
- Mainline CI run #317 will validate the contract, compilation, and full CTest suite.

---

## 2026-11-06 18:56 CST — Roll back failed SHash cuckoo kick chains

### Baseline
- Main commit `260fdc21af769e3f9d406324d8e561f5e57a80da`; SkipDBv2 Regression run #315 passed.
- Scope: failure handling in `SHash_cuckoo_()` and its existing regression fixture.

### Defect and fix
- A bounded kick-chain failure returned `NULL` after swapping records through the two tables. If a subsequent growth attempt could not complete, the map remained partially rewritten and an existing mapping could be lost even though insertion failed.
- Snapshot each touched slot before swapping. On an exhausted kick chain, restore every touched slot in reverse order, restore the caller's input record, and restore the table-balance bit before returning failure. Successful insertion paths retain their existing behavior.

### Regression and contracts
- Extend `shash_compact_test` with deterministic constant-hash collisions that exhaust the kick chain, then verify both mappings, every record slot, the incoming record, the key count, and balance remain unchanged.
- Extend existing workflow source contracts for the rollback sequence and regression. The existing CMake/CTest target is reused; no new files or exported declarations.

### Validation
- Static review confirms the rollback stack is bounded to the maximum of 20 two-table kick iterations plus the optional balancing kick (41 slots), and every `SHashRecord_swap` site records the prior slot contents.
- Mainline CI will compile and run the full SkipDBv2 regression suite.

---

## 2026-11-06 18:29 CST — Normalize Date fractional seconds and guard overflow

### Baseline
- Main commit `d75bb402b6cd102960ecfac1a9f61f2cade6d268`; SkipDBv2 Regression run #314 passed.
- Scope: Date seconds conversion, addition, and second-component setting in `Date.c`.

### Defect and fix
- `Date_fromSeconds_()` and `Date_addSeconds_()` cast arbitrary doubles to `long` without rejecting NaN, infinity, or out-of-range values. The addition path also left `tv_usec` unnormalized and could overflow `tv_sec`.
- Negative fractions were stored as negative microseconds (for example, `-0.5` became `tv_sec=0, tv_usec=-500000`), violating timeval normalization and producing incorrect component/time_t behavior.
- Add one private splitter that validates finite/range-safe input before conversion and returns floor-normalized seconds plus microseconds in [0, 999999]. Use it for construction, checked addition, and `Date_setSecond_()`. Reject invalid/overflowing updates without mutating the Date.

### Regression and contracts
- Extend existing `date_time_init_test` for negative fractions, carry/borrow, non-finite input, `tm_sec` negative fractions, and second-field overflow.
- Extend workflow source contracts for the helper, checked arithmetic, and tests. Existing CMake/CTest target reused; no new files or public declarations.

### Validation
- Static review confirms no floating-to-integer conversion occurs before finite/range validation, signed second arithmetic is guarded, and all successful updates publish normalized microseconds.
- Mainline CI run #315 will validate compilation and the complete regression suite.

---

## 2026-11-06 18:25 CST — Reject BStream read destinations that share stream storage

### Baseline
- Main commit `ebfc4a2bbb8a6c52c35239ce48ba6d8ab1b0bb9f`; SkipDBv2 Regression run #313 passed.
- Scope: plain/tagged array-read destination preflight in `BStream.c`.

### Defect and fix
- The preflight rejected only the exact destination `UArray` object. A distinct `UArray` struct could still share all or part of the stream's backing allocation.
- After reading a payload, `UArray_setData_type_size_copy_()` may reallocate that destination pointer. With a shallow or interior alias, this can invalidate or free the stream's own buffer while `BStream` still retains the old pointer.
- Add a private address-range preflight. Reject exact pointer aliases, destination pointers within the stream allocation (including zero-length views), and partial overlap before either reader consumes the cursor.

### Regression and contracts
- Extend the existing `bstream_tag_boundaries_test` with shared and partially overlapping backing views for both plain and tagged reads. Verify the error latch is set, the cursor remains unchanged, and stream storage metadata is preserved.
- Extend workflow source contracts for the overlap helper, preflight call, and all four regression cases. Reuse the existing CMake/CTest target; no new files or public declarations.

### Validation
- Static review confirms overlap comparisons use `uintptr_t` differences rather than potentially overflowing end-pointer arithmetic, and validation precedes payload reads.
- Mainline CI run #314 will validate compilation and the complete regression suite.

---

## 2026-11-06 18:21 CST — Preflight the complete Stack mark chain before rollback

### Baseline
- Main commit `7193f2ee22ebc4acaecd7fec58fa2e50a915ac4b`; SkipDBv2 Regression run #312 passed.
- Scope: Stack mark-pop and mark-point rollback in `Stack_inline.h`.

### Defect and fix
- `Stack_popMarkPoint_()` stopped validating as soon as it found the requested mark, so it did not validate that mark's predecessor before popping newer marks. A corrupt target predecessor could therefore cause a partial rollback while the function still returned success.
- Validate every predecessor in the chain, including the requested mark, before the first mutation. `Stack_popMark()` now validates its predecessor before invoking optional pop callbacks.

### Regression and contracts
- Extend the existing `stack_lifecycle_test` with a malformed target mark and malformed current mark. Verify rejected rollback leaves the top cursor, last mark, and stack contents unchanged.
- Extend workflow source contracts for validation order, regression assertions, and the existing CMake/CTest registration. No new files, public declarations, or target changes.

### Validation
- Static review confirms the complete mark chain is checked before unwinding and callback invocation follows predecessor validation.
- Mainline CI run #313 will validate compilation and the full regression suite.

---

## 2026-11-06 17:55 CST — Normalize malformed UTF-16 surrogate input

### Baseline
- Main commit `6117729f4642f2c186bf56cca91baaa9a4fed428`; SkipDBv2 Regression run #311 passed.
- Scope: `ConvertUTF16toUTF32()` and `ConvertUTF16toUTF8()` source handling.

### Changes
- Lenient conversion now emits U+FFFD for an unmatched high surrogate followed by a non-low code unit and for an isolated low surrogate. The following source unit remains available to the next iteration.
- Strict conversion preserves the offending source cursor. A trailing high surrogate still returns `sourceExhausted` for streaming callers.
- UTF-16 to UTF-8 output now preflights remaining target capacity before advancing the output pointer.

### Regression and contracts
- Extend the existing ConvertUTF test with strict/lenient isolated-high and isolated-low cases, valid-pair preservation, trailing-high streaming behavior, and target-exhaustion rollback.
- Extend workflow source contracts. Reuse the existing CMake target; no new files or public declarations.

### Validation
- Static review confirms both converters preserve strict/truncated source positions and replace isolated surrogates in lenient mode.
- Mainline CI run #312 will validate compilation and the complete regression suite.

---

## 2026-11-06 17:51 CST — Bound UTF-8 decoder reads by remaining source length

### Baseline
- Main commit `52533d55ccbc40d78e8e09d1e7ef4b494ac7a538`; SkipDBv2 Regression run #310 passed.
- Scope: UTF-8 source-length preflight in `ConvertUTF8toUTF16()` and `ConvertUTF8toUTF32()`.

### Defect and fix
- Both decoders formed `source + extraBytesToRead` before checking whether the requested sequence remained inside the source array. A truncated multibyte sequence could therefore form a pointer beyond one-past the buffer, which is undefined behavior even before the bounds comparison.
- Replace the pointer-addition check with a remaining-byte comparison. The first-byte read remains guarded by the existing non-empty source loop; validation and decode now happen only after the complete sequence length is known to fit.

### Regression and contracts
- Extend the existing `convert_utf8_bounds` CTest with truncated two-byte buffers, malformed continuation bytes, and oversized five/six-byte leads through both UTF-16 and UTF-32 decoders. Verify failures preserve source and destination cursors and output.
- Extend the workflow's implementation, test, and function-order contracts. Reuse the existing CMake target; no new files, public declarations, or target changes.

### Validation
- Static checks confirm both decoders compare remaining bytes before UTF-8 validation and sequence decoding, and the tests exercise both conversion paths.
- Mainline CI run #311 will validate compilation and the complete regression suite.

---

## 2026-11-06 17:40 CST — Correct lenient UTF-32 scalar replacement semantics

### Baseline
- Main commit `cc7ef07d73383573e2f56fd7600368d02ccb6b97`; SkipDBv2 Regression run #309 passed.
- Scope: the UTF-32 source conversion paths in `ConvertUTF.c` only.

### Defects and fixes
- `ConvertUTF32toUTF16()` returned `sourceIllegal` for an out-of-range scalar without restoring the source cursor.
- `ConvertUTF32toUTF8()` encoded isolated surrogate values as invalid UTF-8 in lenient mode, and treated out-of-range scalars as errors even when replacement mode was requested.
- Both UTF-32 encoders now preserve the offending cursor and emit no partial output on strict scalar errors; lenient mode consistently emits U+FFFD. UTF-8 target capacity is checked before advancing the output pointer.

### Regression and contracts
- Extend the existing `convert_utf8_bounds` CTest with strict cursor/no-mutation checks, lenient replacement checks for isolated surrogates and out-of-range scalars, and target-exhaustion rollback.
- Extend the workflow's source contracts for both conversion paths and their regression assertions. Reuse the existing CMake target; no new files, target changes, or exported declarations.

### Validation
- Static review confirms strict invalid-scalar branches restore the source cursor, lenient branches emit the replacement scalar, and target capacity is checked before pointer advancement.
- Mainline CI run #310 will validate compilation and the complete regression suite.

---

## 2026-11-06 17:23 CST — Preserve live-size semantics in LIST_SAFEFOREACH

### Baseline
- Main commit `d46f3f4d0a2dd2b6decb9d4442872a891e449f17`; initial List inline storage hardening is in CI run #308.
- Scope remains the List inline storage/iteration contract.

### Review correction
- `LIST_SAFEFOREACH` previously re-evaluated `List_size()` on every iteration so callbacks could shrink the list safely.
- Retain that live-size behavior while checking `List_hasValidStorage_()` on every pass. Avoid caching the initial length, which could invoke callbacks with synthetic NULL elements after an in-loop shrink.

### Regression and contracts
- Add a callback-driven regression that shrinks a valid two-element List during the first safe-iteration visit and verifies the loop stops at the new size.
- Extend workflow source contracts to enforce the dynamic loop bound and regression case. No new files or CMake changes.

### Validation
- Static checks confirm storage validation precedes each live-size check in `LIST_SAFEFOREACH`.
- Mainline CI run #309 will validate the correction.

---

## 2026-11-06 17:19 CST — Guard List inline operations against corrupt capacity metadata

### Baseline
- Main commit `409631f08834efeaf8e66def09823ae9a044e718`; SkipDBv2 Regression run #307 passed source contracts, configure, full build, and CTest.
- Scope: the shared List storage invariant used by inline accessors and mutators.

### Defect and fix
- `List.c` validated that logical size fit the declared capacity, but the public inline API in `List_inline.h` often checked only NULL pointers or logical indexes before reading or mutating `items`.
- Move the private validator into the inline header as a `static inline` helper, remove its duplicate C-only definition, and apply the invariant before backing-array access in inline readers, writers, removers, stack operations, and iteration macros.

### Regression and contracts
- Extend the existing `list_storage_bounds_test` with malformed source/destination coverage for inline reads, all iteration macros, append, indexed writes/removals, filtering, and stack access; verify malformed storage remains unchanged.
- Update workflow source contracts to enforce validator ownership, preflight ordering, and regression coverage. Reuse the existing CMake target; no new files or public declarations.

### Validation
- Static source checks confirm the validator precedes inline callers and rejects capacity mismatch and missing backing storage before dereference.
- Mainline CI run #308 will validate the complete build and regression suite.

---

## 2026-11-06 17:04 CST — Make UArray byte translation reject invalid inputs safely

### Baseline
- Main commit `b2f2925897f1b3d9eb37dfa10c3c1ba4de7dcb55`; SkipDBv2 Regression run #306 passed source contracts, configure, full build, and CTest.
- Scope: `UArray_translate()` only.

### Defect and fix
- The void translation API called `UArray_error_()` for malformed byte arrays or unequal mapping lengths. That helper exits the process, so a recoverable bad argument could terminate the entire caller.
- Keep the existing byte-array and equal-length requirements, but fail closed before building the map or mutating the receiver. Valid translation still updates the cached hash.

### Regression and contracts
- Add `uarray_translate_validation_test` for valid translation, hash invalidation, NULL arguments, malformed array metadata/storage, and mismatched mapping lengths with no partial mutation.
- Register the test in CMake and extend workflow source contracts for the validation boundary and new target. No public declarations changed.

### Validation
- Static review confirms all validation precedes the translation map and mutation loops, and no fatal error helper remains in this void API.
- Mainline CI will validate the complete build and regression suite.

---

## 2026-11-06 17:01 CST — Preflight UArray split source and delimiter arrays

### Baseline
- Main commit `a73c77d324c3c2756d5ba564be9d79dd63f9bb32`; SkipDBv2 Regression run #305 passed source contracts, configure, full build, and CTest.
- Scope: the `UArray_split_()` input-validation boundary in `UArray_string.c`.

### Defect and fix
- Split validated non-NULL data pointers and the delimiter item type, but did not verify source item geometry or that the pointer-list item width matched the pointer representation.
- Malformed source arrays could enter the scan loop; malformed delimiter arrays could be traversed as though they were valid pointer vectors.
- Add a private preflight for source byte geometry, representable source length, delimiter pointer type/width, delimiter byte geometry, and non-empty backing storage. Reject invalid inputs before allocating results or dereferencing delimiter entries.

### Regression and contracts
- Add `uarray_split_delimiters_test` covering valid multi-delimiter splitting/counting and malformed source/delimiter metadata, missing storage, and size overflow.
- Register the test in CMake and update workflow source contracts for validation order and the new target. No public declarations changed.

### Validation
- Static review confirms zero-width guards short-circuit division and the preflight runs before result allocation and pointer iteration.
- Mainline CI will validate compilation and the complete regression suite.

---

## 2026-11-06 16:57 CST — Make invalid UArray pointer access non-fatal

- Run #304 completed configure/build; the new pointer regression stopped because `UArray_error_()` terminates the process. The test exposed that the attempted defensive checks still used this fatal path for malformed external storage.
- Change raw pointer reads and pointer writes to fail closed with `NULL` / no mutation for invalid pointer-array metadata rather than calling the process-terminating error helper.
- Preserve the existing raw accessor's no-index-bounds-check contract; bounded `UArray_pointerAt_()` continues to validate the index before delegating.
- Run #305 will re-run the pointer regression together with the full suite.

---

## 2026-11-06 16:55 CST — Validate UArray pointer-element access invariants

### Baseline
- Main commit `a068923863755ce796d0c41192e7dba59f14f5a1`; SkipDBv2 Regression run #303 passed contracts, configure, full build, and CTest.
- Scope: pointer getter/setter paths in `UArray.c` only.

### Defect and fix
- `UArray_rawPointerAt_()` dereferenced a NULL receiver and assumed pointer-sized storage without validating the element type, item width, storage multiplication, or backing data.
- `UArray_pointerAt_()` checked only the index and data pointer, so malformed pointer-array metadata could still produce an invalid pointer read.
- `UArray_at_putPointer_()` resized the array before checking that it was a pointer array, allowing unsupported or malformed arrays to be mutated before failure.
- Add one private storage validator; use it in raw/bounded reads and before/after setter growth. Reject unsupported element geometry before resizing and invalidate the cached hash after successful writes.

### Regression and contracts
- Add `uarray_pointer_access_test` for valid pointer round trips, NULL/out-of-range reads, malformed backing data, invalid item width/type, size overflow, no-growth-on-rejection, and hash invalidation.
- Register the test in CMake and extend workflow source contracts. No exported declarations changed.

### Validation
- Static review confirms pointer width/geometry and non-empty backing storage are checked before pointer arithmetic; the setter preflights before any size mutation.
- CI run #304 will verify compilation and the full regression suite.

---

## 2026-11-06 16:50 CST — Repair UArray distance CI boundary matcher

- Run #302 reached the source-contract phase and stopped before configure/build because the newly added Python check escaped the newline delimiter twice, leaving the distance-function slice empty.
- Corrected the workflow boundary matcher to search for an actual newline before the next section marker. The float64 implementation and regression case are unchanged.
- The next run will re-evaluate the source contract and then proceed to build and CTest.

---

## 2026-11-06 16:49 CST — Preserve float64 precision in UArray distance

### Baseline
- Main commit `79a9d686b93ad98aea763492ace8eac3c35f0363`; SkipDBv2 Regression run #301 passed source contracts, configure, full build, and CTest.
- Scope: the float64 branch of `UArray_distanceTo_()` only.

### Defect and fix
- The float64 distance loop stored each double-precision delta in a `float32_t`, silently underflowing small representable differences and overflowing large but representable ones before squaring.
- Keep the delta in `float64_t`. The float32 path and existing unequal-length behavior are unchanged.

### Regression and contracts
- Add `uarray_distance_precision_test` to cover float32 baseline behavior, float64 deltas at (10^{-50}) and (10^{40}), and unequal lengths.
- Register the test in CMake and extend CI contracts to enforce the float64 intermediate and test registration. No public declarations changed.

### Validation
- Static review confirms the changed intermediate matches the source array's 64-bit element type.
- Mainline CI will validate the source contracts, build, and full CTest suite.

---

## 2026-11-06 16:46 CST — Fix UArray extrema on negative-only values

- Baseline `dd707bd254232e86ebe7eefc8de35331e5bfdfc5`; CI run #300 passed.
- `UArray_maxAsDouble()` used `DBL_MIN`, so arrays containing only negative values returned a positive sentinel. `UArray_minAsDouble()` had the symmetric finite-sentinel issue for positive infinity.
- Both functions now preflight receiver/storage geometry and initialize from the first actual element; empty arrays retain the existing zero result.
- Added `uarray_math_extrema_test`, registered it in CMake, and extended workflow source contracts. No public API changes.

---

## 2026-11-06 16:41 CST — Correct UArray bit indexing and assignment semantics

### Baseline
- Main commit `0dc84335e24deab45a3c6b1dbf97e80dce5654e2`; SkipDBv2 Regression run #299 passed source contracts, configure, full build, and CTest after the BStream read-side repair.
- Scope: the raw bit getter/setter pair in `UArray_math.c`; other math operations remain untouched.

### Defects and fixes
- Both accessors calculated the in-byte position as `i - i / 8`, which produces invalid bit offsets after the first byte and can trigger oversized shifts.
- The getter assigned the extracted bit back into the source byte, corrupting the array during a read.
- The setter toggled bits when asked to clear them, so repeated false assignments were not idempotent.
- Centralize the shared storage/overflow preflight, use `i % 8`, make reads strictly non-mutating, and implement explicit set/clear masks. Successful writes invalidate the cached UArray hash.

### Regression and contracts
- Add `uarray_bit_access_test` for cross-byte indices, read-only behavior, idempotent set/clear, out-of-range and maximum-size indices, malformed storage, multiplication overflow, and hash invalidation.
- Register the new test in CMake and add workflow source contracts for the helper, both accessors, target registration, and regression assertions. No exported declarations changed.

### Validation
- Static review confirms the helper rejects zero item width, overflowing byte geometry, and non-empty NULL storage before pointer access or shifts.
- The mainline workflow is authoritative for compile and test validation.

---

## 2026-11-06 16:37 CST — Correct BStream read-contract boundary matching

### CI diagnosis
- SkipDBv2 Regression run #298 stopped in the added BStream source-contract preflight after the existing integration contracts passed.
- The new Python boundary detector searched for a literal backslash-n instead of a newline, so it produced an empty function-body slice. This was a workflow assertion defect; configure, compilation, and CTest were not reached.

### Fix
- Correct both reader-body delimiters to use the same newline escape convention as the existing BStream read-path contracts.
- No C implementation, regression case, CMake target, or public declaration changed in this repair.

### Validation
- Static checks confirm each delimiter occurs once and the BStream read-contract checks remain wired to the existing implementation and test source contracts.
- The next mainline workflow will validate the full preflight, build, and regression suite.

---

## 2026-11-06 16:35 CST — Fail closed on invalid BStream array reads

### Baseline
- Main commit `87283fc7619b47e77922528a664a32c3df47b146`; SkipDBv2 Regression run #297 passed source contracts, configure, the full build, and the configured CTest suite.
- Scope: BStream array-read validation only. Wire encoding, public declarations, and writer behavior remain unchanged.

### Defect and fix
- `BStream_readUArray_()` and `BStream_readTaggedUArray_()` silently returned for invalid destinations, aliased stream storage, malformed backing arrays, and invalid stream cursors. Callers could not distinguish rejected input from an empty result because no sticky error was recorded.
- Add one private preflight that validates stream byte storage, cursor bounds, and destination UArray geometry before either reader consumes input. Invalid arguments now latch a read-specific error and preserve the cursor.

### Regression and contracts
- Extend the existing `bstream_tag_boundaries_test` with NULL/aliased destinations, out-of-range cursor, malformed destination storage, and successful plain/tagged array round trips.
- Extend the existing workflow source contracts to verify the private helper is defined before its callers and both read paths invoke it. Reuse the existing CMake target; no new files, exported symbols, or target changes.

### Validation
- Static structural checks confirm the helper is private, its integer division is short-circuited by the nonzero item-size guard, and preflight runs before any read/tag consumption.
- The pushed SkipDBv2 Regression workflow is authoritative for compilation and the full CTest run.

---

## 2026-11-06 16:15 CST — Escape BStream source contract string correctly

### CI diagnosis
- SkipDBv2 Regression run #296 stopped in source-contract preflight with a Python `SyntaxError`; compilation and CTest were not reached.
- The new BStream implementation contract contained two backslashes before embedded quotes, terminating the Python string incorrectly.

### Fix
- Correct the workflow literal to use one escape before each embedded quote. The BStream implementation and regression cases are unchanged.

### Validation
- The corrected source-contract string is present in the workflow; the next mainline run will validate the preflight and full test suite.

---

## 2026-11-06 16:14 CST — Fail closed on malformed BStream write storage

### Baseline
- Main commit `93b55a18155d0584df97915026689cb9c7d556b3`; SkipDBv2 Regression run #295 passed source contracts, configure, full build, and all 49 CTest cases.

### Defects and fixes
- The shared `BStream_appendBytes_()` primitive validated the UArray item width but did not reject a non-empty byte array with a NULL data pointer before calling `UArray_appendBytes_size_()`.
- `BStream_writeTaggedData_length_()` silently returned for invalid byte width or a non-empty NULL payload, leaving callers without the sticky error state used by the rest of the binary writer API.
- Reject malformed existing storage at the append boundary. Make invalid tagged-data arguments latch an explicit error while preserving the existing terminal-error behavior.

### Regression and contracts
- Extend the existing `bstream_tag_boundaries_test` with malformed backing-store tests for plain and tagged writes, plus a NULL tagged-payload test. Fixtures restore the original UArray fields before normal destruction.
- Extend existing BStream implementation/test source contracts. CMake registration and public declarations remain unchanged.

### Validation
- Static review confirms all invalid test fixtures restore owned storage before destruction and the implementation changes only the BStream write path.
- The next mainline workflow is authoritative for compilation and the full CTest run.

---

## 2026-11-06 16:10 CST — Validate SHash growth metadata before rehash

### Baseline
- Main commit `2ee5cccecd2bd6385f553525975db2c83c8cde40`; SkipDBv2 Regression run #294 passed source contracts, configure, full build, and all 49 CTest cases.

### Changes
- Harden `SHash_grow()` against malformed table geometry, mismatched hash masks, invalid log2 values, and key counts beyond the physical two-table record capacity.
- Keep the callbacks required by SHash and allow the legitimate transient `numKeys > tableSize` state that occurs immediately before a normal growth operation.
- Invalid metadata now returns without reallocating or freeing the existing record table.

### Regression and contracts
- Extend the existing `shash_compact_test` with independent malformed geometry, mask, and impossible-count fixtures. Normal growth across the threshold remains covered by its existing 12-key insertion sequence.
- Extend the existing workflow source/test contracts. No new files, public declarations, or CMake target changes.

### Validation
- Static review confirms the log2 bound short-circuits before the shift and all geometry/count checks precede source-table iteration.
- The post-commit workflow is authoritative for full compilation and runtime regression.

---

## 2026-11-06 16:09 CST — Preserve valid over-capacity PHash growth

### CI diagnosis
- SkipDBv2 Regression run #293 passed source contracts, configure, and compilation, but CTest failed in both `phash_compact_test` and `skipdb_prefix_test`.
- The new `PHash_grow()` guard incorrectly rejected `numKeys > tableSize`. A normal insert increments `numKeys` before it calls grow, so the valid transition state can be `tableSize + 1`; the rejection prevented growth and then broke database PID caching.

### Fix
- Keep geometry and mask validation, but bound the count against the actual two-table storage capacity (`2 * tableSize`), not the post-growth threshold (`tableSize`).
- Adjust the malformed-count fixture to exceed the physical record capacity. Existing tests continue to cover successful growth across the threshold.

### Validation
- No production API, test target, or file set changes. The next CI run must confirm both the PHash growth path and the downstream SkipDB prefix fixture.

---

## 2026-11-06 16:07 CST — Reject malformed PHash growth metadata

### Baseline
- Main commit `1883d9b06e9fb3ba4e3627620541aca82d809c95`; SkipDBv2 Regression run #292 passed source contracts, configure, full build, and all 49 CTest cases.

### Defect and fix
- `PHash_grow()` trusted `tableSize`, `log2tableSize`, `mask`, and `numKeys` independently. A stale or corrupted table size could make the rehash loop read beyond the allocation; a mismatched mask could select records outside the backing table; an impossible key count could be rehashed as though the table were valid.
- Apply the same core geometry/count invariants already enforced by `PHash_compact()` before saving state or iterating source records. Invalid inputs now leave the table untouched.

### Regression and contracts
- Extend the existing `phash_compact_test` with isolated shallow-table cases for inconsistent geometry, mask, and key count. Each case starts with a separately allocated empty table so the test safely verifies the rejected mutation.
- Extend the existing workflow source/test contracts. No new file, public symbol, CMake target, or build registration is introduced.

### Validation
- Static review confirms the shift is evaluated only after the log2 range guard and the source record loop is reached only after all metadata invariants pass.
- The post-commit SkipDBv2 Regression run is authoritative for compilation and the full CTest suite.

---

## 2026-11-06 16:04 CST — Repair UArray stream link failure

### CI diagnosis
- SkipDBv2 Regression run #291 passed source-contract preflight, CMake configure, and compilation of all objects, but the new stream test failed at link time with an undefined reference to `UArray_SizeOfCtype`.
- The header contains a declaration for that name, but the linked Basekit library does not provide its implementation. It was an invalid dependency introduced by the preceding refactor, not a test or CMake registration problem.

### Fix
- Remove the unresolved call and retain the stream reader's existing `self->itemSize == 1` guard as the byte-width check.
- Strengthen the existing workflow source contract to require that guard. No test, build target, or public declaration changes.

### Validation
- Static review confirms the stream reader now depends only on implemented, declared UArray APIs. The next mainline workflow must complete linking and CTest.

---

## 2026-11-06 16:02 CST — Harden UArray stream line and read-error handling

### Baseline
- Main commit `c089a5d8b32080e24894a91c2d4a3741da3fd3f7`; SkipDBv2 Regression run #290 passed source-contract preflight, configure, full build, and CTest.

### Defects and fixes
- `UArray_readNumberOfItems_fromCStream_()` returned a partial item count even after the underlying stream entered an I/O error state. Preserve successfully read bytes, but return the documented `-1` error result consistently with the unbounded stream reader.
- `UArray_readLineFromCStream_()` used `fgets()` chunks and truncated at the first carriage return after the input chunk had already consumed later bytes. Bare-CR line endings could discard the next line; CRLF at a chunk boundary and embedded NUL bytes were also mishandled.
- Replace the heap-allocated line buffer with bounded stack chunks, recognize LF/CRLF/bare CR, push back the first byte after standalone CR, preserve binary payload bytes, and propagate stream/pushback errors after appending successfully read data.

### Regression and contracts
- Add `uarray_stream_io.c` for empty lines, LF/CRLF/bare-CR boundaries, a 4096-byte line crossing the chunk boundary, embedded NUL, EOF without a final newline, partial fixed-count reads, and injected I/O errors.
- Register the focused CTest target and extend GitHub Actions source/build contracts. No production source file or public API is added.

### Validation
- Static review confirms line appends are bounded and type-consistent, no byte after a standalone CR is consumed, and partial input is preserved when an I/O error is reported.
- The workflow triggered by this commit is authoritative for source contracts, compilation, and runtime tests.

---

## 2026-11-06 15:46 CST — Correct UArray per-item byte reversal

### Baseline
- Main commit `e4cee3698225dc24e55e8fffb188f58bf2292f3b`; SkipDBv2 Regression run #289 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- `UArray_reverseItemByteOrders()` used logical element indices as byte offsets, computed the right-hand index without the final-byte adjustment, and swapped across the full item width. It could reverse the wrong bytes and read beyond a single-item array.
- Validate the receiver, backing data, and byte-count multiplication; reverse each item's bytes in place with symmetric byte offsets and only half-width swaps.

### Regression and contracts
- Add `uarray_reverse_item_byte_order.c` for three 16-bit items, a single 32-bit item (the out-of-bounds boundary), NULL, empty, and missing-data inputs.
- Register the focused test in CMake/CTest and extend workflow source and registration contracts.

### Validation
- Static review confirms byte offsets are based on `i * itemSize`, the paired index is `itemSize - 1 - j`, and multiplication is checked before pointer arithmetic.
- The workflow triggered by this commit is authoritative for compilation and runtime tests.

---

## 2026-11-06 15:43 CST — Bound UArray unescape lookahead

### Baseline
- Main commit `2eba5a09c2f27898891438bbcaa9a54361481108`; SkipDBv2 Regression run #288 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- `UArray_unescape()` fetched `getIndex + 1` for every input byte, including a backslash at the final position. A terminal escape marker therefore read one logical element beyond the array.
- Read the lookahead only when another element remains; otherwise feed the existing NUL escape case so a lone trailing backslash is preserved as a literal backslash.

### Regression and contracts
- Add `uarray_unescape_bounds.c` for a single backslash, a trailing backslash after text, newline decoding, escaped-backslash decoding, and a NULL receiver.
- Register the focused test in CMake/CTest and extend workflow source contracts for the bounded lookahead and test registration.

### Validation
- Static review confirms the lookahead is guarded by `getIndex < self->size - 1` before calling `UArray_longAt_()`.
- The workflow triggered by this commit is authoritative for compilation and runtime tests.

---

## 2026-11-06 15:41 CST — Repair Date helper source-contract delimiter

### CI failure
- SkipDBv2 Regression run #287 stopped in source-contract preflight because the new private-helper scan searched for the two literal characters backslash and `n`, instead of the newline before the helper's closing brace.

### Fix
- Correct the embedded Python delimiter to match the existing newline-based function scans. No Date implementation or test changes.

### Validation
- The helper scan now uses the same newline delimiter as the adjacent Date function-boundary checks; the next run will validate the remaining Date contracts, build, and CTest.

---

## 2026-11-06 15:36 CST — Guard Date receiver and time-component access

### Baseline
- Main commit `c2a59c12518e7541d2d9dc83840faf040ea95bc1`; SkipDBv2 Regression run #286 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- Multiple public Date entry points dereferenced caller-owned Date pointers without validation. Component accessors also dereferenced the result of `localtime()` without checking whether the timestamp could be represented.
- Add one private local-time conversion helper, reject NULL receiver/peer arguments before state access, and give value-returning queries neutral failure values. Keep Date's public ABI unchanged.

### Regression and contracts
- Extend the existing `date_time_init.c` test with NULL receiver/argument coverage across copy, comparison, timezone, conversion, component, parsing, duration, and current-time APIs.
- Verify normal copy and fractional-second behavior remains intact.
- Strengthen existing Date workflow contracts for helper use, guard ordering, and regression coverage; reuse the registered CTest target.

### Validation
- Static review verifies the private helper has no exported declaration and checks precede local-time dereferences.
- The workflow triggered by this commit is authoritative for compilation and CTest.

---

## 2026-11-06 15:34 CST — Align List slice CI contract with centralized validator

### CI failure
- SkipDBv2 Regression run #285 stopped at the source-contract preflight. The List refactor moved the existing size/capacity bounds checks into `List_hasValidStorage_()`, but the older slice contract still required those two expressions to remain duplicated inside `List_sliceInPlace()`.

### Fix
- Preserve the pre-existing List memory-accounting and slice-semantic static checks while asserting that slice now delegates storage validation to the shared helper.
- Keep the implementation and test unchanged; this is a source-contract-only correction.

### Validation
- The workflow contract now checks helper use in the slice entry point and continues to require allocation-free inclusive slicing, end clamping, and tail clearing.

---

## 2026-11-06 15:32 CST — Validate List backing-storage invariants

### Baseline
- Main commit `3af56581b92313115e17ed364c6b98d2906ff1c0`; SkipDBv2 Regression run #284 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- List operations trusted `size` even when it exceeded the capacity recorded by `memSize`. Copy/equality and other readers could then read beyond the backing array; mutators could clear or resize beyond the allocation.
- Introduce one private storage-invariant check and apply it before the List APIs in `List.c` read, expose, or modify item storage. Preserve valid empty lists, including lists compacted to zero capacity.

### Regression and contracts
- Add `list_storage_bounds.c` to cover malformed source and destination metadata, clone/equality rejection, and safe UArray-view behavior.
- Register `list_storage_bounds_test` in CMake/CTest and add workflow contracts for the validator, call ordering, regression cases, and target registration.

### Validation
- Structural checks verify the shared guard precedes backing-array accesses in core copy, comparison, resize, allocation, and slice operations.
- The workflow triggered by this commit is authoritative for compilation and runtime tests.

---

## 2026-11-06 15:26 CST — Align UArray path source contract with guard formatting

### CI failure
- SkipDBv2 Regression run #283 stopped during source-contract preflight because the static contract expected the NULL guard on one line, while the implementation wraps the condition after `||`.

### Fix
- Match the contract needle to the actual multi-line guard prefix. No C implementation change.

### Validation
- The source-contract needle now matches `if (NULL == self || NULL == path ||`; the next run will verify preflight, configure, build, and CTest.

---

## 2026-11-06 15:24 CST — Guard UArray path component entry points

### Baseline
- Main commit `f4e38d9f4ed151d73bbb69ebb3889507ac057085`; SkipDBv2 Regression run #282 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- Several `UArray_path.c` entry points dereferenced a NULL receiver before validating it. The component and file-name accessors could crash instead of returning a failure result; `UArray_appendPath_()` and `UArray_removeLastPathComponent()` had the same unsafe receiver handling.
- Add early NULL and backing-storage checks, use consistent `-1` / `NULL` failure results for query functions, and avoid converting a negative component offset to an unsigned size.

### Regression and contracts
- Add `uarray_path_null.c` covering NULL receivers plus normal component, extension, and extension-free paths.
- Register `uarray_path_null_test` in CMake/CTest and add workflow source contracts for guard ordering, test coverage, and registration.
- Keep the public function signatures and existing valid-path behavior unchanged.

### Validation
- Structural checks verify NULL guards precede receiver dereferences and the regression target is wired into CMake/CTest.
- The workflow triggered by this commit is authoritative for compilation and runtime tests.

---

## 2026-11-06 15:15 CST — Prevent overflow in Duration component conversion

### Baseline
- Main commit `778b5bb42814f63cdcc14ef4b7c914416f10c4a7`; SkipDBv2 Regression run #281 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- `Duration_asComponents()` converted the year component multiplied by seconds per year back to `int`. Even a 100-year duration exceeds `INT_MAX` seconds, so that floating-to-integer conversion is out of range and has undefined behavior; the resulting remainder then corrupts day, hour, minute, and second components.
- Keep component extraction bounded by the existing year-range checks, but subtract each extracted component using double-precision arithmetic. Preserve the existing NaN and out-of-range saturation behavior.

### Regression and contracts
- Extend the existing `duration_lifecycle.c` test with positive and negative 100-year spans including day/time remainders.
- Strengthen the existing workflow source contract to require double-precision remainder arithmetic and both regression directions.
- Reuse the registered `duration_lifecycle_test` CMake target; no new source files or targets.

### Validation
- Static review confirms no seconds-per-year product is converted back to `int`, and the test exercises both signs with nonzero lower components.
- The workflow triggered by this commit is authoritative for compilation and CTest.

---

## 2026-11-06 15:05 CST — Fix UArray lookup source-contract scan

### CI failure
- Run #280 stopped in the XY-C1 source-contract preflight: the new lookup guard contract searched for a literal backslash-n instead of the newline before the C function's closing brace.

### Fix
- Correct the embedded Python source scan to locate the actual newline delimiter. No C implementation changes or new files in this CI-repair commit.

### Validation
- Structural preflight confirms the contract now searches for `"\n}"` and still checks NULL rejection before the first `strcmp()`.

---

## 2026-11-06 15:03 CST — Reject NULL type and encoding names

### Baseline
- Main commit `3aebf71fa4d23ab0685b883fd3825b97353155bd`; SkipDBv2 Regression run #279 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- `CTYPE_forName()` and `CENCODING_forName()` passed caller input directly to `strcmp()`. A NULL name dereferenced a null pointer instead of following the functions' existing `-1` unknown-name failure convention.
- Return `-1` for NULL input before any string comparison; known and unknown non-NULL names preserve their existing results.

### Regression and contracts
- Add `uarray_name_lookup.c` covering NULL, known, and unknown names for both lookup functions.
- Register the focused CTest target in CMake and add static plus runtime-order workflow contracts for the guards, regression cases, and target registration.

### Validation
- Structural preflight confirms the NULL guards precede the first `strcmp()` call in each function and the test target is registered.
- The workflow triggered by this commit is authoritative for compilation and CTest.

---

## 2026-11-06 14:59 CST — Fail closed when cloning invalid BStream state

### Baseline
- Main commit `819243c9c3dd17d831694e234b0ead86c97b1f16`; SkipDBv2 Regression run #278 passed source-contract preflight, configure, full build, and CTest.

### Defect and fix
- `BStream_clone()` trusted any non-NULL UArray and ignored that `UArray_copy_()` can silently return when backing bytes are missing. It could return a stream without the source bytes and preserve a cursor beyond the copied buffer.
- Validate the byte-array storage and cursor before allocation; verify copied type, encoding, size, storage, and content before returning the clone. A failed copy releases the partial stream and returns NULL.

### Regression and contracts
- Extend the existing `bstream_tag_boundaries_test` with clone fidelity, out-of-range cursor, and missing-backing-storage cases.
- Extend the existing workflow source contracts for the new guard and regression assertions. Reuse the registered CMake target; no new files, targets, or public symbols.

### Validation
- Structural preflight confirms the source-state checks precede allocation and that copied content is verified before returning the clone.
- The workflow triggered by this commit is authoritative for full compilation and CTest.

---

## 2026-11-06 14:49 CST — Correct Common byte-order detection

### Baseline
- Main commit `04b9fe2f04cabfa82cf86a9545108829508baef2`; SkipDBv2 Regression run #277 passed source contracts, configure, full build, and CTest.

### Defect and fix
- `io_isBigEndian()` returned the first byte of integer 1. That yields 1 on little-endian systems and 0 on big-endian systems—the reverse of the function's contract. `io_uint32InBigEndian()` consequently applied byte swapping on the wrong architectures.
- Return the boolean condition `s[0] == 0`, so the byte-swap helper can correctly preserve values on big-endian systems and swap on little-endian systems.

### Regression and contracts
- Extend the existing Common allocator test binary with a host-order probe and expected 32-bit big-endian conversion.
- Extend static/dynamic workflow source contracts for the corrected probe and both regression assertions. Existing CMake target is reused.

### Validation
- Structural preflight verifies the corrected byte-order predicate and the two runtime assertions.
- The workflow triggered by this commit is authoritative for compilation and test results.

---

## 2026-11-06 14:47 CST — Guard trailing percent in UArray formatter

### Baseline
- Main commit `9889ef828ebfc70da6d1759d7258620d2784035d`; SkipDBv2 Regression run #276 passed source contracts, configure, full build, and CTest.

### Defect and fix
- `UArray_fromVargs_()` advanced the format pointer once for `%` and again at loop end. When the marker was the final byte, this advanced past the NUL terminator and the next loop condition read outside the format string.
- Detect a terminating percent marker immediately, append it as a literal, and return before the loop can advance past the terminator.

### Regression and contracts
- Add an isolated test for `"prefix%"`, a format consisting only of `"%"`, and an ordinary `%i` substitution.
- Register the new test with CMake/CTest and add static/dynamic workflow contracts for the guard, the expected output, and target registration.

### Validation
- Structural preflight verifies the terminator check and literal-percent path are present.
- The workflow triggered by this commit is authoritative for compilation and runtime testing.

---

## 2026-11-06 14:45 CST — Repair RandomGen source-contract delimiter

### CI diagnosis
- SkipDBv2 Regression run #275 stopped during the source-contract preflight. The production change and test were found, but the new embedded Python check searched for a literal backslash-plus-`n` and extracted an empty `RandomGen_randomInt()` body.
- Correct the newline delimiter in the existing workflow checker. No production/test logic changed by this repair.

---

## 2026-11-06 14:43 CST — Keep RandomGen integer output nonnegative

### Baseline
- Main commit `9dee1a29a5b67945dfbc13afa174a459bdf10739`; SkipDBv2 Regression run #274 passed source contracts, configure, full build, and CTest.

### Defect and fix
- `RandomGen_randomInt()` cast an unsigned 32-bit MT output directly to signed `int`, making values above `INT_MAX` implementation-defined and negative on common 32-bit-int targets. This contradicted the generator's retained 31-bit integer convention.
- Shift the generated word right once before conversion, matching the canonical MT19937 `genrand_int31` mapping and guaranteeing a value in `[0, 0x7fffffff]`.

### Regression and contracts
- Strengthen the existing deterministic-seed lifecycle test to reject any negative integer output while continuing to verify identical sequences from identical seeds.
- Update the existing workflow contracts to require the 31-bit mapping and negative-result regression. No new source file or test target.

### Validation
- Structural preflight checks the public integer wrapper, explicit nonnegative assertions, and the regression contract.
- The workflow triggered by this commit is authoritative for compilation and CTest.

---

## 2026-11-06 14:40 CST — Correct PortableStrptime day-of-year semantics

### Baseline
- Main commit `ef13522983552d59acb6335b670b09ba0c826bb3`; SkipDBv2 Regression run #273 passed source contracts, configure, full build, and CTest.

### Defect and fix
- The day-of-year directive `%j` accepted `000`, values above the valid day-of-year range could be handled inconsistently, empty/space-only input could silently succeed, and valid ordinals were stored directly in `tm_yday` even though `struct tm` uses zero-based values.
- Require an ordinal from 1 through 366 and store `i - 1`; absent, blank, zero, and out-of-range input now fail.

### Regression and contracts
- Extend the existing `portable_strptime_bounds_test` for ordinals 001, 060, and 366 plus empty, space-only, 000, and 367 input.
- Strengthen existing workflow contracts for the directive's range/conversion and each key boundary case. Existing CMake test target is reused.

### Validation
- Structural preflight checks the day-of-year branch, zero-based conversion, and regression assertions.
- The workflow triggered by this commit is authoritative for the complete compile and test result.

---

## 2026-11-06 14:37 CST — Correct Date source-contract delimiter

### CI contract repair
- Structural review found the new embedded Python check searched for a literal backslash-plus-`n` instead of the newline preceding the end of `Date_asString()`. This would make its extracted function body empty and fail preflight regardless of the C implementation.
- Correct the delimiter in the existing workflow contract; production code and regression behavior are unchanged.

---

## 2026-11-06 14:36 CST — Harden Date string formatting bounds

### Baseline
- Main commit `cbafd3a806e605b1a1ca6c7c63144bea3446180f`; SkipDBv2 Regression run #271 passed source contracts, configure, full build, and CTest.

### Defect and fix
- `Date_asString()` dereferenced `self`, `format`, `localtime()`, and the newly allocated output without validating them. It allocated according to the format length but always told `strftime` the buffer was only 1024 bytes, then used `strlen` on the buffer instead of the formatter's reported length.
- Validate inputs and conversion results, guard capacity arithmetic, verify the UArray allocation before writing, pass the actual available capacity to `strftime`, and size the result from the returned byte count.

### Regression and contracts
- Add `date_string_bounds_test` for NULL arguments, a deterministic year format, and a 1300-byte literal format.
- Register the test in CMake and extend workflow source contracts for validation order, long-format coverage, and CTest registration.

### Validation
- Structural preflight checks argument/time validation, overflow guard, allocation/storage validation, formatter capacity, and output sizing in order.
- The workflow triggered by this commit is authoritative for compilation and runtime validation.

---

## 2026-11-06 14:33 CST — Bound Quicksort recursion depth

### Baseline
- Main commit `69fbc8719bbe701c3de954d7ffdaba18806bb253`; SkipDBv2 Regression run #270 passed source contracts, configure, full build, and all CTest cases.

### Defect and fix
- `Sorting_quickSort_` recursively processed both partitions. An unfavorable partition sequence could consume O(n) C stack frames even though the public entry point already bounds indices to `int`.
- Recurse only into the smaller partition and loop over the larger partition. This bounds active recursion depth to O(log n) without adding a new algorithm layer or changing comparator/swap APIs; worst-case comparison time remains unchanged.

### Regression and contracts
- Extend the existing `sorting_boundaries_test` with a 4096-element reverse-order input.
- Update the existing workflow source contracts to require the smaller-partition recursion/large-partition iteration pattern and prevent a return to two recursive calls.
- Reuse the existing CMake target; no new files or public symbols.

### Validation
- Structural preflight checks the recursion structure and the retained `INT_MAX` index guard.
- The workflow triggered by this commit is authoritative for full compilation and the regression suite.

---

## 2026-11-06 14:30 CST — Reject one-past-end Stack cursors

### Baseline
- Main commit `e9827701787634511dd0641ee5e644eac63ed28c`; SkipDBv2 Regression run #269 passed source contracts, configure, full build, and all 41 CTest cases.

### Defect and fix
- Several inline readers accepted `top == memEnd`, even though `memEnd` is one-past the allocated slots. Iteration could dereference that out-of-bounds cursor; size/index queries could report capacity as logical contents; `Stack_popMarkPoint_` could report success without making progress.
- Fail closed in `Stack_do_`, `Stack_doUntilMark_`, `Stack_totalSize`, `Stack_at_`, and `Stack_popMarkPoint_` when the allocation end is NULL/inconsistent or the cursor is at/beyond it.

### Regression and contracts
- Extend the existing `stack_lifecycle_test` with a shallow malformed-stack view whose `top` equals `memEnd`; check safe size/index/mark-query behavior and that iterators call no callbacks.
- Strengthen workflow contracts to verify each affected function contains the boundary guard and that the one-past-end regression remains present.
- Reuse the existing CMake test target; no new source or build target is introduced.

### Validation
- Structural preflight confirms all five targeted inline functions independently reject the invalid cursor.
- The workflow triggered by this commit must confirm the full remote compile and regression suite.

---

## 2026-11-06 14:27 CST — Correct case-sensitive UArray search fixture

### CI diagnosis and repair
- SkipDBv2 Regression run #268 compiled all targets and passed workflow source contracts; CTest failed only in `uarray_search_bounds_test`.
- The fixture searched for lowercase `ab` at offset 3, while the source contains uppercase `Ab`. Correct both local test fixtures to `{'A', 'b'}`; retain the production API's intended case-sensitive comparison.
- Add a workflow source contract for the exact fixture to prevent this mismatch from recurring.

### Scope
- Test and contract correction only. No production code or new files added.
- Re-run the full remote build and all regression tests; the next CI result decides whether the branch is green.

---

## 2026-11-06 14:29 CST — Harden UArray search input validation

### Baseline
- Main commit `dee96dddfd7ca32d1ba027cdc2f13eb4a3688ebb`; SkipDBv2 Regression run #267 passed source contracts, configure, full build, and CTest after the UArray character-storage fix.
- Scope is limited to existing UArray search primitives and a focused regression.

### Changes
- Add one private validator for NULL arrays, unsupported item types, mismatched item-size metadata, NULL-backed nonempty arrays, byte-size overflow, and search indices that cannot be returned as `long`.
- Apply it consistently to forward/reverse substring search, case-insensitive search, scalar value search, and offset wrappers before constructing a stack range or entering typed iteration.
- Avoid creating an empty stack range at offset zero or at the end of a source, preventing empty-array wrappers from relying on pointer arithmetic over NULL.
- Preserve public signatures and existing valid-input matching behavior; no new production source module or public symbol is introduced.
- Add focused tests for valid forward/reverse/case-insensitive/scalar searches and NULL, invalid storage metadata, unsupported types, empty input, and oversized lengths.

### Build and CI contracts
- Register `uarray_search_bounds_test` in the existing CMake/CTest suite.
- Extend existing workflow source contracts for validator invariants, every search entry-point guard, test cases, and the test's link/registration contract.

### Validation
- Structural preflight confirms every targeted public search implementation uses the private validator and the new test target is present in both CMake and workflow contracts.
- The last main workflow (run #267) passed the source-contract preflight, CMake configure, full compilation, and CTest. The workflow triggered by this commit remains authoritative for this change.

---

## 2026-11-06 14:26 CST — Correct UArray character storage contract

### CI repair
- Run #266 failed at compile time: `UArray` has no `memSize` field; unlike List, its public structure exposes no allocation-capacity value.
- Remove the invalid field reference and the impossible capacity test. Keep guards for NULL backing data, supported one-byte item types, exact byte item size, and size-to-byte multiplication overflow.
- Retain regression coverage for NULL receivers, signed high-byte ctype inputs, Boolean predicate results, and refusing to reinterpret wider numeric arrays as byte strings.

### Validation
- Confirmed the build error points directly to the new `UArray_character.c` guard.
- CMake target and source-contract checks remain registered; the next CI run must confirm the compile and test path.

---

## 2026-11-06 14:24 CST — Harden UArray byte-character operations

### Scope and changes
- Guard ctype helpers against NULL arrays, NULL backing storage, unsupported item types.
- Pass only `unsigned char` values into the C ctype API, avoiding undefined behavior from negative signed bytes or out-of-range numeric elements.
- Normalize predicate outputs to explicit 0/1 bytes; preserve `tolower`/`toupper` mapping results and invalidate UArray cached state after mutation.
- Keep wide numeric arrays untouched instead of accidentally treating UTF-16/32 code units as locale bytes.

### Regression and contracts
- Add `uarray_character_ctype_test` for Boolean predicate results, ASCII case mapping, signed high-byte input, NULLs, invalid backing storage, and wide-type no-op behavior.
- Register it with CMake/CTest and strengthen workflow source contracts for guards, safe ctype arguments, cache invalidation, test cases, and target registration.
- Baseline main CI run #265 passed the UTF conversion fix on commit `778b8b9`.

---

## 2026-11-06 14:17 CST — Fix platform-dependent UTF-32 width

### CI root cause and repair
- Run #264 shows conversion status 0 while the input UArray remains UTF-8, with item type uint8 and size 3. Inspection traced the failure to `ConvertUTF.h` declaring `UTF32` as `unsigned long`: that is 64 bits on LP64 systems, but UArray's `CTYPE_uint32_t` storage is 32 bits, so `UArray_asUTF32()` rejects the mismatched item size before conversion.
- Define UTF32/UTF16/UTF8 using `uint32_t`/`uint16_t`/`uint8_t`, matching the actual fixed-width storage and preventing ABI-dependent output sizes or writes.
- Keep the diagnostic regression assertion until CI confirms the Euro and supplementary-plane cases pass.

### Contracts
- Extend the workflow source-contract dictionary and embedded Python checks to require the fixed-width ConvertUTF typedefs.
- Re-run configure, full build, and all regression tests; do not claim a green result before the new run completes.

---

## 2026-11-06 14:14 CST — Instrument UTF-32 conversion regression

### CI diagnosis
- Run #263 built successfully, but the Euro-code-point assertion remained red after cursor-based output sizing.
- Replace the generic test failure with a structured diagnostic reporting conversion status, encoding, item type/size, logical length, data pointer, and first converted value. This distinguishes conversion failure, metadata mismatch, incorrect buffer length, and scalar-content errors without changing runtime implementation.

### Validation
- The other 38 regression tests passed in run #263.
- Use the next CI result to make the smallest evidence-led production fix.

---

## 2026-11-06 14:06 CST — Fix UTF-32 converted-length regression

### CI repair
- SkipDBv2 Regression run #262 passed source contracts, configure, and build, but `uarray_encoding_conversion_test` exposed that UTF-32 conversion could retain the preallocated capacity as its logical size.
- After successful conversion, derive the actual item count from `targetStart`, shrink and verify the output before NUL truncation. Apply the same cursor-based sizing to UTF-16 so both paths avoid depending on unused zero-filled capacity.
- The original UArray remains untouched if conversion or output resizing fails.

### Validation
- Keep the regression for the three-byte Euro code point and supplementary-plane UTF-32 value.
- Add workflow source contracts for cursor-derived size updates and resize verification; rerun the full CI build and CTest.

---

## 2026-11-06 13:52 CST — Propagate UArray encoding conversion failures

### Scope and changes
- Make UTF-16/UTF-32 truncation report status and fail conversion if shrinking output to its terminator position did not complete.
- Guard all three in-place encoding conversion wrappers against NULL conversion results, preserving the original UArray after conversion/allocation failure.
- Make `UArray_convertToFixedSizeType()` verify resulting encoding, item type, item size, and storage bounds before returning success.

### Regression and contracts
- Add `uarray_encoding_conversion_test` for ASCII, two-byte UTF-8 to UTF-16, three-/four-byte UTF-8 to UTF-32, NULL wrapper inputs, and invalid-sequence source preservation.
- Register the test in CMake/CTest and extend source contracts for truncation failure, wrapper NULL guards, result metadata, and test registration.
- Baseline main CI run #261 passed List slicing, source contracts, configure, build, and CTest.

---

## 2026-11-06 13:49 CST — Fix List CMake contract variable scope

### CI repair
- List slicing run #260 failed in the workflow's source-contract preflight before CMake configure, build, or CTest.
- The new List test-registration check referenced `cmake_text` before the existing later assignment. Read the CMake file into a local `list_slice_cmake` variable at the check site.
- No C implementation, test expectation, build configuration, or runtime behavior changed in this CI-only repair.

### Validation
- Confirmed the failure log's `NameError: name 'cmake_text' is not defined` and the check's ordering relative to the existing later assignment.
- Re-run the full main workflow after this minimal variable-scope fix.

---

## 2026-11-06 13:46 CST — Make List slicing allocation-free

### Scope and changes
- Replace the temporary-List implementation of `List_sliceInPlace()` with bounded in-place compaction using `memmove` and clearing of discarded pointer slots.
- Preserve inclusive end-index behavior, negative-index wrapping, empty/reversed ranges, out-of-range starts, and existing List allocation capacity.
- Validate logical size against both byte-size overflow and allocated capacity before touching storage; remove slicing's temporary-allocation failure path.

### Regression and contracts
- Add `list_slice_test` for middle slices, end-bound clamping, negative indexes, start-at-size and reversed ranges, cleared tail slots, and capacity preservation.
- Register the regression with CMake/CTest and extend workflow contracts for the public prototype, allocation-free implementation, regression cases, linkage, and test registration.
- Latest main CI before this change: SkipDBv2 Regression run #259 passed after the RandomGen contract repair.

---

## 2026-11-06 13:41 CST — Repair RandomGen source-contract syntax

### CI failure and repair
- Baseline commit: `9c957505e56c0c80d02bc00a3eea1fce446b53a1`; SkipDBv2 Regression run #256 failed in the pre-build source-contract step with a Python `SyntaxError`, before CMake configuration, compilation, or CTest.
- The RandomGen index-bound string had accidentally been left outside the `RandomGen.c` entry in the source-contract dictionary, making the embedded Python invalid.
- Move that existing contract string into the `RandomGen.c` needle list. No C implementation, test, CMake target, or workflow behavior was otherwise changed.

### Validation
- Checked the workflow source context and the failure log; this is a contract-script syntax repair, not a RandomGen runtime failure.
- The post-push SkipDBv2 Regression run is authoritative; no local firmware build is claimed.

---

## 2026-11-06 13:28 CST — Guard RandomGen state-index bounds

### Baseline and scope
- Base commit: `9034acaabd07331dd1b55def9d93e2b7c7006a07`; SkipDBv2 Regression run #255 passed.
- Scope is limited to the MT19937 generator's state-index invariant and a dedicated regression test.

### Changes
- Detect negative or excessive `mti` values before indexing the state array and reset malformed state to the generator's documented default seed.
- Preserve normal seeded sequences and the existing `N+1` uninitialized-state fallback.
- Add a regression for both negative and above-sentinel indexes; register it in CMake and the workflow source contracts.

### Validation
- No local firmware build is claimed; the post-push SkipDBv2 Regression run validates compilation and runtime behavior.

---

## 2026-11-06 13:25 CST — Harden BStreamTag NULL handling

### Baseline and scope
- Base commit: `f94e8c15ed38c4ad30ec5e40fbf588342a2f08be`; SkipDBv2 Regression run #253 passed.
- Scope is limited to BStreamTag public helpers and a focused wire-format boundary test.

### Changes
- Make tag encoding, equality, printing, and type-name lookup safe for NULL inputs.
- Keep NULL equality distinct from a valid encoded zero tag.
- Exercise all 256 valid wire-format combinations through encode/decode/equality round trips.
- Register the regression in CMake and extend the workflow's source contracts.

### Validation
- No local firmware build is claimed; the post-push SkipDBv2 Regression run validates compilation and runtime behavior.

---

## 2026-11-06 13:19 CST — Bound Sorting callback indexes and recursion

### Baseline and scope
- Base commit: `a53c09d2e005c833b6baf569d1402f9649c54ae6`; PHash compaction regression run `42167640249` passed.
- Scope is limited to basekit Sorting, one boundary regression, and its CMake/workflow registration.

### Changes
- Treat arrays shorter than two elements as no-ops and reject missing compare/swap callbacks before invoking either callback.
- Enforce the callback's `int` index contract; reject arrays whose largest index exceeds `INT_MAX` instead of narrowing `size_t`.
- Make internal sorting helpers static, retain `size_t` for the pivot, and guard both recursive partitions before unsigned subtraction/addition.
- Add an include guard and document the callback index boundary in `Sorting.h`.

### CI preflight repair
- Initial branch run `42166824493` stopped before compilation because the recursion contract selected the forward declaration rather than the function definition.
- The scanner now skips the declaration and inspects the implementation body. Corrected pre-rebase branch run `42167028310` passed source contracts, configuration, full build, and CTest.

### Validation
- Add CTest coverage for sorted/reverse/duplicate inputs, null callbacks, zero/one-element inputs, and oversized index ranges.
- Register the test in the existing CMake suite and enforce callback/index validation, pivot bounds, test fixtures, and target registration in the workflow.
- The rebased full-tree regression validates this module together with the latest mainline commits.

---

## 2026-11-06 13:15 CST — Implement transactional PHash compaction

### Baseline and scope
- Base commit: `65833c9286ef579db1c253e88442d8271256080e`; SkipDBv2 Regression run #248 validates SHash compaction before this follow-on is published.
- Scope is limited to the existing PHash_compact API stub, one focused regression, and CMake/workflow registration.

### Changes
- Replace the console-printing stub with a best-effort rehash into a smaller power-of-two table.
- Build replacement storage off to the side, retry larger candidate tables if cuckoo placement fails, and publish only after all records and the recorded key count agree.
- Preserve the old table on allocation or placement failure.
- Test capacity reduction, retained key/value mappings, post-compaction insertion, and compacting an empty table.

### Validation
- Add static and dynamic workflow contracts for transactional rehash ordering and CTest registration.
- The post-push SkipDBv2 Regression run is authoritative for compilation and runtime behavior.

---

## 2026-11-06 13:10 CST — Implement transactional SHash compaction

### Baseline and scope
- Base commit: `97dab20bc60c9b687f1eb1041cc270595837d360`; SkipDBv2 Regression run #246 passed source contracts, compilation, and CTest.
- Scope is limited to the existing SHash_compact API stub, one focused regression, and CMake/workflow registration.

### Changes
- Replace the console-printing stub with a best-effort rehash into the smallest practical smaller power-of-two table.
- Build replacement storage separately and publish it only after every live key/value mapping has been reinserted and the key count matches.
- Retry larger candidate sizes when cuckoo placement cannot fit the smaller table; preserve the original table on placement or allocation failure.
- Cover shrinkage, retained lookups, memory reduction, post-compaction insertion, and empty-table compaction.

### Validation
- Add static and dynamic workflow contracts that enforce candidate-before-publish ordering and check the CTest registration.
- The post-push SkipDBv2 Regression run remains authoritative for compilation and runtime results.

---

## 2026-11-06 13:06 CST — Make Datum invalid-storage comparisons consistent

### Baseline and scope
- Base commit: `58c76700616c42cc27539ad7f8448558a4531704`; preceding green SkipDBv2 Regression run #242 passed. DynLib run #245 is being validated.
- Scope is limited to Datum storage validation/comparison semantics, one boundary regression, and its CMake/workflow registration.

### Changes
- Centralize the check for the invalid state `size > 0 && data == NULL`.
- Give malformed Datum values a stable comparison order instead of returning a positive result in both comparison directions.
- Reject malformed storage consistently in prefix/suffix checks, indexing, slicing, hashing, prefix matching, and UArray conversion.
- Add regression coverage for antisymmetry, invalid sizes, NULL values, empty prefixes/suffixes, and normal valid slices.

### Validation
- Extend the existing workflow source contracts to verify comparator ordering, malformed-input guards, boundary fixtures, and CTest registration.
- No local firmware build is claimed; the post-push SkipDBv2 Regression run is authoritative for compilation and execution.

---

## 2026-11-06 13:05 CST — Harden DynLib allocation, loader errors, and handle lifecycle

### Baseline and scope
- Base commit: `7b881e7c61274ce5b4438fa0508dd8e33f20ae0e`; the latest mainline SkipDBv2 Regression run #241 passed.
- Scope is limited to the basekit DynLib implementation/API, one lifecycle regression, its CMake registration, existing workflow source contracts, and this handoff log.

### Changes
- Make construction and string replacement allocation-safe; copy replacement strings before publishing and freeing the previous value, including when the input aliases the current string.
- Make public accessors, setters, open/close, error, and symbol lookup paths safe for NULL receivers and invalid inputs.
- Correct the POSIX loader-error sequence around `dlopen`, `dlsym`, and `dlclose`; capture transient error messages into DynLib-owned storage.
- Keep repeated open/close idempotent, release a newly opened handle when configured init lookup fails, and close the OS handle even when the configured free callback cannot be resolved.
- Ensure object destruction releases an open OS handle without invoking the optional plugin free callback, preserving the existing destructor behavior.
- Add the existing error-update/query functions to the public header declarations.

### Validation
- Add a focused CTest for NULL safety, aliased string replacement, load failure/recovery, failed init lookup cleanup, repeated open, symbol lookup recovery, failed free callback cleanup, and destruction with an open handle.
- Register the test against the existing basekit source glob and link the platform dynamic-loader library through `CMAKE_DL_LIBS`.
- Extend existing workflow source contracts to verify transactional string ownership, loader error ordering, handle cleanup, public declarations, regression coverage, and CMake registration. Remote Linux regression is authoritative for compilation and execution.

---

## 2026-11-06 12:47 CST — Make MainArgs argument replacement transactional

### Baseline and scope
- Base commit: `3a1231c04acfda8b44fa13ffc5b3dd3ac0fc3d21`; SkipDBv2 Regression run #241 passed source contracts, full compilation, and CTest.
- Scope is limited to MainArgs argument ownership/replacement, its API contract comment, one lifecycle regression, and the existing CMake/workflow registrations.

### Changes
- Make `MainArgs_free(NULL)` safe.
- Validate receiver, argument count, vector, and individual argument pointers before dereferencing.
- Allocate and deep-copy the replacement vector off to the side; publish it only after every allocation succeeds, then free the old snapshot.
- Reject negative/overflowing counts and preserve the previous state on invalid input or allocation failure.
- Remove the misleading comment that described copied strings as borrowed references.

### Validation
- Regression verifies deep-copy ownership, repeated replacement, invalid-input rollback, empty reset, and NULL free.
- Workflow preflight enforces copy-before-publish-before-release ordering; CTest checks runtime behavior.

---

## 2026-11-06 12:44 CST — Unify PortableGettimeofday status semantics

### Baseline and scope
- Base commit: `600892354f789f083657f8ae5f1a1c3d47a89a15`; SkipDBv2 Regression run #240 passed source contracts, compilation, and CTest.
- Scope is limited to the portable gettimeofday adapter and its seconds helper, one regression, and the existing CMake/workflow registrations.

### Changes
- Align the Windows compatibility declaration/implementation with POSIX-style integer status returns, matching the Linux API already used by Date.
- Reject a NULL timeval output and propagate `_ftime_s()` failure through `errno` and a -1 result; timezone output remains optional.
- Make `secondsSince1970()` return a defined zero value when the platform clock query fails instead of consuming uninitialized time fields.
- Add a wall-clock regression that compares the portable helper with the system epoch clock.

### Validation
- Static source contracts verify the Windows adapter signature and error propagation; the test is registered in CTest.
- The Linux workflow validates the shared helper and current platform build. Windows-only adapter code is source-checked but cannot be executed in this Linux runner.

---

## 2026-11-06 12:42 CST — Cast format bytes before ctype classification

### Follow-up
- The parser hardening in `cc4efec884bfdde03ff795fbb2f11434788d9761` converted input-byte ctype calls, but format whitespace classification still passed a potentially signed `char` to `isspace()`.
- Cast the format byte to `unsigned char` as well and cover a high-bit format byte in the boundary regression.

### Validation
- The current source contract now requires unsigned-byte handling for both input and format ctype calls; the high-bit format case is registered in the same regression.
- Previous run #239 passed before this narrowly scoped follow-up; the next run validates it.

---

## 2026-11-06 12:40 CST — Harden PortableStrptime input boundaries

### Baseline and scope
- Base commit: `a3c839f2a5e7c45e046668cebbe0d679b0b28847`; SkipDBv2 Regression run #238 passed source contracts, full compilation, and CTest.
- Scope is limited to the custom `io_strptime()` parser and its private parsing helpers, one boundary regression, and the existing CMake/workflow registrations.

### Changes
- Reject NULL input, format, or `struct tm` pointers before parsing.
- Stop treating a depleted input buffer as a successful match while format directives remain; required fields and literals now reach their own validation paths.
- Make digit parsing report “no digits” as failure instead of accepting a zero value for alphabetic input.
- Cast bytes to unsigned char before passing them to `isdigit`/`isspace`, avoiding undefined behavior on non-ASCII input.
- Keep parsing helpers private to this implementation.

### Validation
- Regression covers NULL arguments, valid date/hour parsing, truncated input, missing required fields, alphabetic numeric fields, and a high-bit input byte.
- Workflow contracts verify the guards and helper behavior; remote regression validates compilation and CTest.

---

## 2026-11-06 12:36 CST — Harden Duration lifecycle and component conversion

### Baseline and scope
- Base commit: `3dbe670ffa029b3729edfb83ec114412201431f1`; SkipDBv2 Regression run #237 passed source contracts, full compilation, and CTest.
- Scope is limited to the Duration value type, its existing public clone declaration, one lifecycle regression, and the existing CMake/workflow registrations.

### Changes
- Implement the already-declared but missing `Duration_clone()` and propagate constructor allocation failure safely.
- Guard copy, compare, free, seconds access, math, formatting, and component operations against NULL receivers/operands.
- Bound year extraction before integer conversion so NaN and values beyond the `int` year range cannot trigger undefined floating-to-integer conversion.
- Return cleanly when formatted-string allocation fails; keep ordinary finite duration behavior unchanged.
- Add tests for clone/copy, formatting, math, NULL handling, and extreme/non-finite input.

### Validation
- Workflow source contracts verify the existing public clone declaration has a concrete failure-safe implementation and that component/formatting guards precede unsafe operations.
- The new test is registered in CTest. The next remote regression run is authoritative for compilation and execution.

---

## 2026-11-06 12:29 CST — Remove newline-sensitive Date source contract

### CI preflight repair
- Commit `cdd6dc2330a52a2015486d49811639a7834fe3c7` added Date allocation and clock-read guards, but its static contract used an escaped-newline match for the timezone setter that was encoded incorrectly.

### Repair
- Replace that match with function-range and statement-order checks for `Date_new()` and `Date_now()`; verify the timezone setter guard by its statement token.
- No Date implementation, regression test, or CMake target changed.

### Validation
- The source contract no longer depends on escaped line breaks and checks the constructor guard precedes initialization and the Date_now guard precedes state writes.
- Run #236 was active at the time of the repair; the next mainline run is authoritative.

---

## 2026-11-06 12:28 CST — Guard Date allocation and clock initialization

### Baseline and scope
- Base commit: `e20c9f5cef0342af933916c8dac531cb913aa576`; SkipDBv2 Regression run #235 passed source contracts, full compilation, and CTest.
- Scope is limited to Date construction and wall-clock initialization, one regression, and its CMake/workflow registration.

### Changes
- Return NULL from `Date_new()` if allocation fails instead of passing a NULL object into `Date_now()`.
- Leave Date state untouched when `Date_now()` receives NULL or the system clock query fails.
- Return a defined zero timestamp if the wall-clock query fails; make the local-timezone setter safe for NULL receivers.
- Add a focused test for NULL receivers, construction, and successful wall-clock initialization.

### Validation
- Source contracts verify allocation-before-initialization and the clock/receiver guards.
- The new regression is registered in CTest and the workflow source contract; no Date structure or public signature changed.

---

## 2026-11-06 12:26 CST — Include the declaration for NULL in ConvertUTF

### CI repair
- Run #234 reached compilation and reported `NULL` undeclared in `isLegalUTF8Sequence()`; `ConvertUTF.c` did not include a header that guarantees the definition.
- Add the standard `<stddef.h>` include directly to the implementation and pin it in the existing source contract.

### Validation
- The boundary logic and test remain unchanged.
- Run #234 is red at compilation; the following mainline run validates the include fix and full regression suite.

---

## 2026-11-06 12:25 CST — Bound-check standalone UTF-8 sequence validation

### Baseline and scope
- Base commit: `a5baaaaf3904db8ec920ae3bb87828a837fe30ad`; SkipDBv2 Regression run #233 passed source contracts, full compilation, and CTest.
- Scope is limited to `isLegalUTF8Sequence()` in `ConvertUTF.c`, one boundary regression, its CMake/workflow registration, and this handoff log.

### Changes
- Reject NULL pointers and empty/reversed ranges before reading the first byte.
- Compare the remaining source length against the expected sequence length using pointer distance, avoiding construction of an out-of-range end pointer.
- Cover ASCII, valid 3/4-byte sequences, NULL/empty ranges, truncation, overlong encodings, and illegal leading bytes.

### Validation
- Workflow source contracts enforce guard ordering and all malformed-input fixtures.
- No conversion API signatures or Unicode tables changed; the next remote regression run validates compilation and CTest.

---

## 2026-11-06 12:23 CST — Make RandomGen source preflight independent of escaped newlines

### CI preflight repair
- Commit `4c6aad0a10aa6eaf318acbe6b82fd7301ab0789d` contained the RandomGen runtime hardening and regression, but its source-contract scanner encoded C line breaks as Python string escapes incorrectly.

### Repair
- Replace the newline-sensitive constructor check with explicit function-range and statement-order checks.
- Verify the Gaussian NULL and zero-radius checks occur before the logarithm, without relying on escaped line delimiters.
- No C source, test, or CMake target changed.

### Validation
- Static inspection confirms the contract compares source positions for the constructor guard, state initialization, zero-radius rejection, and logarithm.
- Run #232 was active when corrected; the following mainline run validates the full suite.

---

## 2026-11-06 12:22 CST — Harden RandomGen lifecycle and Gaussian rejection

### Baseline and scope
- Base commit: `ff0e5b6ebfa16c57cd2641641be4cbbb9d0644b0`; SkipDBv2 Regression run #231 passed source contracts, full compilation, and CTest.
- Scope is limited to the `RandomGen` lifecycle and Gaussian sampling in `RandomGen.c`, one regression, its CMake/workflow registration, and this handoff log.

### Changes
- Handle allocation failure in `RandomGen_new()` before touching the state.
- Make seed selection, state freeing, and scalar random APIs safe for NULL handles.
- Reject the zero-radius Box–Muller sample before evaluating `log(0) / 0`, preventing a rare NaN result.
- Add deterministic-seed, output-range, NULL-handle, and finite-Gaussian regression coverage.

### Validation
- Source contracts require constructor/entry-point guards and the zero-radius rejection condition; the regression is registered as a standalone CTest linked against the existing basekit library.
- No local firmware build is claimed; the post-push workflow validates compilation and tests.

---

## 2026-11-06 12:20 CST — Correct allocator preflight function-body delimiters

### CI preflight repair
- Commit `f3cd5d293a66cef002d1ccbdce05fe34fc8c650e` added the isolated `IO_CHECK_ALLOC` lifecycle test and allocator guards.
- The preflight source scanner repeated the escaped-newline typo in its realloc/calloc body delimiters, which would stop the workflow before compilation.

### Repair
- Correct both Python string literals to search for the actual newline preceding each C function's closing brace.
- No allocator implementation, test behavior, or CMake registration changed.

### Validation
- Verified both source-contract expressions contain one escaped newline and no double-escaped delimiter.
- Run #230 was queued when this repair was pushed; the next mainline run validates the complete allocator test and suite.

---

## 2026-11-06 12:20 CST — Preserve tracked allocations across allocator failures

### Baseline and scope
- Base commit: `85dac34af4ac8dce7a45fb682cd087d9c8d18540`; SkipDBv2 Regression run #229 passed source contracts, compilation, and CTest.
- Scope is limited to the `IO_CHECK_ALLOC` implementation in `Common.c`, one isolated allocator lifecycle regression, its CMake/workflow registration, and this handoff log.

### Changes
- Make the allocation-tracker header layout valid on both 32-bit and 64-bit targets.
- Reject allocation-size addition and calloc multiplication overflow before calling the system allocator.
- Keep the original tracked block linked and accounting unchanged when realloc fails; return NULL rather than dereferencing a failed tracker allocation.
- Make `io_free(NULL)` a no-op and prevent `cpalloc()` from copying through a failed allocation.
- Add an isolated `IO_CHECK_ALLOC` CTest target covering overflow rejection, failed and successful realloc, NULL free, copy allocation, and counter recovery.

### Validation
- Static preflight verifies realloc restores the old list node on failure and calloc validates multiplication before allocation.
- The new test compiles `Common.c` independently with `IO_CHECK_ALLOC` to exercise the otherwise-disabled debug allocator without linking duplicate symbols from `skipdb`.
- No local firmware build is claimed; the next remote regression run is authoritative for the allocator test and complete suite.

---

## 2026-11-06 12:16 CST — Correct UArray byte-size preflight newline matching

### CI finding
- Commit `3af474e575200beb183278a34e0a2df594f5b3d1` added the UArray byte-size regression and source contract, but the contract scanner used a double-escaped newline delimiter when isolating the function body.
- This would make the preflight locate no closing brace and fail before compilation; it is a workflow-contract defect, not a C failure.

### Repair
- Correct the Python source-contract delimiter to match the actual newline before the function's closing brace.
- No C implementation, test, CMake registration, or public declaration changed.

### Validation
- Static inspection confirms the Python string now contains the single escaped newline needed to search the source text.
- The original run #228 was in progress at the time of repair; the next mainline regression run is authoritative.

---

## 2026-11-06 12:16 CST — Guard UArray byte-size queries against invalid state and overflow

### Baseline and scope
- Base commit: `ca58a60d832a1dafc87657114ca5db3cd795d2a3`; SkipDBv2 Regression run #227 passed source contracts, compilation, and CTest.
- Scope is limited to `UArray_sizeInBytes()`, one focused basekit regression, its existing CMake/workflow registration, and this handoff log.

### Changes
- Return zero for a NULL UArray, a zero item size, or a size/item-width multiplication that would overflow `size_t`.
- Keep valid and empty-array behavior unchanged; no public API or structure changes.
- Add regression coverage for NULL, valid, empty, zero-item-size, and overflowing metadata states.
- Register the regression in the existing SkipDBv2 CTest suite and enforce its source/build contract in the existing workflow.

### Validation
- Static preflight verifies all invalid-state and overflow guards precede the byte multiplication and checks that every edge case is represented in the regression.
- Existing `BASEKIT_SRC` glob already compiles `UArray.c`; the new test is explicitly registered in CMake and the workflow source contracts.
- No local firmware build is claimed; the post-push SkipDBv2 Regression run is authoritative for compilation and CTest.

---

## 2026-11-06 11:15 CST — Detach borrowed UArray storage before resizing

### Baseline and scope
- Base commit: `1627ac42d4bce61a8de80511381fbff48b3cf0c3`; SkipDBv2 Regression run #226 passed source contracts, compilation, and CTest.
- Scope is limited to `UArray_setSize_()`, one borrowed-buffer resize regression, its CMake/workflow registration, and this handoff log.

### Changes
- Fix resizing of stack-allocated/borrowed UArray buffers: allocate owned storage and copy the surviving bytes instead of passing a borrowed pointer to `realloc`.
- Bound the copy to the smaller of old and new byte lengths, so both growth and shrink operations are safe.
- Clear the borrowed-storage ownership flag only after the new allocation succeeds; preserve the original source buffer and leave the UArray unchanged on allocation failure.
- Add a dedicated regression for both growing and shrinking borrowed byte arrays, including source-buffer preservation and ownership transition.

### Validation
- Static preflight verifies the borrowed-buffer allocation branch precedes the dynamic `realloc` path, bounds the copied range, and is covered by the new test.
- Register the test in CMake and require the implementation, regression, and registration through the workflow's source contracts.
- No local firmware build is available; the next remote SkipDBv2 Regression run is authoritative.

---

## 2026-11-06 11:12 CST — Report List allocated capacity in memorySize

### Baseline and scope
- Base commit: `3c251bf96817d786cd8e91020a0f89bbe4edcb31`; SkipDBv2 Regression run #225 passed source contracts, compilation, and CTest.
- Scope is limited to `List_memorySize()`, one focused List regression, its CMake/workflow registration, and this handoff log.

### Changes
- Correct `List_memorySize()` to report the List object plus its allocated backing-buffer capacity (`memSize`), not only the number of live elements (`size`).
- Base overflow protection on the byte-sized allocation field, avoiding an incorrect capacity calculation and guarding the object-plus-buffer sum.
- Add a dedicated regression for a new empty List, a grown List with spare capacity, and a compacted List.
- Register the test in the existing CMake suite and enforce the implementation, test, and build registration through the existing workflow source contracts.

### Validation
- Static preflight confirms the implementation uses the allocated byte count and the regression exercises all three allocation states.
- No local firmware build is available; the next remote SkipDBv2 Regression run is authoritative for source contracts, compilation, and CTest.

---

## 2026-11-06 11:06 CST — Keep BStream reads terminal after the first error

### Baseline and scope
- Base commit: `f7036b994d97255eb818b3080e2e5e54e37df770`; SkipDBv2 Regression run #224 completed successfully.
- Scope is limited to BStream's raw byte/slice read boundary, the existing `skipdb_record_storage_test`, its workflow source contracts, and this handoff log.

### Changes
- Make `BStream_readUint8()` and `BStream_readDataOfLength_()` honor the stream's latched error before reading or advancing the cursor.
- Validate the byte-array and error-state prerequisites at both raw read entry points; preserve the original error instead of allowing a later raw read to consume bytes after a failed parse.
- Extend the existing record-storage regression: a truncated 32-bit read is followed by byte and slice reads, which must both leave the cursor and original error unchanged.
- The existing CMake target already links `skipdb` (whose basekit source glob includes `BStream.c`) and registers `skipdb_record_storage_test`; no new target or source path is needed.

### Validation
- Static preflight confirms both sticky-error guards precede the first cursor/bounds read, the regression covers both bypass APIs, and workflow contracts verify the source ordering.
- No local firmware build is available; the next remote SkipDBv2 Regression run is authoritative for compilation and CTest.

---

## 2026-11-06 10:49 CST — Fix Stack inline mark declaration order

### CI finding
- Commit `a0b4e8dd67fa2a40150fc0b1d247a06923092072` passed source contracts and CMake configuration in SkipDBv2 Regression run #223, but compilation stopped in `Stack_inline.h`.
- `Stack_pushMark()` called `Stack_pushMarkPoint()` before the latter's inline definition, producing an implicit declaration and conflicting type under the CI compiler.

### Repair
- Move the `Stack_pushMark()` wrapper below the `Stack_pushMarkPoint()` definition so the compiler sees the complete signature before its call.
- No public declaration, behavior, build registration, test, or new file is added.

### Validation
- Static preflight confirms the mark-point definition now precedes the wrapper and the existing contract needles remain satisfied.
- The next SkipDBv2 Regression run will validate compilation and the full CTest suite.

---

## 2026-11-06 10:48 CST — Align Stack mark guard source contract with implementation

### CI finding
- Commit `b54b8cf317bb168a93633a19cf30a3293d83fdeb` triggered SkipDBv2 Regression run #222 (`42152426798`).
- The source-contract step stopped before configure/build because the new Stack contract required the literal `if (mark <= 0)`, while the implementation correctly includes `mark <= 0` in a compound early-return guard. This was a brittle contract needle, not a C compilation or runtime failure.

### Repair
- Relax the workflow needle to match the semantic guard token `mark <= 0`; implementation, test source, CMake registration, and public declarations are unchanged.
- Correct the preceding Stack refactor log timestamp to the commit's actual Beijing time, 10:46 CST.

### Validation
- Static preflight confirms the updated source needle is present in `Stack_inline.h` and no unrelated workflow contract changed.
- The new remote run must pass source contracts, configuration, compilation, and CTest before the refactor is considered green.

---

## 2026-11-06 10:46 CST — Harden Stack storage and mark lifecycle

### Baseline and scope
- Base commit: `bd6b6d60d84ec3808df6a1894be31a1161a2cb81`; main's latest SkipDBv2 Regression run #221 completed successfully.
- Scope is limited to the basekit Stack implementation and inline API, one focused lifecycle regression, its CMake registration, and the existing workflow source contracts.

### Changes
- Make Stack construction, cloning, copying, freeing, compaction, and growth preserve valid storage on allocation failure; failed reallocations no longer discard the owning pointer or leave stack pointers stale.
- Correct Stack_copy_ to copy the source's active items and mark chain, and correct Stack_memorySize to report allocated bytes.
- Guard empty/null stack operations, failed growth during push, invalid mark positions, and the zero-mark pop case before pointer arithmetic or mutation.
- Add a focused regression for repeated growth, copy/clone state, compaction, pop order, nested mark rollback, and invalid zero-mark handling. Register it in the existing CMake test suite and enforce its source/build contract in the existing GitHub Actions workflow.

### Validation
- Static preflight checked all new helpers remain private, the public Stack API declarations are reused, the existing basekit source glob already compiles Stack.c, and the new test is registered exactly once.
- No local full firmware build is available; the pushed SkipDBv2 Regression run is authoritative for source contracts, compilation, and CTest.

---

## 2026-11-06 07:10 CST — Preserve newer PID-cache mappings during stale-record cleanup

### Baseline
- Base commit: `d9f280d948eb7d17df073b179cd9245021231983`; latest SkipDBv2 Regression run #220 passed all source contracts, compilation, and CTests.
- Scope is limited to SkipDB record-cache PID mapping cleanup, its existing workflow source contract, and this handoff log.

### Changes
- Before removing a record's PID mapping during deallocation, verify that the mapping still points to that exact record.
- Prevent freeing an older duplicate record from deleting a newer record's valid cache entry for the same PID.
- Strengthen the existing SkipDBCache source contract; no new files, symbols, build targets, or test registrations.

### Validation
- Static preflight confirms PID removal is conditional on pointer identity and leaves unrelated mappings untouched.
- Remote SkipDBv2 Regression is authoritative for source contracts, compilation, and CTest.

---

## 2026-11-06 06:28 CST — Honor long IPC deadlines across poll slices

### Baseline and scope
- Base commit: `b838732290714585dc9a13d686760fb3717951a6`; SkipDBv2 Regression run #42121310880 passed all source contracts, compilation, and CTests.
- Scope is limited to the monotonic socket-wait helper in `mgr/ipc_io.h` and its existing workflow source contract.

### Changes
- When a poll timeout is capped at `INT_MAX` milliseconds, a zero result now causes the helper to recalculate the monotonic deadline instead of incorrectly reporting the full wait as timed out.
- Preserve immediate timeout once the absolute deadline is reached and preserve errors/interruption handling. No new files, declarations, tests, or build registrations.

### Validation
- Static preflight confirms the loop recomputes monotonic time after every poll timeout, while true deadline expiry still returns zero.
- Remote SkipDBv2 Regression validates source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 06:25 CST — Fix empty-write argument validation ordering

### CI finding
- Commit `1f4960460b623bd2077fd7d547cff8c9fe9fc1b8` built successfully, but `ipc_transport_test` failed because the existing NULL-buffer guard ran before the zero-length no-op path.

### Repair
- Permit a NULL payload only when the requested write length is zero; continue rejecting NULL payloads for non-empty writes and invalid descriptors/negative lengths.
- Keep the zero-length return ahead of clock access and deadline calculation. No new files, declarations, build targets, or source registrations.

### Validation
- Static control-flow review confirms invalid fd/length are rejected first, zero-length writes return next, and all non-empty writes retain the non-NULL buffer requirement. The new remote regression run is authoritative.

---

## 2026-11-06 06:22 CST — Make zero-length IPC writes a no-op

### Baseline and scope
- Base commit: `94c4ef34c278c1cdf4cae5101ffd1b0acbd1ccef`; the latest SkipDBv2 Regression run #42117226127 passed on `main`.
- Scope is limited to zero-length writes in `mgr/ipc_transport.c`, its existing IPC transport regression, and the matching workflow source contracts.

### Changes
- Return success for a valid zero-length write before reading the monotonic clock or constructing a deadline. Empty writes now match the existing exact-read no-op behavior and cannot fail due to an irrelevant clock error.
- Extend the existing socketpair regression to cover a zero-length write with a NULL payload; strengthen the existing source contract for both implementation and test.
- No new files, declarations, build targets, or source registrations.

### Validation
- Static preflight confirms argument validation remains before the zero-length fast path, and the fast path precedes all clock/deadline work. Existing CMake registration is unchanged.
- Remote SkipDBv2 Regression will validate source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 06:00 CST — Make IPC client sockets close-on-exec

### Baseline and scope
- Base commit: `45d7cd39884c1bbb496b60fe038796f6911591a5` (`fix(skipdb): invalidate cursor state on close`); SkipDBv2 Regression run #216 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to Unix-socket descriptor lifecycle in `mgr/ipc_transport.c`.

### Changes
- Create client sockets with `SOCK_CLOEXEC` atomically when supported, falling back on `EINVAL` to a plain socket and an explicit `FD_CLOEXEC` update.
- Close the descriptor if setting close-on-exec fails, while preserving the primary `errno`.
- Preserve the original `connect()` failure across descriptor cleanup.
- Extend the existing Unix-socket regression and CI source contract to assert that returned client descriptors carry `FD_CLOEXEC`. Existing CMake target and source registration are unchanged.

### Validation
- Static preflight confirms the atomic/fallback paths, checked `fcntl` result, error-preserving close paths, and regression assertion.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest; no local build result is claimed.

---

## 2026-11-06 05:55 CST — Invalidate cursor positions when the database closes

### Baseline and scope
- Base commit: `25558bcc2d6e75f3b138ceafa22bb0925c6043bf` (`fix(jfile): reject zero-length journal records`); SkipDBv2 Regression run #215 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to cursor position and traversal state in `skipdb/SkipDBCursor.c`.

### Changes
- Centralized cursor-position invalidation so operations on a closed or detached database clear the borrowed record pointer and both traversal counters.
- `SkipDBCursor_current()` now returns no record while the backing database is closed, rather than exposing a stale logical position.
- Reworked the step bound to compare before incrementing, avoiding signed overflow at the maximum representable step count while preserving the same traversal limit.
- Extended the existing cursor regression to close/reopen the database while positioned and verify that the cursor invalidates and can be repositioned. Existing CMake target and source registration are unchanged.

### Validation
- Static preflight verifies the shared invalidation path, close-state guards, and pre-increment step bound; the existing SkipDBCursor API declarations are sufficient.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest; no local build result is claimed.

---

## 2026-11-06 05:50 CST — Reject impossible zero-payload journal records

### Baseline and scope
- Base commit: `b22e1ff90779a1b21a6e8e8a6baa14bfb2620290` (`fix(jfile): preserve durability failure errors`); SkipDBv2 Regression run #214 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to record-length validation in `jfile/JFileJournalScan.c` and its existing journal regression.

### Changes
- Reject zero-length payload records during journal scanning. `JFile_fwrite()` explicitly refuses zero-byte writes, so such a record cannot be produced by the supported writer and must not be accepted as a committed transaction.
- Added a regression fixture that writes a syntactically terminated, zero-payload record and verifies that `JFile_verifyLog()` rejects it.
- Extended existing scanner and journal-test source contracts. No new files, declarations, CMake targets, or source registrations.

### Validation
- Static preflight confirms the zero-length guard precedes the record-boundary calculation and the regression observes the scanner's public verification result.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest; no local build result is claimed.

---

## 2026-11-06 05:45 CST — Preserve JFile durability failure causes

### Baseline and scope
- Base commit: `1d191bb7f7da0c2141e68ef92383fff4b859dd35` (`fix(skipd): distinguish increment read errors`); SkipDBv2 Regression run #213 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to descriptor validation and failed-stream cleanup in `jfile/JFileSync.c`, using the existing `jfile_journal_test`.

### Changes
- Invalid descriptors in the hard-sync primitive now report `EBADF` deterministically.
- File and journal sync failures close the affected stream through one private cleanup helper that preserves the primary errno across `fclose`.
- Journal terminator corruption reports `EINVAL`; an unexpected EOF during the final-byte read reports `EIO`.
- Journal truncation now clears stale errno before each failing operation, classifies invalid descriptors, and preserves the truncate error during stream cleanup.
- Extended the existing journal regression to check the invalid-descriptor error and strengthened its existing CI source contract. No new files or CMake targets.

### Validation
- Static preflight confirms file, journal, and truncation failure paths use the shared close helper, and that error classification precedes cleanup.
- The existing `JFile.hardSyncFileDescriptor_` declaration in `JFileInternal.h` is reused; the current test target already links the SkipDB library and exposes the jfile include directory.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest; no local build result is claimed.

---

## 2026-11-06 05:40 CST — Distinguish inc/desc database failures from missing keys

### Baseline and scope
- Base commit: `83c09913af82397a5c07cd2fa2d0e94fdb0c55cc` (`fix(skipd): preserve database marker write errors`); SkipDBv2 Regression run #212 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to the increment/decrement read path in `mgr/skipd_command.c`.

### Changes
- Check `SkipDB_isOpen()` immediately after loading the current numeric value.
- A failed database read now returns a database error instead of being misinterpreted as an absent key with value zero; the command does not attempt a follow-up write on a closed database.
- Extended the existing CI source contract to require the database-state check before the missing-key default. No new files, declarations, CMake targets, or source registrations.

### Validation
- Static preflight confirms the check is ordered between `SkipDB_at_()` and the NULL/missing-key branch, with an early coroutine return.
- The next remote SkipDBv2 Regression run is authoritative for source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 05:35 CST — Preserve SkipDB marker-write failure causes

### Baseline and scope
- Base commit: `2029471d4edd73402cceffcba7e9acddc6c70207` (`fix(skipd): preserve pid file validation errors`); SkipDBv2 Regression run #211 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to temporary commit-marker write/error cleanup in `mgr/skipd_database.c`.

### Changes
- A zero-byte marker write now reports `EIO` deterministically instead of leaking a stale `errno`.
- The failure cleanup retains the original write, file-sync, close, or rename error while closing the descriptor and removing the temporary marker.
- Extended the existing CI source contract to enforce the zero-write mapping and error-preserving cleanup. No new files, declarations, CMake targets, or source registrations.

### Validation
- Static preflight confirms one marker-write branch, explicit zero-byte error mapping, and restoration of the primary error after cleanup.
- The next remote SkipDBv2 Regression run is authoritative for source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 04:58 CST — Preserve skipd PID-file validation errors

### Baseline and scope
- Base commit: `26f2384807a5bc5223e8d01decf3df71d8c43fd7` (`fix(skipd): preserve script log validation errors`); SkipDBv2 Regression run #210 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to the parent-process PID-file descriptor validation in `mgr/daemonize.c`.

### Changes
- Split `fstat`, file-policy checks, and permission/truncation syscalls. A genuine `fstat`, `fchmod`, or `ftruncate` failure keeps its system errno; a successfully inspected but unsafe file deterministically reports `EPERM`.
- Retained the existing symlink, regular-file, single-link, and effective-UID policy, plus the existing child termination and parent exit behavior.
- Updated the existing firmware source contract to enforce the explicit error boundary. No new files, declarations, CMake targets, or build-script changes.

### Validation
- Static preflight confirms every failure reaches one cleanup path, while successful validation alone proceeds to truncate and write the PID.
- Remote SkipDBv2 Regression validates source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 04:56 CST — Make skipd script-log validation errors deterministic

### Baseline and scope
- Base commit: `cdb69a2496bc3d4ee3aeed8ec695ef66a6057ec0` (`fix(skipd): reject hard-linked database markers`); SkipDBv2 Regression run #209 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to opening and validating the existing script log descriptor in `mgr/skipd_script.c`.

### Changes
- Split `fstat`, file-policy checks, and `fchmod` so a genuine syscall failure preserves its errno while a successful inspection that rejects file type, link count, or ownership always reports `EPERM`.
- Retained descriptor-based cleanup and the existing fail-closed logging behavior; no path-based reopen or fallback log file was introduced.
- Extended the existing source contract to require the syscall/error boundary and hard-link/ownership policy. No new files, declarations, CMake targets, or build-script changes.

### Validation
- Static preflight confirms the descriptor is closed on every validation failure and the original failure errno is restored after cleanup.
- Remote SkipDBv2 Regression validates source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 04:54 CST — Reject hard-linked skipd database marker files

### Baseline and scope
- Base commit: `dec3b897a26038d1587126a44fbe7c46c8318663` (`fix(skipd): harden instance lock file validation`); SkipDBv2 Regression run #208 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to the existing marker-file validator in `mgr/skipd_database.c`, shared by marker reads and temporary marker writes.

### Changes
- Reject descriptors whose link count is not exactly one, in addition to the existing regular-file and effective-UID checks.
- This prevents a hard-linked `switch.tmp` path from allowing `ftruncate` or permission changes to affect another pathname. The same policy also applies to the durable `switch` marker.
- Policy failures continue to return deterministic `EPERM`; actual `fstat` errors remain untouched.
- Extended the existing manager source contract to require the hard-link guard. No new files, declarations, CMake targets, or build-script changes.

### Validation
- Static preflight confirms the shared validator checks file type, owner, and link count before callers can chmod or truncate.
- Remote SkipDBv2 Regression validates source contracts, compilation, and CTest; no local build result is claimed.

---

## 2026-11-06 04:52 CST — Harden skipd instance-lock file validation

### Baseline and scope
- Base commit: `c9c6eb9add881d44dbe92db63baf9b75d41fcc1b` (`ci(skipd): align read buffer ownership contract`); SkipDBv2 Regression run #207 passed source contracts, configuration, compilation, and all CTests.
- Scope is limited to the instance-lock descriptor validation in `mgr/skipd_socket.c`.

### Changes
- Separated `fstat`, policy validation, and `fchmod` so real system-call errors are preserved while policy rejection deterministically reports `EPERM`; stale `errno` can no longer leak from a successful `fstat`.
- Reject non-regular, foreign-owned, and multiply linked lock files before changing their permissions. The lock file must have exactly one link.
- Extended the existing CI source contract to enforce descriptor validation, hard-link rejection, and error mapping. No new files, declarations, build targets, or CMake changes.

### Validation
- Static preflight confirms the order is `fstat` → policy checks → `fchmod`, with a single cleanup path preserving the selected error.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest; no local compiler/CTest result is claimed.

---

## 2026-11-06 04:50 CST — Align skipd client CI contract with failure-safe buffer growth

### Baseline and failure
- Base commit: `4d49f6a341e67e184a28477631f36e2d8153d91a` (`fix(skipd): preserve client buffer on realloc failure`).
- SkipDBv2 Regression run #206 failed in the initial XY-C1 source-contract scan before configure/build/CTest. The legacy needle `client->origin_len = 0;` described the removed free-before-allocation path and correctly no longer existed after the ownership fix.

### Repair
- Replaced the stale positive source needle with checks for the new `realloc(client->origin, ...)` flow and its post-success pointer assignment.
- Retained the separate negative contract that rejects resetting capacity or freeing the old allocation before reallocation succeeds. Runtime C code is unchanged.

### Validation
- Static preflight confirms the obsolete needle is removed from the positive contract and the new ownership anchors are present.
- The next remote SkipDBv2 Regression run must pass the source-contract stage before compilation and CTest.

---

## 2026-11-06 04:47 CST — Preserve skipd client read-buffer ownership on allocation failure

### Baseline and scope
- Base commit: `582c374c1c7497cfede331044b9134369cd3e18e` (`fix(skipd): stop client read coroutine on terminal errors`); SkipDBv2 Regression run #205 passed source contracts, configuration, build, and all regression tests.
- Scope is limited to read-buffer growth in `mgr/skipd_client.c`, plus its existing CI source contract.

### Changes
- Replaced free-then-malloc with a temporary-pointer `realloc` flow. The existing buffer remains owned by the client when growth fails, so terminal error handling and `client_release()` cannot double-free a dangling pointer.
- Update `client->origin` and its capacity only after successful reallocation; successful reads and frame limits remain unchanged.
- Extended the existing workflow contract to require failure-safe ownership ordering and reject the former free-before-allocation pattern. No new files, declarations, build targets, or CMake changes.

### Validation
- Static preflight confirms exactly one buffer-growth path, a single realloc call, and pointer/capacity updates only after the NULL check.
- Remote SkipDBv2 Regression validates source contracts, compilation, and CTest. No local compiler/CTest result is claimed.

---

## 2026-11-06 04:36 CST — Make client read failures terminal

### Baseline and scope
- Base commit: `30375b11034ee0b1bcf3b65e6102d553f001d516` (`fix(skipd): restore scheduler startup function boundary`); SkipDBv2 Regression run #204 passed source contracts, configuration, build, and CTest.
- Scope is limited to the read-buffer coroutine in `mgr/skipd_client.c`.

### Changes
- Changed invalid frame-length and read-buffer allocation failures from `ccrReturn` to `ccrStop`.
- These paths are terminal errors handled by the parent request coroutine; yielding them left resumable labels inside a failed operation and allowed automatic locals such as `buf_len` to appear live after the stack frame had returned.
- Extended the existing source and semantic contracts to require exactly two terminal error stops and no resumable error return in `client_ccr_read_util()`. No new files or CMake targets.

### Validation
- Static preflight confirms both terminal error branches reset the child coroutine and the dynamic contract isolates the intended function body.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest.

---

## 2026-11-06 04:31 CST — Repair scheduler startup function boundary

### Baseline and failure
- Base commit: `c0ec8e7ecbcb31f6e6de067a36e9760a69c5d103` (`fix(skipd): bound scheduled value lengths before copying`); SkipDBv2 Regression run #203 failed while compiling `skipd_scheduler.c`.
- The compiler reached end-of-file inside `server_init_delay()`. Inspection found a duplicated function signature introduced while adding the `size_t delay_len` declaration.

### Repair
- Removed the duplicate nested `server_init_delay()` signature; retained the original `int n1` parse result and the new `size_t delay_len` byte-span variable.
- Added a workflow source contract requiring exactly one startup delay parser definition. No other module or build target changed.

### Validation
- Static preflight confirms balanced braces and exactly one `server_init_delay()` definition.
- The next remote SkipDBv2 Regression run is authoritative for compilation and CTest.

---

## 2026-11-06 04:17 CST — Keep scheduled-value lengths wide until bounded

### Baseline and scope
- Base commit: `7a4236052c05b8adaae6a3fe1b4d5e4e55b08258` (`refactor(jfile): harden managed file validation`); SkipDBv2 Regression run #202 passed source contracts, configuration, build, and CTest.
- Scope is limited to scheduled-value parsing in `mgr/skipd_scheduler.c`.

### Changes
- Removed pointer-difference narrowing in both timer callbacks and both startup restoration scans.
- Kept delay/time prefix lengths as `size_t` through validation and bounded copy, preventing large or corrupt persisted values from wrapping to a small positive `int` and bypassing fixed-buffer limits.
- Extended CI source contracts to require wide lengths at all parsing sites and reject the previous narrow casts. No new files, test targets, or CMake changes.

### Validation
- Static preflight confirms four size-aware prefix calculations and no narrowing casts remain in the scheduler.
- Remote SkipDBv2 Regression validates compilation and the existing regression suite.

---

## 2026-11-06 04:14 CST — Harden JFile managed-file validation

### Baseline and scope
- Base commit: `463dee5ba382635e9b6dc0fa57bd7f0fa3a8a310` (`fix(skipd): order marker validator before callers`); SkipDBv2 Regression run #201 passed source contracts, configuration, build, and CTest.
- Scope is limited to managed file opening in `jfile/JFile.c` and its existing journal regression.

### Changes
- Centralized post-open inode validation: actual `fstat` failures retain their system error, while non-regular, foreign-owned, or inode-swapped files deterministically return `EPERM`.
- Corrected the concurrent-create revalidation path to return `EINVAL` for a successfully inspected non-regular path instead of leaking stale `EEXIST`.
- Extended the existing journal test to verify both managed files are owned regular files with mode 0600 and that a symlink data path is rejected.
- Added CI ownership/order contracts for the private validator and regression assertions. No new files or CMake source registrations.

### Validation
- Static preflight confirms one private validator definition and one caller; the existing remote regression run will validate compilation and behavior.

---

## 2026-11-06 04:10 CST — Repair skipd marker validator declaration order

### Baseline and scope
- Base commit: `b39c5a78b8c8d30bb275134defe3d054d0933cbd` (`refactor(skipd): unify database marker validation`); SkipDBv2 Regression run #200 stopped at compilation.
- The compiler reported an implicit declaration at the first call to `verify_owned_regular_file_()`, followed by a conflicting static declaration.

### Repair
- Moved the private validator definition above `read_db_switch_marker()`, retaining its single static owner and two existing call sites.
- Added a CI source-order contract so the helper must appear before the first caller. No behavioral changes, new files, or CMake updates.

### Validation
- Static preflight confirms one helper definition, two uses, and definition-before-call ordering.
- The next remote SkipDBv2 Regression run is authoritative.

---

## 2026-11-06 04:08 CST — Unify skipd database marker file validation

### Baseline and scope
- Base commit: `06cc7d3dccd2a1e1f26c280b2d90260efad95087` (`fix(skipdb): preserve byte spans in prefix queries`); SkipDBv2 Regression run #199 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `mgr/skipd_database.c`; no module boundaries or public interfaces change.

### Changes
- Unified the regular-file and effective-UID ownership check shared by switch-marker reads and temporary-marker writes in one private helper.
- Made policy rejection set `EPERM` deterministically instead of leaking a stale `errno`; retained the actual `fstat`, `fchmod`, and `ftruncate` errors.
- Preserved a failed marker read's original I/O error across descriptor cleanup rather than replacing it with `EINVAL`.
- Extended CI source contracts to enforce one helper definition, both call sites, and the read-error path. No new files or CMake source registrations.

### Validation
- Static preflight confirms exactly one private helper definition and two call sites.
- Remote SkipDBv2 Regression remains authoritative for compilation and tests; no local build result is claimed.

---

## 2026-11-06 04:03 CST — Preserve raw byte spans in SkipDB prefix scans

### Baseline and scope
- Base commit: `d6118034ee0a9e78be608a894597b9ac3ccda023` (`refactor(skipdb): centralize record datum validation`); SkipDBv2 Regression run #198 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBPrefix.c`; the existing prefix regression and its CI source contract are extended in place.

### Changes
- Fixed prefix-length interpretation: a trailing NUL is removed only for C-string datums; a non-terminated byte datum now uses its full declared length.
- Kept empty-string prefix behavior, ordered cursor traversal, callback iteration, and public signatures unchanged.
- Added cursor and callback regressions using a two-byte `al` prefix plus an adjacent `apple` key, proving the byte scan does not widen to every key beginning with `a`.
- Updated the existing CI source contract to enforce the byte-span rule. No new files or CMake source registrations.

### Validation
- Static preflight verifies the helper's bounded prefix length logic and both raw-byte regression paths.
- Remote SkipDBv2 Regression is authoritative for compilation and CTest; no local compiler/build result is claimed.

---

## 2026-11-06 02:01 CST — Centralize SkipDB record datum validation

### Baseline and scope
- Base commit: `5805dc4a2b2c1b651953ab59d14779e6afa1fefd` (`refactor(skipdb): centralize read-key validation`); SkipDBv2 Regression run #197 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBRecord.c`.

### Changes
- Centralized the shared validity rule for datum byte spans used by the record key and value setters: empty data is allowed, but non-empty data requires a non-null pointer.
- Preserved each setter's separate record/member checks and left copy ownership, dirty tracking, and accessor behavior unchanged.
- Extended CI source contracts to require one private helper and both setter call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and two uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:59 CST — Centralize SkipDB read-key validation

### Baseline and scope
- Base commit: `cfe282609933a93dfd5a06ed494c3ac0e5f76ba1` (`refactor(udb): reuse index structural validation`); SkipDBv2 Regression run #196 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBSearch.c`.

### Changes
- Centralized the common database/header readiness and key byte-span validation used by `SkipDB_recordAt_` and `SkipDB_at_`.
- Kept caller-specific failure results unchanged (`NULL` for record lookup, `Datum_Empty()` for value lookup); successful search and update-chain handling remain unchanged.
- Updated CI source contracts to require one private helper and two call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and two uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:57 CST — Reuse UDBIndex structural validation

### Baseline and scope
- Base commit: `bd538ec29edb90ce6b8a9efdbf5de4ec11b80caa` (`refactor(udb): unify commit marker file verification`); SkipDBv2 Regression run #195 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBIndex.c`.

### Changes
- Reused the existing `UDBIndex_isValid_` predicate in both the open and commit entry guards instead of duplicating the four-pointer structural check.
- Kept the entry-specific return value and all file-stream, transaction, reservation, and error-handling logic unchanged.
- Extended CI source contracts to enforce use of the shared validator at both lifecycle entry points and assert the expected three in-file call sites (including `UDBIndex_hasFile_`). No new source files, helper declarations, or CMake changes.

### Validation
- Static preflight confirms all three expected calls route through the existing helper; CI checks the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:54 CST — Unify UDB commit-marker file verification

### Baseline and scope
- Base commit: `33a27eb14aefd351dde01ab82020f5c91dcff211` (`refactor(udb): centralize compaction stream checks`); SkipDBv2 Regression run #194 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBCommitMarker.c`.

### Changes
- Centralized the post-open verification shared by the writable commit marker and read-only journal paths: successful `fstat`, regular-file type, effective-UID ownership, pre-open device/inode identity where applicable, and mode 0600.
- Kept path-specific open flags, create-vs-existing handling, fdopen modes, and caller return conventions unchanged; the helper closes the file descriptor on failure and preserves the original system error except for policy rejection (`EPERM`).
- Updated CI contracts to require one private helper and two call sites while retaining checks for no-follow/nonblocking flags and permission hardening. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and two uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:52 CST — Centralize UDB compaction stream checks

### Baseline and scope
- Base commit: `9bce5689ad3856c552e415a99a1be530a19b903d` (`refactor(skipdb): centralize write datum validation`); SkipDBv2 Regression run #193 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBCompact.c`.

### Changes
- Centralized the paired data/journal stream readiness predicate used after the free-space scan and after truncating a trailing hole.
- Preserved the compactor's existing close-on-corrupt-stream behavior and the short-circuit on truncate failure; the helper also safely rejects missing intermediate records/file pointers.
- Updated CI contracts to require one private helper and two call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and two uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:43 CST — Centralize SkipDB write datum validation

### Baseline and scope
- Base commit: `b7d3ccd5e2964137ca3b119d6e1ad99c78296552` (`refactor(skipdb): centralize cursor positioning resets`); SkipDBv2 Regression run #192 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBWrite.c`.

### Changes
- Centralized the rule that a datum is valid when it is empty or has a non-null byte pointer.
- Reused the predicate for key/value inputs in replace and insert/update paths, and for key input in record removal; the transaction-only restriction on replacement remains independent.
- Updated CI source contracts to require one private helper and five call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and five uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:40 CST — Centralize SkipDB cursor positioning resets

### Baseline and scope
- Base commit: `1eb7abd1c4181000ffd41d279b647bb0b49db0ab` (`refactor(udb): unify verified PID-list insertion`); SkipDBv2 Regression run #191 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBCursor.c`.

### Changes
- Centralized the common database-open guard and directional step-counter reset used by `SkipDBCursor_goto_`, `SkipDBCursor_first`, and `SkipDBCursor_last`.
- Kept each operation's distinct record-selection call and return behavior unchanged.
- Updated CI source contracts to require one private helper and its three positioning call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and three uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:38 CST — Unify verified UDB PID-list insertion

### Baseline and scope
- Base commit: `668187832280be705cf97d387c34e2be5e977d15` (`refactor(udb): centralize records stream readiness`); SkipDBv2 Regression run #190 passed all steps.
- This iteration is limited to `udb/UDBIndexFreeList.c`.

### Changes
- Centralized the append-and-verify sequence used when reserving a PID, adding a reusable hole, and rebuilding the free-PID list.
- Added an overflow guard before checking the expected list-size increment; callers retain their PID validation and error handling.
- Updated CI contracts to require one private helper and three append call sites. Existing index-layer tests continue to cover reservation, reuse, and rebuild behavior. No new source files or CMake changes.

### Validation
- Static preflight confirms one helper definition and three uses; CI enforces the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:35 CST — Centralize UDB records stream readiness

### Baseline and scope
- Base commit: `84c8d0b580508445975a0db68b8a588627b63972` (`refactor(udb): centralize record-write state restoration`); SkipDBv2 Regression run #189 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBRecordsStorage.c`.

### Changes
- Centralized the repeated check that both the records data stream and its journal stream remain open.
- Reused the predicate after open/replay operations, during startup validation, when beginning transactions, and after truncation, while preserving the separate deferred-replay and file-size checks.
- Updated CI contracts to enforce one private predicate and seven call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and seven call sites; CI checks the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:32 CST — Centralize UDB record-write state restoration

### Baseline and scope
- Base commit: `705c0b8482680bafb8beec46f41088257c3d76cf` (`refactor(skipdb): centralize cache pointer validation`); SkipDBv2 Regression run #188 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBRecordWrite.c`.

### Changes
- Centralized restoration of the JFile logical cursor position and file-size bound after failed header writes and after successful rollback of a partial record append.
- Reused the helper in record removal, header persistence, and both journal/non-journal rollback paths without changing the existing physical truncate/seek ordering or transaction rollback policy.
- Extended CI source contracts to enforce one private helper and four call sites. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and four call sites; CI checks the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:31 CST — Centralize SkipDB cache pointer validation

### Baseline and scope
- Base commit: `2ef9d32d877b2ffd52ce9617c3df169a00bce020` (`refactor(skipdb): unify record serialization preconditions`); SkipDBv2 Regression run #187 passed all steps.
- This iteration is limited to `skipdb/SkipDBRecordCache.c`.

### Changes
- Centralized the duplicated level-range and pointer-array consistency checks used before cache pointer iteration and reference cleanup.
- Preserved the distinct public behavior for a null record: `SkipDBRecord_pointersAreEmpty(NULL)` still reports empty, while reference cleanup remains a no-op.
- Updated CI source contracts to enforce one private helper definition and two uses. No new source files, public declarations, or CMake source-registration changes.

### Validation
- Static preflight confirms one helper definition and two call sites; CI asserts the same count.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:30 CST — Unify SkipDB record serialization preconditions

### Baseline and scope
- Base commit: `9b14a9d635b42f54738279b8e4fba6ae7d182213` (`refactor(udb): unify free-list position validation`); SkipDBv2 Regression run #186 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `skipdb/SkipDBRecordStorage.c`.

### Changes
- Centralized the common record/stream readiness checks used by serialization and deserialization.
- Unified the supported skip-list level range check in both directions while retaining the serializer's separate pointer-array validation and the deserializer's allocation verification.
- Expanded CI contracts for one private implementation and two call sites per helper. Existing `skipdb_record_storage_test` covers round-trip data, invalid levels, truncated streams, and trailing bytes. No new source files or CMake registration changes.

### Validation
- Static preflight confirms each private helper has one definition and two call sites.
- The next remote SkipDBv2 Regression run is authoritative; no local CMake/compiler/CTest result is claimed.

---

## 2026-11-06 01:28 CST — Unify UDB free-list position validation

### Baseline and scope
- Base commit: `2fb5465112d8b5ee677b636cadb1150447c5a53f` (`refactor(udb): centralize record payload extent checks`); SkipDBv2 Regression run #185 passed source contracts, configuration, build, and CTest.
- This iteration is limited to `udb/UDBRecordsFreeList.c`.

### Changes
- Centralized the physical-boundary predicate for persisted first-empty offsets, requiring a nonzero offset to start after metadata, remain within the file, and contain a complete record header.
- Reused it when recovering a persisted offset and when writing an updated free-list offset; recovery still downgrades invalid stored hints to zero, while explicit invalid writes are rejected.
- Updated source contracts to enforce a single private helper and two call sites. No new source file, public API declaration, or CMake source registration was added.

### Validation
- Static preflight confirms one helper definition and two call sites; CI verifies the same ownership/count invariant.
- Remote SkipDBv2 Regression is authoritative; no local CMake/compiler/CTest result is claimed.

---

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
