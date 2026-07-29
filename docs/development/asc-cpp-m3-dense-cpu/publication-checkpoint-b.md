# Milestone 3 Publication Checkpoint B

Status: complete local candidate; publication not authorized

Date: 2026-07-28

Branch: `feature/asc-cpp-m3-dense-cpu`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the complete staged Milestone 0 and Milestone 1 candidate plus
the exact unstaged/untracked Milestone 2 layer recorded at its Publication
Checkpoint B, advanced without a commit or publication

## Scope result

The frozen Milestone 3 Dense CPU contract is satisfied with no correction to
the owner-approved scope. The candidate implements checked Dense mappings,
views, host ownership, pointwise evaluation, reductions, and the exact serial
reference linear-algebra subset. It advances the unreleased package candidate
to 0.3.0 and makes `dense` the fifth available component.

Separate production implementation, independent verification,
documentation/API, and portability/GPU/performance agents ran with disjoint
write scopes. The lead alone integrated root and shared CMake, package, CI,
architecture, and cross-directory files. All accepted findings are resolved
and revalidated. No local Publication Checkpoint B blocker remains.

Two release-blocking predecessor compatibility corrections were required by
the frozen contract: `ExpressionOperation::kTerminal` was appended without
renumbering any Milestone 2 value, and an optional recursive expression-access
preflight hook was added so Dense evaluation rejects non-host leaves before
mutation. Neither correction implements a later milestone.

No Sparse API, random storage facet, optimized CPU provider, GPU provider,
solver/factorization surface, general broadcasting, compatibility aggregate,
or later-milestone API was implemented.

No commit, push, pull request, merge, tag, release, branch deletion, or
package-registry write occurred.

## 1. Changed files and reasons

The Milestone 3 layer is cumulative over the owner-approved, uncommitted
Milestone 0--2 predecessor. Several shared paths therefore appear as both
staged predecessor changes and unstaged Milestone 3 changes. The exact
current repository state can be reproduced with:

```sh
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
```

### Build, package, CI, architecture, and live documentation integration

```text
.github/workflows/ci.yml
CHANGELOG.md
CMakeLists.txt
CMakePresets.json
README.md
cmake/ASCCppComponents.cmake
cmake/ASCCppConfig.cmake.in
cmake/ASCCppOptions.cmake
docs/README.md
docs/api.md
docs/development/asc-cpp-architecture/backend-capability-matrix.md
docs/development/asc-cpp-architecture/capability-manifest.yaml
docs/development/asc-cpp-architecture/dependency-manifest.yaml
docs/modules/core.md
docs/modules/expression.md
docs/modules/random.md
docs/modules/utilities.md
docs/modules/dense.md
include/asc/expression/expression.h
```

These paths advance the candidate package to 0.3.0, register and selectively
export Dense, record the exact component closure and runtime-tested
capabilities, update local/hosted validation, document the live API, append
the Dense terminal operation category, and add recursive source-access
preflight. Existing Expression enumerator values are preserved.

