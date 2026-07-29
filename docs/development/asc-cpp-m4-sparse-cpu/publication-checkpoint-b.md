# Milestone 4 Publication Checkpoint B

Status: complete local candidate; publication not authorized

Date: 2026-07-28

Branch: `feature/asc-cpp-m4-sparse-cpu`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the exact cumulative, intentionally uncommitted Milestones 0--3
candidate recorded at the Milestone 3 Publication Checkpoint B

Corrections: None

## Scope result

The frozen **Milestone 4 — sparse CPU** contract is satisfied. The candidate
implements general-rank canonical coordinate storage, rank-two canonical
CSR/CSC storage, the six approved conversions, structure-preserving Sparse
evaluation, a storage-neutral placement/writable Expression protocol, and
deterministic serial CSR SpMV for exactly `float` and `double`.

The unreleased package candidate advances to 0.4.0 and makes `sparse` the
sixth available component. `ASC::sparse` is a genuine compiled static/shared
target with exactly `ASC::core;ASC::expression` as its direct and exported ASC
dependencies.

Separate production implementation, independent verification,
documentation/API, and portability/GPU/performance agents ran with disjoint
write scopes. The lead alone changed shared root CMake, package, CI,
architecture, manifest, registration, and integration paths. All accepted
findings are resolved and the exact stabilized post-review tree has been
revalidated.

No structural Sparse expression evaluation, hidden growth, Dense dependency,
Random storage facet, optimized provider, GPU provider, CSC/transpose SpMV,
SpMM, solver, factorization, preconditioner, aggregate, compatibility layer,
third-party dependency, or later-milestone API was implemented.

No commit, push, pull request, merge, tag, release, package-registry write, or
branch deletion occurred.

## 1. Changed files and reasons

