# Milestone 2 Publication Checkpoint B

Status: complete local candidate; publication not authorized

Date: 2026-07-27

Branch: `feature/asc-cpp-m2-independent-foundations`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the complete staged Milestone 0 and Milestone 1 Publication
Checkpoint B candidate, advanced without a commit or publication

## Scope result

The frozen Milestone 2 contract is satisfied with no correction to the
owner-approved scope. The candidate implements only three mutually independent
provider-free foundations: Utilities command-line configuration and timing,
the storage-neutral Expression protocol and pointwise nodes, and the base
Random Philox4x32-10 plus scalar `Uniform01` sequence.

The production, independent verification, documentation/API, and
portability/GPU/performance roles ran as separate agents with disjoint write
scopes. The lead alone integrated shared CMake, package, CI, Core, architecture,
and cross-directory test files. All accepted findings are resolved and
revalidated. No local Publication Checkpoint B blocker remains.

No file parser, writable expression/storage API, Dense, Sparse, random storage
facet, provider, GPU target, compatibility aggregate, entropy source, later
distribution, or later-milestone API was implemented.

No commit, push, pull request, merge, tag, release, branch deletion, or
package-registry write occurred.

## 1. Changed files and reasons

The exact bounded M2 layer contains 90 paths: 56 additions, 32 modifications,
and two deletions. It remains an unstaged working-tree layer over the exact
334-path staged M0+M1 predecessor. Its inventory is produced by:

```sh
git diff --name-status
git ls-files --others --exclude-standard | sort
```