### Exact new production surface

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
src/dense/CMakeLists.txt
src/dense/linalg.cc
```

These are the seven public headers, one target-owned CMake file, and sole
compiled source frozen by the contract. Layout, view, owner, evaluation, and
reduction templates remain in their owning headers. `linalg.cc` contains only
the approved serial `float`/`double` overloads.

### Architecture, compile, runtime, package, and consumer verification

```text
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/compile/CMakeLists.txt
tests/compile/m3_dependency_check.cmake
tests/compile/m3_exceptions_disabled.cc
tests/compile/m3_header_array.cc
tests/compile/m3_header_dense.cc
tests/compile/m3_header_evaluate.cc
tests/compile/m3_header_export.cc
tests/compile/m3_header_layout.cc
tests/compile/m3_header_linalg.cc
tests/compile/m3_header_view.cc
tests/compile/m3_multi_tu.h
tests/compile/m3_multi_tu_a.cc
tests/compile/m3_multi_tu_b.cc
tests/compile/m3_multi_tu_main.cc
tests/compile/m3_negative_const_view_mutation.cc
tests/compile/m3_negative_owner_copy.cc
tests/compile/m3_negative_unsupported_linalg_scalar.cc
tests/compile/m3_negative_unsupported_owner_element.cc
tests/compile/m3_negative_wrong_rank_linalg.cc
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/random/CMakeLists.txt
tests/consumer/dense/CMakeLists.txt
tests/consumer/dense/main.cc
tests/consumer/subproject/CMakeLists.txt
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/dense/CMakeLists.txt
tests/dense/allocation_probe.cc
tests/dense/allocation_probe.h
tests/dense/array_evaluate_test.cc
tests/dense/counting_memory_resource.h
tests/dense/layout_view_test.cc
tests/dense/linalg_test.cc
tests/dense/test_support.h
benchmarks/dense/dense_benchmark.cc
```

These paths add exact target/inventory/dependency audits; seven generic and
seven explicit header probes; exceptions-disabled and multi-TU coverage; five
required compile failures; independent layout/view/owner/evaluation/reduction
and numerical oracles; allocation instrumentation; a threshold-free
benchmark; and build-tree, installed-relocated, component, subproject, and
isolated Dense consumers. Existing component consumer fixtures advance only
their package-version expectation and closure.

### Contract, provenance, ownership, and independent reviews

```text
docs/development/asc-cpp-m3-dense-cpu/dependency-audit.md
docs/development/asc-cpp-m3-dense-cpu/documentation-api-review.md
docs/development/asc-cpp-m3-dense-cpu/milestone-contract.md
docs/development/asc-cpp-m3-dense-cpu/ownership.md
docs/development/asc-cpp-m3-dense-cpu/portability-review.md
docs/development/asc-cpp-m3-dense-cpu/production-self-review.md
docs/development/asc-cpp-m3-dense-cpu/provenance-record.md
docs/development/asc-cpp-m3-dense-cpu/publication-checkpoint-b.md
docs/development/asc-cpp-m3-dense-cpu/verification-design.md
docs/development/asc-cpp-m3-dense-cpu/verification-review.md
```

These freeze the bounded contract and write ownership, preserve clean-room
provenance, record the contract-first test design, and retain all findings,
resolutions, and final evidence.

The tracked `AGENTS.md` deletion, other cumulative Milestone 0--2 changes,
existing ignored `build/` tree, and the unrelated modified MdeCpp `Makefile`
were preserved and are not attributed to Milestone 3.

## 2. APIs, targets, and direct dependency changes

### Layouts and views

`DenseLayout<Rank>` creates checked `LayoutLeft`, `LayoutRight`, and
`LayoutStride<Rank>` mappings. Rank zero has logical/span size one; any zero
extent has size zero. Offsets, spans, uniqueness, and exhaustiveness are
checked. Explicit-stride owners and repeated-address views remain deferred.

`DenseView<Element, Rank>` is a trivially copyable, non-owning descriptor with
checked host access, one-way mutable-to-const conversion, exact metadata, and
rank-preserving subviews. All published Milestone 3 views require a proven
unique mapping.

### Ownership, evaluation, and reductions

`DenseArray<Element, ExtentsType>` is a move-only host owner backed by an
explicit Core `MemoryResource`. It supports value-initialized creation,
left/right contiguous layout, deep `Clone`, and transactional
`DiscardResize`.

`Evaluate` supports exact-shape ranked expressions and rank-zero scalar
expansion on an explicit serial context. It completes context, source/dest
memory, shape, and alias preflight before mutation. `ReduceSum`,
`ReduceMin`, and `ReduceMax` use deterministic dimension-zero-fastest order.
No operation allocates, packs, transfers, synchronizes, or falls back.

Expression adds:

```text
ExpressionOperation::kTerminal
ValidateExpressionAccess(context, expression)
```

`kTerminal` is appended after `kMultiply`. The access hook is optional for
external adapters, recursively propagated by built-in nodes, and does not
couple Expression to Dense.

### Serial reference linear algebra

The compiled overload set is exactly `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`,
`Gemv`, and `Gemm` for `float` and `double`. It supports the contract-approved
ranks, strided layouts, `DenseTranspose::{kNone,kTranspose}`, stable finite
Nrm2 accumulation, explicit NaN/infinity handling, and `beta == 0` no-read
semantics.

### Targets and package

```text
build target       consumer target    kind       direct ASC dependencies
asc_core           ASC::core          compiled   none
asc_utilities      ASC::utilities     compiled   ASC::core
asc_expression     ASC::expression    interface  ASC::core
asc_dense          ASC::dense         compiled   ASC::core;ASC::expression
asc_random         ASC::random        compiled   ASC::core
```

Available components are exactly `core`, `utilities`, `expression`, `dense`,
and `random`. A required Dense request resolves exactly
`core;expression;dense`. Sparse, random storage facets, provider facets, and
`cpp` remain unavailable.

Direct external link/runtime dependencies: none beyond the platform C++20
standard library. Configure-time dependency: exact released ASCCMake 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7`. No dependency was added.

## 3. Exact commands and pass/fail/skip results

### Repository, branch, and predecessor preflight

