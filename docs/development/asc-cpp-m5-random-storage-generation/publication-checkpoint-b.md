# Milestone 5 Publication Checkpoint B

Status: Ready for owner review; no publication action taken

Date: 2026-07-26

Approved milestone: **Milestone 5 — random storage generation**

Corrections: None from the owner

Branch: `feature/asc-cpp-m5-random-storage-generation`

Baseline and unchanged `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Release candidate: unreleased `0.5.0`

This checkpoint covers only the frozen Milestone 5 contract. The intentionally
uncommitted Milestones 0--4 restart work and the owner's earlier deletion of
the retired implementation remain in the worktree. No commit, push, merge,
tag, release, branch deletion, or other publication action occurred.

## 1. Changed files and reasons

### Contract, ownership, audit, and review evidence

```text
docs/development/asc-cpp-m5-random-storage-generation/milestone-contract.md
docs/development/asc-cpp-m5-random-storage-generation/ownership.md
docs/development/asc-cpp-m5-random-storage-generation/preflight.md
docs/development/asc-cpp-m5-random-storage-generation/dependency-audit.md
docs/development/asc-cpp-m5-random-storage-generation/provenance-record.md
docs/development/asc-cpp-m5-random-storage-generation/production-self-review.md
docs/development/asc-cpp-m5-random-storage-generation/verification-design.md
docs/development/asc-cpp-m5-random-storage-generation/verification-review.md
docs/development/asc-cpp-m5-random-storage-generation/documentation-api-review.md
docs/development/asc-cpp-m5-random-storage-generation/portability-review.md
docs/development/asc-cpp-m5-random-storage-generation/publication-checkpoint-b.md
```

These freeze the approved contract and disjoint ownership, preserve the
read-only repository/dependency/provenance audits, record every distinct
specialist conclusion, and provide this publication checkpoint.

### Production API

```text
include/asc/random/dense.h
include/asc/random/sparse.h
```

These are the only new Milestone 5 production headers. They implement the
approved host/serial dense and sparse generation facets. There is no
production change to core, utilities, expression, dense, sparse, or the
storage-neutral random base.

### Build, package, and architecture integration

```text
CMakeLists.txt
CMakePresets.json
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
src/random/CMakeLists.txt
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/dependency_check.cmake
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/dense/CMakeLists.txt
tests/consumer/sparse/CMakeLists.txt
tests/consumer/random_dense/CMakeLists.txt
tests/consumer/random_sparse/CMakeLists.txt
tests/consumer/cpp/CMakeLists.txt
tests/consumer/subproject/CMakeLists.txt
tests/consumer/subproject/main.cc
tests/consumer/run_core_consumer.cmake
tests/consumer/run_subproject_consumer.cmake
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/release-roadmap.md
```

These changes add the two independent interface facets and the provider-free
aggregate, advance the package candidate to 0.5.0, export all nine components
separately, make no-component lookup request `cpp`, register the M5 tests and
isolated consumers, and audit the exact target, file, and dependency graph.
The existing CI workflow discovers the registered CTest surface; no hosted
CI run is claimed.

### Independent verification, consumers, and benchmark

```text
tests/random_dense/CMakeLists.txt
tests/random_dense/allocation_counter.cc
tests/random_dense/allocation_counter.h
tests/random_dense/random_dense_test.cc
tests/random_dense/test_support.h
tests/random_sparse/CMakeLists.txt
tests/random_sparse/random_sparse_test.cc
tests/random_sparse/test_support.h
tests/compile/m5_random_contract.cc
tests/compile/m5_random_multi_tu.h
tests/compile/m5_random_multi_tu_a.cc
tests/compile/m5_random_multi_tu_b.cc
tests/compile/m5_random_multi_tu_main.cc
tests/compile/m5_random_negative_const_dense.cc
tests/compile/m5_random_negative_missing_dense_facet_header.cc
tests/compile/m5_random_negative_missing_sparse_facet_header.cc
tests/compile/m5_random_negative_result_copy.cc
tests/compile/m5_random_negative_unsupported_dense_scalar.cc
tests/compile/m5_random_negative_unsupported_sparse_scalar.cc
tests/consumer/random_dense/main.cc
tests/consumer/random_sparse/main.cc
tests/consumer/cpp/main.cc
benchmarks/random_storage/allocation_counter.cc
benchmarks/random_storage/allocation_counter.h
benchmarks/random_storage/random_storage_benchmark.cc
```

These independently exercise exact sequence/state/structure oracles,
rank-zero and empty behavior, layout and canonical-order invariants,
transactions and allocation rollback, multi-translation-unit linkage,
negative compile contracts, isolated package closures, concurrency, and
allocation/performance smoke behavior.

### User documentation and release notes

```text
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/random.md
```

These document the storage-neutral random base, explicit addresses, both
storage facets, exact traversal and state advancement, ownership and resource
lifetime, allocation/complexity limits, component use, provenance, and the
unreleased 0.5.0 status. The random module guide contains three independently
compiled and executed examples.

The repository remains cumulatively dirty by design. Existing tracked
deletions are the owner's retained restart state, not Milestone 5 deletion
work. The unrelated modified MdeCpp `Makefile` remains untouched.

## 2. APIs, targets, and direct dependencies

### Exact six-module and facet graph

```text
asc_core        / ASC::core        -> []
asc_utilities   / ASC::utilities   -> [ASC::core]
asc_expression  / ASC::expression  -> [ASC::core]
asc_random      / ASC::random      -> [ASC::core]
asc_dense       / ASC::dense       -> [ASC::core, ASC::expression]
asc_sparse      / ASC::sparse      -> [ASC::core, ASC::expression]

