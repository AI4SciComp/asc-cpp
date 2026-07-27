# Milestone 1 independent verification review

Status: Independent implementation and final portability re-review complete

## Independence record

The verification engineer read the complete runbook, frozen Milestone 1
contract and ownership ledger, all 18 approved ADRs, the architecture
blueprint, dependency and capability manifests, backend matrix, testing
strategy, implementation plan, verified ASCCMake contract, and the complete
Milestone 0 test suite before inspecting any Milestone 1 production
implementation. The following falsification plan was fixed from those
contracts alone.

No MdeCpp source, test structure, literal corpus, or generated data is an input
to this test design. Expected values are derived directly from the approved
ASC contracts, C++20 arithmetic, and documented byte encodings.

## Contract-first falsification plan

### Compile and dependency contracts

- Compile each exact public header as the only ASC include in a strict C++20
  translation unit.
- Repeat representative public-header compilation with exceptions disabled
  where the compiler supports that mode.
- Link representative uses split across multiple translation units to expose
  missing definitions, accidental internal linkage, and ODR violations.
- Mechanically reject public or production includes from any higher ASC
  module, provider SDK, or retired API path.
- Inspect the build-tree and installed `ASC::core` target so its direct ASC
  dependency closure is empty and its public compile contract is C++20 with
  extensions disabled.

### Status, result, and contract failures

- Exercise every stable `ErrorCode` enumerator and preservation of diagnostic,
  provider name, and signed native code.
- Verify OK and non-OK `Status`, value and failure `Result<T>`, move-only
  payloads, valid moves, and destruction.
- Verify that failed-result value access terminates through the release-active
  fatal contract in both Debug and Release.
- Verify `ASC_CHECK` in both configurations and `ASC_DCHECK` only in Debug
  without relying on a mutable process-global failure handler.

### Logical metadata and extents

- Prove the exact signed and unsigned widths and the distinct
  `kDynamicExtent == -1` sentinel at compile time.
- Exercise checked casts across signed/unsigned and width boundaries, checked
  addition/multiplication, and byte-count calculations at zero, exact limits,
  and one-past-overflow inputs.
- Exercise rank-zero logical size one, zero-extent logical size zero, mixed
  static/dynamic extents, negative dynamic extents, arity errors, and complete
  product overflow before publication.

### Configuration transactions

- Exercise all eight exact value alternatives, nested lists/objects, exact
  type discrimination, and the absence of numerical coercion.
- Validate required/default/deprecated/sensitive fields, nested schemas,
  numeric and size bounds, unknown-key rejection, and invalid schema defaults.
- Prove all-or-nothing publication on late recursive failure.
- Verify complete path origins, optional source labels/locations, and
  redaction of sensitive values and diagnostics.
- Mechanically verify no argv, environment, response-file, concrete-file,
  dense, or sparse parsing/value surface appears in core.

### I/O progress, encoding, and file ownership

- Use deterministic project-owned sources/sinks to exercise partial reads and
  writes, zero-byte operations, clean exact completion, EOF/truncation, source
  failure, sink failure, and zero-progress rejection.
- Round-trip fixed-width signed/unsigned values and IEC 60559 float/double
  through explicitly derived little-endian byte sequences, including signed
  zero and representative bit patterns.
- Exercise bounded text-file success, exact limit, one-past-limit rejection,
  and transactional destination publication.
- Exercise file create/open/read/write/flush/close, move construction,
  explicit close before move assignment, repeated close, invalid state, and
  temporary paths containing spaces.

### Host memory and serial execution

- Verify required alignment, zero-byte allocation without publishing storage,
  allocation failure propagation, move-only buffer ownership, moved-from
  state, explicit release, and exactly-once deallocation using a
  failure-injecting counting resource.
- Exercise host view bounds, inaccessible pinned/device/managed requests,
  overlap-safe forward and backward copies, zero-byte copies, destination
  transaction behavior on validation failure, and independent immutable
  serial contexts.
- Verify serial CPU backend/device/determinism metadata, already-complete
  move-only events, event moves, and unavailable/unsupported non-CPU
  backends/spaces without fallback.
- Keep allocation/lifetime tests sanitizer-friendly and avoid undefined
  dangling-view probes.

### Package and consumer contracts

- Compile and run an isolated consumer against build-tree, installed, and
  relocated `ASCCpp 0.1` packages requesting only `core`.
- Verify static and shared producers where supported, paths containing spaces,
  no-component failure, unavailable/unknown required-component failure, and
  optional unavailable-component behavior beside required `core`.
- Verify no user package-registry write and no imported target other than
  `ASC::core`.

## Evidence classification

GPU provider evidence for this CPU-only milestone is required to be
**skipped**. Toolchain or hardware inventory is not asc-cpp configure,
compile, runtime, or parity evidence.

