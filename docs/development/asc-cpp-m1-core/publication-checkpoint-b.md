# Milestone 1 Publication Checkpoint B

Status: Complete local candidate; publication not authorized

Date: 2026-07-27

Branch: `feature/asc-cpp-m1-core`

Base: clean, current `main` and `origin/main` at
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor: the complete staged Milestone 0 Publication Checkpoint B
candidate, advanced without a commit or publication

## Scope result

The bounded Milestone 1 contract is satisfied. The candidate implements only
the provider-free Core CPU foundation as `asc_core` / `ASC::core`, using the
C++20 standard library and no linked dependency. It adds no later module,
facet, provider, parser, numerical container, compatibility facade, or GPU
implementation.

The four explicitly separate production, independent verification,
documentation/API, and portability/GPU/performance roles completed their
disjoint scopes. All accepted findings were resolved and revalidated. No local
Publication Checkpoint B blocker remains.

No commit, push, pull-request mutation, merge, tag, release, branch deletion,
or package-registry write occurred. The cumulative Milestone 8 branch and its
remote remained unchanged.

## 1. Changed files and reasons

The staged cumulative M0+M1 candidate contains 334 changed paths: 100
additions, 41 modifications, and 193 deletions. Its exact per-path inventory
is produced by:

```sh
git diff --cached --name-status main
```

The 75-path bounded M1 layer comprises:

- `CMakeLists.txt`, `CMakePresets.json`, and `cmake/**`: advance the package
  to 0.1.0; build, export, install, and conditionally load Core; retain future
  provider-free names as unavailable; and add warnings/sanitizer controls.
- `.github/workflows/ci.yml`: run the M1 GCC/CMake-minimum, Clang sanitizer
  and clang-tidy, Windows shared MSVC, and AppleClang matrices while retaining
  pinned checkout and least-privilege dependency access.
- `include/asc/core.h` and ten `include/asc/core/*.h` files: add the exact
  approved public Core surface.
- `src/core/CMakeLists.txt` and six `src/core/*.cc` files: build the exact
  approved production library using released ASCCMake target APIs.
- `tests/CMakeLists.txt`, `tests/architecture/**`, `tests/compile/**`,
  `tests/core/**`, `tests/consumer/**`, and `tests/package/**`: enforce target,
  file, dependency, header, API, runtime, death-test, component, install,
  relocation, subproject, and isolated-consumer contracts.
- `README.md`, `CHANGELOG.md`, `SECURITY.md`, `docs/README.md`, and
  `docs/modules/core.md`: describe the live M1 API, ownership, errors,
  lifetimes, package behavior, security boundary, and later-scope exclusions.
- `docs/architecture/dependency-policy.md` and the dependency, capability,
  and backend records: enforce Core-only availability and truthful M1
  evidence while retaining later capabilities as proposed.
- `docs/development/asc-cpp-m1-core/**`: freeze the contract and ownership
  ledger and record production, verification, documentation, portability,
  dependency/provenance, and checkpoint evidence.

The staged M0 clean-restart predecessor remains visible in the cumulative
diff. M1 advances only the live build/package/test/CI/current-documentation
files named by its contract; it does not restore the deleted legacy
implementation.

The pre-existing ignored `build/` directory contains unrelated cumulative
later-milestone artifacts. It was preserved and is excluded from all evidence.

## 2. APIs, targets, and direct dependency changes

### Public C++ API

The exact public inventory is:

```text
include/asc/core.h
include/asc/core/configuration.h
include/asc/core/contracts.h
include/asc/core/execution.h
include/asc/core/export.h
include/asc/core/extents.h
include/asc/core/io.h
include/asc/core/memory.h
include/asc/core/result.h
include/asc/core/status.h
include/asc/core/types.h
```

It provides:

- stable `ErrorCode`, nodiscard `Status` and `Result<T>`, and release-active
  contracts;
- checked logical metadata, arithmetic, casts, byte counts, and mixed
  static/dynamic `Extents`;
- recursive programmatic configuration values, schemas, validation, defaults,
  origins, metadata, rollback, JSON Pointer lookup, and redaction;
- partial byte source/sink interfaces, exact transfer helpers, move-only local
  files, bounded text helpers, and fixed-width little-endian scalars;
- explicit memory spaces, a host resource, move-only raw `Buffer`, and
  non-owning byte views; and
- backend-neutral vocabulary, an immutable serial context, overlap-safe host
  copy, and move-only already-complete events.

### Targets and package

```text
build target:       asc_core
build-tree alias:   ASC::core
installed target:   ASC::core
package/version:    ASCCpp 0.1.0
available component: core
```