### Build, package, CI, architecture, and Core integration

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
include/asc/core/configuration.h
src/core/configuration.cc
src/expression/CMakeLists.txt
src/random/CMakeLists.txt
src/utilities/CMakeLists.txt
```

These paths advance the unreleased package to 0.2.0; build and selectively
export the exact four available components; add command-line origin kind 2;
record truthful M2 capability/backend state; define local/hosted validation;
and document the live API. The compiled M2 libraries use relative install
RPATHs so their relocated shared forms find Core.

### Exact new production surface

```text
include/asc/utilities.h
include/asc/utilities/command_line.h
include/asc/utilities/export.h
include/asc/utilities/timer.h
include/asc/expression.h
include/asc/expression/expression.h
include/asc/random.h
include/asc/random/distribution.h
include/asc/random/engine.h
include/asc/random/export.h
src/utilities/command_line.cc
src/utilities/timer.cc
src/random/distribution.cc
src/random/engine.cc
```

These are the exact ten public headers and four compiled sources frozen by the
contract. Expression is header-only and has no empty implementation source.

### Test and consumer integration

```text
tests/CMakeLists.txt
tests/architecture/CMakeLists.txt
tests/architecture/check_approved_product_targets.cmake
tests/architecture/check_dependency_manifest.cmake
tests/architecture/check_public_file_policy.cmake
tests/architecture/check_m1_product_targets.cmake              (deleted)
tests/architecture/check_m1_public_file_policy.cmake           (deleted)
tests/compile/CMakeLists.txt
tests/compile/dependency_check.cmake
tests/compile/m2_dependency_check.cmake
tests/compile/m2_exceptions_disabled.cc
tests/compile/m2_expression_compile_contracts.cc
tests/compile/m2_expression_multi_tu_a.cc
tests/compile/m2_expression_multi_tu_b.cc
tests/compile/m2_expression_multi_tu_main.cc
tests/compile/m2_expression_negative_no_adapter.cc
tests/compile/m2_expression_negative_wrong_rank.cc
tests/compile/m2_expression_operand.h
tests/consumer/CMakeLists.txt
tests/consumer/core/CMakeLists.txt
tests/consumer/expression/CMakeLists.txt
tests/consumer/expression/main.cc
tests/consumer/random/CMakeLists.txt
tests/consumer/random/main.cc
tests/consumer/run_component_consumer.cmake
tests/consumer/run_core_consumer.cmake
tests/consumer/subproject/CMakeLists.txt
tests/consumer/utilities/CMakeLists.txt
tests/consumer/utilities/main.cc
tests/expression/CMakeLists.txt
tests/expression/expression_test.cc
tests/expression/test_support.h
tests/package/CMakeLists.txt
tests/package/component_unavailable/CMakeLists.txt
tests/package/package_test.cmake
tests/random/CMakeLists.txt
tests/random/distribution_test.cc
tests/random/engine_test.cc
tests/random/test_support.h
tests/utilities/CMakeLists.txt
tests/utilities/command_line_test.cc
tests/utilities/test_support.h
tests/utilities/timer_test.cc
tests/core/configuration_test.cc
```

The two M1-specific architecture scripts are replaced by milestone-aware exact
inventory checks. The remaining paths add self-contained/no-exception header
checks, two required compile failures, multiple-translation-unit Expression
use, dependency scans, independent runtime oracles, and isolated build-tree
and relocated installed consumers for each M2 component.

### Contract, provenance, and review records

```text
docs/development/asc-cpp-m2-independent-foundations/dependency-audit.md
docs/development/asc-cpp-m2-independent-foundations/documentation-api-review.md
docs/development/asc-cpp-m2-independent-foundations/milestone-contract.md
docs/development/asc-cpp-m2-independent-foundations/ownership.md
docs/development/asc-cpp-m2-independent-foundations/portability-review.md
docs/development/asc-cpp-m2-independent-foundations/production-self-review.md
docs/development/asc-cpp-m2-independent-foundations/provenance-record.md
docs/development/asc-cpp-m2-independent-foundations/publication-checkpoint-b.md
docs/development/asc-cpp-m2-independent-foundations/verification-design.md
docs/development/asc-cpp-m2-independent-foundations/verification-review.md
```

These freeze scope and ownership, retain the clean-room Philox source hash and
derivation boundary, and record each independent review and final evidence.

The existing ignored `build/` tree and unrelated work were preserved and are
not cited as evidence.

## 2. APIs, targets, and direct dependency changes

### Utilities

`CommandLineOption`, `CommandLineParseResult`, and
`CommandLineParser::{Create,Parse,RenderHelp,options}` provide an owning,
validated option table and transactional schema-default-plus-command-line
configuration. Supported leaves are bool, signed/unsigned 64-bit integer,
double, and UTF-8 string. `ConfigurationOriginKind::kCommandLine = 2` and
`ConfigurationOrigin::CommandLine` append command-line provenance without
renumbering existing origins.

`TimerState` and
`Timer::{Start,Stop,Elapsed,Last,Average,Reset,state,sample_count}` provide the
frozen `steady_clock` state machine.

### Expression

The public surface is `ExpressionAdapter<T>`, `ReadableExpression`,
`ExpressionValue`, `kExpressionRank`, `AliasToken`, `SparsityEffect`,
`ExpressionOperation`, `ExpressionShape`, `ExpressionRead`, `MayAlias`,
`ExpressionSparsityEffect`, `ExpressionOperationCategory`, and the
`MakeNegate`, `MakeAdd`, `MakeSubtract`, and `MakeMultiply` factories.

It accepts explicit external adapters and arithmetic rank-zero terminals,
captures lvalues by non-owning const reference and rvalues/nodes by value,
checks complete rank/shape, permits only scalar expansion, and defines no
storage, evaluator, writable protocol, allocation, or provider dispatch.

### Random

The exact sequence API is:

```text
Philox4x32Counter, Philox4x32Key, Philox4x32Result
RandomStream, RandomSubsequence, RandomOffset
Philox4x32_10
GeneratePhilox4x32Block
GeneratePhilox4x32Word
AdvanceRandomOffset
Uniform01<float>
Uniform01<double>
```

It is a pure explicit-position Philox4x32-10/raw-word surface with checked
offset advance and exact 24-bit/53-bit binary transforms. It has no default
engine, entropy, standard distribution, state serialization, fill API, or
provider claim.

### Targets and package

```text
build target       consumer target    kind       direct ASC dependency
asc_core           ASC::core          compiled   none
asc_utilities      ASC::utilities     compiled   ASC::core
asc_expression     ASC::expression    interface  ASC::core
asc_random         ASC::random        compiled   ASC::core
```

The package version is the unreleased 0.2.0 candidate. Available components
are exactly `core`, `utilities`, `expression`, and `random`. Dense, Sparse,
`random_dense`, `random_sparse`, and `cpp` remain unavailable and unexported.

Direct external link/runtime dependencies: none beyond the platform C++20
standard library. Configure-time dependency: exact released ASCCMake 0.1.0 at
`8a7dcbad3a97267cce59810aff24de800a3497a7`. No dependency was added.

## 3. Exact commands and pass/fail/skip results

### Repository and predecessor preflight

```sh
git fetch --prune origin
git rev-parse HEAD main origin/main \
  feature/asc-cpp-m0-foundation \
  feature/asc-cpp-m1-core \
  feature/asc-cpp-m2-independent-foundations