asc_random_dense  / ASC::random_dense
  -> [ASC::random, ASC::dense]

asc_random_sparse / ASC::random_sparse
  -> [ASC::random, ASC::sparse]

asc_cpp / ASC::cpp
  -> [ASC::core, ASC::utilities, ASC::expression, ASC::dense, ASC::sparse,
      ASC::random, ASC::random_dense, ASC::random_sparse]
```

The facets and aggregate are functional C++20 interface libraries. The facets
own template generation behavior; `ASC::cpp` owns no behavior. Dense and
sparse remain mutually independent and do not depend on random. The base
random target remains core-only and does not include either facet. No target
has a provider or third-party edge.

The available package components are exactly:

```text
core utilities expression random dense sparse
random_dense random_sparse cpp
```

Required component closures are:

```text
random_dense
  -> core;expression;random;dense;random_dense

random_sparse
  -> core;expression;random;sparse;random_sparse

cpp
  -> core;utilities;expression;random;dense;sparse;
     random_dense;random_sparse;cpp
```

No-component `find_package(ASCCpp)` requests `cpp`. A dense-facet consumer
does not import sparse or random-sparse; a sparse-facet consumer does not
import dense or random-dense.

### Public dense facet

```text
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>
```

Exactly unqualified `float` and `double` participate. The call requires an
explicit serial `ExecutionContext`, host mutable view, and explicit Philox
address. Logical dimension zero varies fastest independent of physical
layout. Rank zero consumes one value; any zero extent consumes none. `float`
uses one word per element and `double` uses two consecutive words, high then
low. All validation and returned-offset arithmetic precede mutation.
Successful generation allocates no storage or workspace and does not visit
padding.

### Public sparse facet

```text
SparseUniform01Generation<Element, ExtentsType>
  array
  next_structure_offset
  next_value_offset

GenerateSparseUniform01<Element>(
    context, extents, exact_count, resource,
    structure_stream, structure_subsequence, structure_offset,
    value_stream, value_subsequence, value_offset)
    -> Result<SparseUniform01Generation<Element, ExtentsType>>
