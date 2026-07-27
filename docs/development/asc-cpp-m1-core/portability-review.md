# Milestone 1 portability, GPU, and performance review

Status: independent review complete; accepted for Publication Checkpoint B with
hosted-platform evidence still pending

Evidence date: 2026-07-26

## Scope and independence

This review covers only **Milestone 1 — core CPU foundation** on
`feature/asc-cpp-m1-core`. The reviewer read the complete runbook, frozen
milestone contract and ownership ledger, all 18 approved ADRs, the Stage A
manifests and backend matrix, released ASCCMake 0.1.0, and the integrated
Milestone 1 implementation before writing this report.

The review was read-only outside this file. It assessed C++20 and toolchain
portability, static/shared linkage, symbol visibility, package consumers,
exception-disabled compilation, sanitizers, undefined behavior, ownership,
thread safety, optional-provider isolation, GPU evidence, performance claims,
license, and provenance.

## Findings and accepted resolutions

All findings below were reported to the lead and corrected in the owning scope
before this review closed.

| ID | Severity | Finding | Accepted resolution and final location |
| --- | --- | --- | --- |
| PORT-M1-001 | blocking evidence defect | A sanitized static `asc_core` archive could not be linked by an uninstrumented nested package consumer; the link correctly failed on unresolved ASan/UBSan runtime symbols. Exporting development sanitizer flags would have polluted the consumer contract. | Keep sanitizer flags target-local. The sanitizer preset excludes all `package` or `consumer` tests at `CMakePresets.json:164`; CI applies the same split at `.github/workflows/ci.yml:128`. Non-instrumented static/shared matrices retain all six package/consumer tests. The policy is documented at `README.md:102` and `docs/modules/core.md:396`. |
| PORT-M1-002 | high portability risk | Whole-class Windows DLL export on configuration types containing `std::variant`, `std::string`, `std::vector`, `std::map`, and `std::optional` risked MSVC `/W4 /WX` C4251 diagnostics and over-exported implementation details. | `include/asc/core/configuration.h:36` now selectively exports the out-of-line constructors, special members, accessors, schema mutators, configuration queries, and free functions. The STL-owning classes themselves are not marked for whole-class export. |
| PORT-M1-003 | high portability risk | Exported namespace functions declared without the same DLL annotation in their friend declarations risked inconsistent linkage diagnostics on MSVC. | Friend and namespace declarations now carry identical `ASC_CORE_EXPORT` annotations at `include/asc/core/configuration.h:223`, `include/asc/core/configuration.h:238`, `include/asc/core/execution.h:74`, and `include/asc/core/execution.h:83`. |
| PORT-M1-004 | medium correctness risk | `ExecutionContext::Create` accepted out-of-range `Backend` and `Determinism` enum values, allowing invalid provider vocabulary into an immutable context. | Exhaustive validation now rejects unknown values with `kInvalidArgument` at `src/core/execution.cc:31`; regressions are at `tests/core/memory_execution_test.cc:284`. |
| PORT-M1-005 | medium build risk | Registering `-fno-exceptions` standard-library compile tests on MSVC would either pass an unsupported GNU option or encounter unsupported exception-disabled library diagnostics under warnings-as-errors. A plain Clang compiler-ID check would also catch clang-cl. | Exception-disabled targets are registered only for GNU and GNU-style Clang/AppleClang frontends, explicitly excluding the MSVC frontend variant, at `tests/compile/CMakeLists.txt:31`; normal header and consumer tests remain enabled on MSVC and clang-cl. |
| PORT-M1-006 | high compiler/UB risk | The one-byte little-endian encoder shifted an integer-promoted 8-bit value by eight, which Clang 19 rejected as a shift count equal to the promoted type width. | `include/asc/core/io.h:117` compiles the shift only when `sizeof(T) > sizeof(std::uint8_t)`; signed and unsigned 8-bit round trips remain in `tests/core/io_test.cc:216`. |
| PORT-M1-007 | low style/build risk | An intermediate configuration header snapshot did not pass the repository clang-format policy. | The complete Milestone 1 C++ write set now passes `clang-format-19 --dry-run --Werror`. |
| PORT-M1-008 | low documentation defect | `SECURITY.md` still described the repository as Milestone 0, version 0.0.0, with no public target. | `SECURITY.md:5` now identifies the unreleased 0.1.0 provider-free `ASC::core` candidate and accurately states that no released support window exists. |
| PORT-M1-009 | medium contract-documentation defect | The production self-review incorrectly grouped `ReadExact` and `WriteAll` with transactional destination publication. Both operations can expose successful prefix progress before a later failure. | `docs/development/asc-cpp-m1-core/production-self-review.md:73` now records partial read mutation and non-rollback of accepted write prefixes, matching `docs/modules/core.md:259`. |

