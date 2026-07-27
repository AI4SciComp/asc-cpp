# Milestone 6 Independent Verification Review

Status: Independent verification complete, with explicit skips and no open
production defect

Date: 2026-07-27

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Role: independent verification

## Independence and scope

The verification design was frozen before production inspection. Verification
then reviewed the integrated Milestone 6 implementation, added only
verifier-owned tests, consumers, benchmark code, and this report, and reported
findings to production and the lead. MdeCpp, the deleted legacy asc-cpp CUDA
implementation, and other specialists' initial conclusions were not used to
derive the test oracles.

The review covers the approved CUDA core and dense facets only:

```text
ASC::core_cuda  -> ASC::core; CUDA::cudart
ASC::dense_cuda -> ASC::dense; ASC::core_cuda; CUDA::cublas
```

There is no sparse, random, cuSPARSE, cuRAND, cuSOLVER, reduction, solver,
complex, mixed-precision, batching, external-stream, or native-handle surface
in the verified milestone.

## Environment and evidence classification

| Item | Verified value |
| --- | --- |
| Host | Linux under WSL, x86-64 |
| CMake | 4.1.2 |
| C++ compiler | GCC 11.4.0 |
| CUDA compiler/toolkit | NVCC/CUDAToolkit 12.9.86 |
| cuBLAS headers | 12.9.1.4 |
| Compute Sanitizer | 2025.2.1.0 |
| GPU | NVIDIA GeForce RTX 3060 Laptop GPU |
| Driver | 576.83 |
| Compute capability | 8.6 |
| Device memory | 6144 MiB |
| Requested architecture | 86 |
| Detected CUDA devices | 1 |

The Milestone 6 provider evidence is:

- **configure-tested**: CUDA 12.9 discovery, CUDA language enablement,
  `CUDA::cudart`, `CUDA::cublas`, and architecture 86 configuration succeeded;
- **compile-tested**: provider sources, CUDA kernels, public headers,
  exception-disabled header checks, multi-TU tests, negative compilation,
  benchmark, and build/install-tree consumers compiled;
- **runtime-tested**: core resource/copy/event and dense
  storage/evaluator/algebra/concurrency operations ran on the identified GPU;
- **parity-tested**: float and double pointwise, Copy, Scal, Axpy, Gemv, and
  Gemm results passed exact or independent host-oracle comparisons; and
- **skipped**: multi-device teardown/current-device restoration runtime testing
  requires at least two devices. This host has one. The source-private device
  guards and single-device stability were reviewed/tested, but are not
  mislabeled as multi-device runtime evidence.

## Independent conclusions

1. `CudaMemoryResource`, CUDA `ExecutionContext`, and `CompletionEvent`
   preserve the approved ownership and lifetime model. Resource objects are
   stable-address, noncopyable, and nonmovable; contexts are copyable shared
   state; events are move-only and retain execution state.
2. Core CUDA copy validates the declared memory space against CUDA pointer
   attributes, including the inverse case in which pinned, device, or managed
   storage is falsely declared pageable host. Genuine unregistered pageable
   host pointers remain supported.
3. CUDA operations are stream/event based. No success path uses a device-wide
   synchronization. Pageable host transfer may stage or block at the CUDA
   Runtime boundary; returned events still represent stream completion.
4. Dense storage continues to own its memory through the core resource
   abstraction. Device and managed views do not become host-dereferenceable
   through `At`.
5. The pointwise provider supports only the frozen rank-zero-through-eight,
   float/double, one-level expression subset. It is storage-specific in dense,
   while the expression module remains storage-neutral.
6. Dense algebra accepts unique device mappings only. Copy exact-self is a
   no-op; Axpy exact same-index in-place executes; partial or differently
   mapped overlap is rejected transactionally. Gemv/Gemm output overlap is
   rejected.
7. Gemv/Gemm use column-major-compatible mappings and checked cuBLAS provider
   widths. Padded leading dimensions, positive vector increments, all approved
   transpose pairs, `beta == 0`, nonzero beta, and zero inner dimensions pass.
