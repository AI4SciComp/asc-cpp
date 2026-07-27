# Milestone 6 Contract: GPU Core and Dense

Status: Frozen after owner approval

Date: 2026-07-26

Owner approval: Milestone 6 with no corrections

Exact milestone: **Milestone 6 — GPU core and dense**

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Baseline commit and unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the intentionally uncommitted Milestones 0--5 publication
candidate and the owner's earlier deletion of the retired implementation are
retained in place. Milestone 6 does not restore, reset, discard, commit, or
reclassify that work.

## Purpose

Implement the first approved optional GPU backend as two owner-specific
facets:

```text
ASC::core_cuda  -> ASC::core
ASC::dense_cuda -> ASC::dense;ASC::core_cuda
```

`core_cuda` owns CUDA runtime allocation, copies, streams, completion events,
execution state, and CUDA runtime error translation. `dense_cuda` owns a
CUDA dense provider context, a bounded built-in pointwise evaluator, and a
selected asynchronous float/double cuBLAS subset.

The provider-free six-module graph, both random storage facets, and
`ASC::cpp` remain unchanged. CPU-only configuration must not discover CUDA,
load a CUDA package, compile CUDA, import a provider target, or require a CUDA
runtime library.

## Product targets and version

```text
build target:    asc_core_cuda
exported target: ASC::core_cuda
kind:            compiled C++20 provider facet
direct ASC link: ASC::core
private external dependency: CUDA::cudart

build target:    asc_dense_cuda
exported target: ASC::dense_cuda
kind:            compiled C++20/CUDA provider facet
direct ASC links: ASC::dense;ASC::core_cuda
private external dependency: CUDA::cublas

release line:    unreleased 0.6.0 candidate
```

`CUDA::cudart` and `CUDA::cublas` are the actual imported targets supplied by
CMake `FindCUDAToolkit`; `CUDAToolkit::cudart` is not an API and must not be
invented. cuSOLVER is not linked or exposed in this milestone because no
factorization or solver operation contract has been approved by ADR 0013.

## Configuration and packaging

Add exactly one provider switch:

```text
ASC_CPP_ENABLE_CUDA=OFF
```

When false, configuration is the existing provider-free build and performs no
CUDA discovery. When true:

- require `CUDAToolkit` version 12 or newer through standard CMake;
- enable the CUDA language only because `dense_cuda` owns compiled kernels;
- honor standard `CMAKE_CUDA_ARCHITECTURES`; do not infer a build-global
  architecture from visible hardware;
- fail configuration if the toolkit, compiler, runtime, or cuBLAS target is
  unavailable;
- build and export both provider facets; and
- retain target-local warnings, C++20, sanitizer policy, and CUDA compilation.

The cumulative known component set becomes:

```text
core utilities expression random dense sparse
random_dense random_sparse cpp core_cuda dense_cuda
```

Available components remain the nine provider-free components when CUDA is
disabled and add `core_cuda;dense_cuda` only when enabled. Required closures
are:

```text
core_cuda  -> core;core_cuda
dense_cuda -> core;expression;dense;core_cuda;dense_cuda
```

The installed package calls `find_dependency(CUDAToolkit 12)` only after a
requested component closure includes a CUDA facet. A no-component or
provider-free component request does not discover CUDA. `ASC::cpp` remains
provider-free.

## Core provider-neutral evolution

`ExecutionContext` and `CompletionEvent` may acquire private, provider-neutral
opaque state so a context owns one provider stream and an event owns one
provider completion record without a global registry. Common core headers
contain no CUDA declaration or SDK type.

Preserve:

- `ExecutionContext::Serial()` and serial copy behavior;
- `ExecutionContext::Create(...)` as a provider-free validator that does not
  discover or instantiate CUDA;
- copyable immutable execution contexts;
- move-only completion events;
- failure before pointer access when the context cannot access a space; and
- no automatic fallback.

Add a query operation to `CompletionEvent`:

```text
Query() -> Result<bool>
```

`Wait()` waits only for that event. Destruction does not synchronize the
device. Provider event state may retain its execution/stream state, but never
user arrays.

`CopyBytes` dispatches through the explicit context. A CUDA copy validates
view sizes, nullability, spaces, device, and overlap before enqueueing, uses
the context stream, and returns a completion event. Exact self-copy is a
no-op event; partially overlapping CUDA ranges are rejected. No device-wide
synchronization or fallback is permitted.

## Public core CUDA API

Public provider-specific files are:

```text
include/asc/core/providers/cuda.h
include/asc/core/providers/cuda_export.h
```

They are self-contained and contain no CUDA SDK type in a public signature.
The exact public surface is:

```text
CudaDeviceCount() -> Result<std::int32_t>

CudaMemoryResource::Create(device, memory_space)
    -> Result<std::unique_ptr<CudaMemoryResource>>

CreateCudaExecutionContext(
    device, determinism = Determinism::kDeterministic)
    -> Result<ExecutionContext>

RecordCudaEvent(context) -> Result<CompletionEvent>
```

`CudaMemoryResource` is noncopyable and nonmovable so its address remains
stable for every `Buffer` allocated from it. The factory accepts exactly
`kPinnedHost`, `kDevice`, or `kManaged`; ordinary host allocation remains
owned by `HostMemoryResource`. It validates a CUDA device, translates
allocation errors with provider/native codes, returns null for zero bytes, and
deallocates exactly once through the matching CUDA operation.

`CreateCudaExecutionContext` validates a CUDA device and creates one owned
nonblocking CUDA stream. There is no default device, native-stream adoption,
global current context, provider registry, or mutable process-global ASC
state. Provider calls restore any CUDA current-device state that they
temporarily change.

## Dense device storage

Add provider-neutral explicit uninitialized allocation to `DenseArray`:

```text
DenseArray::CreateUninitialized(extents, resource, layout)
    -> Result<DenseArray>
```

It accepts every valid core memory space and allocates the exact unique,
exhaustive owner span without touching element storage. Existing `Create`
remains host-only and value-initializing. Elements remain trivial arithmetic
types. Device owners expose normal non-dereferenceable `DenseView`
descriptors. Host dereference remains explicit; pinned host may be
host-dereferenceable, while device and managed views do not silently migrate
through `At`.

`Clone(destination_resource, context)` remains a synchronous named deep copy:
it explicitly allocates the destination, enqueues `CopyBytes`, waits for the
returned event, and publishes the new owner only after success. Failed
allocation, copy, or wait publishes no owner. No implicit copy occurs in
construction, view creation, resize, or evaluation.

## Public dense CUDA API

Public provider-specific files are:

```text
include/asc/dense/providers/cuda.h
include/asc/dense/providers/cuda_export.h
```

No CUDA or cuBLAS SDK type appears in a public signature.

The provider context is:

```text
DenseCudaContext::Create(ExecutionContext)
    -> Result<DenseCudaContext>

execution_context() -> const ExecutionContext&
```

It is move-only, requires a CUDA context, owns one cuBLAS handle bound to the
context stream, uses host scalar pointer mode, disallows atomic reduction
algorithms, and selects pedantic/default-precision math for deterministic
requests. Creation may allocate provider state; operations allocate no ASC
workspace. One instance is not concurrently mutable; separate instances and
streams may execute concurrently. The context, operands, and any required
provider state outlive returned events.

### Built-in pointwise evaluator

Expose `CudaEvaluate` returning `Result<CompletionEvent>` for exactly:

- a dense terminal copied into a distinct destination;
- a rank-zero scalar fill;
- one-level `Negate` of a dense terminal; and
- one-level `Add`, `Subtract`, or `Multiply` whose operands are dense terminals
  or rank-zero scalars.

It supports exactly `float` and `double`, compile-time rank zero through eight,
and unique non-negative-stride device views. Nested nodes and arbitrary
external expression adapters return or fail with an explicit unsupported
contract; they are not silently evaluated on the host. Logical traversal is
dimension-zero-fastest and independent of left/right/stride storage.

All rank, shape, placement, device, span, and alias validation completes before
launch. Exact terminal self-evaluation is a no-op event. Every other possible
destination overlap is rejected. Success launches on the explicit stream,
uses no packing, transfer, workspace, allocation, fallback, or hidden
synchronization, and returns an event.

### Dense CUDA algebra

Expose asynchronous `Result<CompletionEvent>` operations:

```text
CudaCopy
CudaScal
CudaAxpy
CudaGemv
CudaGemm
```

The first three support float/double rank-one and rank-two unique
non-negative-stride device views through project-owned kernels.

`CudaGemv` and `CudaGemm` use typed cuBLAS float/double operations and support
`MatrixOperation::{kNone,kTranspose}`. They accept only cuBLAS-compatible
column-major `LayoutLeft`/leading-dimension mappings; row-major and arbitrary
strided matrices return `kUnsupported` before enqueueing. Vector increments
and all cuBLAS dimensions/leading dimensions are checked before narrowing to
the provider integer width. Output overlap with any input is rejected.
`beta == 0` does not require prior output values.

