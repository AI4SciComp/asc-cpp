# Milestone 6 Portability, GPU, and Performance Review

Status: Review complete, with explicit skips and no open production defect

Date: 2026-07-27

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Role: independent portability/GPU/performance reviewer

Writable scope:

```text
docs/development/asc-cpp-m6-gpu-core-dense/portability-review.md
```

## Review boundary

This role read the complete frozen Milestone 6 contract and ownership ledger,
preflight and dependency audit, provenance boundary, relevant accepted ADRs
0001--0004, 0007--0011, 0013, 0017, and 0018, the architecture blueprint,
testing strategy, release roadmap, dependency and capability manifests,
backend matrix, current core/dense CUDA public headers and implementation,
root/component/package CMake, tests, isolated consumers, benchmark, module
documentation, production self-review, independent verification artifacts,
and independent documentation/API review.

The first conclusions were formed independently before the documentation/API
review was read. Findings were then compared and reconciled without weakening
the frozen contract.

No MdeCpp implementation or test, user-deleted asc-cpp implementation or test,
third-party sample, or provider SDK example was inspected or copied. This role
did not edit production, tests, CMake/package files, manifests, module
documentation, or another report.

## Environment inspected

```text
repository:         /home/yicai/AI4SciComp/asc-cpp
unchanged HEAD:     33b261ea33616a6395c4ad3b20646093103344f7
host:               Linux 6.18.33.2-microsoft-standard-WSL2 x86_64
CPU:                Intel Core i7-11800H, 8 cores / 16 logical CPUs
CMake:              3.25.0 and 4.1.2
GCC:                11.4.0
Clang:              19.0.0
clang-format:       19.0.0
Ninja:              unavailable
CUDA compiler:      nvcc 12.9.86
CUDA toolkit:       12.9
Compute Sanitizer:  2025.2.1
GPU:                NVIDIA GeForce RTX 3060 Laptop GPU
compute capability: 8.6
GPU memory:         6144 MiB
driver:             576.83
```

Native MSVC, AppleClang, macOS, Windows CUDA, a second CUDA device, HIP, and
SYCL GPU execution were unavailable. Their absence is reported as a skip, not
converted into evidence.

## Findings and resolutions

### M6-PORT-01: pointwise launch geometry truncated large logical ranges

Initial severity: release-blocking correctness and signed-overflow defect

Initial evidence: the original CUDA launch path formed
`logical_size + block_size - 1` in signed `extent_t`, narrowed an unbounded
block count to `unsigned int`, and gave each thread only one ordinal. A
representable large logical range could therefore overflow during block
calculation or silently leave the tail unprocessed after grid truncation.

Resolution:

- quotient/remainder block calculation avoids rounded-add overflow;
- the grid is capped at a deliberate finite block count;
- both project-owned kernels use grid-stride traversal; and
- loop termination compares the remaining distance before increment, so
  `ordinal + ordinal_stride` cannot overflow near `INT64_MAX`.

The source-level proof now covers the representable signed boundary. The
exact-self large-metadata test remains metadata evidence only, while a focused
host-testable regression now exercises the launch-block calculation and
grid-stride ordinal advance at the former one-ordinal capacity and at
`INT64_MAX`.

Disposition: resolved.

### M6-PORT-02: cuBLAS handle destruction selected no owning device

Initial severity: release-blocking multi-device lifetime defect

Initial evidence: `DenseCudaContextState` destroyed its cuBLAS handle on
whichever CUDA device happened to be current. NVIDIA documents a cuBLAS handle
as associated with the current device at creation and requires that device to
be current for later handle calls.

Resolution: dense provider state retains its device ordinal, establishes a
restoring device guard around handle destruction, and declines cleanup if the
owning device cannot be selected. The same behavior covers destruction caused
by move assignment.

The one-device host can prove ordinary current-device stability but cannot
runtime-prove restoration across two devices. The two-device branch therefore
remains skipped on this host.