8. Deterministic contexts configure cuBLAS host pointer mode, atomics disabled,
   and pedantic math. Float and double alpha/beta are converted without
   extending user scalar lifetimes.
9. Kernel launch geometry is overflow-free and grid-stride. The exact
   source-private helpers used by the kernels pass no-allocation compile/runtime
   boundaries at zero, one, the former 65,535-block capacity on both sides, and
   `extent_t` maximum termination.
10. Separate context/stream/handle/buffer sets pass concurrent host-thread
    execution. One mutable dense provider context is intentionally not shared
    concurrently.
11. Static and shared producers, their focused concurrency executables, and
    build-tree/relocated consumers use the shared CUDA Runtime. Link commands
    contain no `cudart_static`; CUDA Runtime definitions are not embedded in
    ASC producer binaries; and the loader initializes one `libcudart.so.12`.

## Findings and resolutions

| ID | Finding | Resolution and regression |
| --- | --- | --- |
| M6-VER-01 | Exact self-copy returned before CUDA pointer/space/device validation. | Validation now precedes the no-op event. A host pointer mislabeled device is rejected without mutation. |
| M6-VER-02 | A blanket asynchronous-copy statement was too strong for pageable host memory, which CUDA may stage or block. | The contract/documentation now distinguishes event-based stream completion from stronger pinned/device/managed host-asynchronous behavior. |
| M6-VER-03 | Pointwise negate initially accepted a scalar operand beyond the frozen dense-terminal-only surface. | Dense-terminal recognition is explicit; scalar and nested negate reject. |
| M6-VER-04 | cuBLAS dimensions were checked, but vector increments and leading dimensions could narrow unchecked. | `m/n/k`, increments, and `lda/ldb/ldc` are checked before provider access. Synthetic over-width tests reject before pointer use. |
| M6-VER-05 | Kernel block-count arithmetic could overflow/truncate and did not scale beyond one grid. | Production uses capped quotient/remainder block count plus a grid-stride loop. Source-private helper boundaries include `INT64_MAX` without allocation or launch. |
| M6-VER-06 | cuBLAS handle destruction was not guarded to the owning CUDA device. | Dense context state stores the device ordinal and guards destruction/restoration. Multi-device runtime remains an explicit one-GPU-host skip. |
| M6-VER-07 | A failed CUDA Runtime call could leave sticky state that a later valid operation attributed to itself. | Translated errors are consumed and submission paths isolate prior sticky state. A failed `cudaSetDevice(-1)` followed by valid fill/scale/copy now passes. |
| M6-VER-08 | Dense algebra plans initially lost declared placement and mapping-uniqueness information; alias policy also required exact in-place Axpy clarification. | Public template validation rejects non-device or nonunique views before plan erasure. Float/double tests cover every algebra family, inverse host/managed labels naming valid device pointers, no mutation, exact in-place Axpy, and partial-overlap rejection. |
| M6-VER-09 | Core `MemorySpace::kHost` initially accepted CUDA-tracked pointers based only on the declaration. | CUDA pointer classification now accepts only unregistered pageable host for `kHost`. Inverse pinned/device/managed source and destination labels reject, preserve bytes, and do not poison the next valid copy. |
| M6-VER-10 | The architecture test retained the later-roadmap capability name `selected cuBLAS/cuSOLVER` while approved M6 implements selected cuBLAS only. | The lead reconciled the oracle to `CUDA dense evaluation and selected cuBLAS`; cuSOLVER remains excluded. |
| M6-VER-11 | The documentation/API audit found unchecked half-open CUDA copy address-span ends. | Production checks both `std::uintptr_t` additions. The public-API regression uses two real device allocations and synthetic large view sizes to force source-end and destination-end overflow independently; both return `kOverflow` before enqueue under normal runtime, UBSan, and Compute Sanitizer memcheck. |
| M6-VER-12 | The documentation/API audit found that a post-enqueue event-record failure could lose the only completion handle for live work. | Production creates completion state before enqueue and performs a failure-only stream drain if record fails. This is independently source-reviewed. Deterministic runtime fault injection is skipped because the approved API exposes neither native handles nor a provider fault injector; the verifier did not breach that boundary. |
| M6-VER-13 | M6-PORT-12 found that NVCC's default static Runtime linkage could give CUDA-owning targets a private Runtime instance in addition to `CUDA::cudart`. | The lead sets `CMAKE_CUDA_RUNTIME_LIBRARY=Shared` immediately after CUDA language enablement, sets `asc_dense_cuda`'s target property to `Shared`, and asserts it during configure. Fresh static/shared link, symbol, loader, concurrency, build-tree consumer, and relocated-install consumer audits find only the shared Runtime. |