The selective export design is accepted for this pre-1.0 C++ API. Exported
out-of-line symbols were present in the Clang shared object, and the isolated
shared consumer called representative configuration, status, execution, I/O,
and memory symbols successfully. Hosted MSVC remains the decisive Windows
compile/link evidence.

## Independent local evidence

The review used CMake/CTest 4.1.2, GCC 11.4.0, Clang 19.0.0, and
clang-format 19.0.0 on Linux x86_64 under WSL2. Released ASCCMake was the clean
`v0.1.0` checkout at
`8a7dcbad3a97267cce59810aff24de800a3497a7`.

### Clang shared, package, and relocation matrix

```text
cmake -S . -B /tmp/asc-cpp-m1-clang-shared.9Johbb \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++-19 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON
cmake --build /tmp/asc-cpp-m1-clang-shared.9Johbb --parallel 4
ctest --test-dir /tmp/asc-cpp-m1-clang-shared.9Johbb \
  --output-on-failure --parallel 4
```

Result: configure passed, the shared library and all targets built with
warnings-as-errors, and **44/44 tests passed**. This includes all 11 public
headers in normal and exception-disabled translation units, multi-TU linkage,
runtime and death tests, build-tree and subproject consumers, installation,
relocation, paths containing spaces, component behavior, and package-registry
non-mutation.

`nm -D --defined-only -C` confirmed representative public definitions for the
selectively exported configuration API, `Status::Ok`, `ExecutionContext`,
`CopyBytes`, `File`, `HostMemoryResource`, and `Buffer`. `ldd` showed only the
platform C/C++ runtime closure and no optional provider library.

### GCC ASan/UBSan matrix

```text
cmake -S . -B /tmp/asc-cpp-m1-san-final.NRvIg2 \
  -DCMAKE_PREFIX_PATH=/home/yicai/AI4SciComp/asc-cmake/build/prefix \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_SHARED_LIBS=OFF \
  -DASC_CPP_BUILD_TESTING=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_INSTALL=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON
cmake --build /tmp/asc-cpp-m1-san-final.NRvIg2 --parallel 4
ctest --test-dir /tmp/asc-cpp-m1-san-final.NRvIg2 \
  --output-on-failure --parallel 4 -LE 'package|consumer'
```

Result: configure and build passed; **38/38 selected instrumented tests passed**
with no ASan or UBSan finding. Exactly six package/consumer tests were excluded
and are covered by the non-instrumented package matrices; they are not
misreported as sanitizer runtime evidence.

### Static analysis, formatting, and source closure

```text
mapfile -t files < <(
  rg --files include/asc/core src/core tests/core tests/compile \
    tests/consumer/core tests/consumer/subproject |
  rg '\.(h|cc)$'
)
clang-format-19 --dry-run --Werror "${files[@]}"
git diff --check
rg -n -i \
  'mdecpp|gpl|sobol|lebedev|curand|cublas|cusolver|cusparse|cuda_runtime|hip/|sycl|mkl' \
  include/asc/core src/core tests/core tests/compile tests/consumer
```

Results: formatting passed and whitespace validation passed. The scan found
only approved provider-neutral SYCL vocabulary/tests and the dependency-audit
regex; it found no MdeCpp/GPL/corpus or provider-SDK include. Production
includes close over the C++20 standard library and `asc/core` only. No local
`clang-tidy` executable was available; the Clang 18 CI job owns that pending
evidence.

## Portability and safety disposition

- `asc_core` / `ASC::core` is the only production target and has no direct ASC,
  external link, or exported transitive dependency. ASCCMake 0.1.0 is an exact
  configure-time dependency only.
- C++ extensions are disabled through the released ASCCMake C++20 helper.
  Public headers are self-contained `.h` files and implementation is in six
  compiled `.cc` files in flat namespace `asc`.
- Linux static and shared builds, hidden visibility, installed consumers,
  relocation, subproject use, and paths containing spaces are runtime-tested.
  Windows uses wide native file paths; POSIX uses native byte paths.