git diff --cached --name-status main
git diff --name-status
git -C ../asc-cmake status --short --branch
git -C ../asc-cmake rev-parse HEAD
git -C ../asc-cmake describe --tags --exact-match HEAD
```

Result: pass. Every ASCCpp ref above is `33b261e`; `main` equals
`origin/main`. The required predecessor was exactly 334 staged paths with no
unstaged predecessor change when M2 began. ASCCMake is clean `main`, exact tag
`v0.1.0`, at `8a7dcba`. The feature branch was reused without moving HEAD.

### Final minimum-CMake GCC Release/static

```sh
M2_FINAL_MIN=/tmp/asc-cpp-m2-final-min.yiUBDu
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . -B "$M2_FINAL_MIN/build" -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build "$M2_FINAL_MIN/build" --parallel 4
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir "$M2_FINAL_MIN/build" -C Release --output-on-failure
```

Result: pass with CMake 3.25.0 and GCC 11.4, 79/79; zero failed and zero
skipped. Both configure-time negative Expression contracts were rejected as
required.

### Final Clang Debug/shared

```sh
M2_FINAL_CLANG=/tmp/asc-cpp-m2-final-clang.oHEslc
cmake -S . -B "$M2_FINAL_CLANG/build" -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake \
  -DCMAKE_C_COMPILER=clang-19 \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build "$M2_FINAL_CLANG/build" --parallel 4
ctest --test-dir "$M2_FINAL_CLANG/build" \
  -C Debug --output-on-failure
```

Result: pass with CMake 4.1.2 and Clang 19, 79/79; zero failed and zero
skipped. CMake emitted only the expected harmless warning that the C-only
compiler variable is unused by this C++-only project.

Independent verification additionally passed:

```text
GCC 11.4 / Debug / static / Werror:             79/79
GCC 11.4 / Release / shared / Werror:           79/79
Clang 19 strict focused compile/runtime:         11/11
Expression required compile failures:             2/2
```

The dedicated portability agent independently repeated Clang 19
Release/shared with warnings as errors: 79/79 passed.

### Formatting, dependency, and static-analysis tools

```sh
find include/asc src tests -type f \
  \( -name '*.h' -o -name '*.cc' \) -print0 |
  xargs -0 /usr/bin/clang-format-19 --dry-run --Werror
cmake -DSOURCE_DIR:PATH="$PWD" \
  -P tests/compile/m2_dependency_check.cmake