```sh
git fetch --prune origin
git branch --show-current
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m2-independent-foundations \
  feature/asc-cpp-m3-dense-cpu
git diff --cached --name-status main
git diff --name-status
git ls-files --others --exclude-standard | sort
git -C ../asc-cmake status --short --branch
git -C ../asc-cmake rev-parse HEAD
git -C ../asc-cmake describe --tags --exact-match HEAD
git -C /home/yicai/repo/MdeRepo/MdeCpp status --short --branch
git -C /home/yicai/repo/MdeRepo/MdeCpp rev-parse HEAD
```

Result: pass. The active branch is
`feature/asc-cpp-m3-dense-cpu`; all named ASCCpp refs and `origin/main` are
`33b261ea33616a6395c4ad3b20646093103344f7`. Main is current. The required
predecessor exactly matched the prior checkpoint: 334 staged Milestone 0--1
paths and the recorded 90-path Milestone 2 working-tree layer before M3 work.
ASCCMake is clean `main`, exact tag `v0.1.0`, at `8a7dcba`. MdeCpp is current
`main` at `f6294e9` with its pre-existing modified `Makefile` untouched.

`main:AGENTS.md`, the architecture package, ADRs 0001--0018, dependency and
capability manifests, backend matrix, previous checkpoint, repository audit,
implementation/testing/CI documents, and the supplied runbook instructions
were read before the contract was frozen. No material owner decision remained
unresolved.

### Final GCC Debug/static

```sh
cmake -S . -B /tmp/asc-cpp-m3-gcc-debug \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m3-gcc-debug --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-gcc-debug \
  -C Debug --output-on-failure -j 4
```

Result: pass with CMake 4.1.2 and GCC 11.4, 109/109; zero failed.

### Final Clang Release/shared

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-clang-release-shared \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m3-clang-release-shared --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-clang-release-shared \
  -C Release --output-on-failure -j 4
```

Result: pass with CMake 4.1.2 and Clang 19.0, 109/109 after all final
corrections; zero failed.

### Minimum CMake GCC Release/static

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . -B /tmp/asc-cpp-m3-cmake325-gcc-release \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build /tmp/asc-cpp-m3-cmake325-gcc-release --parallel 4
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir /tmp/asc-cpp-m3-cmake325-gcc-release \
  -C Release --output-on-failure -j 4
```

Result: pass with CMake 3.25.0 and GCC 11.4, 109/109; zero failed.

All three complete matrices include five configure-time expected compile
failures. Those source probes failed compilation as required; CTest therefore
records their gate as passing.

### Formatting, dependency, and static analysis

```sh
clang-format-19 --dry-run --Werror \
  include/asc/expression/expression.h include/asc/dense.h \
  include/asc/dense/*.h src/dense/linalg.cc \
  tests/dense/*.cc tests/dense/*.h \
  benchmarks/dense/dense_benchmark.cc \
  tests/consumer/dense/main.cc \
  tests/compile/m3_*.cc tests/compile/m3_*.h
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m3_dependency_check.cmake
git diff --check
command -v clang-tidy-19 || command -v clang-tidy
```

Result: formatting, dependency inventory/policy, and patch whitespace pass.
No local clang-tidy executable exists, so clang-tidy is **skipped** locally
and remains a hosted-CI gate.

### Non-product failed attempts

- A preset configure without `ASCCMake_DIR` failed before product
  compilation because this workspace does not install ASCCMake in a default
  prefix. The explicit exact released package passed all matrices.
- A portability Ninja configure failed before product configuration because
  Ninja is not installed. Unix Makefiles passed.
- The first standalone LSan all-Dense link collided with deliberate strong
  test allocation overrides. The compatible subset passed; ASan+UBSan covers
  the complete allocation-sensitive Dense runtime suite.
- An early broad sanitized CTest run selected independently configured static
  package consumers that were not linked with sanitizer runtime flags. The
  sanitizer acceptance set and uninstrumented static/shared package matrices
  are separated below.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### ASan and UBSan

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-clang-asan-ubsan \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m3-clang-asan-ubsan --parallel 4
ctest --test-dir /tmp/asc-cpp-m3-clang-asan-ubsan \
  --output-on-failure -I 1,95
```

Result: pass, 95/95 compatible instrumented architecture, compile, Core,
Utilities, Expression, Dense, Random, and benchmark tests; zero sanitizer
diagnostics. The final focused Dense acceptance set
`layout_view_test|array_evaluate_test|linalg_test|multi_tu|benchmark` passed
5/5.

Independently configured package/consumer tests 96--109 are intentionally
excluded from the instrumented acceptance set because development sanitizer
link flags are not an installed usage requirement. They pass in all three
uninstrumented complete package matrices.

### Standalone LeakSanitizer and ThreadSanitizer

```sh
CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-portability-clang-lsan.otTN6S \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_LEAK_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m3-portability-clang-lsan.otTN6S \
  --parallel 2 --target asc_dense_m3_header_dense \
    asc_dense_m3_header_array asc_dense_m3_header_evaluate \
    asc_dense_m3_header_linalg asc_dense_multi_tu