This Milestone 4 layer is cumulative over the approved uncommitted Milestones
0--3 predecessor. Shared files can therefore contain changes from more than
one milestone. The exact repository state is reproduced by:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
```

At checkpoint freeze, the repository reports 334 staged predecessor paths, 34
tracked unstaged paths, 164 untracked files, and 420 porcelain status entries.
The tracked `AGENTS.md` deletion, the cumulative Milestones 0--3 work, and the
existing ignored `build/` tree are preserved. The unrelated modified
MdeCpp `Makefile` is outside this repository and remains untouched.

### Exact Sparse production surface

```text
include/asc/sparse.h
include/asc/sparse/compressed.h
include/asc/sparse/coordinate.h
include/asc/sparse/evaluate.h
include/asc/sparse/export.h
include/asc/sparse/linalg.h
src/sparse/CMakeLists.txt
src/sparse/reference_linalg.cc
```

These files provide the approved vocabulary, coordinate and compressed
owners/views, conversions, evaluation, exported SpMV API, genuine target, and
the sole compiled Sparse implementation source.

### Authorized predecessor compatibility surface

```text
include/asc/expression.h
include/asc/expression/expression.h
include/asc/expression/writable.h
include/asc/dense/view.h
src/expression/CMakeLists.txt
```

Expression gains only storage-neutral placement, alias-span, writable,
uniqueness, and access-validation protocol vocabulary. Dense views opt in to
that neutral protocol without including or depending on Sparse. Expression's
install file set includes the new public header.

### Root build, package, CI, architecture, and live documentation integration

```text
.github/workflows/ci.yml
CHANGELOG.md
CMakeLists.txt
CMakePresets.json
README.md
cmake/ASCCppComponents.cmake
cmake/ASCCppOptions.cmake
docs/README.md
docs/api.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/modules/core.md
docs/modules/dense.md
docs/modules/expression.md
docs/modules/random.md
docs/modules/sparse.md
docs/modules/utilities.md
```

These paths advance the candidate to 0.4.0, register and selectively export
Sparse, retain exact ASCCMake 0.1.0, update the six-component package closure,
add Sparse to local/hosted source validation, record the runtime-tested CPU
capabilities and skipped GPU scope, and document the public API and its
lifetime/complexity boundaries.

### Architecture, compile, runtime, package, consumer, and performance evidence

```text
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/m2_dependency_check.cmake
tests/compile/m4_dependency_check.cmake
tests/compile/m4_dense_interop.cc
tests/compile/m4_exceptions_disabled.cc
tests/compile/m4_header_compressed.cc
tests/compile/m4_header_coordinate.cc
tests/compile/m4_header_evaluate.cc
tests/compile/m4_header_export.cc
tests/compile/m4_header_linalg.cc
tests/compile/m4_header_sparse.cc
tests/compile/m4_header_writable.cc
tests/compile/m4_multi_tu.h
tests/compile/m4_multi_tu_a.cc
tests/compile/m4_multi_tu_b.cc
tests/compile/m4_multi_tu_main.cc
tests/compile/m4_negative_builder_copy.cc
tests/compile/m4_negative_const_value_mutation.cc
tests/compile/m4_negative_coordinate_owner_copy.cc
tests/compile/m4_negative_invalid_compressed_format.cc
tests/compile/m4_negative_non_extents_builder.cc
tests/compile/m4_negative_same_format_conversion.cc
tests/compile/m4_negative_structure_mutation.cc
tests/compile/m4_negative_unadapted_writable.cc
tests/compile/m4_negative_unsupported_element.cc
tests/compile/m4_negative_unsupported_spmv_scalar.cc
tests/compile/m4_negative_wrong_rank_spmv.cc
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/dense/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/sparse/CMakeLists.txt
tests/consumer/sparse/main.cc
tests/consumer/subproject/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/sparse/CMakeLists.txt
tests/sparse/allocation_probe.cc
tests/sparse/allocation_probe.h
tests/sparse/compressed_conversion_test.cc
tests/sparse/coordinate_test.cc
tests/sparse/evaluate_test.cc
tests/sparse/external_vector.h
tests/sparse/linalg_test.cc
tests/sparse/test_expression.h
tests/sparse/test_resources.h
tests/sparse/test_support.h
benchmarks/sparse/sparse_benchmark.cc
```

These paths enforce exact target/file/dependency boundaries; header
self-containment with exceptions enabled and disabled; multi-TU use; 11
expected compile failures; independent coordinate, CSR/CSC, conversion,
evaluation, and SpMV oracles; rollback and allocation evidence; an external
Sparse-only vector consumer; Dense interoperability only when explicitly
included; complete package relocation; and threshold-free allocation and
timing evidence.

The historical M2 audit now removes only the explicitly approved M4
`expression/writable.h` addition before checking the exact M2 layer. The M4
audit independently owns and validates that new file.

### Contract, provenance, ownership, and independent reviews

```text
docs/development/asc-cpp-m4-sparse-cpu/dependency-audit.md
docs/development/asc-cpp-m4-sparse-cpu/documentation-api-review.md
docs/development/asc-cpp-m4-sparse-cpu/milestone-contract.md
docs/development/asc-cpp-m4-sparse-cpu/ownership.md
docs/development/asc-cpp-m4-sparse-cpu/portability-review.md
docs/development/asc-cpp-m4-sparse-cpu/production-self-review.md
docs/development/asc-cpp-m4-sparse-cpu/provenance-record.md
docs/development/asc-cpp-m4-sparse-cpu/publication-checkpoint-b.md
docs/development/asc-cpp-m4-sparse-cpu/verification-design.md
docs/development/asc-cpp-m4-sparse-cpu/verification-review.md
```

These freeze the bounded contract, disjoint write ledger, clean-room
provenance, contract-first verification design, dependency audit, findings,
resolutions, remaining risks, and final evidence.

## 2. APIs, targets, and direct dependency changes

### Coordinate storage

The public coordinate family is:

```text
SparseElement
SparseViewElement
SparseExtents
DuplicatePolicy::{kReject,kSum}
ExplicitZeroPolicy::{kKeep,kDrop}
SparseCompressedFormat::{kCsr,kCsc}
CoordinateBuilder<Element, ExtentsType>
CoordinateArray<Element, ExtentsType>
CoordinateView<Element, Rank>
```

The builder is move-only, fixed-capacity, host-resource-backed, and has no
hidden growth. Consuming `Finalize` performs stable lexicographic
canonicalization, insertion-order duplicate handling, checked integral sums,
explicit-zero keep/drop, and transactional rollback. Finalized owners are
move-only; views are non-owning and trivially copyable, expose immutable
structure and const-propagating mutable values, and act as
structure-preserving readable/writable expression terminals.

### Compressed storage and conversions

The public compressed family is:

```text
CompressedSparseView<Element, Format>
CompressedSparseArray<Element, Format>
CsrView<Element>
CscView<Element>
CsrArray<Element>
CscArray<Element>
```

CSR/CSC are rank two, zero based, and canonical. The six named conversion
directions are coordinate-to-CSR, coordinate-to-CSC, CSR-to-coordinate,
CSC-to-coordinate, CSR-to-CSC, and CSC-to-CSR. Same-format compressed cloning
is constrained out. Conversions require explicit serial execution and a
destination host resource and create no hidden coordinate or dense temporary.

### Expression and evaluation compatibility

Expression adds:

```text
ExpressionAliasMetadata
ExpressionPlacementAdapter<T>
PlacedReadableExpression<T>
WritableExpressionAdapter<T>
WritableExpression<T>
ExpressionSpace(...)
ExpressionAlias(...)
WritableExpressionShape(...)
WritableExpressionAlias(...)
WritableExpressionIsUnique(...)
WriteExpression(...)
ValidateWritableExpressionAccess(...)
ExpressionMayOverlap(...)
```

Sparse `Evaluate` accepts only exact-rank, exact-shape,
structure-preserving expressions and caller-owned coordinate/CSR/CSC
destination structure. It preflights context, access, placement, effect,
shape, and overlap before mutation, allocates no workspace, and changes values
only.

### Serial reference linear algebra

The sole Sparse algebra operation is:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

It supports canonical CSR and exactly `float`/`double`; validates serial/host
access, rank-one vector lengths, output uniqueness, and overlap; traverses
deterministically; and does not read old output when `beta == 0`.

### Targets, package, and dependencies

```text
build target       consumer target    kind       direct ASC dependencies
asc_core           ASC::core          compiled   none
asc_utilities      ASC::utilities     compiled   ASC::core
asc_expression     ASC::expression    interface  ASC::core
asc_dense          ASC::dense         compiled   ASC::core;ASC::expression
asc_sparse         ASC::sparse        compiled   ASC::core;ASC::expression
asc_random         ASC::random        compiled   ASC::core
```

Available components are exactly `core`, `utilities`, `expression`, `dense`,
`sparse`, and `random`. Required `sparse` expands exactly to
`core;expression;sparse`. `random_dense`, `random_sparse`, `cpp`, and every
provider facet remain unavailable.

Direct external link/runtime dependencies added by Milestone 4: none beyond
platform C++ runtimes. Configure-time dependency: exact released ASCCMake
0.1.0 at `8a7dcbad3a97267cce59810aff24de800a3497a7`. No dependency was added.

## 3. Exact commands and pass/fail/skip results

### Repository, branch, and predecessor preflight

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m3-dense-cpu \
  feature/asc-cpp-m4-sparse-cpu
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
git worktree list --porcelain
git -C ../asc-cmake status --short --branch
git -C ../asc-cmake rev-parse HEAD
git -C ../asc-cmake describe --tags --exact-match HEAD
git -C /home/yicai/repo/MdeRepo/MdeCpp status --short --branch
git -C /home/yicai/repo/MdeRepo/MdeCpp rev-parse HEAD
```