All production findings are resolved. The public provider header necessarily
contains `internal_dense_cuda` plan/opaque-launch bridge declarations so host
templates can reach compiled CUDA launchers. No CUDA or cuBLAS SDK type/header
leaks through that public header, but changes to the bridge remain an ABI risk
to manage before a stable release.

## Functional and parity coverage

Core runtime coverage includes:

- device inventory and invalid ordinals;
- pinned, device, and managed allocate/deallocate, alignment, zero size,
  impossible allocation, move/reset, and provider/native diagnostics;
- pageable/pinned/device/managed transfer routes and device-to-device;
- exact self-copy, partial overlap, size/null/backend/space/device failures,
  inverse placement labels, checked source/destination address-span ends,
  destination canaries, and recovery;
- event query/wait/move and event-retained context lifetime; and
- provider-free CUDA creation rejection.

Dense storage/evaluator coverage includes:

- float/double host/device/pinned/managed owners, clone, zero extent, rank zero,
  move state, and host-dereference rejection;
- evaluator ranks zero through eight, rank nine rejection, left/right/padded
  layouts, hole canaries, supported terminal/scalar combinations, shape and
  nesting rejection;
- partial overlap and wrong declared host/managed placement naming actual
  device pointers, all with downloaded no-mutation evidence; and
- extreme metadata rejection plus above-former-grid-capacity no-op metadata
  validation.

Dense algebra coverage includes:

- float/double rank-one and rank-two Copy/Scal/Axpy;
- bit-sensitive Copy for signed zero, infinity, and NaN;
- exact-self Copy, exact same-index Axpy, partial-overlap transactionality, and
  nonunique const-source rejection;
- Gemv none/transpose, padded matrix/vector/output strides, row-major
  rejection, output overlap, invalid operation, checked dimension/stride
  boundaries, and zero-inner beta zero/scale behavior;
- Gemm all four transpose pairs, padded `lda/ldb/ldc` with hole canaries,
  row-major rejection, output overlap, checked provider widths, and zero-inner
  beta zero/scale behavior; and
- wrong placement, moved context, independent streams, and sequential error
  isolation.

## Commands and outcomes

Fresh CUDA configure and warning-as-error build:

```sh
cmake -S . -B /tmp/asc-cpp-m6-independent.8at7vJ/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-independent.8at7vJ/build --parallel 4
```

Result: PASS.

Focused registered provider matrix:

```sh
ctest --test-dir /tmp/asc-cpp-m6-independent.8at7vJ/build \
  -R '^asc_cpp\.(core_cuda|dense_cuda)\.' --output-on-failure
```

Result: PASS, 13/13. This includes four runtime/numerical/concurrency tests,
two positive compile/link contracts, six expected compile failures, and the
benchmark smoke test.

Standalone provider public-header and exceptions-disabled checks:

```sh
ctest --test-dir /tmp/asc-cpp-m6-independent.8at7vJ/build \
  -R '^asc_cpp\.compile\.m6_header' --output-on-failure
```

Result: PASS, 8/8.

Isolated provider build-tree and install/relocation consumers:

```sh
ctest --test-dir /tmp/asc-cpp-m6-independent.8at7vJ/build \
  -R '^asc_cpp\.consumer\.(core_cuda|dense_cuda)\.' --output-on-failure
```

Result: PASS, 4/4: `core_cuda` and `dense_cuda` each passed from the build
tree and a relocated install.

Full Milestone 6 architecture, compile, runtime, consumer, relocation, package,
registry, and requested-unavailable selection:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier.vmqPjT/build \
  -L milestone-6 --output-on-failure