The eight later provider-free names remain known but unavailable:
`utilities`, `expression`, `dense`, `sparse`, `random`, `random_dense`,
`random_sparse`, and `cpp`. No provider component or target is live.

`find_package(ASCCpp 0.1 CONFIG REQUIRED COMPONENTS core)` succeeds and
creates exactly `ASC::core`. No-component lookup requests required `cpp` and
fails. Required unavailable/unknown components fail. A required `core` plus
an optional unavailable component succeeds, reports that component false, and
imports only Core.

### Direct dependencies

- Direct ASC target dependencies: none.
- Direct external link dependencies: none.
- Public/runtime dependencies: C++20 standard library only.
- Configure-time build dependency: exact released `ASCCMake 0.1.0` at
  `8a7dcbad3a97267cce59810aff24de800a3497a7`.
- CI action dependency: `actions/checkout` pinned to
  `3d3c42e5aac5ba805825da76410c181273ba90b1`.

The library uses the actual released `asc_target_enable_cxx20`,
`asc_target_enable_warnings`, and `asc_target_enable_sanitizers` APIs.
Standard CMake owns the conditional component export. No unapproved
dependency was added.

## 3. Exact commands and pass/fail/skip results

### Repository and predecessor preflight

```sh
git fetch --prune origin
git rev-parse main origin/main HEAD \
  feature/asc-cpp-m0-foundation feature/asc-cpp-m1-core
git -C ../asc-cmake status --short --branch
git -C ../asc-cmake rev-parse HEAD
git -C ../asc-cmake describe --tags --exact-match HEAD
```

Result: passed. All five ASCCpp refs were `33b261e`; `main` equals
`origin/main`. ASCCMake was clean `main`, exact tag `v0.1.0`, at `8a7dcba`.
The M0 predecessor had 288 staged paths and no unstaged change when M1
advanced.

### Minimum CMake, GCC Release static, paths with spaces

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S ../asc-cmake \
  -B "/tmp/asc-cpp-m1-final.wNcn5u/asc cmake 3.25" \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . \
  -B "/tmp/asc-cpp-m1-final.wNcn5u/release static with spaces" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-final.wNcn5u/asc cmake 3.25" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build "/tmp/asc-cpp-m1-final.wNcn5u/release static with spaces" \
  --parallel 4

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir "/tmp/asc-cpp-m1-final.wNcn5u/release static with spaces" \
  --output-on-failure
```

Result: passed after the final selective-export correction, 44/44 tests, zero
failed, zero skipped.

### Current CMake, Clang Debug shared, paths with spaces

```sh
cmake -S ../asc-cmake \
  -B "/tmp/asc-cpp-m1-final.wNcn5u/asc cmake current" \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

cmake -S . \
  -B "/tmp/asc-cpp-m1-final.wNcn5u/clang debug shared with spaces" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-final.wNcn5u/asc cmake current" \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON

cmake --build \
  "/tmp/asc-cpp-m1-final.wNcn5u/clang debug shared with spaces" \
  --parallel 4

ctest --test-dir \
  "/tmp/asc-cpp-m1-final.wNcn5u/clang debug shared with spaces" \
  --output-on-failure
```

Result: passed, 44/44 tests, zero failed, zero skipped.

Independent verification also ran current-CMake GCC Debug static and Release
shared warnings-as-errors matrices. Both passed 44/44. The separate
portability role repeated minimum-CMake Release static, GCC Release shared,
and Clang Debug static; each passed 44/44.

### Whole-library exceptions-disabled build and installed consumer

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S . \
  -B "/tmp/asc-cpp-m1-final.wNcn5u/release no exceptions" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-final.wNcn5u/asc cmake 3.25" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS=-fno-exceptions \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=OFF \
  -DASC_CPP_BUILD_TESTING=OFF \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_INSTALL_PREFIX="/tmp/asc-cpp-m1-final.wNcn5u/no exceptions prefix"

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build "/tmp/asc-cpp-m1-final.wNcn5u/release no exceptions" \
  --parallel 4

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --install "/tmp/asc-cpp-m1-final.wNcn5u/release no exceptions"

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S tests/consumer/core \
  -B "/tmp/asc-cpp-m1-final.wNcn5u/no exceptions isolated consumer" \
  -DASCCpp_DIR="/tmp/asc-cpp-m1-final.wNcn5u/no exceptions prefix/lib/cmake/ASCCpp" \
  -DASCCPP_EXPECT_LIBRARY_TYPE=STATIC_LIBRARY \
  -DASCCPP_OPTIONAL_COMPONENT=utilities \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS=-fno-exceptions

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build "/tmp/asc-cpp-m1-final.wNcn5u/no exceptions isolated consumer" \
  --parallel 4

"/tmp/asc-cpp-m1-final.wNcn5u/no exceptions isolated consumer/\
asc_cpp_core_consumer"
```

