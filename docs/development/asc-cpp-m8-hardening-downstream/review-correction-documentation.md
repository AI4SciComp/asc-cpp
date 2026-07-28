# Milestone 8 Post-Checkpoint Documentation and API Correction Review

Status: documentation corrections and corrected integration evidence recorded

Date: 2026-07-28

## Scope and authority

This review implements only the documentation/API/security scope assigned by
the frozen
[`review-correction-contract.md`](review-correction-contract.md) and
[`review-correction-ownership.md`](review-correction-ownership.md). It was
performed against the approved Stage A architecture and ADRs 0002, 0005,
0008, 0013, 0014, 0015, 0017, and 0018.

No product source, public target, component, direct dependency, architecture
manifest, ADR, root/package CMake file, test source, other repository, Git
history, branch, tag, release, or remote state was changed by this role.

## Corrections

### Security surface

`SECURITY.md` now describes the unreleased `0.9.0` Milestone 8 correction
candidate rather than Milestone 1 Core. It explicitly covers:

- all six provider-free modules, both Random storage facets, the aggregate,
  and all six opt-in CUDA facets;
- numerical overflow, sparse structural aliasing, external adapters/resources,
  asynchronous CUDA lifetime, package/tooling deletion paths, and concurrency;
- package, install, relocation, CI, hardening, ABI, benchmark, provenance, and
  evidence-reporting boundaries;
- the absence of a published version, support window, or production-readiness
  claim; and
- exact provider-free, build-only, and optional CUDA dependency boundaries.

### Retained historical documents

Exactly 20 retained-history banners no longer call Milestone 0 the current API
or package. Each banner is milestone-neutral and links both to the live
documentation index and the approved Stage A architecture:

- `docs/architecture.md`, `docs/build-system.md`,
  `docs/optional-backends.md`, and `docs/testing.md`;
- all seven retained files under `docs/design/`;
- all seven retained files under `docs/migration/`; and
- `docs/modules/array.md` and `docs/modules/linalg.md`.

The historical bodies remain unchanged audit evidence.

### Package and downstream behavior

The live package documentation now distinguishes producer availability from
consumer dependency resolvability. A CUDA component requested only through
`OPTIONAL_COMPONENTS` remains nonfatal when CUDAToolkit cannot be discovered:
the optional CUDA component is false, no partial CUDA closure is imported, and
required provider-free targets remain usable. The same component is fatal when
required.

The four default asc-xde-shaped trials are documented as asc-cpp-owned
synthetic package tests that never inspect a sibling checkout. Real repository
inspection is a separate read-only opt-in using exactly:

```text
ASC_CPP_ENABLE_ASC_XDE_REPOSITORY_AUDIT
ASC_CPP_ASC_XDE_REPOSITORY
ASC_CPP_ASC_XDE_EXPECTED_COMMIT
```

Both repository path and expected commit default to empty and are required
when the opt-in is enabled. The audit verifies that the exact commit and
worktree status are unchanged; it does not clean or require a pristine
checkout.

### API, ABI, numerical, and lifetime wording

The live API/performance/extension documentation now records:

- integral `ReduceSum` returns `ErrorCode::kOverflow` rather than publishing a
  wrapped signed or unsigned result;
- `Timer::Stop` transactionally checks interval, total, and count arithmetic,
  while `Average` checks count conversion and the frozen result-less `Elapsed`
  saturates at `Duration::max()` on an unrepresentable running total or clock
  regression;
- Sparse alias handling conservatively includes every values, coordinate,
  offset, and index span read by evaluation and SpMV;
- when completion-event creation or recording fails after a CUDA copy, project
  kernel, or cuBLAS operation may have enqueued work, ASCCpp drains the
  affected stream and preserves the original provider status;
- exact-environment ELF baseline enforcement requires the complete recorded
  platform/toolchain/linkage/provider selector; unlike environments receive a
  non-enforcing observation or explicit skip; and
- absence of a CUDA device is CTest skip code 77, not a runtime pass or product
  failure.