git diff --check
find /usr -type f -name 'clang-tidy*' -print
```

Result: formatting, dependency policy, and patch whitespace pass. No local
`clang-tidy` executable exists, so clang-tidy is **skipped** locally and
remains a hosted-CI gate.

### Resolved failed attempts

- The first integrated run was 76/79 because capability and optional-component
  fixtures still expressed M1 state. The lead updated them; the repeated run
  passed 79/79.
- Independent Release/shared was initially 78/79 because relocated Random
  could not load its transitive Core shared library. The relative install
  RPATH correction passed a focused 2/2 rerun and the complete 79/79 suite.
- Standalone LeakSanitizer and ThreadSanitizer all-target builds encounter
  link-time collisions between sanitizer allocation interceptors and
  test-local strong global `new`/`delete` overrides. Compatible M2 targets
  were selected explicitly and passed 6/6 under each sanitizer.
- The documentation agent's first Ninja configure failed because Ninja is not
  installed. Its Unix Makefiles build and documentation executable passed;
  this was not a product failure.
- A portability concern based on upstream libc++ history was closed as a
  non-defect after Apple documentation confirmed floating `from_chars`
  support for deployment targets macOS 13.3+, covering the required macOS 15
  runner. No fallback or API behavior change was made.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### ASan and UBSan

```sh
cmake -S . \
  -B /tmp/asc-cpp-m2-sanitizers.dnW0rA/clang-asan-ubsan \
  -G "Unix Makefiles" \
  -DASCCMake_DIR=/tmp/asc-cpp-m2-sanitizers.dnW0rA/asc-cmake \
  -DCMAKE_CXX_COMPILER=/usr/lib/llvm-19/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build \
  /tmp/asc-cpp-m2-sanitizers.dnW0rA/clang-asan-ubsan --parallel 4
ctest --test-dir \
  /tmp/asc-cpp-m2-sanitizers.dnW0rA/clang-asan-ubsan \
  --output-on-failure \
  -R '^(asc_cpp\.(architecture|compile|core|utilities|expression|random))'
```

Result: pass, 67/67 instrumented architecture, compile, Core, Utilities,
Expression, and Random tests. Package and nested consumer tests are
intentionally validated in the uninstrumented full static/shared matrices so
development sanitizer flags do not leak across package boundaries.

### Standalone LeakSanitizer and ThreadSanitizer

```sh
cmake -S . -B /tmp/asc-cpp-m2-lsan.cvCZ0K/build \
  -G "Unix Makefiles" \
  -DASCCMake_DIR=/tmp/asc-cpp-m2-lsan.cvCZ0K/asc-cmake \
  -DCMAKE_CXX_COMPILER=/usr/lib/llvm-19/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_LEAK_SANITIZER=ON
cmake --build /tmp/asc-cpp-m2-lsan.cvCZ0K/build --target \
  asc_utilities_command_line_test asc_utilities_timer_test \
  asc_expression_compile_contracts asc_expression_multi_tu \
  asc_random_distribution_test asc_random_engine_test --parallel 4
ctest --test-dir /tmp/asc-cpp-m2-lsan.cvCZ0K/build --output-on-failure \
  -R '^(asc_cpp\.utilities\.(command_line_test|timer_test)|asc_cpp\.expression\.(compile_contracts|multi_tu)|asc_cpp\.random\.(distribution_test|engine_test))$'

cmake -S . -B /tmp/asc-cpp-m2-tsan.iae3fg/build \
  -G "Unix Makefiles" \
  -DASCCMake_DIR=/tmp/asc-cpp-m2-tsan.iae3fg/asc-cmake \
  -DCMAKE_CXX_COMPILER=/usr/lib/llvm-19/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_THREAD_SANITIZER=ON
cmake --build /tmp/asc-cpp-m2-tsan.iae3fg/build --target \
  asc_utilities_command_line_test asc_utilities_timer_test \
  asc_expression_compile_contracts asc_expression_multi_tu \
  asc_random_distribution_test asc_random_engine_test --parallel 4