Result: passed configure, six-source library compile/link, installation,
isolated consumer configure/build/link, and runtime.

### Formatting, source policy, presets, symbols, and dependencies

```sh
find include/asc/core src/core tests/core tests/compile \
  tests/consumer/core tests/consumer/subproject \
  -type f \( -name '*.h' -o -name '*.cc' \) -print0 |
  xargs -0 clang-format-19 --dry-run --Werror

cmake --list-presets=all
git diff --cached --check

llvm-nm-19 -D --defined-only --demangle \
  "/tmp/asc-cpp-m1-final.wNcn5u/clang debug shared with spaces/\
src/core/libasc_core.so"

ldd \
  "/tmp/asc-cpp-m1-final.wNcn5u/clang debug shared with spaces/\
src/core/libasc_core.so"
```

Result: passed. Seven configure/build and five test presets parsed. The
selectively exported execution/event/Core symbols were present. The shared
library has only the platform C/C++ runtime closure. No local `clang-tidy`
executable exists, so clang-tidy is `skipped` locally and remains a pinned
Clang 18 hosted-CI gate.

### Resolved failed attempts

- Initial integrated package tests failed 2/44 because the inherited M0
  consumer requested 0.0.0 and then conflated optional-only with
  required-Core-plus-optional behavior. The lead corrected the fixture and
  added both distinct cases; final matrices pass 44/44.
- Initial independent formatting found drift in test files. The verification
  owner formatted its exclusive scope; the final whole-scope format gate
  passes.
- Documentation's first Ninja configure failed because Ninja is absent, and
  one subsequent reviewer run used the wrong executable name. Unix Makefiles
  configure/build and the correct executable both passed; neither was a
  product failure.

## 4. Sanitizer, package, relocation, and isolated-consumer results

### Sanitizers

```sh
cmake -S . \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/asan ubsan static" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-portability.Uyf6Vw/asc cmake package" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/asan ubsan static" --parallel 2

ctest --test-dir \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/asan ubsan static" \
  --output-on-failure --label-exclude "package|consumer"
```

Result: ASan+UBSan passed 38/38 instrumented architecture, compile, contract,
and Core tests. The six package/consumer tests are intentionally
non-instrumented so development sanitizer flags do not leak to nested
consumers; they pass in every static/shared matrix.

Standalone LeakSanitizer configure/compile/link and the selected memory,
ownership, copy, and event runtime test passed without a reported leak.

ThreadSanitizer is configure-tested and compile-tested. Runtime is
**skipped**, not passed: the local runtime terminates before the test with
`FATAL: ThreadSanitizer: unexpected memory mapping`.

### Package, relocation, and consumers

Build-tree, copied/relocated package, installed package, moved installed
prefix, static, shared, path-with-spaces, optional unavailable component,
subproject, and isolated consumers all pass.

Manual installation produced exactly the Core library, 11 headers,
`ASCCppConfig.cmake`, `ASCCppConfigVersion.cmake`, the Core target files, and
the Apache-2.0 license. Installed CMake files contain no source/build/temp
absolute path. Only `ASCCppCoreTargets.cmake` is a component target export.
The package-registry snapshot is unchanged.

## 5. CPU/GPU provider evidence

| Surface | Classification | Result |
| --- | --- | --- |
| serial CPU Core | configure-tested, compile-tested, runtime-tested | Host allocation, configuration, I/O, synchronous serial context/event, and overlap-safe byte copy passed static/shared/sanitized/package/consumer matrices. |
| optimized CPU providers | skipped | No OpenMP, Eigen, BLAS/LAPACK, MKL, TBB, or SYCL provider is implemented or discovered by M1. |
| CUDA, HIP/ROCm, SYCL, or any GPU provider | **skipped** | No GPU option, language, discovery, SDK include/link, source, target, export, allocation, runtime operation, hardware execution, or parity oracle exists. |

No GPU evidence is classified configure-tested, compile-tested,
runtime-tested, or parity-tested. Hardware/toolkit inventory is not ASCCpp
provider evidence.

## 6. Review findings and resolutions

1. Production found exact origin overrides below configuration lists were
   accepted but ignored. Recursive metadata recording now applies each exact
   override while preserving inherited sensitivity/deprecation; tests pass.