Disposition: resolved; multi-device runtime evidence skipped.

### M6-PORT-03: Windows provider redeclaration added `dllimport`

Initial severity: release-blocking Windows public-header portability defect

Initial evidence: provider factory functions were first declared as
unannotated friends in `execution.h`, then redeclared with
`ASC_CORE_CUDA_EXPORT` in the provider header. A direct Clang MSVC-target
language probe confirmed that adding `dllimport` on the later redeclaration is
ill-formed.

Resolution: core provider construction uses the existing internal access
classes; provider functions are no longer first declared as unannotated
friends. Provider headers are now their first declarations. Linux-hosted
`_WIN32`/`__declspec` syntax review finds no remaining conflicting
redeclaration.

A native MSVC shared-library and consumer build is still required before
Windows DLL support is claimed.

Disposition: source defect resolved; native Windows compile/runtime skipped.

### M6-PORT-04: sticky CUDA launch errors could lose accepted work

Initial severity: release-blocking asynchronous-lifetime defect

Initial evidence: a kernel launch used `cudaPeekAtLastError` without isolating
prior thread-local Runtime error state. CUDA documents that Runtime error
queries may surface errors from previous asynchronous launches. A newly
accepted kernel could consequently be reported as a failed submission and
return without a completion event while still using caller storage.

Resolution:

- a resetting query consumes already reported thread-local state before
  submission;
- a resetting post-launch query checks the new launch;
- any failure after possible acceptance drains only the affected stream before
  returning without an event; and
- a sequential injected-error test proves a later valid operation is not
  poisoned by the reported earlier error.

Disposition: resolved.

### M6-PORT-05: asynchronous copy and event failure paths could lose work

Initial severity: release-blocking asynchronous-lifetime defect

Initial evidence: `cudaMemcpyAsync` and event publication had paths that could
return a failed `Result` after work was accepted, without a returned completion
handle or a safety-establishing wait.

Resolution:

- completion state is created before submission;
- pre/post Runtime error state is isolated with resetting queries;
- copy, event-record, and pre-record asynchronous failure paths perform a
  failure-only stream-scoped drain when accepted work is possible; and
- the original provider/native diagnostic is retained.

Successful copy/event paths remain stream ordered and do not device-wide
synchronize.

Disposition: resolved.

### M6-PORT-06: cuBLAS call failure could leave live work untracked

Initial severity: release-blocking asynchronous-lifetime defect

Initial evidence: a non-success return from typed cuBLAS GEMV/GEMM immediately
discarded the precreated pending event and returned a status. A provider call
may perform multiple internal submissions, so a failure return is not a proof
that no work was accepted.

Resolution: any non-success cuBLAS result drains the context stream before
returning without the event. The raw provider result remains the reported
diagnostic. Failure cleanup is stream scoped; successful calls still publish
an event.

Diagnostic construction is ordered after the drain, so allocation failure
while constructing a message cannot bypass lifetime safety.

Disposition: resolved.

### M6-PORT-07: dense algebra dropped declared placement and uniqueness

Initial severity: release-blocking contract and data-race defect

Initial evidence: public CUDA algebra templates converted views into an opaque
plan containing pointer, shape, and strides but no declared `MemorySpace` or
mapping uniqueness. Provider validation then hardcoded `kDevice`. A device
pointer mislabeled as host/managed could be accepted, and a const repeated-
address mapping could enter a project kernel outside the approved unique-view
surface.

Resolution: every algebra wrapper validates that every view is declared
exactly `MemorySpace::kDevice` and has a proven-unique mapping before building
the opaque plan. Provider pointer attributes then independently verify that
the pointer belongs to device memory on the explicit context device.

Negative tests cover all Copy/Scal/Axpy/Gemv/Gemm operand positions, host and
managed mislabels over real device allocations, repeated-address vector and
matrix mappings, and unchanged storage after rejection.

Disposition: resolved.

### M6-PORT-08: CUDA AXPY rejected its approved exact in-place form