No public function, type, header ownership, target, component, direct
dependency, version, or evidence label was added or renamed by these
documentation corrections.

## Corrected evidence

The lead supplied the exact clean post-integration results from the correction
logs. This role transcribed them without inferring results from the earlier
revision-2 matrix:

| Evidence | Corrected result |
| --- | --- |
| GCC 11.4 Debug/static CPU | full suite passed 193/193, zero failed; 129.71 seconds |
| GCC 11.4 Debug/shared CPU | full suite passed 195/195, zero failed; 132.96 seconds |
| Clang 19 Debug/shared CPU | full suite passed 194/194, zero failed; 150.54 seconds |
| GCC 11.4 ASan+UBSan | selected suite passed 139/139; 2.08 seconds |
| Clang 19 LSan | bounded subset passed 12/12; 0.13 seconds |
| Clang 19 TSan | bounded subset passed 12/12; 0.29 seconds |
| final CPU package/tooling selection | passed 25/25; 83.84 seconds |
| GCC/NVCC 12.9.86 Release/shared, architecture 86 | 262 registered: 252 passed, 10 intentional forced-no-device CTest skips, zero failed; 1483.00 seconds |
| dedicated hook-enabled static focused selection | passed 4/4; 1.24 seconds |
| Compute Sanitizer memcheck | passed 13/13 with zero errors and zero leaks |
| final CUDA package aggregate with strengthened workspace guard | passed 2/2: build-tree 305.07 seconds; install/relocate 296.75 seconds; total 601.82 seconds |
| official CMake 3.25.3, CUDA 12.9.86/GNU 11.4 host, Release/shared, architecture 86 | configured; built `asc_core_cuda`; selector selection passed 3/3 in 29.29 seconds; generated `elf_abi` baseline was empty |
| official CMake 3.30.9, CUDA 12.9.86/GNU 11.4 host, Release/shared, architecture 86 | configured; built `asc_core_cuda`; selector selection passed 3/3 in 29.94 seconds; generated `elf_abi` baseline was empty |
| explicit outer CMake 3.30.9 raw-cache spoof with `GNU`/`11.4` values | selector and unavailable-identity tests passed 2/2 in 0.01 seconds; variables were ignored and generated `elf_abi` baseline was empty |
| official CMake 3.30.9 with NVCC default host, no explicit host override, Release/shared, architecture 86 | configured with host compiler cache empty; built `asc_core_cuda`; generated `elf_abi` baseline was empty; selector selection passed 3/3 in 30.53 seconds and independent portability rerun passed 3/3 in 32.37 seconds |
| CMake 4.1.2 trusted-field guard, selector unit, and exact integration | passed 3/3 in 36.39 seconds within 262 registered tests |
| forced CUDA enumeration failure | exact process exit 2, passed; not a CTest skip |
| successful forced zero-device enumeration | exact process exit 77, correctly skipped |
| Release/shared benchmark selection | passed 6/6 in 11.04 seconds; all rows passed their operation-specific guards |
| explicit real asc-xde audit | passed 1/1 in 2.89 seconds; expected commit and exact worktree status unchanged |

The CUDA run used an NVIDIA GeForce RTX 3060 Laptop GPU, driver 576.83,
compute capability 8.6. All six CUDA facets are `configure-tested`,
`compile-tested`, and `runtime-tested`; Dense, Sparse, Random, Random Dense,
and Random Sparse CUDA are also `parity-tested`. Core CUDA is not classified
as `parity-tested`.

The ten no-device cases are exact CTest code-77 skips from the dedicated
forced-no-device seam. Actual `CUDA_VISIBLE_DEVICES` masking is not claimed
because CUDA 12.9 `libcudart` independently aborts under that masking mechanism
before ASCCpp classification. Environmental/provider skips remain explicit in
the support matrix.

Only a successful zero-device enumeration returns 77. Enumeration/provider
failure prints the provider status and returns 2; its exact forced-failure
regression passed without `SKIP_RETURN_CODE`.

The benchmark result is execution/correctness evidence for the six registered
CPU and CUDA probes. Its exact final one-run observations are:

| Probe row | Observation |
| --- | --- |
| Dense CPU double 32x32 | evaluate 58,445.4 ns/iteration; GEMM 18,160.8 ns/iteration |
| Sparse CPU CSR 128x256, 512 nonzeros | evaluation 144,669 ns/iteration; SpMV 6,350.32 ns/iteration |
| Random storage CPU | Dense layout-left 24,205,012 ns and layout-right 25,822,548 ns over 100 repetitions; Sparse 298,307,733 ns over 10 repetitions |
| Dense CUDA | H2D 6.999 GB/s; D2H 7.381 GB/s; D2D 79.689 GB/s; terminal evaluation 6.053 GB/s; AXPY 30.607 GFLOP/s; GEMV 25.196 GFLOP/s; GEMM 2.469 TFLOP/s |
| Sparse CUDA CSR SpMV | float 119.649 million nonzeros/s; double 112.434 million nonzeros/s |
| Random CUDA | raw Philox 15.377 billion items/s; Dense `Uniform01` 11.639 billion items/s; Sparse `Uniform01` 26,808.3 items/s |

Dense and Sparse CPU, CPU Dense and Sparse Random, and Dense CUDA use
operation-specific independent oracles and print `oracle=independent`. Sparse
CUDA compares float/double CSR results with independent host reference rows.
Random CUDA independently reconstructs raw words, Dense values, and Sparse
priority/ordinal selection and values from the Philox specification.

The implementations and `docs/performance.md` agree: correctness verification
is outside the timed repetitions; every CUDA timed repetition waits for
completion; setup, allocation, or transfer is timed only when explicitly
named; and the probes emit workload, measured repetitions, elapsed unit,
allocation scope, checksum/oracle, and pass/fail/skip disposition. Dense and
Sparse CPU each perform one untimed operation before their measured rows. CPU
Random has zero warmups; all 100 Dense and 10 Sparse generations are timed.
Dense and Random CUDA use three warmups and 12 measured repetitions; Sparse
CUDA uses three warmups and 20 measured repetitions. The lead's integration
record supplies exact branch/base-plus-build-input-digest candidate identity,
including final product/build-input SHA-256
`e7feae4784079c3edf331940b1ea8373f9c0748d4e20ad49ae0b2cbcae401f69`,
OS, CPU, memory, compiler/standard-library/CMake, Release/shared
`-O3 -DNDEBUG` flags, CUDA compiler/host/driver/GPU/architecture, and the WSL2
scheduling/frequency/thermal/contention noise boundary.

These timing, throughput, allocation, and checksum values are
single-environment, threshold-free observations. Neither this review nor the
support matrix turns them into a performance threshold, cross-machine
guarantee, speedup claim, or normalized-noise result.

## Final correction re-review: findings 15--18

The documentation/API role independently re-read the final product, CMake,
benchmark, test, package, verification, and performance-methodology evidence
for the four amended findings.

- **Finding 15, recursive deletion: resolved.** Every recursive fixture
  deletion, including former direct `cmake -E rm -rf` registrations, routes
  through the guarded workspace helper. The guard regression recursively
  scans every test CMake file and rejects all supported recursive-deletion
  spellings outside that helper. The final CUDA build-tree and
  install/relocate aggregates passed 2/2 in 601.82 seconds.
- **Finding 16, CUDA-host ELF selector: resolved.** Baseline selection uses
  CMake's actual detected CUDA host compiler ID and version. The unlike-host
  integration configured ordinary C++ as GNU and NVCC host as Clang 19.0.0,
  selected an empty non-enforcing baseline, and passed within the final CMake
  4.1.2 trusted-field guard/unit/exact selection, 3/3 in 36.39 seconds.
  Compile/runtime evidence for that unlike-host pairing remains explicitly
  skipped.
- **Finding 17, enumeration disposition: resolved.** A provider error is exit
  2 and a successful zero-device enumeration is exit 77; the exact direct
  regressions passed and only the latter is classified as skipped.