ctest --test-dir /tmp/asc-cpp-m3-portability-clang-lsan.otTN6S \
  --output-on-failure \
  -R '^asc_cpp\.(compile\.m3_header_(dense|array|evaluate|linalg)|dense\.multi_tu)$'

CC=clang-19 CXX=clang++-19 cmake -S . \
  -B /tmp/asc-cpp-m3-portability-clang-tsan.EZiKOp \
  -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_THREAD_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m3-portability-clang-tsan.EZiKOp \
  --parallel 2 --target asc_dense_m3_header_dense \
    asc_dense_m3_header_array asc_dense_m3_header_evaluate \
    asc_dense_m3_header_linalg asc_dense_multi_tu
ctest --test-dir /tmp/asc-cpp-m3-portability-clang-tsan.EZiKOp \
  --output-on-failure \
  -R '^asc_cpp\.(compile\.m3_header_(dense|array|evaluate|linalg)|dense\.multi_tu)$'
```

Result: LSan pass 5/5 and TSan pass 5/5 with zero diagnostics. Allocation-probe
targets are **skipped as incompatible** under standalone LSan because both the
test probe and sanitizer runtime intentionally define strong allocation
symbols. A separate Dense consumer LSan smoke also passed with no diagnostic.

### Package, relocation, and isolated consumers

Tests 96--109 in each complete matrix cover:

- Core, Utilities, Expression, Dense, and Random build-tree consumers;
- all five installed-and-relocated isolated component consumers;
- the complete foundations subproject consumer;
- build-tree component selection;
- installed package relocation/component selection; and
- package-registry preservation.

Result: 14/14 pass under GCC Debug/static, Clang Release/shared, and
CMake 3.25 GCC Release/static. Paths containing spaces, static/shared
libraries, build/install trees, moved prefixes, required/optional/unavailable
components, exact Dense dependency closure, and absence of forbidden sibling
components all pass.

Dynamic inspection of the relocated Clang shared install reports
`NEEDED libasc_core.so`, `RUNPATH [$ORIGIN]`, and exactly 20 approved exported
`float`/`double` linear-algebra overload symbols. The Apple source path uses
`@loader_path`; native macOS relocation remains a hosted gate.

## 5. CPU/GPU provider evidence

| Surface | Classification | Result |
| --- | --- | --- |
| provider-free serial CPU Dense | runtime-tested | GCC 11.4 and Clang 19, static/shared, CMake 3.25/current, sanitizers, numerical oracles, allocation checks, packages, relocation, subproject, and isolated consumers pass. |
| optimized CPU providers | skipped | No OpenMP, TBB, Eigen, BLAS/LAPACK, oneMKL, or SYCL target, discovery, dependency, or dispatch exists. |
| any GPU provider | **skipped** | No GPU option, language, source, target, component, SDK include/link, hardware execution, or CPU/GPU parity surface exists in Milestone 3. |

GPU evidence is exactly **skipped**. It is not configure-tested,
compile-tested, runtime-tested, or parity-tested.

## 6. Review findings and resolutions

1. Documentation/API review found that the M2 expression operation enum had
   no terminal category required by Dense leaves. The lead appended
   `kTerminal`; existing values are unchanged. Header, multi-TU, and
   expression/dense tests pass.
2. Direct-include review found missing `<concepts>` and `<limits>` ownership
   in `view.h`, and owner constraints admitted cv types and `bool`. Includes
   are now self-contained; the public owner concept rejects those element
   forms. Positive and required-failure compilation pass.
3. Verification found const views could publish repeated-address mappings.
   Mutable and const view creation now both require proven uniqueness.
4. Verification found scaled Nrm2 mishandled leading NaN and repeated
   infinity. It now propagates NaN, returns positive infinity when applicable,
   retains stable finite accumulation, and returns positive zero for zero.
5. Portability found that direct or nested device-marked source expressions
   could reach a fatal read. Recursive access preflight now returns
   `kMemoryAccess` before mutation; independent transactionality regressions
   pass.
6. Portability found the allocation probe used non-MSVC aligned allocation.
   The verifier added `_aligned_malloc`/`_aligned_free`, overflow-safe
   non-MSVC rounding, and complete ordinary/aligned scalar/array nothrow
   allocation families.
7. Focused ASan then exposed an allocation-family mismatch in the test probe.
   Matching placement deletes and nothrow overloads resolved it; final
   ASan+UBSan passes 95/95 and Dense-focused 5/5.
8. Dependency/API/package reviews confirm the exact target/header/source
   inventories, actual released ASCCMake APIs, component closure, sibling and
   provider isolation, clean-room provenance, and absence of later scope.

No unresolved production, numerical, allocation, transactionality, API,
documentation, dependency, package, portability, GPU-isolation, provenance,
or performance blocker remains locally.

## 7. Performance evidence

The threshold-free allocation benchmark warms up, preallocates operands,
checks nonzero results, and measures 32-by-32 pointwise evaluation and serial
Gemm. Final independent observations include:

```text
Clang 19 Release/shared
evaluate: 64 iterations, 4,867,777 total ns, 76,059 ns/iteration,
          checksum 1283, allocations 0