Initial severity: release-blocking CPU/GPU contract divergence

Initial evidence: the serial dense contract admits exact same-index AXPY, and
the frozen verification design requires allowed exact in-place forms. CUDA
validation rejected every source/destination overlap, including identical
data, shape, and strides.

Resolution: CUDA validation admits only exact same-index AXPY after placement,
shape, uniqueness, span, pointer, and device validation. Partial and
conservative overlap remains rejected. Exact in-place AXPY executes the kernel
and is not treated as a no-op. Float/double parity and unchanged-destination
rejection tests cover the distinction.

Disposition: resolved.

### M6-PORT-09: pageable-host labels accepted CUDA-tracked allocations

Initial severity: release-blocking explicit-memory contract defect

Initial evidence: core CUDA validation returned success immediately for every
declared `MemorySpace::kHost`. A device, managed, or pinned allocation
mislabeled as pageable host could therefore be silently redispatched by
`cudaMemcpyDefault`.

Resolution: CUDA pointer attributes are inspected for every nonempty declared
space. Pageable host accepts only unregistered memory, including the CUDA-
version-specific `cudaErrorInvalidValue` classification after consuming that
expected Runtime error. CUDA-tracked host, device, and managed allocations
mislabeled as pageable host are rejected.

Regression tests cover pinned, device, and managed allocations mislabeled as
pageable host in both source and destination positions and verify unchanged
storage after rejection.

Disposition: resolved.

### M6-PORT-10: CUDA-compiled shared symbols lacked a visibility policy

Initial severity: shared-library symbol-hygiene portability defect

Initial evidence: the mixed C++/CUDA dense provider set only
`CXX_VISIBILITY_PRESET hidden`. CMake visibility presets are language
specific, so host symbols emitted from `kernels.cu` were not covered by the
C++ property.

Resolution: `asc_dense_cuda` now also sets `CUDA_VISIBILITY_PRESET hidden`.
The intended provider ABI remains explicitly exported from the C++ provider
header.

Shared `asc_core_cuda` and `asc_dense_cuda` targets subsequently built and ran
focused core/linalg tests. Dynamic-symbol inspection found the declared public
surface and the SDK-neutral internal bridge needed between the dense provider
and core provider, but no project kernel entry point emitted from `kernels.cu`.

Disposition: resolved.

### M6-PORT-11: CUDA cleanup continued after device-guard failure

Initial severity: explicit-device failure-path resource risk

Initial evidence: core CUDA stream/event destructors and memory deallocation
created an owning-device guard but ignored a failed result and continued with
destroy/free on a possibly different current device.

Resolution: cleanup consumes already reported thread-local Runtime state,
establishes the owning-device guard, and stops if device selection fails. In
an unrecoverable provider failure this can leak the provider resource, which
is safer than acting on an unselected device. Normal successful cleanup still
restores the caller's current device.

Disposition: resolved.

### M6-PORT-12: shared dense provider embedded a second CUDA Runtime

Initial severity: release-blocking shared-library state, packaging, and
redistribution defect

Initial evidence: a fresh `BUILD_SHARED_LIBS=ON` build left CMake's CUDA
runtime selection at the NVCC default. The `asc_dense_cuda` link line therefore
contained `-lcudart_static`, while `asc_core_cuda` and the test executable used
the approved `CUDA::cudart` shared library. `nm` found locally defined
`cudaGetLastError`, `cudaSetDevice`, and launch entry points inside
`libasc_dense_cuda.so`; `readelf` had no `NEEDED` entry for `libcudart.so.12`.

This was observable behavior, not symbol hygiene alone. Both fresh Debug and
Release shared builds failed `asc_cpp.dense_cuda.concurrency` reproducibly:
after its intentional `cudaSetDevice(-1)` failure, the valid
`CudaEvaluate` fill failed. Dense cleared one embedded Runtime's thread-local
error state, while core validation observed the other Runtime's still-pending
error. The same test passed in the static producer configuration.