Result: pass. The active branch is
`feature/asc-cpp-m4-sparse-cpu`. HEAD, `main`, `origin/main`, and both named
milestone branches are
`33b261ea33616a6395c4ad3b20646093103344f7`. There is one ASCCpp worktree.
The intentionally dirty predecessor matches its prior checkpoint; no
unexpected divergence was found.

ASCCMake is clean/current `main`, exact tag `v0.1.0`, at `8a7dcba`. MdeCpp is
current `main` at `f6294e9`; its pre-existing modified `Makefile` is untouched.

Repository `main:AGENTS.md`, the architecture package, ADRs 0001--0018,
dependency/capability manifests, backend matrix, previous milestone report,
repository audit, implementation/testing/CI/ASCCMake guidance, and the
supplied runbook instructions were read before freezing the contract. No
material design owner decision remained unresolved.

### Final GCC Debug/static

```sh
cmake -S . -B /tmp/asc-cpp-m4-gcc-debug \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m4-gcc-debug --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-gcc-debug \
  -C Debug --output-on-failure -j 4
```

Result: pass with CMake 4.1.2 and GCC 11.4, **141/141**; zero failed.

### Final Clang Release/shared

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m4-clang-release-shared \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m4-clang-release-shared --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-clang-release-shared \
  -C Release --output-on-failure -j 4
