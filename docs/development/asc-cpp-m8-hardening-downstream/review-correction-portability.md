# Milestone 8 Post-Checkpoint Correction Portability/GPU/Performance Review

Status: final independent re-review complete; all twenty correction findings
are resolved; Publication Checkpoint B accepted

Date: 2026-07-28

Milestone: **Milestone 8 — packaging/API/performance/downstream hardening**

## Review boundary and inputs

This is the independent final portability, GPU, performance, package, security,
license, and provenance review required by the frozen correction contract and
ownership ledger. The candidate was reviewed read-only. This report is the only
repository path written by this role.

The review used:

- `main:AGENTS.md`;
- the Milestone 8 contract, ownership ledger, verification design, production
  review, verification review, documentation/API review, portability review,
  dependency audit, provenance record, and Publication Checkpoint B;
- the frozen review-correction contract and ownership ledger;
- the correction production, independent verification, documentation/API, and
  lead integration records;
- ADRs 0004, 0008--0015, 0017, and 0018;
- the dependency/capability manifests and backend matrix;
- the affected source, tests, CMake/package integration, ABI baselines,
  security text, and performance methodology;
- clean validation artifacts below
  `/tmp/asc-cpp-m8-correction-final.s2jUoW`; and
- the independent default-host compatibility tree
  `/tmp/asc-cpp-m8-port-reopen-default-host.uEEkaz`.

Repository state observed during the review:

```text
repository: /home/yicai/AI4SciComp/asc-cpp
branch: feature/asc-cpp-m8-hardening-downstream-r2
base HEAD/main/origin-main recorded by integration:
  33b261ea33616a6395c4ad3b20646093103344f7
candidate commit: none; cumulative Milestones 0--8 candidate is uncommitted
tag at base HEAD: none
```

No production, test, package, shared root CMake, branch, remote, tag, release,
or external repository state was modified by this review.

## Decision

The initial pass opened one high-severity deletion-safety bypass and three
medium-severity evidence or classification defects. All four were accepted
into the frozen contract as corrections 15--18, implemented, and independently
re-reviewed as resolved.

The compatibility pass then found one medium-severity portability defect in
that amended CUDA-host selector test: it unconditionally consumed CMake
variables introduced in 3.31 even though CMake 3.25 is the candidate minimum.
Independent verification subsequently showed that raw pre-3.31 cache entries
could spoof those variable names. Correction 19 now uses separate trusted
values populated only by CMake 3.31+ detection; real CMake 3.25.3 and 3.30.9
spoof-cache regressions prove an empty non-enforcing baseline.

The final compatibility pass found that the first spoof-cache regression
itself rejected a valid pre-3.31 configuration when NVCC chose its default
host and `CMAKE_CUDA_HOST_COMPILER` was empty. That finding was accepted as
correction 20. The driver now treats the explicit host override as optional,
and the same official CMake 3.30.9 default-host trial passes independently.
No actionable portability, GPU, performance, package, security, license, or
provenance finding remains.

| ID | Severity | Area | Decision |
| --- | --- | --- | --- |
| M8R2-FINAL-PORT-001 | high | guarded recursive deletion | resolved |
| M8R2-FINAL-PORT-002 | medium | exact CUDA ELF environment selection | resolved |
| M8R2-FINAL-PORT-003 | medium | CUDA no-device classification | resolved |
| M8R2-FINAL-PORT-004 | medium | performance evidence and correctness oracles | resolved |
| M8R2-FINAL-PORT-005 | medium | CMake 3.25--3.30 CUDA selector integration | resolved as contract correction 19 |
| M8R2-FINAL-PORT-006 | medium | pre-3.31 NVCC default-host regression | resolved as contract correction 20 |

## Actionable findings

### M8R2-FINAL-PORT-001: three recursive fixture deletions bypass the guard

Severity: **high**

Final re-review: **resolved**.

The three hardening cleanup registrations now place their targets below
`ASC_CPP_TEST_WORKSPACE_ROOT` and invoke
`tests/cmake/RemoveTestWorkspace.cmake`, which calls the execution-time
`asc_cpp_remove_test_workspace()` validator. The guard regression now
recursively scans every test `CMakeLists.txt` and `*.cmake` file for
`file(REMOVE_RECURSE)`, native or `cmake -E` recursive `rm`, and
`remove_directory`; only the guarded helper contains recursive removal.
Independent re-review ran `asc_cpp.test_workspace.guard`: **pass** in 0.05 s.
The lead's fresh CUDA package aggregates passed 2/2 in 601.82 s.