The embedded Runtime also contradicted the frozen boundary that CUDA remains a
separately supplied provider dependency and that ASCCpp does not redistribute
a CUDA binary.

Resolution:

- the root CUDA-enabled configuration selects standard
  `CMAKE_CUDA_RUNTIME_LIBRARY=Shared` immediately after enabling CUDA, so all
  producer targets and tests use one CUDA Runtime instance;
- `asc_dense_cuda` independently fixes its target-local
  `CUDA_RUNTIME_LIBRARY` property to `Shared`;
- the CMake dependency audit asserts that target property; and
- both static and shared final link lines were checked to contain `-lcudart`
  and no `-lcudart_static`.

Fresh final shared Release provider tests passed 7/7, including concurrency and
the benchmark; shared Debug concurrency passed 1/1; static Release provider
runtime passed 4/4. Static and shared build-tree and installed/relocated CUDA
consumers passed. `readelf`, `ldd`, and `nm` confirmed that shared provider
libraries and final static consumer executables depend on
`libcudart.so.12` and contain no local CUDA Runtime API definitions.

Disposition: resolved.

## C++20, headers, ABI, and provider isolation

The public provider surface uses portable C++20 concepts, fixed-width integer
metadata, `std::array`, move-only RAII, and explicit `Status`/`Result`. It uses
no C++23 facility or public compiler extension. Public names remain in flat
`namespace asc`; implementation helpers are in `internal_*` namespaces.

All reviewed public headers are self-contained `.h` files with full-path
guards and direct standard/project includes. Provider-neutral headers compile
without a CUDA SDK include path. Neither common core/dense headers nor the
provider public headers expose `cuda*`, `CU*`, or `cublas*` SDK types.

The target graph is exactly:

```text
ASC::core_cuda  -> ASC::core
                  private CUDA::cudart

ASC::dense_cuda -> ASC::dense;ASC::core_cuda
                  private CUDA::cublas
```

`ASC::cpp` remains provider free. CUDA-disabled configuration does not enable
the CUDA language, discover CUDAToolkit, define provider targets, or advertise
provider components. A CUDA-enabled installation discovers CUDAToolkit only
when the requested installed component closure contains `core_cuda` or
`dense_cuda`.

Static builds propagate provider `*_STATIC_DEFINE` definitions. Shared builds
use separate core/dense/provider import/export macros and hidden C++/CUDA
visibility. The public dense provider header necessarily contains bounded
`internal_dense_cuda` plan types and exported opaque bridge declarations so
consumer-instantiated templates can cross the compiled boundary. No SDK type
leaks, but these internal layouts remain an installed ABI coupling to monitor.

Every CUDA-enabled producer target selects the shared CUDA Runtime, matching
the explicit `CUDA::cudart` dependency owned by `core_cuda`. Shared
`libasc_dense_cuda.so`, static build-tree consumers, and relocated static
consumers all resolve CUDA Runtime calls through `libcudart.so.12`; none
contains an embedded Runtime implementation.

Strict provider-header probes passed:

```bash
g++-11 -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions \
  -Iinclude -fsyntax-only tests/compile/m6_provider_contract.cc

clang++-19 -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions \
  -Iinclude -fsyntax-only tests/compile/m6_provider_contract.cc
```

The Linux-hosted `__declspec` probe is syntax assistance only. It is not a
native MSVC compile.

## Memory, execution, and error audit

CUDA resources expose only pinned-host, device, and managed ownership for one
explicit device. Allocation and deallocation use matching CUDA families,
zero-byte allocation returns null, alignment is checked, and a failed
allocation publishes no owner. Ordinary `cudaFree` and `cublasDestroy` can
synchronize; they are teardown costs, not hidden successful-operation costs.

