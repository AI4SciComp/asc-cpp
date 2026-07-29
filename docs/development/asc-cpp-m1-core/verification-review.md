# Milestone 1 independent verification review

## Review status

Milestone 1 is independently verified locally at Publication Checkpoint B.
The reviewed branch was `feature/asc-cpp-m1-core`, based on
`33b261ea33616a6395c4ad3b20646093103344f7` before the uncommitted milestone
changes.

The review was designed from the frozen milestone contract and ADRs 0004
through 0009 before inspecting the production implementation. The resulting
tests cover only the Milestone 1 core surface. No Milestone 2 behavior is
accepted by this review.

## Independent test design

The verification suite covers:

- all eleven public headers as self-contained C++20 translation units, both
  normally and with compiler exceptions disabled;
- aggregate exception-disabled compilation and a multi-translation-unit
  include/link test;
- the exact Milestone 1 public header and source inventories, target topology,
  direct-link dependency set, provider-include policy, include-guard policy,
  and fatal-path restrictions;
- stable error-code values, status metadata and rendering, move-only result
  behavior, `Result<Status>`, and conditional `noexcept`;
- exact metadata types, checked casts and arithmetic, byte-count helpers, and
  scalar, zero, dynamic, invalid, and overflowing extents;
- all configuration value alternatives, UTF-8 validation, exact numeric
  typing, recursive schema validation, defaults, bounds, deprecation,
  sensitivity, JSON-pointer paths, origin precedence, redaction, and
  transactional rollback;
- partial I/O, EOF, short operations, injected failures, zero progress,
  over-reporting, little-endian integer and floating-point encoding, text-file
  limits, file modes, moves, flushes, and closes;
- host allocation alignment and ownership, injected allocation failures,
  invalid resource returns, serial and unavailable contexts, overlap-safe
  copies, event moves and completion, and rejection before mutation;
- fatal contract behavior for checks, debug checks, invalid result access, and
  active file move assignment, including a failed-result fatal path that does
  not allocate; and
- build-tree, relocated-install, and subproject isolated consumers of
  `ASC::core`, including exact component and exported-target assertions.

## Findings and resolutions

1. An early independent configuration test accidentally used the Milestone 2
   command-line origin. It was corrected before acceptance to test only
   Milestone 1 default and programmatic origins.
2. The existing unavailable-component package fixture requested version
   `0.0.0`, so it failed at version selection instead of component selection.
   Integration corrected the fixture to request package version `0.1.0`.
3. The optional-component package fixture expected `ASC::core` after requesting
   only unavailable utilities. Integration retained that optional-only case
   with conditional exports and added the distinct required-core plus optional
   unavailable-component case.
4. The lead formatting audit found style drift in independent C++ tests.
   `clang-format-19` was applied to the exclusive verification scope, and all
   affected validation configurations were rebuilt and rerun.

No unresolved production defect remains from independent local review.

## Validation evidence

The local environment was:

- CMake 4.1.2;
- GCC/G++ 11.4.0;
- GNU Make 4.3;
- clang-format 19.0.0; and
- ASCCMake 0.1.0 at
  `8a7dcbad3a97267cce59810aff24de800a3497a7`.

The evidence root was `/tmp/asc-cpp-m1-verification.ulmd1P`.

### Debug static, warnings as errors

```sh
cmake -S "$PWD" \
  -B /tmp/asc-cpp-m1-verification.ulmd1P/debug-static \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$PWD/../asc-cmake/build/prefix/share/ASCCMake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m1-verification.ulmd1P/debug-static \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m1-verification.ulmd1P/debug-static \
  -C Debug --output-on-failure
```

Result: pass, 44 of 44 tests; zero failures and zero skips.

This run passed the build-tree consumer, relocated installed consumer with
spaces in its path, subproject consumer, component fixtures, and registry
preservation checks.

### Release shared, warnings as errors

```sh
cmake -S "$PWD" \
  -B /tmp/asc-cpp-m1-verification.ulmd1P/release-shared \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$PWD/../asc-cmake/build/prefix/share/ASCCMake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m1-verification.ulmd1P/release-shared \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m1-verification.ulmd1P/release-shared \
  -C Release --output-on-failure
```

Result: pass, 44 of 44 tests; zero failures and zero skips.

This run independently confirms shared-library export behavior and the
release-only disabled behavior of `ASC_DCHECK`.

### Address and undefined behavior sanitizers

```sh
cmake -S "$PWD" \
  -B /tmp/asc-cpp-m1-verification.ulmd1P/asan-ubsan \
  -G "Unix Makefiles" \
  -DASCCMake_DIR:PATH="$PWD/../asc-cmake/build/prefix/share/ASCCMake" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build /tmp/asc-cpp-m1-verification.ulmd1P/asan-ubsan \
  --parallel 4
ctest --test-dir /tmp/asc-cpp-m1-verification.ulmd1P/asan-ubsan \
  -C Debug --output-on-failure \
  -R '^(asc_cpp\.architecture|asc_cpp\.compile|asc_cpp\.core)'
```

Result: pass, 38 of 38 focused tests; zero failures and zero skips. ASCCMake's
sanitizer compile and link probes also passed. Package and isolated-consumer
tests were intentionally excluded from the instrumented run and passed in both
the static and shared configurations above.

### Formatting and whitespace

```sh
clang-format-19 --dry-run --Werror \
  $(rg --files tests/core tests/compile tests/consumer/core \
    -g '*.cc' -g '*.h')
rg -n '[[:blank:]]+$' tests/core tests/compile tests/consumer/core
git diff --check -- tests/core tests/compile tests/consumer/core \
  docs/development/asc-cpp-m1-core/verification-review.md
```

Result: pass. The trailing-whitespace search returned no matches.

## Provider, portability, and performance classification

The CPU serial context and host memory provider are **runtime-tested** by both
static and shared semantic suites and by the ASan+UBSan run.

GPU evidence is **skipped** because Milestone 1 has no GPU provider target or
runtime surface. Adding one would implement a later milestone.

No dedicated performance threshold applies to this foundational milestone.
The exhaustive checked-arithmetic tests over all signed eight-bit operand
pairs are functional evidence only and make no performance claim.

## Remaining risks

- GCC 11.4 on WSL2 is the only compiler and host combination exercised by this
  independent local review. Clang, AppleClang, MSVC, native Windows, macOS, and
  non-x86 hosts remain hosted-CI evidence.
- CMake 3.25 was unavailable locally, so the minimum supported CMake version
  remains hosted-CI evidence.
- ThreadSanitizer and standalone LeakSanitizer were not run in this independent
  wave. AddressSanitizer and UndefinedBehaviorSanitizer passed.
- No GPU evidence is applicable to the approved Milestone 1 scope.