Evidence:

- `tests/hardening/CMakeLists.txt:92-97` registers
  `cmake -E rm -rf` for the copied build package.
- `tests/hardening/CMakeLists.txt:121-124` registers
  `cmake -E rm -rf` for the install prefix.
- `tests/hardening/CMakeLists.txt:153-156` registers
  `cmake -E rm -rf` for the relocated prefix.
- `tests/cmake/test_workspace_guard_test.cmake:84-108` scans only nine named
  `*.cmake` drivers. It requires `asc_cpp_prepare_test_workspace(` and rejects
  only the literal `file(REMOVE_RECURSE` spelling. It does not scan
  `tests/hardening/CMakeLists.txt` and does not reject
  `cmake -E rm -rf`.
- `tests/cmake/PrepareTestWorkspace.cmake:3-164` does provide strict validation
  before its two recursive removals, but the three fixture commands above do
  not call it.
- `review-correction-integration.md:77` therefore overstates that every
  destructive driver uses the guarded helper.

Rigorous reasoning:

The frozen correction requires unsafe targets to be rejected before **any**
recursive removal, including root, source/build roots, parents, traversal, and
symlink escape. The three direct commands rely only on the current values of
derived variables. They have no guard file, strict-descendant, canonical-path,
or symlink validation at execution time. A later refactor, cache-value
override, or fixture-registration error can therefore turn an intended
build-tree cleanup into an unguarded recursive deletion. The regression scan
cannot detect this because both the file containing the commands and the
command spelling are outside its search.

Reproduction:

```sh
rg -n \
  'REMOVE_RECURSE|(^|[[:space:]])-E[[:space:]]+rm[[:space:]]+-rf|rm -rf' \
  tests cmake CMakeLists.txt
```

Observed relevant output:

```text
tests/cmake/PrepareTestWorkspace.cmake:145: file(REMOVE_RECURSE ...)
tests/cmake/PrepareTestWorkspace.cmake:164: file(REMOVE_RECURSE ...)
tests/hardening/CMakeLists.txt:95: "${CMAKE_COMMAND}" -E rm -rf
tests/hardening/CMakeLists.txt:123: COMMAND "${CMAKE_COMMAND}" -E rm -rf ...
tests/hardening/CMakeLists.txt:155: COMMAND "${CMAKE_COMMAND}" -E rm -rf ...
```

Required correction and tests:

1. Put all three cleanup targets below the dedicated guarded test-workspace
   root and execute their removal through
   `asc_cpp_remove_test_workspace()` or an equivalently strict single helper.
2. Keep the guard check immediately before deletion in the CTest process; a
   configure-time-only path assertion is insufficient.
3. Expand the regression to scan every recursive-deletion registration and
   driver, including `CMakeLists.txt`, `file(REMOVE_RECURSE)`, and
   `cmake -E rm -rf` spellings.
4. Add fixture-level rejection cases proving that an overridden root, parent,
   source/build root, normalized traversal, direct symlink, and
   symlink-descendant target preserves sentinels and performs no deletion.
5. Rerun CPU and CUDA package/install/relocation aggregates because these
   fixture paths are shared package integration.

### M8R2-FINAL-PORT-002: the CUDA ELF selector is fed the C++ compiler identity,
not the configured CUDA host compiler identity

Severity: **medium**

Final re-review: **resolved for the CMake 4.1 exact-host path**.

`tests/hardening/CMakeLists.txt:574-575` now supplies CMake's detected
`CMAKE_CUDA_HOST_COMPILER_ID` and
`CMAKE_CUDA_HOST_COMPILER_VERSION`. The nested integration configures GNU as
the ordinary C++ compiler and Clang 19.0.0 as NVCC's host, verifies CMake's
generated compiler record, and requires an empty non-enforcing ELF baseline.
Independent re-review initially ran that integration: **pass** in 34.96 s.
The subsequently discovered minimum-CMake defect is separated and resolved as
M8R2-FINAL-PORT-005.

Evidence:

- `tests/hardening/SelectElfBaseline.cmake:79-80` correctly requires the exact
  supplied CUDA host compiler ID and version for the approved CUDA baseline.
- `tests/hardening/select_elf_baseline_test.cmake:101` exercises a synthetic
  host-version mismatch.
- `tests/hardening/CMakeLists.txt:524-525`, however, passes
  `CMAKE_CXX_COMPILER_ID` and `CMAKE_CXX_COMPILER_VERSION` as
  `CUDA_HOST_COMPILER_ID` and `CUDA_HOST_COMPILER_VERSION`.
- The configuration supports a distinct `CMAKE_CUDA_HOST_COMPILER`; the clean
  CUDA command explicitly sets it. The integration does not interrogate that
  executable for its actual compiler identity/version.
- `review-correction-integration.md:81` consequently overstates that applicable
  CUDA host fields are exact for the integrated selector.

Rigorous reasoning:

`CMAKE_CXX_COMPILER` and the compiler used by NVCC for host compilation may be
different executables or versions. When they differ, the current integration
can pass the approved GCC 11.4 CUDA-host predicate using only the unrelated
project C++ compiler fields. The selector can then enforce a CUDA ELF baseline
on a host toolchain for which that baseline was not recorded. The unit selector
test proves comparison behavior only after values are supplied; it does not
prove that configuration supplies truthful values.

Reproduction:

```sh
rg -n \
  'CUDA_HOST_COMPILER_(ID|VERSION)|CMAKE_CUDA_HOST_COMPILER' \
  tests/hardening CMakeLists.txt cmake
```

The only integrated identity source is the C++ compiler at
`tests/hardening/CMakeLists.txt:524-525`.

Required correction and tests:

1. Resolve the exact executable used as `CMAKE_CUDA_HOST_COMPILER`, including
   the CMake/NVCC default when the cache variable is not explicitly set.
2. Derive and record that executable's compiler ID and full version rather than
   copying `CMAKE_CXX_COMPILER_*`.
3. If the host identity cannot be established exactly, select no enforcing
   baseline and emit an observational/non-enforcing result.
4. Add a configure-level regression with different project C++ and CUDA host
   compiler identities/versions and require that the approved CUDA baseline is
   not selected.
5. Retain the existing pure selector mismatch cases and rerun the exact ELF,
   architecture, ordinary CUDA, and package selections.

### M8R2-FINAL-PORT-003: Dense CUDA benchmark converts device-enumeration errors
to a no-device skip

Severity: **medium**

Final re-review: **resolved**.

`DeviceCountDisposition` now returns 2 and prints the provider status for an
enumeration error, returns 77 only for a successful zero count, and continues
for a positive count. The forced-error test requires exact exit 2 through
`ExpectExit.cmake` and has no skip property. Independent re-review observed
the direct forced-error exit 2 and the registered regression **pass** in
0.06 s; the separate forced-no-device path remains code 77.

Evidence:

- `benchmarks/dense_cuda/benchmark.cc:124-126` returns code 77 for both
  `!count.ok()` and a successful zero device count.
- `tests/dense_cuda/CMakeLists.txt:72-87` classifies code 77 as skipped for the
  normal benchmark and its forced-no-device registration.
- The corrected Core/Dense executable rule documented in
  `review-correction-verification.md:94-96` is that successful zero enumeration
  is skipped while enumeration failures remain failures.
- `review-correction-integration.md:78` explicitly includes the Dense CUDA
  benchmark among the corrected no-device cases, but it does not satisfy that
  error/zero distinction.

Rigorous reasoning:

A provider/runtime/driver enumeration error is materially different from a
successful report of zero available devices. Mapping both to code 77 causes
CTest to hide a broken CUDA runtime or provider integration as environmental
absence. The deterministic forced-no-device seam proves only the intentional
skip path; it does not prove enumeration-error behavior.

Required correction and tests:

1. Return a failing nonzero code when `CudaDeviceCount()` returns an error.
2. Return 77 only after successful enumeration reports zero devices, or when
   the explicit pre-runtime forced-no-device seam is set.
3. Add a deterministic enumeration-failure seam or directly unit-test a small
   shared classification helper. The test must require:
   successful positive count -> continue, successful zero -> 77, provider
   error -> failure other than 77.
4. Rerun both the forced-no-device selection and the normal real-device
   benchmark selection.