Copy validation checks view size, nullability, representable address ends,
declared space, CUDA pointer type, device ordinal, and overlap before enqueue.
Exact same-pointer/same-space copy publishes a stream-ordered no-op event after
provider validation. Partial CUDA overlap is rejected. Pageable-host Runtime
submission may stage or block the host; pinned storage is required when
strong host-asynchronous behavior matters.

An execution context is a copyable immutable value sharing one nonblocking
stream state. An event is move-only, retains execution/completion state, and
does not retain user views or storage. Query is nonwaiting; Wait is event
local. Event destruction does not synchronize and does not make early storage
destruction safe.

Every provider call selects the explicit device and restores the caller's
previous current device on the normal path. Handled `cudaErrorNotReady` and
failure-drain Runtime errors are consumed before restoration so they do not
poison the guard. A completion event exists before work submission whenever
possible. If work may have been accepted but an event cannot be returned, the
implementation drains only the affected stream. No successful operation calls
`cudaDeviceSynchronize`.

## Dense expression, storage, and algebra audit

`CreateUninitialized` owns host, pinned, device, or managed storage but does
not initialize elements. `At` remains host-only and does not silently migrate
managed memory. `Clone` is the one named synchronous deep-copy operation: it
allocates, submits, waits, and publishes the destination only after success.

The pointwise provider accepts exactly float/double, rank zero through eight,
unique non-negative-stride device views, and the frozen one-level terminal,
scalar, negate, add, subtract, and multiply expression forms. Unsupported
nested/adapted expressions return explicit status and never fall back to the
serial evaluator. Logical traversal is dimension-zero-fastest and preserves
padded holes.

Project kernels implement Copy, Scal, and AXPY for rank one/two views without
packing, transfer, allocation, workspace, fallback, or hidden synchronization.
Typed cuBLAS GEMV/GEMM accept only float/double column-major-compatible
matrices, positive vector increments, checked dimensions/leading dimensions,
and none/transpose operations. Every provider-width conversion occurs before
pointer attributes or enqueue. `beta == 0` never requires the prior output;
zero inner dimension uses the project scale-or-zero kernel.

cuBLAS state is one handle per dense context and stream. Host pointer mode
makes stack `alpha`/`beta` valid for the synchronous provider call. Atomics are
disabled. Deterministic contexts use pedantic math; backend-default contexts
use default math. One handle is not concurrently submitted from multiple
threads. Independent context/handle/stream pairs may execute concurrently when
mutable storage does not overlap.

NVIDIA documents bitwise reproducibility only within bounded toolkit/platform
conditions; no cross-toolkit or cross-GPU reproducibility claim is made.

## Sanitizer and concurrency assessment

ASCCMake 0.1.0 applies its warning and sanitizer flags only to C++ compile
language. Consequently:

- C++ provider/control code can be covered by host ASan/UBSan;
- CUDA device code in `.cu` is not host-sanitizer instrumented;
- CUDA memory, initialization, race, and synchronization evidence must come
  from Compute Sanitizer; and
- CUDA ThreadSanitizer evidence is not inferred from a host TSan build.

The concurrency test uses two CPU threads, each with its own execution context,
dense context, stream, resource, and storage. This is functional independent-
stream evidence. It deliberately does not submit the same mutable cuBLAS handle
concurrently. It also does not prove physical overlap timing or absence of a
device-wide wait by timing alone; source audit and synchronization-tool
evidence carry those claims.

Standalone UBSan passed all four focused core/dense provider executables.
Compute Sanitizer memcheck, initcheck, and synccheck reported zero errors, and
racecheck reported zero errors and zero warnings. ASan configuration and
compilation with asc-cmake's actual `ADDRESS UNDEFINED` sanitizer arguments
succeeded, but CUDA runtime execution is **skipped**: the first `cuInit` under
WSL reports a double-free inside the Microsoft WSL NVIDIA driver libraries,
before ASC test work begins.

CUDA TSan is **skipped**. ASCCMake does not instrument `.cu` device code with
TSan, and CUDA Runtime execution under host TSan is not treated as valid
device-race evidence. Compute Sanitizer racecheck is the applicable Milestone
6 GPU-race result.