```

The original invocation passed 48/49. Its only failure was M6-VER-10, the
stale architecture capability name. Both long package tests passed in that
same invocation (`build_tree_components` in 239.04 seconds and
`install_and_relocate_components` in 232.20 seconds), as did both CUDA
build-tree/install-relocate consumers and requested-CUDA-unavailable behavior.
After the lead corrected the oracle:

```sh
ctest --test-dir /tmp/asc-cpp-m6-verifier.vmqPjT/build \
  -R '^asc_cpp\.architecture\.dependency_manifest$' --output-on-failure
```

Result: PASS, 1/1. Thus every registered test in the selection has a passing
post-correction result; the ten-minute aggregate command was not repeated
solely to re-execute already-passing package permutations.

Formatting and diff hygiene:

```sh
clang-format-19 --dry-run --Werror <all verifier-owned M6 .cc/.h files>
git diff --check
```

Result: PASS.

UndefinedBehaviorSanitizer:

```sh
cmake -S . -B /tmp/asc-cpp-m6-independent-ubsan.zz4ZLe/build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  '-DASC_CPP_SANITIZER_ARGUMENTS=UNDEFINED' \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-independent-ubsan.zz4ZLe/build \
  --target asc_core_cuda_test asc_dense_cuda_storage_evaluate_test \
           asc_dense_cuda_linalg_test asc_dense_cuda_concurrency_test \
  --parallel 4
UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1' <each focused executable>
```

Result: PASS, 4/4.

AddressSanitizer configuration and compilation with the actual asc-cmake API
(`ADDRESS UNDEFINED`) succeeded. Runtime is **skipped**, not passed: on the
first `cuInit` under WSL, ASan reports a double-free wholly within the
Microsoft WSL NVIDIA driver libraries (`libcuda.so.1.1` /
`libnvdxgdmal.so.1`), before ASC test work. Compute Sanitizer and standalone
UBSan provide the usable evidence on this host. The reproduced build is
`/tmp/asc-cpp-m6-independent-asan.hwgycl/build`.

Compute Sanitizer:

```sh
compute-sanitizer --tool memcheck --report-api-errors no \
  --error-exitcode 99 <core/dense focused executable>
compute-sanitizer --tool racecheck --report-api-errors no \
  --error-exitcode 99 <dense concurrency executable>
compute-sanitizer --tool initcheck --report-api-errors no \
  --error-exitcode 99 <each dense runtime executable>
compute-sanitizer --tool synccheck --report-api-errors no \
  --error-exitcode 99 <each dense runtime executable>
```

Results:

- memcheck: PASS, zero errors for core, storage/evaluate, linalg, and
  concurrency;
- racecheck: PASS, zero errors and zero warnings for concurrency;
- initcheck: PASS, zero errors for all three dense runtime executables; and
- synccheck: PASS, zero errors for all three dense runtime executables.

`--report-api-errors no` is required because negative tests intentionally
exercise failed CUDA API calls, including impossible allocation and sticky
error isolation. Device memory/race/initialization/synchronization errors
remain enabled.

## Post-integration CUDA Runtime linkage re-audit

The lead-owned M6-PORT-12 correction was independently re-audited after
integration. Both configurations used GCC 11.4.0, NVCC/CUDAToolkit 12.9.86,
architecture 86, warnings as errors, and the actual asc-cmake package.

```sh
cmake -S . -B /tmp/asc-cpp-m6-runtime-static.z5zvn6/build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-runtime-static.z5zvn6/build \
  --target asc_utilities asc_random asc_dense asc_sparse \
           asc_core_cuda asc_dense_cuda asc_dense_cuda_concurrency_test \
  --parallel 4

cmake -S . -B /tmp/asc-cpp-m6-runtime-shared.hgkhHB/build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=86 \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/test-debug
cmake --build /tmp/asc-cpp-m6-runtime-shared.hgkhHB/build \
  --target asc_utilities asc_random asc_dense asc_sparse \
           asc_core_cuda asc_dense_cuda asc_dense_cuda_concurrency_test \
  --parallel 4