## Implemented verification surface

The implementation-aligned suite is entirely project-owned and uses no
third-party test dependency:

- `tests/compile` creates paired normal and exceptions-disabled executables
  for all 11 exact public headers, plus an aggregate exceptions-disabled
  consumer, a multi-translation-unit executable, and a mechanical file,
  include, namespace, provider, direct-link, and fatal-allocation audit.
- `tests/core/status_result_test.cc` covers all 19 stable error-code values,
  provider/native diagnostics, move-only results, and conditional
  `noexcept`.
- `tests/core/types_extents_test.cc` covers checked conversion, exhaustive
  signed 8-bit add/multiply properties, 64-bit boundaries, byte counts,
  rank-zero, zero extent, dynamic-argument constraints, and extent product
  overflow.
- `tests/core/configuration_test.cc` covers exact alternatives and integer
  signedness, strict UTF-8 acceptance/rejection, nested schemas, defaults,
  required/unknown/type/bounds failures, effective object size after defaults,
  JSON Pointer escaping, origin, sensitivity, deprecation, rollback, and
  redaction.
- `tests/core/io_test.cc` covers partial and zero-progress sources/sinks,
  EOF/truncation/failure/over-reporting, fixed-width endian encodings,
  floating bit preservation, bounded text files, path spaces, file modes,
  moves, close, and invalid state.
- `tests/core/memory_execution_test.cc` covers host alignment, injected
  allocation/null/zero/misalignment failures, exactly-once release, buffer and
  event moves, serial context metadata, unavailable backends, inaccessible
  spaces, transactional copy failure, and overlap-safe copying.
- Contract subprocesses cover release-active `ASC_CHECK`, debug-only
  `ASC_DCHECK`, failed-result access, allocation-free fatal entry, and the
  open-destination file move-assignment contract.
- `tests/consumer/core` requests only `core`, calls out-of-line symbols, checks
  the C++20 requirement and imported-target isolation, and supports
  build-tree, installed/relocated, static/shared, and optional-unavailable
  package cases.

## Findings and resolutions

The verification wave reported the following defects before accepting their
fixes:

1. GNU visibility placed after `[[nodiscard]]` made `Status` fail GCC header
   compilation. Export annotations now decorate out-of-line methods in a
   grammar accepted by GCC while preserving Windows shared-library exports.
2. `Result<T>` move assignment advertised `noexcept` from move assignment
   alone. It now also requires nothrow move construction, matching
   `std::optional<T>`.
3. variadic `Extents::Create` accepted floating inputs and performed unchecked
   casts. It now accepts only checked integers and reports out-of-range
   unsigned input as `kOverflow`.
4. the endian concept admitted signed 8- and 16-bit types while its unsigned
   representation mapped them to 64 bits. The mapping now covers 1, 2, 4, and
   8-byte integral widths.
5. failed-result value access rendered `Status::ToString()` before entering
   `FatalContract`, allowing allocation on the fatal path. It now forwards an
   existing string view, with both a source audit and allocation-failing death
   regression.
6. the `File::Close() noexcept` failure branch built an allocating diagnostic.
   It now returns an allocation-free `kIo` status with native code; the
   destructor uses a direct non-allocating close and explicit `Close()` remains
   idempotent.
7. invalid moved-from buffer view access constructed an allocating status
   inside methods declared `noexcept`. Those view accessors no longer
   over-promise `noexcept`.
8. `Buffer::Allocate` trusted a custom resource's alignment. It now rejects a
   deliberately misaligned pointer and deallocates it exactly once before
   returning failure.
9. raw configuration strings could not uphold the UTF-8 contract. The only
   string entry point is now the fallible `Utf8String` factory, with strict
   malformed, truncated, overlong, surrogate, and Unicode-range rejection.
10. after removing raw string constructors, character pointers could still
    select the Boolean constructor. Compile contracts now exclude `char*` and
    `const char*`, and production constrains Boolean construction to actual
    `bool`.
11. object size bounds were initially evaluated before defaults. They now
    apply to the effective object, with transactional maximum failure and
    minimum-after-default success regressions.
12. `Status::Ok()` initially lacked an explicit shared-library export.
    Isolated consumers now call it and `ErrorCodeName` so hosted Windows shared
    builds link-test the exported surface.
13. file move assignment semantics were ambiguous. The accepted API requires
    the destination to be explicitly closed first so close errors remain
    observable; assigning into an open destination is a release-active fatal
    contract and has dedicated death coverage.
14. arbitrary underlying values of `Backend` and `Determinism` were not
    explicitly rejected before backend dispatch. `ExecutionContext::Create`
    now validates both enumerations, with regressions for unknown values.
15. the little-endian encoder shifted its working value after the only byte
    of an 8-bit scalar. It now omits that width-sized final shift; signed and
    unsigned 8-bit round trips pass under UBSan.