## Performance evidence assessment

The project-owned benchmark covers float/double pointwise add, AXPY, GEMV,
GEMM, and separately reported host/device transfers. It uses explicit warmups,
keeps operations on one stream, waits for the last event, computes an observed
checksum, and carries no speed threshold.

Its durations are end-to-end host observations of event construction,
submission, queued GPU execution, and final completion. They are not isolated
kernel timestamps. One aggregate duration per case provides smoke evidence,
not a distribution or stable baseline. The benchmark therefore correctly
prints:

```text
benchmark_classification=smoke-only
performance_claim=none
environment_metadata=reported-by-validation-harness
```

The final checkpoint must pair observations with the exact device, driver,
toolkit/cuBLAS, architecture, compiler, configuration, and flags. No CPU/GPU
speedup, regression threshold, or cross-machine comparison is approved.

One independent Release-like smoke run on the environment recorded above
observed:

| Scalar | H2D (us) | Add (us) | Axpy (us) | Gemv (us) | Gemm (us) | D2H (us) | Checksum |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| float | 4714 | 20046 | 4181 | 2103 | 811 | 1027 | 349.632 |
| double | 3771 | 26949 | 7944 | 1459 | 14664 | 2029 | 349.632 |

The vector size was 1,048,576 with 80 repetitions, GEMV was 512 by 512 with
80 repetitions, GEMM was 256 by 256 with 40 repetitions, and each case used
three warmups. These single observations establish executable smoke coverage
only. They are not a comparative or release performance claim.

## Evidence classification

Only the following exact labels are used:

| Facet/evidence | Classification | Scope |
| --- | --- | --- |
| CUDA language/toolkit/targets/architecture | **configure-tested** | CMake 4.1.2, CUDA 12.9.86, architecture 86 |
| static core/dense provider targets and consumers | **compile-tested** | GCC 11.4/NVCC 12.9.86, C++20/CUDA 20 |
| shared core/dense provider targets | **compile-tested** | GCC 11.4/NVCC 12.9.86, hidden C++/CUDA visibility |
| GCC/Clang SDK-free provider public headers | **compile-tested** | GCC 11.4 and Clang 19, no CUDA include path |
| core CUDA resources/copies/events | **runtime-tested** | device 0 on the identified RTX 3060 |
| shared core/dense provider runtime | **runtime-tested** | all focused shared-library runtime/concurrency tests, device 0 |
| dense CUDA numerical comparison | **parity-tested** | float/double evaluator and Copy/Scal/Axpy/Gemv/Gemm |
| multi-device behavior | **skipped** | one CUDA device detected |
| CUDA execution under ASan | **skipped** | WSL NVIDIA driver fails in `cuInit` before ASC test work |
| CUDA execution under TSan | **skipped** | host TSan is not CUDA device-race evidence |
| native Windows/MSVC CUDA | **skipped** | toolchain/host unavailable |
| AppleClang/macOS CUDA | **skipped** | toolchain/host unavailable |
| hosted GPU CI | **skipped** | no hosted GPU runner configured |
| sparse/random CUDA and cuSOLVER | **skipped** | outside Milestone 6 |

## Commands and outcomes

The independent clean CUDA configuration and warnings-as-errors build was:

```sh
cmake -S . -B /tmp/asc-cpp-m6-verifier.vmqPjT/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-verifier.vmqPjT/build --parallel 4
```

Result: PASS. The focused provider selection was:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier.vmqPjT/build \
  -R '^asc_cpp\.(core_cuda|dense_cuda)\.' --output-on-failure
```

Result: PASS, 13/13. The initial full Milestone 6 selection passed 48/49;
the only failure was the stale architecture text `selected
cuBLAS/cuSOLVER`. The lead corrected that oracle to the approved M6
capability `selected cuBLAS`, after which:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier.vmqPjT/build \
  -R '^asc_cpp\.architecture\.dependency_manifest$' --output-on-failure
```