```

Exactly unqualified `float` and `double` participate. The structure and value
domains must use distinct `(stream, subsequence)` pairs. Structure candidates
use last-dimension-fastest canonical ordinals and two words per logical
coordinate to form high/low 64-bit priorities. Repeated scans select the exact
smallest `(priority, ordinal)` pairs; builder finalization rejects duplicates
and keeps explicit zero. Values use the independent value domain in canonical
stored-entry order.

Validation precedes allocation. The caller supplies a host
`MemoryResource` that outlives the move-only result. Successful nonempty
generation makes exactly the builder's coordinate and value allocations and
uses no selection workspace. The frozen reference costs are
`O(exact_count * logical_size)` selection plus the sparse builder's
`O(exact_count^2 * Rank)` reference finalization.

### ASCCMake contract

The exact consumed ASCCMake release remains:

```text
repository: /home/yicai/AI4SciComp/asc-cmake
remote: git@github.com:AI4SciComp/asc-cmake.git
branch: main
commit: 8a7dcbad3a97267cce59810aff24de800a3497a7
tag: v0.1.0
annotated tag object: 620b2e912ac5bac7561e09529a65cce965ebc920
```

Only the four verified helpers are called:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

Standard CMake owns interface-target features and component exports. No
ASCCMake API was invented.

## 3. Exact validation commands and results

All commands below were run from
`/home/yicai/AI4SciComp/asc-cpp`. Every listed configure/build/test command
passed unless a result is explicitly classified as skipped.

### Lead GCC 11.4 debug/static matrix

```bash
cmake -S . -B /tmp/asc-cpp-m5-gcc.cJHaYC/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m5-gcc.cJHaYC/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m5-gcc.cJHaYC/build \
  -L milestone-5 --output-on-failure
ctest --test-dir /tmp/asc-cpp-m5-gcc.cJHaYC/build \
  -LE 'package|consumer' --output-on-failure
ctest --test-dir /tmp/asc-cpp-m5-gcc.cJHaYC/build \
  -R '^asc_cpp\.consumer\.core\.' --output-on-failure
```

Results:

```text
configure/build: pass
Milestone 5 selection: 39/39 pass
complete non-package/consumer matrix: 137/137 pass
remaining core consumer matrix: 2/2 pass
union of the selections: complete 159-test static inventory
```

The final 39/39 result is after the package-argument correction and includes
runtime, compile, architecture, package, relocation, subproject, consumer,
negative-contract, and benchmark cases.

After every specialist report and this checkpoint were present, the lead
rebuilt the static tree and reran the final focused production/contract set:

```bash
cmake --build /tmp/asc-cpp-m5-gcc.cJHaYC/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m5-gcc.cJHaYC/build \
  --output-on-failure \
  -R '^asc_cpp\.random_(dense|sparse|storage)\.'
```

Result: rebuild passed; **11/11** dense, sparse, positive, multi-TU, six
negative, and benchmark tests passed.

### Lead minimum-CMake 3.25 GCC 11.4 release/shared matrix

```bash
/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake \
  -S . -B /tmp/asc-cpp-m5-shared-325.6kkKLI/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake \
  --build /tmp/asc-cpp-m5-shared-325.6kkKLI/build --parallel 4
/tmp/asc-cmake-3.25.pTlOcP/venv/bin/ctest \
  --test-dir /tmp/asc-cpp-m5-shared-325.6kkKLI/build \
  -L milestone-5 --output-on-failure
```

Result: configure/build passed; **39/39** M5 tests passed. The matrix includes
the shared installed package, relocation, paths with spaces, and isolated
consumers under the supported minimum CMake.

### Lead Clang 19 ASan and UBSan

```bash
cmake -S . -B /tmp/asc-cpp-m5-clang-sanitizer.VWUaxI/build \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m5-clang-sanitizer.VWUaxI/build --parallel 4
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m5-clang-sanitizer.VWUaxI/build \
  -L milestone-5 -LE 'package|consumer|benchmark|negative' \
  --output-on-failure
ctest --test-dir /tmp/asc-cpp-m5-clang-sanitizer.VWUaxI/build \
  -R '^asc_cpp\.random_storage\.m5_random_negative_' \
  --output-on-failure
```

Results:

```text
ASan+UBSan applicable M5 matrix: 12/12 pass, no finding
expected negative compile contracts: 6/6 pass
```

### Independent compiler and ThreadSanitizer evidence

The independent verifier additionally ran strict direct GCC 11.4 and Clang 19
runtime, positive-contract, multi-TU, header-alone, and exception-disabled
compilation. All passed; all six negative sources failed compilation for their
intended reason.

Its fresh Clang 19 Debug/shared focused CMake selection passed:

```bash
ctest --test-dir /tmp/asc-m5-verifier-cmake-clang.hvXgt6 \
  -L milestone-5 -LE 'package|consumer' --output-on-failure