### M8R2-FINAL-PORT-004: recorded timing rows do not satisfy the project's own
performance-evidence contract

Severity: **medium**

Final re-review: **resolved**.

Dense and Sparse CPU rows now check full independent expected results. CPU
Dense and Sparse Random reconstruct Philox values, selection, structure, and
offsets independently. Dense CUDA checks each transfer/evaluation/AXPY/GEMV
row and distributed GEMM samples against a row-specific oracle; Sparse and
Random CUDA retain their host-reference, deterministic-bit, structure, and
value oracles. The integration record now supplies OS/CPU/memory/toolchain/
flags/GPU, synchronization/timing boundaries, uncontrolled-noise limits, and
base commit plus the reproducible build-input digest. Independent re-review
recomputed
`e7feae4784079c3edf331940b1ea8373f9c0748d4e20ad49ae0b2cbcae401f69`
exactly and ran the six benchmark registrations: **6/6 pass** in 12.18 s,
with `oracle=independent` on every amended row. The lead's recorded final
observation was 6/6 pass in 11.04 s. All numbers remain threshold-free,
single-host observations without speedup or regression claims.

Evidence:

- `docs/performance.md:14-30` requires exact candidate identity, OS, CPU,
  memory, compiler/standard-library/CMake/build/link/flag details, provider/GPU
  metadata, operation shape, repetitions, timing boundary, independent
  correctness/checksum, status, and known noise. It states that a number
  without this record is not ASCCpp performance evidence.
- The performance section of `review-correction-integration.md` records one-run
  figures but omits CPU and memory identity, relevant compiler/link flags,
  scheduling/frequency/thermal/contention/timer-noise conditions, and a
  content fingerprint for the uncommitted dirty candidate. A base commit plus
  worktree counts does not identify the measured source.
- `benchmarks/dense_cuda/benchmark.cc:294-315` computes one final finite
  checksum after the timed H2D, D2H, D2D, evaluate, AXPY, GEMV, and GEMM
  sequence, then attaches that same checksum to every operation row. It does
  not compare each row's result with an independent expected result.
- The CPU Dense and Sparse probes accept nonzero/finiteness checks in places
  rather than recording independent exact or tolerance-based expected results.
- `review-correction-integration.md` says every probe passed its
  checksum/parity guard, and `review-correction-documentation.md:134-137`
  characterizes the benchmark run as correctness evidence. Those statements
  are stronger than the implemented per-operation oracles.

Rigorous reasoning:

One later finite checksum cannot detect an incorrect earlier transfer or
operation whose result was overwritten before the final readback. It also
cannot attribute correctness to seven distinct timing rows. A nonzero checksum
is not an independent numerical oracle. Separately, an uncommitted candidate
cannot be reproduced from the base commit and porcelain-entry counts.
Therefore the numbers are useful threshold-free execution observations, but
they do not meet the repository's stated definition of performance evidence or
operation-level correctness evidence.

Required resolution, choosing one truthful level:

1. For performance evidence, add independent expected-result checks for every
   reported operation row, with explicit tolerance where floating arithmetic
   is not exact, and capture every field required by `docs/performance.md`.
   Identify the dirty candidate with a reproducible content manifest or diff
   digest.
2. Alternatively, retain the probes but classify the numbers only as
   threshold-free execution smoke observations. Remove claims that every
   operation row is independently correctness/parity checked and do not call
   the figures ASCCpp performance evidence.
3. Add a regression that deliberately corrupts or bypasses each operation
   result and proves its own oracle fails before that row can be reported as
   correctness checked.
4. Rerun the six benchmark registrations after either correction and preserve
   the absence of speedup, threshold, or cross-machine claims.

### M8R2-FINAL-PORT-005: the new CUDA-host integration test is incompatible
with the supported CMake 3.25--3.30 range

Severity: **medium**

Final re-review: **resolved as contract correction 19**.

`tests/hardening/CMakeLists.txt` now registers the exact unlike-host
integration only with CMake 3.31 or newer. On older CMake it instead registers
`cuda_host_elf_selector_unavailable`, which requires empty detected host ID/
version, requires the integrated baseline to be empty, and calls the selector
again with an otherwise exact environment to prove fail-closed non-enforcement.
Separate trusted host ID/version variables start empty and are populated from
CMake's detected fields only on CMake 3.31 or newer. Raw pre-3.31 cache values
are passed only to the unavailable-identity observation and never to the
selector.