passed 1/1. Both long package tests had already passed in the original
selection: build-tree components in 239.04 seconds and install/relocate
components in 232.20 seconds. Provider build-tree and installed/relocated
consumers, provider-free consumers against a CUDA-enabled install, component
closure, registry, and requested-unavailable selection all passed.

The shared-provider check was:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release \
  --target asc_core_cuda_test asc_dense_cuda_storage_evaluate_test \
           asc_dense_cuda_linalg_test asc_dense_cuda_concurrency_test \
           asc_m6_provider_contract asc_m6_provider_multi_tu \
           asc_dense_cuda_benchmark --parallel 4
ctest \
  --test-dir /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency|compile_contract|multi_tu|benchmark))$' \
  --output-on-failure
readelf -d \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release/src/dense/libasc_dense_cuda.so
ldd \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release/src/dense/libasc_dense_cuda.so
nm \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release/src/dense/libasc_dense_cuda.so
```

Result: configure/build PASS; focused runtime/compile/benchmark PASS, 7/7.
`readelf` reported `NEEDED` entries for `libasc_core_cuda.so`,
`libcublas.so.12`, and `libcudart.so.12`; `ldd` resolved the separately
supplied shared Runtime; `nm` found no local CUDA Runtime API definition.
Dynamic-symbol inspection also found no project kernel entry point exported
from `kernels.cu`.

The defect was reproduced before the correction by the same fresh shared
Release configuration: five of six selected tests passed, while
`asc_cpp.dense_cuda.concurrency` failed. Three direct reruns and an independent
shared Debug build reproduced the same valid-fill failure. The Debug and
Release commands above passed after the shared-Runtime correction; Debug
concurrency passed 1/1.

The final static provider and consumer checks were:

```sh
cmake -S . \
  -B /tmp/asc-cpp-m6-portability-postfix.eUoqGF/static-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/static-release --parallel 4
ctest \
  --test-dir /tmp/asc-cpp-m6-portability-postfix.eUoqGF/static-release \
  -R '^asc_cpp\.(core_cuda\.runtime|dense_cuda\.(storage_evaluate|linalg|concurrency))$' \
  --output-on-failure
ctest \
  --test-dir /tmp/asc-cpp-m6-portability-postfix.eUoqGF/static-release \
  -R '^asc_cpp\.consumer\.(core_cuda|dense_cuda)\.(build_tree|install_relocate)$' \
  --output-on-failure
```

Result: configure/full build PASS; focused runtime PASS, 4/4; static
build-tree and installed/relocated provider consumers PASS, 4/4. Producer,
build-tree-consumer, and relocated-consumer link lines contained shared
`cudart` and no `cudart_static`; `ldd` and `nm` confirmed one shared Runtime
and no local CUDA Runtime definitions.

The corresponding shared build-tree provider consumers passed 2/2. The first
shared installed-consumer invocation failed 2/2 because only focused targets
had been built and the full install step could not find the unbuilt
`libasc_utilities.so`. After the required full producer build, the exact
installed/relocated consumer selection passed 2/2. This was a validation
precondition failure, not a package defect.

An independent CPU-only isolation check was:

```sh
cmake -S . -B /tmp/asc-cpp-m6-portability.fm3ehg/cpu \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_ENABLE_CUDA=OFF \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-portability.fm3ehg/cpu --parallel 4
ctest --test-dir /tmp/asc-cpp-m6-portability.fm3ehg/cpu \
  -R '^asc_cpp\.(architecture\.(dependency_manifest|approved_product_targets)|core\.|dense\.)' \
  --output-on-failure