ctest --test-dir /tmp/asc-cpp-m2-tsan.iae3fg/build --output-on-failure \
  -R '^(asc_cpp\.utilities\.(command_line_test|timer_test)|asc_cpp\.expression\.(compile_contracts|multi_tu)|asc_cpp\.random\.(distribution_test|engine_test))$'
```

Result: LSan pass 6/6 and TSan pass 6/6 for the compatible M2 tests, with zero
reported sanitizer errors. The full all-target suite is **skipped as
incompatible** under standalone LSan/TSan because existing Core and Expression
allocation-counting tests deliberately override the same global allocation
symbols as the sanitizer runtimes.

### Package, relocation, and isolated consumers

Each final 79-test matrix includes 12 package-labelled cases:

- Core, Utilities, Expression, and Random build-tree consumers;
- Core, Utilities, Expression, and Random installed-and-relocated consumers;
- the complete foundations subproject consumer;
- build-tree component selection;
- installed package relocation/component selection; and
- package-registry preservation.

Result: 12/12 pass under minimum-CMake GCC Release/static and 12/12 pass under
Clang Debug/shared. Paths containing spaces, static/shared libraries,
build/install trees, moved prefixes, required/optional/unavailable components,
and absence of forbidden siblings all pass.

The portability agent's `readelf -d` inspection confirmed that relocated
`libasc_utilities.so` and `libasc_random.so` each have
`NEEDED libasc_core.so` and `RUNPATH [$ORIGIN]`. The Apple equivalent
`@loader_path` is source-inspected but not runtime-tested locally.

## 5. CPU/GPU provider evidence

| Surface | Classification | Result |
| --- | --- | --- |
| provider-free serial CPU Core/Utilities/Expression/Random | runtime-tested | Configured and compiled with GCC 11.4 and Clang 19, static/shared, CMake 3.25/current, and sanitizers; runtime, exact random vectors, package, relocation, subproject, and isolated consumers pass. |
| optimized CPU providers | skipped | No OpenMP, Eigen, BLAS/LAPACK, MKL, TBB, or SYCL provider is implemented or discovered. |
| any GPU provider | **skipped** | No GPU option, discovery, language, source, target, component, SDK include/link, runtime operation, hardware execution, or CPU/GPU parity surface exists in M2. |

GPU evidence is exactly **skipped**. It is not configure-tested,
compile-tested, runtime-tested, or parity-tested.

## 6. Review findings and resolutions

1. Documentation/API review found separated command-line values recorded the
   consumed value token rather than the option token as their origin location.
   Production now captures the zero-based option-token index before consuming
   the value; focused and full tests pass.
2. Verification corrected two test-only C++ construction errors without
   changing their independent expected semantics.
3. The first integrated capability checker and Core optional-component
   fixtures retained M1 assumptions. The lead advanced implemented
   capabilities through M2 and changed the unavailable probe to Dense.
4. The Core guide omitted the appended command-line origin, and a Utilities
   documentation example omitted `SetRequired(true)`. Both documents were
   corrected and the combined documentation example compiled, linked, and ran.
5. Independent Release/shared review found the real transitive shared-library
   relocation defect. The lead added `$ORIGIN`/`@loader_path` only to compiled
   M2 install RPATHs. Focused relocation, full shared, final clean shared, and
   ELF dynamic-section checks pass.
6. Portability raised a provisional older-libc++ floating `from_chars`
   concern. Apple platform documentation establishes macOS 13.3 as the minimum
   supported deployment target for that facility, so the macOS 15 CI row is
   compatible. The finding was closed without changing parsing semantics.
7. Dependency/API reviews confirm the exact target/header/source inventories,
   actual ASCCMake APIs, sibling/provider isolation, clean-room random
   provenance, and absence of later scope.

No unresolved production, package/API, verification, documentation,
dependency, portability, GPU-isolation, provenance, or performance blocker
remains locally.

## 7. Performance evidence

No throughput, latency, scaling, or regression threshold is approved in M2,
so no benchmark was invented.

Applicable bounded evidence is:

- Philox performs exactly ten fixed rounds; positioned-word generation
  computes one block and selects one lane, while callers needing four words
  can request a block directly.
- `Uniform01` is a constant-time scalar bit transform and invokes no standard
  distribution, entropy source, allocation, or provider.
- Expression construction performs no framework result allocation, scalar
  read, destination mutation, transfer, synchronization, or provider dispatch;
  independent instrumentation observed zero allocations for built-in
  construction and reads.
- Shape validation is linear in rank and scalar reads recurse through the
  expression tree. No deep-tree compile-time/code-size threshold is claimed.
- Timer operations use native `steady_clock::duration`; state updates are
  constant time apart from the clock call and error-status construction.
- Parser lookup is linear in option count and intentionally uses ordinary
  standard containers and transactional configuration allocation.

These are contract/structural observations, not benchmark claims.

## 8. Remaining risks

- Hosted MSVC/Windows and AppleClang/macOS jobs have not run locally.
  Apple `@loader_path` relocation remains inspected rather than
  runtime-tested.
- Local clang-tidy is unavailable; the pinned hosted gate remains required.
- Standalone LSan/TSan cannot link the complete test graph because deliberate
  test-local allocator overrides collide with sanitizer interceptors; the
  compatible M2 subset passes 6/6 under each.
- Only `C`, `C.utf8`, and `POSIX` locales were available locally. Complete
  locale-independent parsing rejects comma decimals, but positive parsing
  while a comma-decimal process locale is active remains hosted/manual
  evidence.
- Non-x86, big-endian, 32-bit `size_t`, exotic non-IEEE floating
  representations, and macOS deployment targets older than 13.3 are not
  validated.
- External Expression adapters and non-owning views retain caller-enforced
  index, extent, storage-lifetime, and synchronization duties. `Timer` also
  requires caller synchronization.
- The 0.2 API and sequence are pre-1.0 and claim no stable cross-toolchain ABI.
- Parser scaling, deep expression trees, and random throughput have no
  approved numerical threshold.
- The candidate is cumulative and uncommitted: M0+M1 is staged, while the M2
  layer is unstaged/untracked. Publication must review and preserve the whole
  M0-through-M2 unit unless the owner separately approves a commit split.
- The existing ignored `build/` tree remains unrelated and must not be used as
  M2 evidence.

## 9. Exact proposed remote and branch-cleanup actions

No remote or cleanup action is authorized at Checkpoint B. This checkpoint is
not a release decision, so no tag or GitHub release is proposed.

After separate publication approval, the exact cumulative publication actions
would be:

```sh
git add -A
git diff --cached --check
git commit -m "Establish asc-cpp through milestone 2 independent foundations"
git push --set-upstream origin \
  feature/asc-cpp-m2-independent-foundations
gh pr create \
  --base main \
  --head feature/asc-cpp-m2-independent-foundations \
  --title "Implement asc-cpp milestone 2 independent foundations" \
  --body-file \
  docs/development/asc-cpp-m2-independent-foundations/publication-checkpoint-b.md
```

Do not amend, force-push, or push directly to `main`. Inspect all required
hosted checks and independent review before a separately authorized merge.
Do not create a tag unless the approved roadmap and a separate publication
authorization designate this milestone as a release.

No branch deletion is proposed before a separately approved merge. After a
future merge and explicit cleanup authorization, only this branch may be
considered with these exact gates:

```sh
git fetch --prune origin
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor \
  origin/feature/asc-cpp-m2-independent-foundations origin/main
git worktree list --porcelain
git push origin --delete feature/asc-cpp-m2-independent-foundations
git branch -d feature/asc-cpp-m2-independent-foundations
```

Run each delete command only if ancestry succeeds, no worktree uses the
branch, and no intended unique commit is unpreserved. Never delete `main`,
release tags, M0/M1/M8 or recovery branches, active worktree branches, or
unpreserved work.
