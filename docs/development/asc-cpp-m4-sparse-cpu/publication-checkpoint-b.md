# Milestone 4 Publication Checkpoint B

Status: Ready for owner review; no publication action taken

Date: 2026-07-26

Approved milestone: **Milestone 4 — sparse CPU**

Corrections: None from the owner

Branch: `feature/asc-cpp-m4-sparse-cpu`

Baseline and unchanged `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

This checkpoint covers only the frozen Milestone 4 contract. The intentionally
uncommitted Milestones 0--3 restart work and the owner's earlier deletion of
the retired implementation remain in the worktree. No commit, push, merge,
tag, release, branch deletion, or other publication action occurred.

## 1. Changed files and reasons

### Contract, ownership, audit, and review evidence

```text
docs/development/asc-cpp-m4-sparse-cpu/milestone-contract.md
docs/development/asc-cpp-m4-sparse-cpu/ownership.md
docs/development/asc-cpp-m4-sparse-cpu/dependency-audit.md
docs/development/asc-cpp-m4-sparse-cpu/provenance-record.md
docs/development/asc-cpp-m4-sparse-cpu/production-self-review.md
docs/development/asc-cpp-m4-sparse-cpu/verification-design.md
docs/development/asc-cpp-m4-sparse-cpu/verification-review.md
docs/development/asc-cpp-m4-sparse-cpu/documentation-api-review.md
docs/development/asc-cpp-m4-sparse-cpu/portability-review.md
docs/development/asc-cpp-m4-sparse-cpu/publication-checkpoint-b.md
```

These files freeze the approved scope and disjoint ownership, record the
dependency/provenance boundary, preserve each independent review, and provide
this publication checkpoint.

### Production API and implementation

```text
include/asc/sparse.h
include/asc/sparse/export.h
include/asc/sparse/coordinate.h
include/asc/sparse/compressed.h
include/asc/sparse/evaluate.h
include/asc/sparse/linalg.h
src/sparse/reference_linalg.cc
```

These are the exact new sparse production files. They implement the public
umbrella/export surface, general-rank canonical coordinate storage, canonical
rank-two CSR/CSC storage, all six approved conversions, sparse expression
evaluation, and the compiled serial float/double CSR SpMV kernel.

```text
include/asc/expression/expression.h
include/asc/expression/writable.h
include/asc/expression.h
include/asc/dense/view.h
```

These are the only approved predecessor-module compatibility changes. They
add the storage-neutral placed/writable protocol, span-aware conservative alias
metadata, and a dense-view protocol specialization. They add no storage,
evaluation, sparse include, or sparse target edge to expression or dense.

### Build, package, CI, and architecture integration

```text
.github/workflows/ci.yml
CMakeLists.txt
CMakePresets.json
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
src/expression/CMakeLists.txt
src/sparse/CMakeLists.txt
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/dependency_check.cmake
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/dense/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/run_subproject_consumer.cmake
tests/consumer/sparse/CMakeLists.txt
tests/consumer/subproject/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/sparse/CMakeLists.txt
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/release-roadmap.md
```

These changes register the compiled sparse target, install/export it as an
independent component, advance the unreleased package candidate to 0.4.0,
register all M4 tests and consumers, audit the approved graph/inventory, add
the sparse CI/preset surface, and update the accepted architecture manifests.

### Verification, consumers, and benchmark

```text
tests/sparse/test_support.h
tests/sparse/allocation_counter.h
tests/sparse/allocation_counter.cc
tests/sparse/coordinate_test.cc
tests/sparse/compressed_test.cc
tests/sparse/conversion_test.cc
tests/sparse/evaluate_test.cc
tests/sparse/linalg_test.cc
tests/compile/m4_sparse_contract.cc
tests/compile/m4_sparse_multi_tu.h
tests/compile/m4_sparse_multi_tu_a.cc
tests/compile/m4_sparse_multi_tu_b.cc
tests/compile/m4_sparse_multi_tu_main.cc
tests/compile/m4_sparse_negative_const_mutation.cc
tests/compile/m4_sparse_negative_invalid_extents.cc
tests/compile/m4_sparse_negative_missing_writable.cc
tests/compile/m4_sparse_negative_owner_copy.cc
tests/compile/m4_sparse_negative_spmv_integral.cc
tests/compile/m4_sparse_negative_unsupported_element.cc
tests/consumer/sparse/main.cc
benchmarks/sparse/allocation_free_benchmark.cc
```

These independently exercise structural/numerical oracles, transactional
failure, compile contracts, multi-translation-unit behavior, isolated package
use, and allocation/no-densification behavior.

### User documentation and release notes

```text
README.md
CHANGELOG.md
docs/README.md
docs/api.md
docs/modules/expression.md
docs/modules/sparse.md
```

These document the new component, actual API, neutral writable/alias protocol,
canonical formats, explicit costs and obligations, installed example,
provider boundary, and unreleased 0.4.0 status.

The repository remains cumulatively dirty by design. Existing tracked
deletions are the owner's retained restart state, not Milestone 4 deletion
work. The unrelated modified MdeCpp `Makefile` remains untouched.

## 2. APIs, targets, and direct dependencies

### Exact target and six-module graph

```text
asc_core        / ASC::core        -> []
asc_utilities   / ASC::utilities   -> [ASC::core]
asc_expression  / ASC::expression  -> [ASC::core]
asc_random      / ASC::random      -> [ASC::core]
asc_dense       / ASC::dense       -> [ASC::core, ASC::expression]
asc_sparse      / ASC::sparse      -> [ASC::core, ASC::expression]
```

`asc_sparse` is a genuine compiled static/shared library. Its direct build and
interface links are exactly `ASC::core;ASC::expression`. It has no utilities,
dense, random, aggregate, retired array/linalg, optional-provider, or SDK edge.

The available package components are exactly:

```text
core utilities expression random dense sparse
```

Required component `sparse` expands in package configuration to:

```text
core expression sparse
```

`random_dense`, `random_sparse`, `cpp`, and every provider facet remain
unavailable and unexported.

### Public sparse API

The public sparse vocabulary and owners/views are:

```text
SparseElement
DuplicatePolicy::{kReject,kSum}
ExplicitZeroPolicy::{kKeep,kDrop}
SparseCompressedFormat::{kCsr,kCsc}