```

Result: configure/build PASS and focused provider-free regression PASS, 22/22.
CMake reported `CMAKE_DISABLE_FIND_PACKAGE_CUDAToolkit` as unused, which is
the expected evidence that the CUDA-disabled path never attempted
`find_package(CUDAToolkit)`.

Standalone UBSan used the actual asc-cmake argument surface:

```sh
cmake -S . -B /tmp/asc-cpp-m6-ubsan.DETbqJ/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  '-DASC_CPP_SANITIZER_ARGUMENTS=UNDEFINED' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-ubsan.DETbqJ/build \
  --target asc_core_cuda_test asc_dense_cuda_storage_evaluate_test \
           asc_dense_cuda_linalg_test asc_dense_cuda_concurrency_test \
  --parallel 4
UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1' <each-focused-executable>
```

Result: PASS, 4/4. The corresponding `ADDRESS UNDEFINED` configuration and
compilation passed; runtime is the ASan skip described above.

Compute Sanitizer ran the core/dense focused executables as follows:

```sh
compute-sanitizer --tool memcheck --report-api-errors no \
  --error-exitcode 99 <focused-executable>
compute-sanitizer --tool racecheck --report-api-errors no \
  --error-exitcode 99 <dense-concurrency-executable>
compute-sanitizer --tool initcheck --report-api-errors no \
  --error-exitcode 99 <dense-runtime-executable>
compute-sanitizer --tool synccheck --report-api-errors no \
  --error-exitcode 99 <dense-runtime-executable>
```

Results: memcheck zero errors for all four focused executables; racecheck zero
errors and zero warnings for concurrency; initcheck and synccheck zero errors
for all three dense runtime executables. `--report-api-errors no` suppresses
expected API-error reports from intentional negative tests, not device
memory/race/initialization/synchronization findings.

The final corrected shared Release concurrency executable was rerun with:

```sh
compute-sanitizer --tool memcheck --report-api-errors no \
  --error-exitcode 99 \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release/tests/dense_cuda/asc_dense_cuda_concurrency_test
compute-sanitizer --tool racecheck --report-api-errors no \
  --error-exitcode 99 \
  /tmp/asc-cpp-m6-portability-postfix.eUoqGF/shared-release/tests/dense_cuda/asc_dense_cuda_concurrency_test
```

Memcheck reported zero errors; racecheck reported zero errors, zero warnings,
and zero displayed hazards. The test's one-device multi-device branch remained
**skipped**.

## Remaining risks

- Native MSVC/Windows CUDA, AppleClang/macOS, multi-device execution, and hosted
  GPU CI have no local evidence.
- ASCCMake warning/sanitizer helpers do not instrument CUDA compile language;
  nvcc warning cleanliness and device diagnostics require separate evidence.
- The one-Runtime producer link policy is verified on GNU/Linux with CMake
  4.1/NVCC 12.9. Native MSVC and other linker behavior remain part of the
  corresponding platform skip, not an inferred portability claim.
- The installed opaque plan bridge is SDK-neutral but expands the provider ABI
  surface beyond the named user API.
- Provider teardown can synchronize and failure-path cleanup can leak when an
  owning device is irrecoverably unavailable.
- CUDA/cuBLAS asynchronous error reporting cannot attribute every hardware
  failure to the exact high-level submission; event wait remains the defined
  completion/error boundary.
- The benchmark is smoke-only and supplies no durable performance baseline.

## Primary provider references

- CMake 3.25 `FindCUDAToolkit`:
  <https://cmake.org/cmake/help/v3.25/module/FindCUDAToolkit.html>
- CMake `CMAKE_CUDA_RUNTIME_LIBRARY`:
  <https://cmake.org/cmake/help/latest/variable/CMAKE_CUDA_RUNTIME_LIBRARY.html>
- CUDA Runtime error handling:
  <https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__ERROR.html>
- CUDA Runtime API 12.9.1:
  <https://docs.nvidia.com/cuda/archive/12.9.1/pdf/CUDA_Runtime_API.pdf>
- cuBLAS 12.9.1:
  <https://docs.nvidia.com/cuda/archive/12.9.1/cublas/index.html>

These references establish provider contracts only. They do not substitute for
ASC configure, compile, runtime, or parity evidence.