Official Kitware CMake 3.25.3 and 3.30.9 each configured the CUDA 12.9.86,
GNU 11.4 host, Release/shared, architecture-86 candidate, built
`asc_core_cuda`, and registered an empty ELF baseline. Independent re-review
inspected each older-CMake three-test selector selection: **3/3 pass** for
3.25.3 in 29.29 s and **3/3 pass** for 3.30.9 in 29.94 s. Those selections
include a nested configure that injects plausible raw GNU 11.4 identity cache
values and proves an empty baseline, no exact-host test, and no generated host
ID/version fields. A separate outer CMake 3.30.9 spoof-cache configure passed
its selector/unavailable pair **2/2** in 0.01 s and emitted the expected
ignored-cache message. The current CMake 4.1.2 workspace retained its trusted
exact-host behavior: workspace guard, selector unit, and exact
GNU-C++/Clang-CUDA-host integration **3/3 pass** in 36.39 s. Exact host
identity enforcement on CMake before 3.31 is truthfully `skipped`; safe
non-enforcing behavior is directly tested.

Evidence:

- `CMakeLists.txt:1`, `README.md:65`, and `docs/support-matrix.md:21` define
  CMake 3.25 or newer as the candidate contract.
- CMake's official
  [`CMAKE_<LANG>_HOST_COMPILER_ID` documentation](https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_HOST_COMPILER_ID.html)
  states that the variable was added in 3.31. The corresponding host compiler
  version variable is part of the same 3.31 facility.
- The locally installed CMake 3.22
  `Modules/CMakeCUDACompiler.cmake.in` likewise contains neither host ID nor
  host version, while the validated CMake 4.1 template contains both.
- `tests/hardening/cuda_host_elf_selector_integration_test.cmake:74-83`
  unconditionally requires the generated `CMakeCUDACompiler.cmake` to contain
  `CMAKE_CUDA_HOST_COMPILER_ID "Clang"` and a 19.x host version.
- `tests/hardening/CMakeLists.txt:389-420` registers that integration whenever
  CUDA and the two local compilers are available; it has no CMake 3.31 gate or
  older-CMake behavior.
- The production selector itself fails safely on older CMake: undefined host
  identity fields cannot match the enforcing CUDA baseline. The defect is the
  unconditional integration regression, which will fail an otherwise
  supported CMake 3.25--3.30 CUDA test configuration.

Rigorous reasoning:

The exact-host regression reads variables which its minimum supported CMake
cannot generate. Its failure on CMake 3.25--3.30 would not reveal an unsafe
baseline selection; it would be a false portability failure introduced by the
correction. This conflicts with the explicit CMake 3.25-or-newer contract and
prevents the corrected CUDA test tree from being accepted across that range.

Required correction and tests:

1. Either derive the actual CUDA host identity correctly on CMake 3.25--3.30,
   or register the exact-host integration only when CMake 3.31 or newer exposes
   the detected fields.
2. For CMake 3.25--3.30, directly prove that absent host identity selects an
   empty, non-enforcing baseline; classify exact host-identity enforcement as
   `skipped`, not passed.
3. Run a CMake 3.25 or 3.30 CUDA configure and the applicable selector
   regression, plus the current CMake 4.1 unlike-host integration.
4. Record the version boundary in the support/evidence report so a CMake 4.1
   pass is not generalized to every supported CMake version.

### M8R2-FINAL-PORT-006: the pre-3.31 spoof-cache regression rejected NVCC's
default host selection

Severity: **medium**

Final re-review: **resolved as contract correction 20**.

The initial correction-19 driver treated `CUDA_HOST_COMPILER` as mandatory.
Official CMake 3.30.9 can validly configure CUDA while leaving
`CMAKE_CUDA_HOST_COMPILER` empty and allowing NVCC to choose its default host.
An independent configuration using that supported path configured
successfully and recorded an empty ELF baseline, but the focused selector
selection failed **2/3** because the nested spoof-cache script stopped with
`CUDA_HOST_COMPILER is required`.

