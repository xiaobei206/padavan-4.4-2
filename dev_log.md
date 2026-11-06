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