```

Result: **19/19** passed.

Clang 19 ThreadSanitizer results:

```text
complete sparse adversarial runtime suite: pass
dedicated dense disjoint-destination concurrency executable: pass
complete dense allocation-counter harness: skipped
```

The complete dense harness is not TSan-linkable because its deliberate global
allocation hooks conflict with TSan interceptors. The dedicated dense
concurrency path has runtime TSan evidence.

### Formatting and whitespace

```bash
find include src tests benchmarks -type f \
  \( -name '*.h' -o -name '*.cc' \) -print0 |
  sort -z |
  xargs -0 clang-format-19 --dry-run --Werror
git diff --check
```

Results: both passed after final integration.

## 4. Sanitizer, package, relocation, and isolated-consumer results

- ASan and UBSan: lead applicable M5 matrix 12/12 and independent final dense
  and sparse suites passed with no finding.
- TSan: full sparse and dedicated dense concurrency passed; the full dense
  allocation-hook harness was skipped for the documented interceptor conflict.
- Static package: passed with GCC 11.4.
- Shared package: passed with GCC 11.4/CMake 3.25 and with an independent
  Clang 19 installed-consumer matrix.
- Build-tree and copied-build-tree consumers: passed.
- Installed and relocated-prefix consumers: passed.
- Prefix and consumer paths containing spaces: passed.
- Required/optional/unknown component behavior: passed.
- Required and quiet no-component aggregate lookup: passed after correction.
- CMake package registry preservation and package-root isolation: passed.
- Subproject aggregate consumer: passed.
- Isolated `random`, `random_dense`, `random_sparse`, and `cpp` consumers:
  configured, built, linked, and ran.
- Dense-only and sparse-only consumers confirmed the forbidden sibling
  targets were not imported.
- All three exact examples in `docs/modules/random.md` passed format,
  GCC/Clang strict compile, installed-library link, and runtime checks.

## 5. CPU and GPU provider evidence

CPU evidence:

```text
backend: built-in deterministic serial reference
configuration: configure-tested
compilation: compile-tested with GCC 11.4 and Clang 19
runtime: runtime-tested
sequence/structure/state oracles: runtime-tested
allocation and rollback behavior: runtime-tested
optimized provider: absent and not claimed
```

The local CUDA 12.9 toolkit, driver 576.83, and NVIDIA RTX 3060 Laptop GPU
with 6144 MiB are inventory only. They were not configured, compiled, or run
for this CPU-only milestone.

Milestone 5 GPU evidence is exactly:

```text
classification: skipped
configure-tested: no
compile-tested: no
runtime-tested: no
parity-tested: no
```

There is no GPU option, target, provider discovery, source, device
compilation, execution, or CPU/GPU parity operation in the frozen milestone.

## 6. Independent review findings and resolutions

| Finding | Resolution and final evidence |
| --- | --- |
| M5-DOC-01 / M5-VERIFY-01: ordinary `StructurePriority` definition in a public header was not inline | Production made it explicitly `inline`; strict GCC/Clang two-TU link/run and the registered multi-TU regression passed. |
| M5-VERIFY-02: required no-component package test omitted `expect_core_target` | Lead supplied the required Boolean argument; the corrected test passed independently and both final 39/39 matrices passed. |
| Verifier positive contract used nonexistent `std::copy_assignable` | Test-only harness corrected to `std::is_copy_assignable_v`; strict positive and negative compiler matrices passed. |
| Root target-inventory test queried directory-scoped `Threads::Threads` from the deferred root context | Lead removed the irrelevant imported-target loop; approved product-target inventory and full matrices passed. |
| Subproject runner searched for a retired consumer executable name | Lead updated it to `asc_cpp_subproject_consumer`; subproject matrices passed. |
| New verification corpus initially failed repository formatting | Mechanical `clang-format-19` correction applied; final format and whitespace checks passed. |

Production self-review, independent verification, documentation/API review,
and portability/GPU/performance review all accept the corrected result. No
release-blocking finding remains.

## 7. Performance evidence

The lead's minimum-CMake Release/shared benchmark was run:

```bash
/tmp/asc-cpp-m5-shared-325.6kkKLI/build/tests/random_dense/\
asc_random_storage_benchmark
```

Observed:

```text
compiler=GCC 11.4.0
configuration=Release-like
backend=serial-reference
dense_shape=64x64
dense_layout=unique-padded-stride
dense_iterations=500
dense_allocations_in_operations=0
dense_elapsed_us=79342
dense_next_offset=2048000
dense_checksum=2046.81
sparse_shape=32x32
sparse_count=64
sparse_iterations=20
sparse_resource_allocation_calls=40
sparse_resource_deallocation_calls=40
sparse_elapsed_us=79137
sparse_next_structure_offset=40960
sparse_next_value_offset=2560
sparse_checksum=39724
```

The portability reviewer ran the GCC Release/static benchmark five more
times. Dense elapsed times were 34599, 35402, 35278, 35266, and 35724
microseconds (median 35278); sparse times were 39207, 41211, 40712, 41006,
and 42400 microseconds (median 41006). Every run retained the exact allocation
counts, offsets, and checksums above.

This is correctness, state-advance, allocation, and timing smoke evidence
only. There is no speed gate, stable regression threshold, optimized-provider
comparison, scalability claim, or performance guarantee. The repeated-scan
sparse selector and reference builder finalization are intentionally slow for
large valid inputs.

## 8. Remaining risks and retained work

- The worktree is a cumulative, intentionally uncommitted restart through
  Milestone 5. There is no clean commit boundary yet, and the owner's legacy
  tracked deletions remain mixed with cumulative M0--M5 additions.
- Hosted CI has not run because publication is not approved. Local evidence
  does not include native MSVC/Windows, AppleClang/macOS, multi-config
  generators, Ninja, or clang-tidy.
- The complete dense adversarial test cannot be linked under Clang TSan
  because its global allocation hooks conflict with TSan interceptors; the
  actual dense concurrency path has dedicated TSan runtime evidence.
- Only explicit host memory, serial execution, and unqualified
  `float`/`double` are implemented. GPU, device memory, asynchronous
  execution, provider random generation, and CPU/GPU parity remain later
  milestones.
- Sparse selection is `O(exact_count * logical_size)` and current builder
  finalization is quadratic in the selected count. Large valid cases can be
  impractically slow.
- The sparse structure algorithm is deterministic pseudorandom exact-count
  selection; formal statistical uniformity over subsets is not claimed.
- A caller-provided sparse `MemoryResource` must outlive the returned owner,
  its views, and final deallocation. Concurrent use remains subject to the
  destination/resource's own synchronization contract.
- Diagnostic `Status` messages may allocate. No-allocation claims cover
  successful computational storage/workspace only.
- Pre-1.0 source, ABI, and random-sequence evolution remains governed by
  accepted ADR 0018.
- The unrelated modified
  `/home/yicai/repo/MdeRepo/MdeCpp/Makefile` is retained and untouched.

None of these is an unresolved Milestone 5 release blocker.

License/provenance status:

```text
asc-cpp Apache-2.0 LICENSE SHA-256:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4

