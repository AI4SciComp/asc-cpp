# Milestone 1 portability, GPU, and performance review

Status: complete and acceptable for Publication Checkpoint B on 2026-07-27

## Scope and result

This independent review covers only the frozen Milestone 1 provider-free Core
CPU foundation. It reviewed the complete integrated diff, the preceding role
reports, the approved architecture and ADRs, the released ASCCMake 0.1.0
implementation, and the dependency, capability, and backend records.

The current candidate is acceptable for Publication Checkpoint B. Two
portability/evidence findings were reported to the lead and resolved. No
release-blocking portability, GPU-isolation, performance, security, license,
or provenance finding remains locally.

## Portability and ABI review

- The exact eleven public headers use C++20 standard-library facilities and no
  provider SDK. GCC 11.4 and Clang 19 compile the complete current candidate
  with warnings as errors; every header also compiles alone and with
  exceptions disabled.
- Fixed-width logical metadata and bytewise little-endian encoding avoid
  native integer-width and object-layout serialization assumptions.
- POSIX file opening uses native byte paths. The Windows path selects
  `_wfopen_s` and binary mode, so it does not narrow a native wide path.
- Static consumption exports `ASC_CORE_STATIC_DEFINE`. Shared compilation uses
  hidden ELF visibility and selectively exports the supported Core operations.
- The installed static and shared target files contain no source-tree,
  build-tree, or temporary absolute path. The build-tree export intentionally
  names its build artifacts and source include tree.
- Shared-library inspection found no ASC or third-party link dependency beyond
  the platform C and C++ runtime libraries. The public package target has no
  `INTERFACE_LINK_LIBRARIES`.
- Top-level, subproject, build-tree, installed, relocated, path-with-spaces,
  static, shared, and isolated-consumer modes passed locally. Subproject
  testing and installation default off.

MSVC and AppleClang execution remains hosted evidence. This review does not
convert source inspection into a compiler pass for either toolchain.

## Findings and resolutions

### P1: whole-class DLL export exposed standard-library implementation members

Initial severity: medium; probable MSVC `/W4 /WX` blocker.

`ExecutionContext` and `CompletionEvent` initially applied
`ASC_CORE_EXPORT` to the complete class while storing `std::shared_ptr` and
`std::unique_ptr` members. MSVC commonly diagnoses that DLL-interface pattern
with C4251, and the proposed workflow treats warnings as errors.

Resolution: the lead removed whole-class export and selectively exported the
out-of-line factories, operations, move members, and destructor. Inline
accessors remain header-local and private constructors remain internal.
Post-resolution GCC shared and Clang static suites passed 44/44. Dynamic-symbol
inspection found all supported `ExecutionContext`, `CompletionEvent`, and
`CopyBytes` operations and no private execution-state constructor.

### P2: serial backend evidence remained marked pending

Initial severity: low.

The backend matrix initially retained `pending clean validation` after the
integrated static/shared/runtime/package matrix had passed.

Resolution: the lead replaced the three serial cells with the exact CMake,
compiler, exceptions-disabled, runtime, package, relocation, subproject, and
consumer evidence. Every deferred CPU provider and GPU provider cell remains
exactly `skipped`.

## Clean local validation

The review evidence root was:

```text
/tmp/asc-cpp-m1-portability.Uyf6Vw
```

The host used CMake 4.1.2, CMake 3.25.0 from the preserved minimum-version
environment, GCC 11.4, Clang 19.0, GNU Make 4.3, and released ASCCMake 0.1.0
at `8a7dcbad3a97267cce59810aff24de800a3497a7`.

### Released ASCCMake build-tree package

```sh
cmake -S /home/yicai/AI4SciComp/asc-cmake \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/asc cmake package" \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF
```

Result: passed.

### Minimum CMake, GCC Release static

```sh
/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cmake \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/minimum asc cmake" \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/minimum release static" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-portability.Uyf6Vw/minimum asc cmake" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Release

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/cmake \
  --build \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/minimum release static" \
  --parallel 2

/tmp/asc-cpp-m0-cmake325.6g4uic/venv/bin/ctest \
  --test-dir \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/minimum release static" \
  --output-on-failure
```

Result: passed, 44/44 tests, zero failed, zero skipped.

### GCC Release shared after selective-export correction

```sh
cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/release shared" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-portability.Uyf6Vw/asc cmake package" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake --build \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/release shared" --parallel 2

ctest --test-dir \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/release shared" \
  --output-on-failure
```

Result: passed, 44/44 tests, zero failed, zero skipped. Build-tree, installed,
relocated, path-with-spaces, shared, subproject, optional-component, and
isolated consumers all passed.

### Clang Debug static after selective-export correction

```sh
CC=clang-19 CXX=clang++-19 cmake \
  -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/clang debug static" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-portability.Uyf6Vw/asc cmake package" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/clang debug static" --parallel 2

ctest --test-dir \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/clang debug static" \
  --output-on-failure
```

Result: passed, 44/44 tests, zero failed, zero skipped.

### AddressSanitizer and UndefinedBehaviorSanitizer

```sh
cmake -S /home/yicai/AI4SciComp/asc-cpp \
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

Result: passed, 38/38 instrumented architecture, compile, contract, and Core
tests; zero failed and zero skipped. Package and consumer behavior was tested
separately without leaking development sanitizer flags into installed targets.

### ThreadSanitizer

```sh
cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/tsan static" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-portability.Uyf6Vw/asc cmake package" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_THREAD_SANITIZER=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/tsan static" \
  --target asc_core_memory_execution_test --parallel 2