16. the first compiler gate for exception-disabled tests admitted clang-cl
    because its compiler ID is `Clang`, then passed the GNU-only
    `-fno-exceptions` spelling. A Clang 19 clang-cl-mode probe reproduced the
    warnings-as-errors failure. The accepted gate now requires an exact
    GNU/Clang/AppleClang ID and excludes the MSVC frontend variant.

No accepted finding required a higher-module dependency, optional provider,
third-party library, copied MdeCpp material, or later-milestone API.

## Final portability integration re-review

- Selective export annotations cover every out-of-line public core symbol.
  Configuration and status export their ABI-bearing methods without exporting
  their standard-library storage classes wholesale, while polymorphic and
  ownership types retain class exports. Private friend declarations and their
  namespace-scope free-function declarations carry the same annotation.
- A Clang 19 shared build with hidden default visibility exported the complete
  public out-of-line surface. Its isolated build-tree, relocated, and
  subproject consumers linked and ran. A separate `_WIN32` producer and
  consumer syntax probe accepted the selective `__declspec` placement in
  configuration, execution, I/O, memory, and status headers.
- Unknown `Backend` and `Determinism` regressions return
  `kInvalidArgument`; known serial, CUDA, HIP, and SYCL results remain
  unchanged.
- The 8-bit endian path is covered for signed and unsigned values. The final
  encoder structure cannot shift a one-byte scalar by its width.
- Exception-disabled registrations cover only GNU-like GNU, Clang, and
  AppleClang frontends. MSVC and clang-cl skip them instead of receiving an
  unsupported flag.
- The sanitizer preset's `package|consumer` label exclusion is intentional:
  those tests launch nested, uninstrumented configure/build processes. All
  instrumented architecture, compile, runtime, and contract tests remain.

## Independent local evidence

An external tree at
`/tmp/asc-cpp-m1-verification.oXsRAE` used CMake 4.1.2 and GCC 11.4:

```text
cmake -S /home/yicai/AI4SciComp/asc-cmake \
  -B <root>/asc-cmake \
  -DBUILD_TESTING=OFF \
  -DASC_CMAKE_BUILD_TESTING=OFF \
  -DASC_CMAKE_INSTALL=OFF

cmake -S /home/yicai/AI4SciComp/asc-cpp \
  -B <root>/asc-cpp-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=<root>/asc-cmake

cmake --build <root>/asc-cpp-debug --parallel 1
```

The static Debug warnings-as-errors tree built all production and verification
targets. In the first integrated CTest pass, architecture, all 25 compile
contracts, all five runtime suites, and all five contract subprocesses passed.
Build-tree, install/relocation, and subproject isolated consumers also passed
when rerun individually. After the final effective-object-size regression,
the focused configuration build and test passed. The lead independently
reported a complete 44/44 GCC Debug static warnings-as-errors pass before that
last regression; the complete final clean matrix remains the lead's
integration responsibility.

The final portability re-review used a clean Clang 19 shared-library tree:

```text
cmake -S . -B /tmp/asc-cpp-m1-independent-clang-shared.YUaHM2 \
  -G "Unix Makefiles" \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/dev-debug
cmake --build /tmp/asc-cpp-m1-independent-clang-shared.YUaHM2 -j2
ctest --test-dir /tmp/asc-cpp-m1-independent-clang-shared.YUaHM2 \
  --output-on-failure -j2
```

That tree passed 44/44 tests, including package and isolated-consumer tests.
A dynamic-symbol audit found every public out-of-line core definition in the
shared library's exported symbol table. The independently rerun Clang 19
ASan/UBSan tree passed all 38 instrumented, non-nested tests after applying
the preset's `package|consumer` exclusion. The lead separately reports a
44/44 minimum-CMake GCC Release static pass, a GCC no-exceptions install pass,
and the same 38/38 sanitizer result in its final matrix.

`clang-format` 19 formatted the assigned C++ sources and its dry-run check plus
`git diff --check` passed. The hosted repository policy uses Clang 18, so its
format result remains hosted/final-matrix evidence rather than an independent
local claim.

## Remaining evidence and risks

- MSVC, AppleClang, Clang 18, and hosted CI are not locally claimed. The
  `_WIN32` probe establishes syntax only, not an MSVC shared-library link.
  The out-of-line consumer calls are specifically intended to catch Windows
  shared-export failures in hosted CI.
- There is no concurrency, numerical kernel, or performance operation in this
  milestone. TSan and benchmarks are not applicable to the implemented
  surface.
- GPU evidence is **skipped**: no provider facet or provider source is in
  Milestone 1.

There is no unresolved release-blocking defect in the assigned verification
scope. Final acceptance remains with the lead.