The corrected script no longer lists `CUDA_HOST_COMPILER` among its required
arguments. It appends `-DCMAKE_CUDA_HOST_COMPILER` to the nested configure only
when the outer value is nonempty, preserving NVCC's default choice otherwise.
Independent re-review reran the same already-configured CMake 3.30.9 tree:
selector unit, unavailable-identity, and nested spoof-cache integration passed
**3/3** in 32.37 s.

The nested adversarial configure retained raw cached GNU/11.4 identity values,
while its generated `CMakeCUDACompiler.cmake` recorded
`CMAKE_CUDA_HOST_COMPILER ""` and contained no host ID/version fields. Its
exact-host integration was absent and its `elf_abi` registration carried an
empty `ASC_CPP_HARDENING_BASELINE`. The lead's separate clean default-host
configure/build trial also built `asc_core_cuda` and passed the same selection
**3/3** in 30.53 s.

Evidence:

- `tests/hardening/CMakeLists.txt:614-629` passes the outer explicit host value,
  which may validly be empty, to the nested driver.
- `tests/hardening/cuda_host_elf_selector_untrusted_cache_integration_test.cmake:3-16`
  requires the source, workspace, generator, C++ compiler, architecture, and
  ASCCMake location, but not an explicit CUDA host path.
- `tests/hardening/cuda_host_elf_selector_untrusted_cache_integration_test.cmake:48-52`
  appends the nested host override only when it is defined and nonempty.
- Independent artifact:
  `/tmp/asc-cpp-m8-port-reopen-default-host.uEEkaz/build`.

Required correction and tests, now satisfied:

1. Accept an empty explicit CUDA host path in the spoof-cache driver.
2. Omit the nested host cache argument when the outer value is empty.
3. Run pre-3.31 explicit-host and default-host configurations; require the
   selector, unavailable-identity, and spoof-cache tests to pass with empty
   enforcing baselines.

## Twenty-risk closure audit

| Contract risk | Result | Independent reasoning |
| --- | --- | --- |
| 1. CUDA post-enqueue failure drain | resolved | Core copy and record-event failures, Dense custom-kernel/cuBLAS failures, and clone rollback drain the affected stream while preserving the original provider status; private real-device fault tests cover event create/record boundaries and storage lifetime |
| 2. Sparse readable-structure aliasing | resolved | Coordinate values/coordinates and Compressed values/offsets/indices are checked as disjoint exact spans; SpMV explicitly checks its output byte span against CSR structure; legal exact/partial overlap and no-write regressions pass on GCC and Clang |
| 3. guarded recursive deletion | resolved | all recursive fixture cleanups run through the execution-time guard; the repository scan and focused test pass |
| 4. Core/Dense CUDA no-device result | resolved | successful zero enumeration is code 77; provider failure is exact exit 2 and passes its non-skip regression |
| 5. optional CUDA package discovery | resolved | required CUDA uses dependency discovery; optional-only missing toolkit marks only unavailable optional closure false; build-tree/install/relocation/repeated/static/shared coverage passes |
| 6. synthetic default downstream | resolved | default is repository-independent; real asc-xde audit is explicit opt-in, commit-pinned, and proves byte-identical status before/after |
| 7. exact ELF selector | resolved | CMake 3.31+ uses actual detected CUDA host fields; older CMake requires and tests an empty non-enforcing baseline |
| 8. current security boundary | resolved | `SECURITY.md` covers the unreleased 0.9.0 six-module, CUDA, numerical, lifetime, concurrency, package/tooling, and provenance boundaries without a support promise |
| 9. rectangular padded GEMM | resolved | float/double 2x3 by 3x4, all four transpose pairs, distinct padded mappings, expected values, and untouched holes are exercised |
| 10. integral reduction overflow | resolved | checked integral accumulation rejects signed positive/negative and unsigned overflow before publication, with safe controls |
| 11. Random partition properties | resolved | irregular, empty, reordered, threaded CPU, two-context GPU, and float/double partitions compare with whole-domain generation |
| 12. Timer overflow | resolved | interval/total/count/divisor arithmetic is checked transactionally; state preservation, saturation, average overflow, and clock regression have deterministic tests |
| 13. Sparse Random priority collision | resolved | CPU and real-device tests call the exact private host/device comparator used by production and require ordinal tie-break order 1,2,5,7 |
| 14. neutral historical banners | resolved | documentation consistency requires exactly 20 neutral retained-history banners with live pointers |
| 15. all recursive fixture deletion guarded | resolved | former direct hardening cleanup registrations now use one guarded script below the dedicated workspace root |
| 16. actual CUDA host identity | resolved | exact detected fields and unlike-host behavior pass on CMake 4.1; older CMake is explicitly non-enforcing |
| 17. enumeration errors fail | resolved | `DeviceCountDisposition` and the exact-exit regression distinguish provider error, zero, and positive counts |
| 18. operation-specific performance evidence | resolved | per-row independent oracles, reproducible candidate digest, complete environment, timing, synchronization, and noise record are present and freshly exercised |
| 19. minimum-CMake CUDA selector | resolved | separate trusted values are populated only from CMake 3.31+ detection; real CMake 3.25.3 and 3.30.9 compile Core CUDA and pass unavailable/spoof-cache regressions with empty baselines; CMake 4.1 retains the exact unlike-host path |
| 20. pre-3.31 NVCC default host | resolved | the nested spoof driver accepts an empty explicit host and omits that cache argument; official CMake 3.30.9 default-host configuration builds Core CUDA and passes 3/3 with an empty baseline |