CoordinateBuilder<Element, ExtentsType>
CoordinateArray<Element, ExtentsType>
CoordinateView<Element, Rank>

CompressedSparseView<Element, Format>
CompressedSparseArray<Element, Format>
CsrView<Element>
CscView<Element>
CsrArray<Element>
CscArray<Element>
```

Named conversions cover canonical coordinate to CSR/CSC, CSR/CSC to canonical
coordinate, and CSR-to-CSC/CSC-to-CSR. Sparse owns
structure-preserving `Evaluate` into existing coordinate/compressed
destinations and:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

for serial host CSR and exactly `float` or `double`.

The expression-owned protocol adds:

```text
AliasToken::FromAddressSpan
AliasTokensMayOverlap
ExpressionPlacementAdapter<T>
PlacedReadableExpression<T>
WritableExpressionAdapter<T>
WritableExpression<T>
ExpressionSpace(...)
WritableExpressionShape(...)
WritableExpressionAlias(...)
WriteExpression(...)
```

Identity-token equality remains supported. Span-aware comparisons are
symmetric and conservative. Base readable/placed protocols reject top-level
volatile descriptors; writable additionally rejects top-level const.

### ASCCMake contract

The exact consumed ASCCMake release remains:

```text
repository: /home/yicai/AI4SciComp/asc-cmake
remote: git@github.com:AI4SciComp/asc-cmake.git
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

No ASCCMake API was invented.

## 3. Exact validation commands and results

All commands below were run from
`/home/yicai/AI4SciComp/asc-cpp`. Every listed configure/build/test command
passed unless a result is explicitly classified as skipped.