gemm:      4 iterations, 66,532 total ns, 16,633 ns/iteration,
          checksum 20790.7, allocations 0

GCC 11.4 Debug/static
evaluate: 64 iterations, 155,124,939 total ns, 2,423,830 ns/iteration,
          checksum 1283, allocations 0
gemm:      4 iterations, 13,375,893 total ns, 3,343,970 ns/iteration,
          checksum 20790.7, allocations 0
```

Result: pass. Both timed operation families performed zero allocation. The
timings are uncontrolled local observations with no approved speed threshold;
they are not an optimized-provider or stable performance claim.

## 8. Remaining risks

- Native MSVC/Windows and AppleClang/macOS jobs have not run locally. Windows
  allocation/DLL paths and Apple `@loader_path` are source-reviewed; hosted
  execution remains a publication gate.
- Local clang-tidy is unavailable; the pinned hosted gate remains required.
- Standalone LSan cannot link allocation-counting test targets because of
  deliberate strong-symbol interception; its compatible subset and the
  complete ASan+UBSan Dense suite pass.
- `uintptr_t` interval aliasing is appropriate for the approved hosted
  platforms but is not a claim for capability or segmented-pointer systems.
- Raw-pointer views cannot validate allocation capacity, alignment, lifetime,
  concurrency, or dangling use after owner destruction, move, or resize.
- Conservative uniqueness and span-based alias analysis may reject a
  mathematically safe mapping or disjoint padded region.
- Deterministic serial accumulation is not a correctly rounded,
  overflow-free, vectorized, parallel, or performance-stable promise.
- No GPU support is present or implied; GPU evidence is **skipped**.
- The 0.3 API is pre-1.0 and may change at a later approved minor version.
- The candidate is cumulative and uncommitted: Milestones 0--1 are staged,
  while Milestones 2--3 are unstaged/untracked and overlap shared files.
  Publication must review and preserve the complete M0-through-M3 unit unless
  the owner separately approves a commit split.
- The existing ignored `build/` tree and unrelated MdeCpp `Makefile` change
  remain outside Milestone 3 evidence.

## 9. Exact proposed remote and branch-cleanup actions

No remote or cleanup action is authorized at Checkpoint B. The roadmap gives
Milestone 3 an earliest 0.3.x release line but does not designate this
checkpoint as a release. No tag or GitHub release is proposed.

After separate publication approval, the exact cumulative publication actions
would be:

```sh
git add -A
git diff --cached --check
git commit \
  -m "Establish asc-cpp through milestone 3 Dense CPU"
git push --set-upstream origin feature/asc-cpp-m3-dense-cpu
gh pr create \
  --base main \
  --head feature/asc-cpp-m3-dense-cpu \
  --title "Implement asc-cpp milestone 3 Dense CPU" \
  --body-file \
  docs/development/asc-cpp-m3-dense-cpu/publication-checkpoint-b.md
```

Do not amend, force-push, or push directly to `main`. Inspect all required
hosted checks and independent review before a separately authorized merge.
Do not create a tag or release unless a separate owner decision designates an
approved immutable release.

No branch deletion is proposed before a separately approved merge. After a
future merge and explicit cleanup authorization, only this branch may be
considered with these exact gates:

```sh
git fetch --prune origin
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor \
  origin/feature/asc-cpp-m3-dense-cpu origin/main
git worktree list --porcelain
git push origin --delete feature/asc-cpp-m3-dense-cpu
git branch -d feature/asc-cpp-m3-dense-cpu
```

Run each delete command individually and only if ancestry succeeds, no
worktree uses the branch, and no intended unique commit is unpreserved. Never
delete `main`, release tags, active worktree branches, recovery branches,
other milestone branches, or unpreserved work.