The rejected intermediate Sparse bounding-span implementation is correctly
recorded: Clang exposed its false-positive gap alias, and the final
disjoint-span implementation removes that defect.

## Validation artifacts independently inspected

The candidate was not rebuilt by this review role. The lead's fresh full
matrix, clean artifacts, generated test metadata, and exact integration record
were inspected. This role independently reran the focused guard/
host-selector/enumeration tests and all six benchmark registrations against
the already-built current candidate, and independently configured the
official CMake 3.30.9 NVCC-default-host compatibility path.

| Matrix facet | Recorded final result | Review disposition |
| --- | --- | --- |
| GCC 11.4 Debug/static CPU | 193/193 pass in 129.71 s | accepted |
| GCC 11.4 Debug/shared CPU | 195/195 pass in 132.96 s | accepted |
| Clang 19 Debug/shared CPU | 194/194 pass in 150.54 s | accepted |
| GCC ASan+UBSan bounded safe selection | 139/139 pass; no diagnostic | accepted as bounded selection |
| Clang LSan bounded selection | 12/12 pass; no leak diagnostic | accepted as bounded selection |
| Clang TSan bounded selection | 12/12 pass; no race diagnostic | accepted as bounded selection |
| ordinary GCC/NVCC Release/shared CUDA | 262 registered; 252 pass; 10 forced skips; 0 fail in 1483.00 s | accepted for the CMake 4.1 executed paths |
| private CUDA fault build | 4/4 pass | accepted; hook absent from ordinary library |
| forced Core/Dense no-device selection | 10/10 code-77 skips | accepted for explicit seam only |
| forced Dense CUDA enumeration error | exact exit 2; registered test pass | accepted; not skipped |
| CMake 3.25.3 CUDA minimum, explicit host | configure pass; `asc_core_cuda` build pass; selector/unavailable/spoof 3/3 pass in 29.29 s; integrated baseline empty | accepted; exact host identity `skipped` |
| CMake 3.30.9 CUDA compatibility, explicit host | configure pass; `asc_core_cuda` build pass; selector/unavailable/spoof 3/3 pass in 29.94 s; integrated baseline empty | accepted; exact host identity `skipped` |
| CMake 3.30.9 outer raw-cache spoof | selector/unavailable 2/2 pass in 0.01 s; raw GNU/11.4 ignored; integrated baseline empty | accepted as adversarial fail-closed evidence |
| CMake 3.30.9 NVCC-default host | lead clean configure/build and 3/3 pass in 30.53 s; reviewer configure and 3/3 pass in 32.37 s; compiler host empty; baseline empty | accepted; exact host identity `skipped` |
| unlike CUDA-host selector, CMake 4.1 | final guard/selector/integration 3/3 pass in 36.39 s; Clang 19 host and empty baseline | accepted |
| CPU package/tooling selection | 25/25 pass | accepted |
| CUDA package build-tree/install/relocation aggregates | 2/2 pass in 601.82 s | accepted |
| explicit real asc-xde audit | 1/1 pass; commit/status unchanged | accepted |
| architecture selection | 8/8 pass | accepted for tested configuration |
| Compute Sanitizer memcheck | 13/13; zero errors and leaks | accepted for applicable executables |
| release/shared benchmark selection | lead 6/6 in 11.04 s; reviewer 6/6 in 12.18 s | accepted as correctness-checked, threshold-free local observations |