- **Finding 18, operation-specific performance evidence: resolved.** Every
  reported CPU and CUDA operation has an independent element, scalar,
  structural, bit, sampled-numerical, or host-reference oracle appropriate to
  the operation. The six-probe selection passed 6/6 in 11.04 seconds with the
  complete environment, flags, candidate identity, synchronization, and noise
  record described above.

No actionable API, documentation, package, downstream-usability, or
performance-methodology risk remains from findings 15--18.

## Final CMake-minimum re-review: finding 19

The correction contract and ownership ledger accepted finding 19 before
implementation writes began.

**Finding 19, CMake 3.25--3.30 CUDA-host identity: resolved.** The declared
CMake 3.25 minimum remains unchanged. CMake's detected CUDA host compiler ID
and version fields are available only on CMake 3.31 or newer, so ASCCpp
registers the exact unlike-host integration only in that range and passes only
those trusted detected fields to the selector. Supported CMake 3.25 through
3.30 instead ignore same-named raw cache values, register direct
unavailable-identity and nested cache-spoof regressions, and require the
generated CUDA ELF baseline to be empty. This is fail-closed, non-enforcing
behavior; it does not infer or allow a caller to assert that the configured
host matches an approved baseline.

Real official Kitware CMake 3.25.3 and 3.30.9 archives each configured the
Release/shared architecture-86 candidate with CUDA 12.9.86 and GNU 11.4 host,
built `asc_core_cuda`, and passed their three selector/guard registrations:
3/3 in 29.29 seconds on CMake 3.25.3 and 3/3 in 29.94 seconds on CMake 3.30.9.
Each generated `elf_abi` registration carried an empty baseline.

The explicit adversarial outer CMake 3.30.9 configure supplied raw cached
CUDA-host values `GNU` and `11.4`. The selector and unavailable-identity tests
passed 2/2 in 0.01 seconds, the configure reported those values ignored, and
the generated `elf_abi` baseline remained empty. Current CMake 4.1.2 retained
the trusted-field guard, selector unit, and exact unlike-host path; all three
passed in 36.39 seconds within the unchanged total of 262 registered tests.

For the two minimum-range trials, CUDA and `ASC::core_cuda` are respectively
`configure-tested` and `compile-tested`. Exact-host identity enforcement is
`skipped` because those CMake versions do not expose the required fields;
the unavailable-identity and spoof-cache behavior is directly tested. Runtime
and parity were not exercised in those version-specific builds. No API,
target, component, package dependency, or minimum-version change was
introduced by the compatibility gate.

No actionable documentation, API, package, downstream-usability, or
selector-spoof risk remains from finding 19.

## Final default-host re-review: finding 20

The correction contract and ownership ledger accepted finding 20 before its
implementation writes.

**Finding 20, pre-3.31 default NVCC host: resolved.** The nested spoof-cache
driver now accepts an empty explicit CUDA host override and omits the host
cache argument from its nested configure when the outer project lets NVCC
choose its default host. The unavailable detected identity still selects an
empty, non-enforcing ELF baseline; the test does not manufacture or trust an
identity from the absent override.

An official CMake 3.30.9 Release/shared architecture-86 trial configured CUDA
12.9.86 without `CMAKE_CUDA_HOST_COMPILER`, built `asc_core_cuda`, recorded
the host compiler cache entry as empty, and generated an empty `elf_abi`
baseline. Its selector selection passed 3/3 in 30.53 seconds. The independent
portability rerun passed the same 3/3 selection in 32.37 seconds.

This version-specific path is `configure-tested`, and `ASC::core_cuda` is
`compile-tested`. Exact host identity, runtime, and parity remain `skipped`
because CMake 3.30.9 does not expose trusted detected host identity and those
operations were not part of this focused trial. No API, target, component,
package dependency, or minimum-version change was introduced.

No actionable documentation, API, package, downstream-usability,
selector-spoof, or default-host risk remains from findings 19--20.

## API and package audit findings

The corrected documentation remains consistent with:

- the six-module, nine-provider-free-target, and fifteen-CUDA-enabled-target
  inventories;
- the exact direct dependency graph and private CUDA Runtime/cuBLAS/cuSPARSE
  provider edges;
