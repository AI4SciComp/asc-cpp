# Milestone 6 Portability, GPU, and Performance Review

Date: 2026-07-28

Status: complete for Publication Checkpoint B. The review findings that required
source or test changes were resolved on the milestone branch.

## Scope and authorities

This review is bounded to the Milestone 6 CUDA runtime-core and dense-provider
contract. It assesses:

- CUDA language and toolkit discovery;
- exported target and package dependency behavior;
- device, stream, event, resource, pointer, and provider semantics;
- integer-width and layout validation at the cuBLAS boundary;
- runtime correctness and Compute Sanitizer evidence;
- isolated build-tree and relocated-install consumers;
- the milestone performance-smoke methodology; and
- the exact GPU evidence classifications permitted by the frozen contract.

It does not approve Milestone 7 CUDA sparse/random work, HIP/ROCm work, or any
additional provider dependency.

The governing decisions are ADR-0004, ADR-0007, ADR-0008, ADR-0009, ADR-0011,
ADR-0013, and ADR-0018, together with the dependency manifest, capability
manifest, backend matrix, Milestone 6 contract, and ownership ledger.

## Outcome

The implementation is suitable for the Milestone 6 checkpoint on the tested
Linux/NVIDIA configuration. The provider targets retain the approved dependency
edges, provider-free consumers do not discover CUDA, provider operations are
stream ordered, and the amended tests cover the required runtime routes,
layout rejection, and provider-width validation.

No release-quality cross-platform or multi-device claim is made. GPU evidence
is limited to the machine and commands recorded below.

## Review findings and resolutions

### Resolved: moved context teardown order

The original defaulted `DenseCudaContext` move assignment could replace its
execution context, and therefore destroy its old stream, before destroying the
old cuBLAS handle bound to that stream. Production replaced the defaulted
operation with an explicit self-safe move assignment that resets the provider
state first. A lifecycle regression test now exercises move assignment with
initialized source and destination contexts.

Resolution evidence:

- `src/dense/cuda/context.cc` explicitly destroys provider state before moving
  the execution context; and
- `tests/dense_cuda/dense_cuda_move_assignment_test.cc` passes in the focused
  CUDA suite.

### Resolved: runtime-route coverage

The initial Core CUDA runtime test allocated managed memory without exercising
it and did not prove a successful ordinary pageable-host transfer route. The
test now covers pinned-to-managed-to-pinned and pageable-host-to-device-to-
pageable-host copies.

The final focused rebuild and execution passed
`asc_cpp.core_cuda.core_cuda_runtime_test`.

### Resolved: layout and provider-width boundary coverage

The initial linalg coverage used `LayoutStride` for one right-layout rejection
case and did not exercise all `std::int64_t` to cuBLAS `int` narrowing
boundaries. The amended test uses true nondegenerate and degenerate
`LayoutRight` mappings and rejects provider extent, leading dimension, input
increment, output increment, and GEMM inner-dimension values greater than
`INT_MAX` before allocation or provider dispatch.

The final focused rebuild and execution passed
`asc_cpp.dense_cuda.dense_cuda_linalg_test`.

### Resolved: performance-smoke metadata and allocation evidence

The initial benchmark hard-coded architecture 86 and printed an unmeasured
allocation count. It now:

- reports the GPU, compute capability, runtime and driver versions, cuBLAS
  header version, and build configuration;
- derives device architecture from the runtime instead of asserting a
  hard-coded value;
- wraps ASC pinned and device resources in counting resources;
- reports measured ASC resource allocations separately for each operation;
- performs three warmups and twelve measured repetitions with explicit event
  completion; and
- emits one finite checksum per operation.

The benchmark remains a performance smoke test, not a throughput gate.

### Validation-process lesson: rebuild after concurrent interface edits

One intermediate independent run observed evaluator failures after
`ViewDescriptor` changed while related translation units were compiling. An
exact object rebuild eliminated the failures. This was a stale-object ABI
artifact of concurrent integration, not a product defect. Clean, from-scratch
configuration and build are required after final integration; incremental
results spanning an interface change are not checkpoint evidence.

## Portability and package audit

### Configuration

- The root project remains C++-only unless `ASC_CPP_ENABLE_CUDA=ON`.
- CUDA language and `CUDAToolkit` discovery are conditional on that option.
- The toolkit minimum is 12 and the tested toolkit is 12.9.86.
- CUDA architecture remains caller-provided; the package does not select a GPU
  architecture.
- The configured CUDA runtime is shared, consistently with the single-runtime
  contract.
- CUDA-disabled package tests prove that provider-free configuration does not
  discover CUDA.

### Target and dependency closure

The generated static exports were inspected:

- `ASC::core_cuda` exports `ASC::core` and privately retains
  `CUDA::cudart` through a link-only edge.
- `ASC::dense_cuda` exports `ASC::dense` and `ASC::core_cuda` and privately
  retains `CUDA::cublas` through a link-only edge.
- `find_dependency(CUDAToolkit)` is emitted only when the requested component
  closure contains `core_cuda` or `dense_cuda`.
- Provider-free `ASC::cpp` consumers do not acquire a CUDA dependency.
- No unapproved CUDA toolkit library is linked by a production target.

C++-only isolated consumers passed for both build-tree and relocated installed
provider packages. A relocated provider consumer can therefore resolve the
exported CUDA dependency without enabling CUDA as a consumer language.

Shared-library inspection on the integration build showed:

- Core CUDA needs `libasc_core` and `libcudart.so.12`.
- Dense CUDA needs `libasc_dense`, `libasc_core_cuda`, `libasc_core`,
  `libcublas.so.12`, and `libcudart.so.12`.

No toolkit binary is bundled into the package. Build-tree runtime paths are
build paths; the install runtime path is relative as configured by the
project. Relocated installed-consumer execution passed in the lead validation.

### Runtime and provider semantics

- Temporary device changes use a guard that restores the previous current
  device.
- Completion events retain execution state, not user arrays, and wait only for
  the recorded event.
- No operation uses device-wide or stream-wide synchronization.
- Pointer metadata and device compatibility are checked before enqueue.
- Copy overlap is rejected.
- Kernel indexing uses checked signed extents and grid-stride traversal.
- Every cuBLAS `int` conversion is preceded by range validation.
- Dense CUDA configures host pointer mode and does not enable atomics or fast
  Tensor Core math.
- The deterministic path uses the approved pedantic math configuration where
  supported by the frozen contract.
- Steady-state ASC operation allocation counts are zero in the performance
  smoke test. cuBLAS may retain implementation-owned handle state created with
  the context; this is not attributed to an ASC memory resource.
- A context is not claimed to be concurrently usable by multiple host threads.
  Independent contexts and streams are covered.

The device-guard destructor cannot report a failed restoration call. This is a
normal destructor limitation and remains a low-probability residual risk.

## Independent validation

Test host:

- OS/provider: Linux with NVIDIA driver 576.83
- GPU: NVIDIA GeForce RTX 3060 Laptop GPU, compute capability 8.6, 6144 MiB
- CMake: 4.1.2
- C++ compiler: GCC 11.4
- CUDA compiler/toolkit: nvcc 12.9.86 / CUDAToolkit 12.9.86
- CUDA runtime and driver API values: 12090 / 12090

### Configure and compile

Command:

```text
cmake -S . -B /tmp/asc_cpp_m6_portability_cuda_make -G 'Unix Makefiles' -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/prefix/share/ASCCMake -DASC_CPP_ENABLE_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=86 -DASC_CPP_BUILD_TESTING=ON -DBUILD_TESTING=ON -DASC_CPP_INSTALL=ON -DASC_CPP_WARNINGS_AS_ERRORS=ON -DCMAKE_BUILD_TYPE=Debug
```

Result: PASS. CUDA 12.9.86 and architecture 86 were configured.

Command:

```text
cmake --build /tmp/asc_cpp_m6_portability_cuda_make --parallel 4
```

Result: PASS with warnings treated as errors.

### Final amended coverage

Command:

```text
cmake --build /tmp/asc_cpp_m6_portability_cuda_make --target asc_core_cuda_core_cuda_runtime_test asc_dense_cuda_dense_cuda_linalg_test --parallel 2
ctest --test-dir /tmp/asc_cpp_m6_portability_cuda_make --output-on-failure -R '^asc_cpp\.(core_cuda\.core_cuda_runtime_test|dense_cuda\.dense_cuda_linalg_test)$' -j 1
```

Result: PASS, 2/2 tests, 1.36 seconds total.

### Package-negative evidence

Command:

```text
ctest --test-dir /tmp/asc_cpp_m6_portability_cuda_make --output-on-failure -R '^asc_cpp\.package\.cuda_(disabled_isolation|requested_unavailable)$' -j 1
```

Result: PASS, 2/2 tests, 2.86 seconds total.

### Compute Sanitizer

Commands and results:

```text
compute-sanitizer --tool memcheck --error-exitcode=99 /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_linalg_test
```

Result: PASS, zero errors.

```text
compute-sanitizer --tool racecheck --error-exitcode=99 /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_concurrency_test
```

Result: PASS, zero hazards, errors, or warnings.

```text
compute-sanitizer --tool initcheck --error-exitcode=99 /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_owner_test
```