```

Result: PASS for both configure/builds. The configure-time assertion reports
no target-property failure: `asc_dense_cuda` has
`CUDA_RUNTIME_LIBRARY=Shared`.

For each build:

```sh
ctest --test-dir <build> \
  -R '^asc_cpp\.(dense_cuda\.concurrency|consumer\.(core_cuda|dense_cuda)\.)' \
  --output-on-failure
```

Results:

- static producer: PASS, 5/5;
- shared producer: PASS, 5/5;
- each result includes concurrency plus `core_cuda` and `dense_cuda`
  build-tree and relocated-install runtime consumers.

The producer and consumer link/symbol/loader audit used:

```sh
rg 'cudart_static|libcudart_static' <static-and-shared-builds> \
  -g link.txt -g '*.make' -g '*.ninja'
readelf -d <provider-library-or-concurrency/consumer-executable>
ldd <provider-library-or-concurrency/consumer-executable>
nm -C <provider-library-or-concurrency/consumer-executable>
LD_DEBUG=libs <static-or-shared-concurrency-executable>
```

Result: PASS.

- No generated producer or isolated-consumer link command contains
  `cudart_static` or `libcudart_static`.
- The static concurrency executable and all four static consumers link
  `libcudart.so`; their ELF dynamic tables require `libcudart.so.12`.
- `libasc_core_cuda.so` and `libasc_dense_cuda.so` each require
  `libcudart.so.12`. CUDA Runtime API and fat-binary registration symbols are
  undefined versioned imports, not private definitions in an ASC library or
  executable.
- Shared consumers link the ASC shared providers; the provider dependency
  chain resolves the same `libcudart.so.12`.
- Loader tracing for both concurrency executables shows exactly one
  `libcudart.so.12` search, initialization, and finalization.

This evidence closes M6-PORT-12 for the tested Linux/GCC/NVCC static and shared
configurations. It does not extend runtime-linkage evidence to Windows, macOS,
another host linker, or another CUDA toolkit.

## Benchmark evidence

The benchmark is a smoke harness, not a performance claim or gate. It reports
`benchmark_classification=smoke-only`, `performance_claim=none`, separates
transfers from operations, performs warmups, waits on the final event, and
checks a finite checksum. In the independent Debug-like run, float and double
pointwise add, Axpy, Gemv, and Gemm completed with checksum `349.632`. Float
timings in microseconds were H2D `4815`, add `20055`, Axpy `4268`, Gemv `4282`,
Gemm `2596`, and D2H `1415`; double timings were H2D `3353`, add `27001`, Axpy
`7994`, Gemv `4191`, Gemm `14884`, and D2H `1842`. These single Debug-like
measurements are smoke evidence only. The environment table above, rather than
a hardcoded benchmark string, records device, driver, toolkit, cuBLAS, and
architecture.

## Remaining risks and explicit skips

1. Multi-device handle/stream/event teardown and current-device restoration are
   compile/source-reviewed and single-device-tested, but runtime-skipped on this
   one-GPU host.
2. ASan cannot supply usable CUDA runtime evidence on this WSL NVIDIA driver
   because it fails during driver initialization. This is an environment/tool
   incompatibility; it is not converted into a project pass.
3. Only architecture 86 and one NVIDIA GPU/driver/toolkit combination are
   runtime/parity-tested here. Other configured architectures remain
   compile/configure evidence unless separately exercised.
4. The public internal opaque-launch bridge is SDK-neutral but remains an ABI
   maintenance surface.
5. Post-enqueue `cudaEventRecord` failure safety is source-reviewed, but
   deterministic runtime fault injection is skipped because no approved public
   provider fault injector or native-handle API exists.
6. Timing output is smoke evidence only. No regression threshold or
   comparative performance conclusion is approved.

## Verdict

The independently exercised Milestone 6 production surface satisfies the
frozen core/dense CUDA contract on the identified CUDA 12.9 / RTX 3060
environment. All independently identified production defects are resolved.
Publication may proceed only with the explicit ASan and multi-device skips
retained, with the event-record fault-injection skip retained, and without
extending the evidence to untested devices, architectures, providers, or later
milestones.