- Allocation alignment is checked, allocation failures do not publish owners,
  custom-resource misalignment is released exactly once, and host aligned
  new/delete are paired. Buffers and events are move-only; views and resource
  pointers retain the documented non-owning lifetime contract.
- Exact I/O and local-file behavior disclose partial progress, non-atomic file
  replacement, and unobservable destructor-time close errors. Recoverable
  failures use status values; release-active programmer-contract failures enter
  a non-allocating fatal path.
- Checked casts/arithmetic and extent products reject overflow before evaluation
  or owner publication. Little-endian conversion is fixed-width and exercised
  for 1/2/4/8-byte integers and IEC 60559 float/double under both GCC and Clang.
- There is no mutable process-global context, provider registry, failure
  handler, resource, or diagnostic sink. Separate objects may be used
  concurrently; callers must serialize mutation and overlapping byte access.
- Sensitive configuration values are redacted in rendering. Package tests
  verify no CMake user package-registry write. CI has read-only repository
  permissions, pinned checkout actions, and does not persist checkout
  credentials.

No portability, undefined-behavior, sanitizer, ownership, concurrency, or
security blocker remains in the locally exercised scope.

## CPU, GPU, and performance evidence

| Surface | Evidence | Disposition |
| --- | --- | --- |
| serial CPU core | configure-tested, compile-tested, runtime-tested | Available: host allocation, synchronous serial context/event, and overlap-safe host byte copy. |
| optimized CPU providers | skipped | OpenMP, BLAS/LAPACK, MKL, TBB, Eigen, and SYCL inventory is not an implemented dependency or capability. |
| CUDA, HIP/ROCm, SYCL, or any other GPU provider | **skipped** | Milestone 1 contains no provider discovery, provider target, SDK include/link edge, provider allocation, kernel, runtime path, or parity oracle. Stage A toolkit/hardware inventory is not ASCCpp evidence. |

No numerical kernel or benchmark exists in this milestone, so wall-clock,
throughput, scaling, and CPU/GPU parity measurements are not applicable.
Performance evidence is limited to contract-level costs: checked metadata is
allocation-free, configuration validation is tree-linear plus ordered-map
costs, exact I/O and byte copy are byte-linear, nonzero host buffer creation
performs one resource allocation, and `CopyBytes` allocates, packs, transfers,
falls back, and implicitly synchronizes nothing.

## Dependency, license, and provenance

The installed license is Apache-2.0 and is byte-identical to the released
ASCCMake Apache-2.0 license. No third-party production or test dependency was
added.

MdeCpp was inspected only as the separate GPLv3 comparison repository at
`/home/yicai/repo/MdeRepo/MdeCpp`, tracked commit
`f6294e9079262682ce63ae7ff2d8a643e658bf5d`. Its unrelated modified `Makefile`
was preserved. No MdeCpp source, test, literal corpus, generated data, or
mechanical translation appears in the Milestone 1 production or verification
surface; no `THIRD_PARTY_NOTICES` entry is required for an accepted import
because there is no accepted import.

## Remaining evidence boundaries and risks

- Hosted Clang 18 with clang-tidy, MSVC 2022 Debug/Release shared-library
  compile/link/package tests, and AppleClang on macOS arm64 remain pending until
  the branch is published and CI runs. They are not claimed as local evidence.
- The selective DLL annotations are structurally consistent and pass GNU/Clang
  shared linkage, but only hosted MSVC can close the Windows C4251/C4273 and
  import-library risk.
- The CI jobs require the repository secret `ASC_CMAKE_READ_TOKEN` to read the
  private exact asc-cmake commit. Missing or insufficient secret access is an
  external publication blocker, not a local source workaround.
- A Windows static package is not a distinct hosted job. The public
  `ASC_CORE_STATIC_DEFINE` propagation is structurally configured and Linux
  static consumers pass, but Windows-specific static consumption remains an
  evidence gap if that configuration is required for the first release.
- Non-owning views and buffer resource pointers remain caller-lifetime risks by
  design. File-destructor close errors remain unobservable by design; callers
  needing durability must explicitly `Flush()` and `Close()`.
- GPU evidence is **skipped**, and no performance or parity claim may be made.

Subject to the explicitly pending hosted evidence above, this independent role
finds no unresolved Milestone 1 blocker and accepts the integrated candidate for
Publication Checkpoint B. This is not approval to push, merge, tag, release, or
delete a branch.