```

Result: pass with CMake 4.1.2 and Clang 19.0.0, **141/141**; zero
failed.

### Minimum CMake GCC Release/static

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . -B /tmp/asc-cpp-m4-cmake325-gcc-release \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build /tmp/asc-cpp-m4-cmake325-gcc-release --parallel 4
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir /tmp/asc-cpp-m4-cmake325-gcc-release \
  -C Release --output-on-failure -j 4
```

Result: pass with CMake 3.25.0 and GCC 11.4, **141/141**; zero failed.

All three complete matrices were rerun after verification froze its final
tests. Each includes 11 configure-time expected compile failures; those
sources failed compilation as required.

### Formatting, dependency, and static analysis

```sh
clang-format-19 --dry-run --Werror \
  include/asc/sparse.h include/asc/sparse/*.h \
  include/asc/expression.h include/asc/expression/expression.h \
  include/asc/expression/writable.h include/asc/dense/view.h \
  src/sparse/reference_linalg.cc \
  tests/sparse/*.cc tests/sparse/*.h \
  tests/compile/m4_*.cc tests/compile/m4_*.h \
  tests/consumer/sparse/main.cc \
  benchmarks/sparse/sparse_benchmark.cc
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m4_dependency_check.cmake
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m3_dependency_check.cmake
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m2_dependency_check.cmake
git diff --check
command -v clang-tidy-19 || command -v clang-tidy
```

Result: format, dependency inventories/policies, and patch whitespace pass.
No clang-tidy executable exists, so local clang-tidy is **skipped** and
remains a hosted-CI gate.

### Resolved initial failures

The first GCC full run passed 139/141 and failed:

1. the coordinate overflow oracle used an NNZ larger than the `(3,4)` logical
   domain, so the implementation correctly returned `kShape` before the
   expected byte-overflow check; and
2. the historical exact M2 inventory rejected the approved additive M4
   Expression header.

Verification changed the overflow probe to valid shape
`(numeric_limits<extent_t>::max(),1)` with maximum NNZ, reaching the intended
checked byte overflow. The lead made the historical M2 audit ignore only the
M4-owned header before checking M2. Both focused reruns passed, followed by
the three clean 141/141 post-review matrices above.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### ASan and UBSan

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m4-clang-asan-ubsan \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m4-clang-asan-ubsan --parallel 4
env ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:print_stacktrace=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/asc-cpp-m4-clang-asan-ubsan \
    -C Debug --output-on-failure -I 1,125 -j 4
```

Result: **125/125** compatible instrumented architecture, compile, Core,
Utilities, Expression, Dense, Sparse, Random, and benchmark tests pass with
zero ASan/UBSan diagnostics.

The 16 independently configured package/consumer subprocess tests are
excluded because sanitizer development link flags are not an installed usage
requirement. They pass in every uninstrumented complete matrix.

### Standalone LSan and TSan

The two standalone configurations use the same common arguments as above,
replace the ASan/UBSan options with exactly one of:

```sh
-DASC_CPP_ENABLE_LEAK_SANITIZER=ON
-DASC_CPP_ENABLE_THREAD_SANITIZER=ON
```

Their compatible target set is:

```text
asc_sparse_m4_header_sparse
asc_sparse_m4_header_coordinate
asc_sparse_m4_header_compressed
asc_sparse_m4_header_evaluate
asc_sparse_m4_header_linalg
asc_sparse_m4_header_writable
asc_sparse_m4_dense_interop
asc_sparse_multi_tu
asc_sparse_coordinate_test
asc_sparse_compressed_conversion_test
```

Exact runtime selections:

```sh
env LSAN_OPTIONS=exitcode=23:report_objects=1 \
  ctest --test-dir /tmp/asc-cpp-m4-clang-lsan \
    -C Debug --output-on-failure \
    -R '^asc_cpp\.(compile\.m4_(header_(sparse|coordinate|compressed|evaluate|linalg|writable)|dense_interop)|sparse\.(multi_tu|coordinate_test|compressed_conversion_test))$' \
    -j 4

env TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1 \
  ctest --test-dir /tmp/asc-cpp-m4-clang-tsan \
    -C Debug --output-on-failure \
    -R '^asc_cpp\.(compile\.m4_(header_(sparse|coordinate|compressed|evaluate|linalg|writable)|dense_interop)|sparse\.(multi_tu|coordinate_test|compressed_conversion_test))$' \
    -j 4
```

Result: LSan **10/10** and TSan **10/10**, with zero diagnostics.
Allocation-probe executables are intentionally outside these standalone
compatible subsets; the complete allocation-sensitive suite is covered by
ASan+UBSan.

### Package, relocation, and isolated consumers

Tests 126--141 in each complete matrix cover:

- Core, Utilities, Expression, Dense, Sparse, and Random build-tree
  consumers;
- all six installed-and-relocated isolated component consumers;
- the cumulative foundations subproject consumer;
- build-tree component selection;
- installed package relocation/component selection; and
- package-registry preservation.

Result: **16/16** pass in each of the three complete matrices. Paths containing
spaces, static/shared libraries, copied build trees, installed and moved
prefixes, required/optional/unavailable components, exact Sparse dependency
closure, Sparse-only external vector use, and absence of forbidden sibling
targets all pass.

Dynamic inspection command:

```sh
readelf -d \
  '/tmp/asc-cpp-m4-clang-release-shared/tests/consumer/sparse install relocate work/relocated prefix with spaces/lib/libasc_sparse.so'
llvm-nm-19 -D --defined-only -C \
  '/tmp/asc-cpp-m4-clang-release-shared/tests/consumer/sparse install relocate work/relocated prefix with spaces/lib/libasc_sparse.so'
```

Result: installed `libasc_sparse.so` has `RUNPATH [$ORIGIN]`, needs
`libasc_core.so` plus platform C++ runtimes, and exports exactly the approved
`float` and `double` `SpmvReference` bridge symbols. The installed
`ASC::sparse` target records
`INTERFACE_LINK_LIBRARIES "ASC::core;ASC::expression"`.

## 5. CPU/GPU provider evidence

CPU serial reference evidence is **runtime-tested**:

- canonical coordinate and compressed storage plus all six conversions;
- structure-preserving evaluation;
- float/double CSR SpMV numerical oracles;
- empty/degenerate and beta-zero behavior;
- rollback, alias, placement, uniqueness, and allocation checks; and
- external Sparse-only and explicit Dense-interoperability vector adapters.

There is no separately selectable CPU provider component or fallback path;
the compiled serial reference SpMV bridge belongs to the base Sparse target.

GPU evidence is exactly **skipped**.

Milestone 4 contains no GPU option, discovery, language enablement, SDK
include/link, target, component, dispatch, runtime, or parity path. Hardware
inventory is not relabeled as configure-tested, compile-tested,
runtime-tested, or parity-tested.

## 6. Review findings and resolutions

| Finding | Resolution |
| --- | --- |
| Generic SpMV could not prove an external output had a unique logical mapping. | The frozen protocol clarification requires `IsUnique` and exposes `WritableExpressionIsUnique`; SpMV rejects nonunique output before mutation. |
| `CoordinateArray::Create` initially copied caller storage without completing canonical validation. | It now validates a const external coordinate view before any allocation or publication. |
| Rank-zero coordinate paths could form `nullptr + 0`. | Access, lookup, validation, sorting, compaction, and owner-copy paths now branch without null-pointer arithmetic. |
| Coordinate views admitted Boolean/non-arithmetic elements. | `SparseViewElement` accepts only mutable/const nonvolatile forms of a valid `SparseElement`. |
| Unknown duplicate/zero policy enum values could follow an ordinary branch. | Both policy enums are validated before sorting or mutation. |
| Invalid compressed formats and same-format `FromCompressed` exceeded the approved surface. | Invalid formats are rejected and `FromCompressed` requires the opposite format. |
| Several headers relied on transitive declarations. | Direct standard, Core, and writable-protocol includes were added; standalone parsing passes. |
| Wrong-rank Sparse evaluation returned a runtime status but still instantiated an invalid destination-rank read. | Equal-rank traversal is compile-time guarded; mismatches compile and return `kShape` transactionally. |
| External pointer spans checked byte counts but not complete integer address ranges. | Coordinate, value, index, and offset spans reject `uintptr_t` end overflow. |
| The first coordinate overflow oracle violated canonical shape first. | Verification retained the overflow assertion with a valid maximum logical shape. |
| The historical M2 exact inventory rejected the approved M4 Expression addition. | The M2 audit removes only the M4-owned file; the exact M4 audit owns it. |

Production self-review, independent verification, documentation/API review,
and portability/GPU/performance review all report no unresolved blocker.

## 7. Performance evidence

The benchmark uses deterministic CSR shape `128 x 256`, NNZ 512, 64
structure-preserving evaluation iterations, and 256 SpMV iterations. It
records compiler/configuration, shape, NNZ, format, operation, allocations,
elapsed time, and checksum without a speed gate.

Independent GCC 11.4 Release/shared evidence:

```text
operation=structure_preserving_evaluate iterations=64
total_ns=12030979 per_iteration_ns=187984
checksum=-574.125 allocations=0