### Independent GCC 11.4 debug/static full matrix

```bash
cmake -S . \
  -B /tmp/asc-cpp-m4-verifier-install.3cf715/build \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake
cmake --build /tmp/asc-cpp-m4-verifier-install.3cf715/build --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-verifier-install.3cf715/build \
  --output-on-failure --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-verifier-install.3cf715/build \
  --output-on-failure -R '^asc_cpp\.sparse\.'
```

Results after every review correction:

```text
configure/build: pass
full CTest: 138/138 pass
M4-labelled tests in full suite: 24 pass
consumer tests in full suite: 13 pass
package tests in full suite: 16 pass
focused sparse CTest: 14/14 pass
```

The focused set contains five runtime tests, the positive concept contract,
multi-TU link, six expected compile failures, and the allocation benchmark.

### Lead GCC 11.4 debug/static install-enabled matrix

```bash
cmake -S . -B /tmp/asc-cpp-m4-gcc-debug.cFM5Ju \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build /tmp/asc-cpp-m4-gcc-debug.cFM5Ju --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-gcc-debug.cFM5Ju \
  --output-on-failure --parallel 4
```

Result: strict configure/build and the full **138/138** matrix passed. After
the last volatile-boundary correction, the tree was rebuilt and the sparse
linalg/compile contracts plus all four architecture tests passed again. The
independent full GCC matrix above then supplied the final-state full-suite
recheck.

### Lead Clang 19 debug/shared full matrix

```bash
cmake -S . -B /tmp/asc-cpp-m4-clang-shared.ypYA22 \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build /tmp/asc-cpp-m4-clang-shared.ypYA22 --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-clang-shared.ypYA22 \
  --output-on-failure --parallel 4
```

Result after all corrections: configure/build passed; **138/138** passed.

### Minimum CMake 3.25, GCC 11.4 release/static full matrix

```bash
/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake \
  -S . -B /tmp/asc-cpp-m4-cmake325-release.DkarKI \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
/tmp/asc-cmake-3.25.pTlOcP/venv/bin/cmake \
  --build /tmp/asc-cpp-m4-cmake325-release.DkarKI --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-cmake325-release.DkarKI \
  --output-on-failure --parallel 4
```

Result after all corrections: configure/build passed; **138/138** passed.

### Clang 19 ASan and UBSan

Lead complete non-package/consumer runtime matrix:

```bash
cmake -S . -B /tmp/asc-cpp-m4-clang-asan-ubsan.dCfU03 \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-release
cmake --build /tmp/asc-cpp-m4-clang-asan-ubsan.dCfU03 --parallel 4
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m4-clang-asan-ubsan.dCfU03 \
  --output-on-failure --parallel 4 -LE 'package|consumer'
```

Result after all corrections: **122/122** passed with no ASan, leak, or UBSan
finding. Package/consumer processes were intentionally excluded from this
instrumented run; the noninstrumented static/shared matrices cover them.

The independent verifier also configured the focused sanitizer build at
`/tmp/asc-cpp-m4-verifier-san.on2Fhn/build`, confirmed generated compile/link
commands contained `-fsanitize=address,undefined`, and ran:

```bash
ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:handle_segv=0:detect_leaks=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/asc-cpp-m4-verifier-san.on2Fhn/build \
  --output-on-failure -R '^asc_cpp\.sparse\.'
```

Result: **14/14** passed with no sanitizer finding.

### Strict headers, exceptions-disabled compilation, and formatting

Each new or modified public header and `src/sparse/reference_linalg.cc`
passed GCC 11 and Clang 19 strict C++20 syntax checks, both with and without
exceptions:

```bash
<compiler> -std=c++20 [-fno-exceptions] -pedantic-errors \
  -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
  -Iinclude -x c++ -fsyntax-only <public-header>

<compiler> -std=c++20 [-fno-exceptions] -pedantic-errors \
  -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
  -DASC_SPARSE_BUILDING_LIBRARY -Iinclude \
  -fsyntax-only src/sparse/reference_linalg.cc

clang-format-19 --dry-run --Werror \
  include/asc/expression/expression.h \
  include/asc/expression/writable.h include/asc/expression.h \
  include/asc/dense/view.h include/asc/sparse.h include/asc/sparse/*.h \
  src/sparse/reference_linalg.cc \
  benchmarks/sparse/allocation_free_benchmark.cc \
  tests/consumer/sparse/main.cc tests/sparse/*.cc tests/sparse/*.h \
  tests/compile/m4_sparse_*.cc tests/compile/m4_sparse_*.h

git diff --check
```

Results: all passed. `clang-tidy` and Ninja were not available locally and are
not claimed.

## 4. Sanitizer, package, relocation, and isolated-consumer results

- ASan/UBSan: 122/122 lead non-package/consumer tests and 14/14 independent
  focused sparse tests passed with no finding.
- Static package: passed under GCC 11.4.
- Shared package: passed under Clang 19.
- Minimum-CMake package: passed under CMake 3.25/GCC 11.4.
- Build-tree consumer: passed.
- Copied-build-tree consumer: passed.
- Installed consumer: passed.
- Relocated-prefix consumer: passed.
- Prefix and build paths containing spaces: passed.
- Component-by-component build/install and unavailable-component rejection:
  passed.
- CMake package registry and package-root isolation: passed.
- Sparse-only installed consumer: compiled, linked only `ASC::sparse`, rejected
  imported sibling/aggregate/provider targets, and ran CSR SpMV with an
  external vector adapter.
- Subproject consumer: passed without weakening component isolation.
- The exact example in `docs/modules/sparse.md` was extracted byte-for-byte,
  formatted, configured against a fresh installed prefix, compiled with
  warnings as errors, linked, and run. It produced the expected `{5.0, 6.0}`.

## 5. CPU and GPU provider evidence

CPU evidence:

```text
backend: built-in deterministic serial reference
configuration: configure-tested
compilation: compile-tested with GCC 11.4 and Clang 19
runtime: runtime-tested
allocation behavior: runtime-observed
optimized provider: absent and not claimed
```

The locally installed BLAS/LAPACK, oneMKL, CUDA 12.9, cuSPARSE, and RTX 3060
were inventory only. No optional provider was discovered, linked, loaded, or
invoked.

GPU evidence is exactly:

```text
skipped
```

It is not configure-tested, compile-tested, runtime-tested, or parity-tested.
Milestone 4 contains no GPU target, option, source, SDK dependency, device
storage, kernel, execution, or parity operation.

## 6. Independent review findings and resolutions

| Finding | Resolution and evidence |
| --- | --- |
| M4-DOC-01: top-level const destination satisfied `WritableExpression` | Reject top-level const at the public writable concept boundary; negative concept oracle passed. |
| M4-DOC-02: external values could overlap sparse structure | Validate checked physical spans and reject every relevant pairwise overlap before publication; runtime cases passed. |
| M4-DOC-03: host-style iterable spans on non-host views | Expose raw address descriptors only; checked dereference remains host-only. |
| M4-DOC-04: incomplete coordinate SpMV output silently missed writes | Require full canonical rank-one coordinate coverage before mutation; failure is unchanged and transactional. |
| M4-DOC-05: overloaded unary address-of broke external adapters | Use `std::addressof`; deleted-`operator&` adapter compiled and ran. |
| M4-DOC-06: volatile base readable concept mismatch | Reject top-level volatile and const-volatile at `ReadableExpression`; GCC/Clang negative oracles passed. |
| M4-VERIFY-RANK: rank mismatch instantiated an invalid evaluation body | Gate mutation with `if constexpr`; mismatched/rank-zero calls compile, reject, and do not mutate. |
| M4-LEAD-ALIAS: independently created sparse spans could partially overlap | Publish and symmetrically compare validated physical spans; both overlap directions reject. |
| M4-PORT-01: identity-only neutral alias tokens missed partial overlap | Add checked `AliasToken::FromAddressSpan` and `AliasTokensMayOverlap`; dense/sparse roots publish spans. |
| M4-PORT-02: volatile placed/writable descriptors reached incompatible adapters | Reject volatile at protocol concept boundaries while retaining const readable/placed use. |