- the CMake 3.25 minimum with version-gated exact CUDA-host identity handling;
- the `SameMinorVersion` unreleased `0.9.0` package boundary;
- the capacity-carrying `CudaFillPhilox4x32` API with no bare-pointer overload;
- provider-free component lookup that does not discover CUDAToolkit; and
- the five exact GPU evidence labels: `configure-tested`, `compile-tested`,
  `runtime-tested`, `parity-tested`, and `skipped`.

No documentation workaround was used for a known code or package defect.
The supplied corrected package/tooling, guarded CUDA package aggregate,
downstream audit, CUDA failure-hook, no-device classification, full-suite,
sanitizer, real-device, benchmark, and Compute Sanitizer results close the
former integration-evidence gap.

The audit identified three exact lead-owned documentation follow-ups outside
this role's write scope:

- `docs/modules/utilities.md` must add the checked `Stop`, saturating
  `Elapsed`, checked `Average`, and clock-regression behavior to its Timer
  contract;
- `docs/modules/dense.md` must state that integral `ReduceSum` returns
  `kOverflow` without publishing a wrapped result; and
- `CHANGELOG.md` must distinguish the always-synthetic default downstream
  trial from the separately enabled real-repository audit.

These were reported without crossing the ownership boundary. The lead
subsequently updated all three files, and the final documentation audit
confirmed the checked Timer, integral `ReduceSum`, and synthetic/opt-in
downstream wording.

## Documentation validation

The documentation role ran:

```sh
rg -l \
  'does not describe the current Milestone 0 API or package' \
  docs/architecture.md docs/build-system.md docs/design docs/migration \
  docs/optional-backends.md docs/testing.md \
  docs/modules/array.md docs/modules/linalg.md
```

Result: passed, zero stale banners.

```sh
rg -l \
  'does not describe the active API or package' \
  docs/architecture.md docs/build-system.md docs/design docs/migration \
  docs/optional-backends.md docs/testing.md \
  docs/modules/array.md docs/modules/linalg.md
```

Result: passed, exactly 20 neutral banners.

```sh
git diff --check -- \
  SECURITY.md docs/architecture.md docs/build-system.md docs/design \
  docs/migration docs/optional-backends.md docs/testing.md \
  docs/modules/array.md docs/modules/linalg.md docs/support-matrix.md \
  docs/api-compatibility.md docs/package-capabilities.md \
  docs/extension-guide.md docs/downstream-integration.md \
  docs/performance.md \
  docs/development/asc-cpp-m8-hardening-downstream/\
review-correction-documentation.md
```

Result: passed, no whitespace diagnostics.

Relative targets introduced by this correction were checked against the
worktree: `docs/README.md`, the Stage A architecture blueprint, the
asc-xde-shaped fixture, `abi/`, and every live cross-reference exist.

## Files changed by this role

Security:

```text
SECURITY.md
```

Retained-history banners:

```text
docs/architecture.md
docs/build-system.md
docs/optional-backends.md
docs/testing.md
docs/design/architecture_blueprint_v1.md
docs/design/architecture_review_v1.md
docs/design/array_design.md
docs/design/core_design.md
docs/design/linalg_design.md
docs/design/random_design.md
docs/design/utilities_design.md
docs/migration/array.md
docs/migration/core.md
docs/migration/handoff.md
docs/migration/inventory.md
docs/migration/linalg.md
docs/migration/random.md
docs/migration/utilities.md
docs/modules/array.md
docs/modules/linalg.md
```

Live package/API/evidence documentation:

```text
docs/support-matrix.md
docs/api-compatibility.md
docs/package-capabilities.md
docs/extension-guide.md
docs/downstream-integration.md
docs/performance.md
```

Review record:

```text
docs/development/asc-cpp-m8-hardening-downstream/review-correction-documentation.md
```

## Handoff

Documentation/API/security correction and its corrected evidence update are
complete within this role's write scope. The lead still owns the corrected
Publication Checkpoint B and final independent review disposition. No
publication or remote action is authorized by this report.