2. Production omitted an unapproved command-line origin and kept future
   provider state inside `execution.cc`, preserving the exact M1 file/API
   contract.
3. Documentation found `File` move assignment's release-active
   closed-destination precondition was invisible. The public header and Core
   guide now state it.
4. Verification removed an accidental M2-only command-line-origin test,
   corrected test formatting, and identified the inherited package-version
   and optional-component fixture defects. All were resolved.
5. Portability identified probable MSVC C4251 failures from whole-class
   export of execution/event types containing standard smart pointers. The
   lead changed them to selective out-of-line DLL annotations; post-fix
   static/shared tests and dynamic-symbol inspection pass.
6. Portability found the serial backend row still marked pending. It now
   records the exact final configure/compile/runtime evidence; all deferred
   providers remain `skipped`.
7. Final integration updated stale M0 wording in the live security policy and
   mechanically enforces implemented/unavailable component lists and the
   M1-versus-later capability-status split.

No unresolved production, package/API, verification, documentation,
portability, GPU-isolation, license, provenance, or performance blocker
remains locally.

## 7. Performance evidence

No numerical kernel, benchmark, or performance threshold is approved in M1,
so throughput, scaling, accelerator parity, and regression measurements are
not applicable.

The bounded code/allocation evidence is:

- checked arithmetic and `Extents` allocate nothing;
- nonzero `Buffer::Allocate` performs one resource allocation; zero-byte
  allocation performs none;
- views, `ReadExact`, `WriteAll`, serial event completion, and serial
  `CopyBytes` allocate no operation storage;
- serial copy uses synchronous overlap-safe `memmove` with no packing,
  transfer, fallback, or hidden synchronization; and
- configuration validation is tree-linear plus ordered-map costs and
  intentionally builds one transactional output and metadata map.

These are contract and implementation observations, not benchmark claims.

## 8. Remaining risks

- Hosted Clang 18 clang-tidy, Windows MSVC 2022 shared Debug/Release, and
  AppleClang/macOS arm64 remain pending until publication and CI. Source
  review is not relabeled as a compiler pass.
- Hosted CI requires a repository-administered least-privilege
  `ASC_CMAKE_READ_TOKEN` for the exact private ASCCMake checkout.
- TSan runtime is skipped because the local runtime cannot initialize.
- Native Windows runtime paths, Windows static consumers, non-x86 hosts,
  32-bit `size_t`, and big-endian execution are not locally exercised.
- Non-owning resource pointers and memory views require caller-enforced
  lifetime and synchronization.
- `File` destruction cannot report close failure; callers needing observable
  durability must explicitly call `Flush()` and `Close()`.
- The unreleased 0.1 API uses standard-library value types and claims no
  stable cross-toolchain ABI.
- The cumulative staged candidate contains M0 and M1 because the owner
  advanced from an uncommitted M0 checkpoint. Remote review must treat the
  complete staged diff as one publication unit unless the owner separately
  requests a commit split.
- The ignored local `build/` tree remains unrelated cumulative work and must
  not be cited as M1 evidence.

## 9. Exact proposed remote and branch-cleanup actions

No remote or cleanup action is authorized at Checkpoint B. Milestone
completion is not a release decision, so no tag or GitHub release is proposed.

After separate publication approval, the exact cumulative publication actions
would be:

```sh
git commit -m "Establish asc-cpp foundation and implement milestone 1 core"
git push --set-upstream origin feature/asc-cpp-m1-core
gh pr create \
  --base main \
  --head feature/asc-cpp-m1-core \
  --title "Implement asc-cpp milestone 1 core foundation" \
  --body-file \
  docs/development/asc-cpp-m1-core/publication-checkpoint-b.md
```

Do not amend or force-push the validated candidate. Inspect all required
hosted checks and independent review before merge.

No branch deletion is proposed before a separately approved merge. After a
future merge and separate cleanup authorization, the M1 branch alone may be
considered with these exact ancestry/worktree gates:

```sh
git fetch --prune origin
git switch main
git pull --ff-only origin main
git merge-base --is-ancestor \
  origin/feature/asc-cpp-m1-core origin/main
git worktree list --porcelain
git push origin --delete feature/asc-cpp-m1-core
git branch -d feature/asc-cpp-m1-core
```

The delete commands must run only if ancestry succeeds, no worktree uses the
branch, and no intended unique commit is unpreserved. Do not delete `main`,
the M0 or M8 branches, a recovery branch, any active worktree branch, or any
tag as part of this checkpoint.