operation=spmv iterations=256
total_ns=1840564 per_iteration_ns=7189.7
checksum=977.375 allocations=0
```

Lead Clang 19 Release/shared rerun:

```text
operation=structure_preserving_evaluate iterations=64
total_ns=14237001 per_iteration_ns=222453
checksum=-574.125 allocations=0

operation=spmv iterations=256
total_ns=2287779 per_iteration_ns=8936.64
checksum=977.375 allocations=0
```

Both operations report zero process allocations. The benchmark holds only
CSR-sized buffers and constructs no dense matrix or hidden conversion
workspace. Timings are observations, not regression thresholds or provider
comparisons.

## 8. Remaining risks

- Native MSVC and AppleClang compilation/runtime are not available on this
  Linux workstation; their existing CI jobs remain the next authoritative
  evidence after publication is approved.
- No local clang-tidy executable exists; clang-tidy remains a hosted-CI gate.
- External raw views and adapters remain responsible for truthful pointer
  provenance, allocation length, alignment, lifetime, shape, placement,
  alias, and uniqueness metadata.
- Coordinate finalization, several conversions, and compressed evaluation use
  documented deterministic repeated-scan reference algorithms. They are not
  optimized for large inputs and have no unstable speed threshold.
- The candidate is cumulative and uncommitted. Publication must audit the
  exact 334 staged, 34 tracked-unstaged, and 164 untracked-file manifest
  before creating its first commit; no remote branch yet contains this local
  candidate.

## 9. Exact proposed remote and branch-cleanup actions

None of the following commands has been run. They require a separate owner
publication approval.

Because the validated candidate is cumulative over Milestones 0--3, first
freeze and inspect the exact intended commit:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
git add -A -- .
git diff --cached --check
git diff --cached --stat
git commit -m "Add asc-cpp Milestone 4 Sparse CPU foundation"
```

Publish only the approved feature branch and open the pull request:

```sh
git push --set-upstream origin feature/asc-cpp-m4-sparse-cpu
gh pr create \
  --base main \
  --head feature/asc-cpp-m4-sparse-cpu \
  --title "Milestone 4: Sparse CPU" \
  --body-file docs/development/asc-cpp-m4-sparse-cpu/publication-checkpoint-b.md
```

After all required CI and independent review pass, inspect and merge without a
force push or direct push to `main`:

```sh
gh pr checks <PR_NUMBER> --watch
gh pr view <PR_NUMBER> --json reviewDecision,statusCheckRollup
gh pr merge <PR_NUMBER> --merge --delete-branch=false
```

Only after merge, verify ancestry and worktree safety before deleting the M4
branch individually:

```sh
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor feature/asc-cpp-m4-sparse-cpu main
git worktree list --porcelain
git branch -d feature/asc-cpp-m4-sparse-cpu
git push origin --delete feature/asc-cpp-m4-sparse-cpu
```

No other local or remote branch is proposed for deletion. The M0--M3 roadmap
bookmarks, later-milestone bookmarks, and the unique remote M8 hardening
branch require separate ancestry/unique-commit audits and are outside this
checkpoint.

The roadmap identifies Milestone 4 as an unreleased 0.4.0 candidate, not a
release boundary. No tag or GitHub release action is proposed.