Dot and Nrm2 are deferred because their current host-scalar result contract
would require hidden synchronization or a separately approved asynchronous
scalar-result owner. Factorization, solvers, mixed precision, complex values,
Tensor Core/fast-math modes, batched operations, native handles, external
streams, explicit cuBLAS workspaces, and cuSOLVER are excluded.

## Error, execution, and evidence contract

All public provider failures use existing `Status`/`Result`, stable ASC error
codes, provider name, and signed native code. Diagnostic text is not stable.
No production exceptions are introduced.

Runtime operations are asynchronous unless their contract explicitly says
otherwise (`DenseArray::Clone` is synchronous). The caller keeps contexts,
resources, arrays/views, and provider objects alive until the completion event
has completed. No operation silently waits, changes device, transfers, packs,
allocates workspace, changes precision, or falls back.

Milestone 6 requires all four CUDA evidence levels on the local identified
hardware:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
```

The evidence must record CUDA toolkit/compiler, runtime, cuBLAS, driver,
device, compute capability, architecture code, configuration, operations,
layouts, sizes, tolerances, allocation/copy/event behavior, and skips.

## Required verification

- CPU-only configure/build/test/install with CUDA disabled and no CUDA package
  discovery or imported provider target;
- requested-CUDA failure with a deliberately unavailable toolkit/compiler;
- CUDA 12.9 configure/compile on architecture 86;
- core CUDA device count, three resource spaces, zero/allocation/failure,
  alignment, move-only buffer release, copy directions, exact no-op,
  overlap/size/null/space/device rejection, independent streams, event query,
  event wait, moved event, and lifetime ordering on the real GPU;
- dense uninitialized device owner/view, clone host-to-device and
  device-to-host, failure transactions, and no host dereference;
- pointwise evaluator exact oracles across scalar/terminal/negate/add/subtract/
  multiply, rank zero, zero extent, left/right/padded stride, alias, placement,
  unsupported node/rank, independent streams, and CPU parity;
- Copy/Scal/Axpy/Gemv/Gemm float/double parity, transpose pairs, degenerate
  shapes, leading dimensions, `beta == 0`, NaN/infinity where provider
  behavior is defined, narrowing, layout, alias, and failure-before-enqueue;
- event ordering and no implicit device-wide synchronization;
- public headers alone, exceptions-disabled host compilation, provider SDK
  isolation, multi-TU linkage, and useful negative compilation;
- exact target and installed-package dependency closures;
- build tree, copied build tree, install, relocation, paths with spaces,
  static/shared, provider-only consumers, provider-free consumers against a
  CUDA-enabled install, and subproject consumption;
- ASan/UBSan for applicable host/provider-management code;
- NVIDIA Compute Sanitizer memcheck and racecheck/initcheck/synccheck where
  supported, with each tool result reported separately; and
- a project-owned CPU/GPU benchmark with transfers separated from operations,
  warmup, checksums, compiler/configuration/hardware/provider details, and no
  unstable speed gate.

## Provenance and dependencies

The implementation is clean-room from the frozen contract and current
official CMake, CUDA Runtime, and cuBLAS documentation. MdeCpp/deleted asc-cpp
CUDA source, tests, literal outputs, wrapper names, macros, and prose are not
inspected or copied. No source or data is imported.

CUDA is an external provider dependency, not vendored material. Provider
discovery is not a redistribution or licensing claim. Apache-2.0 project
licensing remains unchanged; installed/package notices must accurately state
that CUDA is optional and separately supplied.

## Explicit exclusions

- No production change to utilities, expression, sparse, random, or either
  random storage facet.
- No sparse CUDA or random CUDA target, header, source, option, package
  component, test claim, or provider discovery.
- No `ASC::cpp` provider edge.
- No cuSOLVER operation, factorization, solver, Dot/Nrm2 asynchronous result,
  reduction, general broadcasting, arbitrary external GPU expression,
  negative stride, runtime-rank owner, managed-memory host dereference,
  native handle/stream adoption, multi-device peer copy, graph capture,
  unified-memory prefetch, pool allocator, or fallback.
- No OpenMP, Eigen, BLAS/LAPACK, oneMKL, TBB, SYCL, HIP, or ROCm dependency.
- No third-party source, test, benchmark framework, or copied MdeCpp/deleted
  asc-cpp material.
- No Milestone 7 or 8 behavior.
- No commit, push, pull request, merge, tag, release, or branch deletion.

## Publication gate

The lead reviews the cumulative diff, reconciles every specialist finding,
runs fresh CPU-only and CUDA compiler/runtime/parity/package matrices, records
exact pass/fail/skip evidence, and stops at Publication Checkpoint B. Remote
writes and history mutation require separate owner approval.