Production self-review, independent verification, documentation/API review,
and portability/GPU/performance review all accepted the corrected result. No
release-blocking finding remains.

## 7. Performance evidence

The Release serial-reference allocation/no-densification benchmark was run:

```bash
/tmp/asc-cpp-m4-cmake325-release.DkarKI/tests/sparse/\
asc_sparse_allocation_free_benchmark
```

Observed:

```text
compiler=GCC 11.4.0
configuration=Release-like
backend=serial-reference
format=CSR-zero-based-canonical
shape=128x128
nnz=382
vector_layout=contiguous-external
iterations=200
operation=spmv
allocations_in_operations=0
elapsed_us=204
checksum=382
```

The independent verifier observed 501 microseconds in a separate Release run;
the portability review observed 2595 microseconds in a Debug-like run. These
are functional smoke observations only. There is no speed threshold,
historical baseline, optimized-provider comparison, scalability claim, or
performance guarantee. The accepted claim is zero operation allocations and
no densification for the exercised successful serial-reference operation.

## 8. Remaining risks and retained work

- The worktree is a cumulative, intentionally uncommitted restart through
  Milestone 4. There is no clean commit boundary yet, and the owner's legacy
  tracked deletions remain mixed with cumulative M0--M4 additions.
- Hosted CI has not run because publication is not approved. Local evidence
  does not include MSVC, AppleClang, Ninja, clang-tidy, ThreadSanitizer, or a
  GPU runtime.
- External adapters must truthfully report complete physical spans or another
  conservative root identity, shape, placement, total unique writability, and
  lifetime. Concepts cannot prove those runtime facts.
- External raw pointers and non-host descriptors retain caller obligations for
  provenance, length, alignment, lifetime, and actual accessibility.
- Coordinate finalization and reference conversions intentionally use
  transparent deterministic correctness algorithms that can be quadratic or
  repeated-scan. No optimized CPU performance claim is made.
- Only float/double serial CSR SpMV is present. CSC SpMV, transpose, SpMM,
  solvers, optimized providers, device storage, asynchronous execution, and
  sparse GPU work remain later milestones.
- Diagnostic `Status` strings may allocate. The successful-operation
  no-allocation claim does not cover rejected calls or undisclosed allocation
  inside an external adapter.
- Pre-1.0 public/ABI evolution remains possible under the accepted release
  policy.
- The unrelated modified
  `/home/yicai/repo/MdeRepo/MdeCpp/Makefile` is retained and untouched.

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

No command in this section has been executed. Because the worktree contains
the owner's cumulative restart and retained deletions, publication must review
and stage the complete intended cumulative state rather than stage only the
visibly new Milestone 4 paths.

After explicit publication approval, the proposed remote action is exactly:

```bash
cd /home/yicai/AI4SciComp/asc-cpp
git status --short
git diff --check
git add --all
git diff --cached --check
git diff --cached --stat
git commit -m "Rebuild asc-cpp through Milestone 4 sparse CPU"
git push --set-upstream origin feature/asc-cpp-m4-sparse-cpu
```

Proposed remote:

```text
origin = git@github.com:AI4SciComp/asc-cpp.git
branch = feature/asc-cpp-m4-sparse-cpu
```

Only after the branch is separately reviewed and merged, and only after
separate branch-cleanup approval, the proposed cleanup is:

```bash
cd /home/yicai/AI4SciComp/asc-cpp
git switch main
git pull --ff-only origin main
git branch --merged main
git branch -d feature/asc-cpp-m4-sparse-cpu
git push origin --delete feature/asc-cpp-m4-sparse-cpu
```

The merged-branch check is mandatory. No force deletion is proposed.

Milestone 4 is stopped at Publication Checkpoint B pending owner approval.