asc-cmake LICENSE SHA-256:
c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4

MdeCpp distinct license SHA-256:
230184f60bae2feaf244f10a8bac053c8ff33a183bcc365b4d8b876d2b7f4809
```

No MdeCpp source, test, literal vector, table, prose, notice, or mechanical
translation was copied. No third-party dependency or provider material was
added.

## 9. Proposed remote and branch-cleanup actions

No command in this section has been executed. The branch currently still
points at the baseline commit; pushing it now would not publish any uncommitted
M0--M5 work. Because the worktree contains the owner's cumulative restart and
retained deletions, publication must review and stage the complete intended
cumulative state rather than stage only the visibly new M5 paths.

After explicit publication approval, the proposed remote action is exactly:

```bash
cd /home/yicai/AI4SciComp/asc-cpp
git status --short
git diff --check
git add --all
git diff --cached --check
git diff --cached --stat
git commit -m "Rebuild asc-cpp through Milestone 5 random storage generation"
git push --set-upstream origin feature/asc-cpp-m5-random-storage-generation
```

Proposed remote:

```text
origin = git@github.com:AI4SciComp/asc-cpp.git
branch = feature/asc-cpp-m5-random-storage-generation
```

Only after the branch is separately reviewed and merged, and only after
separate branch-cleanup approval, the proposed cleanup is:

```bash
cd /home/yicai/AI4SciComp/asc-cpp
git switch main
git pull --ff-only origin main
git branch --merged main
git branch -d feature/asc-cpp-m5-random-storage-generation
git push origin --delete feature/asc-cpp-m5-random-storage-generation
```

The merged-branch check is mandatory. No force deletion is proposed.

Milestone 5 is stopped at Publication Checkpoint B pending owner approval.