"/tmp/asc-cpp-m1-portability.Uyf6Vw/tsan static/tests/core/\
asc_core_memory_execution_test"
```

Result: configure-tested and compile-tested, but runtime `skipped`. The host
runtime terminated before the test with:

```text
FATAL: ThreadSanitizer: unexpected memory mapping
```

This is not a ThreadSanitizer pass and not an ASCCpp test failure.

### LeakSanitizer

```sh
cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B "/tmp/asc-cpp-m1-portability.Uyf6Vw/lsan static" \
  -DASCCMake_DIR="/tmp/asc-cpp-m1-portability.Uyf6Vw/asc cmake package" \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_LEAK_SANITIZER=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/lsan static" \
  --target asc_core_memory_execution_test --parallel 2

"/tmp/asc-cpp-m1-portability.Uyf6Vw/lsan static/tests/core/\
asc_core_memory_execution_test"
```

Result: passed the compile/link probes, target build, and selected ownership,
allocation, copy, and event runtime test with no reported leak.

### Symbols, dependencies, formatting, and policy

```sh
ldd \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/release shared/src/core/libasc_core.so"

nm -D --defined-only \
  "/tmp/asc-cpp-m1-portability.Uyf6Vw/release shared/src/core/libasc_core.so" |
  c++filt |
  rg 'asc::(ExecutionContext|CompletionEvent|CopyBytes)'

clang-format-19 --dry-run --Werror \
  $(rg --files include/asc src/core tests/core tests/compile \
    tests/consumer/core -g '*.h' -g '*.cc')

git diff --check -- \
  include/asc src/core tests/core tests/compile tests/consumer/core \
  docs/development/asc-cpp-m1-core
```

Result: passed. The shared object depends only on the platform C/C++ runtime
closure. Selectively exported execution/event symbols were present. Formatting
and whitespace checks passed. No local `clang-tidy` executable was available;
the pinned hosted Clang workflow owns that pending evidence.

## Ownership, concurrency, and security

Core has no mutable process-global handler, registry, context, resource,
provider state, or hidden cache. Immutable contexts and separate objects may be
used concurrently. Same-object mutation, move, close, event query/wait, and
destination-memory synchronization remain caller responsibilities and are
documented.

Allocation/resource failure paths, invalid resource returns, moved-from owners,
exactly-once release, failed-result fatal access, overlap-safe copy,
inaccessible memory, I/O short progress, size bounds, integer overflow, UTF-8
validation, unknown configuration keys, rollback, and sensitive-value
redaction are tested. The module adds no network, parser, dynamic plugin,
provider, or downloaded-data surface.

## GPU and provider evidence

The CPU serial reference path is runtime-tested locally in static, shared,
sanitized, package, relocation, subproject, and isolated-consumer modes.

GPU evidence classification: **skipped**.

Milestone 1 has no GPU option, language, discovery, SDK include, source, target,
export, allocation, transfer, runtime operation, or parity oracle. CUDA,
HIP/ROCm, SYCL, OpenMP, Eigen, and BLAS/LAPACK are all outside the approved M1
scope. Toolkit or hardware inventory is not provider evidence.

## Performance review

No numerical benchmark or performance threshold applies to this foundational
milestone. The reviewed cost contracts are appropriate:

- checked casts/arithmetic and `Extents` perform no success-path allocation;
- nonzero `Buffer::Allocate` performs one resource allocation, while a
  zero-byte buffer performs none;
- `ReadExact`, `WriteAll`, memory views, event completion, and serial
  `CopyBytes` allocate no operation storage;
- serial copies use synchronous overlap-safe `memmove` with no packing,
  transfer, fallback, or hidden synchronization; and
- configuration validation is linear in the traversed tree plus ordered-map
  costs and intentionally constructs a complete transactional output and
  metadata map.

These are code, contract, and allocation-counter observations, not benchmark
claims.

## License and provenance

ASCCpp and released ASCCMake are Apache-2.0. The installed package includes the
ASCCpp root license. Core uses only the C++ standard library and adds no
third-party runtime dependency, copied table, generated corpus, or notice
obligation.

Production and verification report clean-room implementation from the approved
contract. Repository scans found no MdeCpp marker, GPL notice, provider SDK
include, generated data, or numerical corpus in the Milestone 1 production and
verification scope.

## Remaining risks

- Actual MSVC/Windows shared compilation and AppleClang/macOS compilation,
  installation, and execution remain hosted-CI evidence. The selective-export
  correction is source-reviewed and locally validated but is not relabeled an
  MSVC pass.
- Hosted CI requires the repository-administered least-privilege
  `ASC_CMAKE_READ_TOKEN`; local validation cannot establish that secret.
- ThreadSanitizer runtime evidence is skipped because the local runtime cannot
  initialize. The current tests do not make a race-detector pass claim.
- Native Windows path handling, non-x86 targets, 32-bit `size_t`, and
  big-endian execution are not locally exercised.
- Non-owning resource pointers and memory views depend on caller-enforced
  lifetime and synchronization. The type system cannot prove those
  obligations.
- `File` destruction cannot report close failure; durability-sensitive callers
  must call `Flush()` and `Close()`.
- The unreleased 0.1 API exposes standard-library value types and does not
  claim stable cross-toolchain ABI. Exact ABI/symbol compatibility remains a
  later hardening and release decision.