Whole-suite LSan and TSan remain `skipped` because the allocator-interposition
fixtures conflict with those runtimes. The exact bounded selections are not
treated as whole-suite sanitizer evidence.

## GPU evidence classification

The required classification terms are used exactly:

| Facet | Evidence accepted from this candidate |
| --- | --- |
| `core_cuda` | `configure-tested`; `compile-tested`; `runtime-tested` |
| `dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_dense_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |
| `random_sparse_cuda` | `configure-tested`; `compile-tested`; `runtime-tested`; `parity-tested` |

Core CUDA is not classified `parity-tested`.

The following are `skipped`:

- actual masked/no-device enumeration, because the host CUDA 12.9 runtime
  independently aborts under both tested `CUDA_VISIBLE_DEVICES` masks;
- trusted device CSC;
- multi-GPU, peer-access, and MIG behavior;
- other CUDA toolkit, driver, host-compiler, and compute-capability
  combinations;
- non-Linux GPU configurations and hosted GPU CI;
- HIP, SYCL, and other unapproved providers;
- the native-state impossible-allocation case under Compute Sanitizer; and
- Compute Sanitizer `racecheck`, `initcheck`, and `synccheck`.

The tested environment is NVCC/CUDAToolkit 12.9.86 with driver 576.83 on an
NVIDIA GeForce RTX 3060 Laptop GPU, compute capability 8.6. These labels do not
generalize beyond that environment. Exact CUDA-host identity enforcement on
CMake 3.25--3.30 is `skipped` because those CMake versions do not expose the
detected fields; their fail-closed, non-enforcing selector behavior is tested.

## Numerical and performance disposition

The numerical correction evidence is accepted for rectangular/padded GEMM,
integral reduction overflow, exact Sparse structure alias rejection, irregular
Random partitions, and forced priority collisions. Dense, Sparse, and all
Random CUDA facets have independent numerical, bit, structure, or deterministic
parity tests outside the benchmark-only claims.

The six Release/shared benchmark registrations executed successfully and their
operation-specific oracles passed. The resulting values are accepted as
one-run, threshold-free, correctness-checked local observations under the
recorded build-input digest and environment. No speedup, regression,
cross-machine timing, stable-noise, or optimization claim is accepted.

## Security, license, and provenance

The security boundary is current and the guarded deletion, CUDA failure-drain,
and Sparse alias corrections address the reviewed deletion, lifetime, and
memory-safety risks. No new network, parser, serialization, dynamic-plugin, or
privilege boundary was introduced.

The project remains Apache-2.0. No third-party source, data, generated payload,
runtime dependency, license, or notice obligation was added. The absence of
`THIRD_PARTY_NOTICES` remains consistent with the approved provenance audit.
ASCCMake 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7` is build-only. CUDA Runtime,
cuBLAS, and cuSPARSE remain private provider implementation dependencies.
The explicit asc-xde audit was read-only and left its expected commit and
worktree bytes unchanged.

No provenance, license, or unapproved-dependency finding remains open.

## Evidence limits that are not code findings

The following remain truthful environment limits:

- no Windows/MSVC, macOS/AppleClang, other CPU architecture/standard library,
  Ninja, or hosted-CI matrix;
- no other CUDA toolkit/driver/host/compiler/architecture matrix;
- no real masked-device evidence on this host;
- no whole-suite LSan/TSan result;
- no trusted device CSC path;
- no non-memcheck Compute Sanitizer result; and
- exact-environment pre-1.0 ELF observations do not establish cross-minor or
  cross-toolchain ABI compatibility.

## Final acceptance

All twenty contracted findings are resolved. The exact-host path is enforced
only where CMake exposes the detected identity, and older supported CMake
versions fail closed without trusting raw cache values or rejecting NVCC's
default-host selection. The corrected candidate retains the approved
architecture, APIs, targets, dependencies, package boundary, GPU evidence
labels, security/license/provenance boundary, and Milestone 8 scope.

This independent portability/GPU/performance review accepts Publication
Checkpoint B. No actionable finding remains. The environmental evidence limits
listed above remain explicit; they are not converted into passes.