Result: PASS, zero errors.

```text
compute-sanitizer --tool synccheck --error-exitcode=99 /tmp/asc_cpp_m6_portability_cuda_make/tests/dense_cuda/asc_dense_cuda_dense_cuda_evaluate_test
```

Result: PASS, zero errors.

```text
compute-sanitizer --tool memcheck --leak-check full --error-exitcode=99 /tmp/asc_cpp_m6_portability_cuda_make/tests/core_cuda/asc_core_cuda_core_cuda_runtime_test
```

Result: PASS, zero leaked bytes and zero errors.

These Compute Sanitizer runs assess CUDA runtime behavior. Host ASan/UBSan
evidence belongs to the lead's clean checkpoint validation and is not inferred
from these commands.

## Performance evidence

Independent debug-build smoke output:

```text
gpu="NVIDIA GeForce RTX 3060 Laptop GPU" compute_capability=8.6 runtime=12090 driver=12090 cublas_headers=12.9.1 configuration=debug
h2d elapsed_ns=7772271 repetitions=12 throughput=6475796842 bytes/s checksum=64.54296875 asc_resource_allocation_calls=0
d2h elapsed_ns=7518266 repetitions=12 throughput=6694581969 bytes/s checksum=64.54296875 asc_resource_allocation_calls=0
d2d elapsed_ns=1293285 repetitions=12 throughput=3.891767708e+10 bytes/s checksum=64.54296875 asc_resource_allocation_calls=0
terminal_evaluate elapsed_ns=8508775 repetitions=12 throughput=5915263713 bytes/s checksum=64.54296875 asc_resource_allocation_calls=0
axpy elapsed_ns=1083150 repetitions=12 throughput=2.323392328e+10 flop/s checksum=64.54296875 asc_resource_allocation_calls=0
gemv elapsed_ns=737863 repetitions=12 throughput=3.410636392e+10 flop/s checksum=64.54296875 asc_resource_allocation_calls=0
gemm elapsed_ns=1475672 repetitions=12 throughput=2.182887167e+12 flop/s checksum=64.54296875 asc_resource_allocation_calls=0
```

The measurements include launch, event-recording, and event-wait overhead and
use one final checksum. They demonstrate finite execution and zero
steady-state allocations through ASC resources; they must not be interpreted
as optimized or cross-system performance claims.

## GPU evidence classification

The classifications below use the frozen vocabulary exactly.

| Capability | Evidence |
|---|---|
| CUDA runtime core: resource construction, metadata, H2D/D2H/D2D copies, managed route, pageable-host route, event wait, same-device restoration | runtime-tested |
| CUDA dense: ownership, view/evaluation, scalar operations, AXPY, GEMV, GEMM, validation, independent-stream behavior | runtime-tested |
| CUDA dense numerical comparison with approved CPU reference paths | parity-tested |
| CUDA provider headers, targets, package exports, and isolated consumers | compile-tested |
| CUDA toolkit discovery and architecture-86 configuration | configure-tested |
| Cross-device current-device restoration | skipped — only one GPU was available |
| MSVC/Windows CUDA provider build | skipped — no Windows/MSVC environment |
| Apple CUDA provider build | skipped — no supported Apple CUDA environment |
| Clang CUDA provider build | skipped — tested CUDA host compiler was GCC |
| CUDA sparse and random providers | skipped — Milestone 7 scope |
| HIP/ROCm providers | skipped — later milestone scope |

The milestone-level classification is therefore:

- Core CUDA: configure-tested, compile-tested, runtime-tested.
- Dense CUDA: configure-tested, compile-tested, runtime-tested, parity-tested.

## Remaining risks

- Runtime and parity evidence comes from one Linux/NVIDIA/GCC machine with one
  compute-capability-8.6 GPU.
- Cross-device restoration and multi-GPU behavior remain untested.
- Windows/MSVC, Apple, and Clang CUDA portability remain untested.
- `cudaFree` and `cudaFreeHost` can synchronize during resource teardown;
  callers must honor the documented lifetime/completion contract.
- Device restoration failure in a destructor cannot be propagated.
- cuBLAS implementation-owned allocations and initialization costs are outside
  the ASC memory-resource counter.
- Performance numbers are debug-build smoke data, not regression thresholds.
- A final clean build is required after all integration edits because an
  incremental build that overlaps an interface edit can retain stale objects.

## Review conclusion

The Milestone 6 CUDA core and dense implementation meets the bounded
portability, packaging, runtime, and smoke-performance contract on the tested
configuration. All findings raised by this review that were actionable within
Milestone 6 have corresponding implementation or test resolutions. The
remaining risks are explicitly limited or skipped and do not authorize later
milestone work.
