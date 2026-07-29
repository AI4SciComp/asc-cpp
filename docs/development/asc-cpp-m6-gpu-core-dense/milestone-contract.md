# Milestone 6 Contract: GPU Core and Dense

Status: Frozen after owner instruction

Date: 2026-07-28

Owner instruction: continue the approved roadmap with Milestone 6; no
corrections supplied

Exact milestone: **Milestone 6 — GPU core and dense**

Branch: `feature/asc-cpp-m6-gpu-core-dense`

Baseline commit and unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the intentionally uncommitted Milestones 0--5 Publication
Checkpoint B candidate and the owner's earlier deletion of the retired
implementation are retained in place. Milestone 6 does not restore, reset,
discard, commit, or reclassify that work.

## Purpose

Implement exactly the first two approved optional CUDA provider facets:

```text
ASC::core_cuda  -> ASC::core
ASC::dense_cuda -> ASC::dense;ASC::core_cuda
```

`core_cuda` owns CUDA runtime device discovery, allocation, copies, one stream
per context, completion events, and CUDA runtime error translation.
`dense_cuda` owns a CUDA Dense provider context, a bounded built-in
pointwise evaluator, project-owned Copy/Scal/Axpy kernels, and selected
asynchronous float/double cuBLAS operations.

The six base modules, both Random storage facets, and `ASC::cpp` retain their
provider-free graphs. CPU-only configuration must not discover CUDA, enable
the CUDA language, compile CUDA, import a provider target, or require a CUDA
runtime library.

## Product targets, dependencies, and version

```text
build target:        asc_core_cuda
consumer target:     ASC::core_cuda
kind:                compiled C++20 provider facet
direct ASC link:     ASC::core
private external:    CUDA::cudart

build target:        asc_dense_cuda
consumer target:     ASC::dense_cuda
kind:                compiled C++20/CUDA provider facet
direct ASC links:    ASC::dense;ASC::core_cuda
private external:    CUDA::cublas

release line:        unreleased 0.6.0 candidate
```

The actual CMake `FindCUDAToolkit` targets are `CUDA::cudart` and
`CUDA::cublas`. No invented CUDAToolkit target name, cuSOLVER, cuSPARSE,
cuRAND, or other provider link is approved.

## Configuration and package contract

Add exactly one provider option, default off:

```text
ASC_CPP_ENABLE_CUDA=OFF
```

When enabled:

- require `CUDAToolkit` 12 or newer with standard CMake;
- enable the CUDA language because Dense owns compiled kernels;
- honor caller-provided `CMAKE_CUDA_ARCHITECTURES`;
- select the shared CUDA Runtime for every CUDA-language target so all facets
  observe one Runtime state;
- fail if the toolkit, compiler, runtime target, or cuBLAS target is absent;
- build/export both facets; and
- apply target-local C++20, warnings, and applicable sanitizer policy using
  only the released ASCCMake 0.1.0 APIs.

Known component names become:

```text
core utilities expression dense sparse random
random_dense random_sparse cpp core_cuda dense_cuda
```

When CUDA is disabled, only the nine provider-free components are available.
When enabled, both CUDA components are also available. Required closures are:

```text
core_cuda  -> core;core_cuda
dense_cuda -> core;expression;dense;core_cuda;dense_cuda
```

The installed config executes `find_dependency(CUDAToolkit 12)` only when the
requested closure contains a CUDA component. Provider-free and no-component
requests never discover CUDA. `ASC::cpp` gains no provider edge.

## Provider-neutral Core evolution

`ExecutionContext` and `CompletionEvent` may acquire private opaque state, but
provider-neutral headers contain no CUDA declaration or SDK type.

Preserve:

- `ExecutionContext::Serial()` and serial `CopyBytes`;
- provider-free `ExecutionContext::Create(...)` validation;
- copyable immutable contexts;
- move-only events;
- failure before pointer access when the context cannot access a space; and
- no automatic transfer, synchronization, provider selection, or fallback.

Add:

```text
CompletionEvent::Query() -> Result<bool>
```

`Wait()` waits only for that event. Event destruction does not synchronize the
device. Event state may retain stream/execution state, never user arrays.

CUDA `CopyBytes` validates size, nullability, spaces, device, and overlap
before enqueue. It uses the explicit context stream and returns a completion
event. Exact self-copy is a no-op event; partial overlap is rejected. No
device-wide synchronization is permitted.

## Public Core CUDA API

Public files:

```text
include/asc/core/providers/cuda.h
include/asc/core/providers/cuda_export.h
```

Exact public surface:

```text
CudaDeviceCount() -> Result<std::int32_t>

CudaMemoryResource::Create(device, memory_space)
    -> Result<std::unique_ptr<CudaMemoryResource>>

CreateCudaExecutionContext(
    device, determinism = Determinism::kDeterministic)
    -> Result<ExecutionContext>

RecordCudaEvent(context) -> Result<CompletionEvent>
```

No CUDA SDK type appears in a public signature.

`CudaMemoryResource` is noncopyable and nonmovable so every allocated
`Buffer` may retain a stable resource address. It accepts exactly pinned host,
device, or managed space. Ordinary host allocation remains
`HostMemoryResource` work. It validates device/alignment/size, translates
provider/native errors, returns null for zero bytes, and deallocates once with
the matching CUDA operation.

`CreateCudaExecutionContext` validates the device and creates one owned
nonblocking CUDA stream. There is no default device, global current context,
provider registry, native-stream adoption, or mutable ASC global state.
Provider calls restore any temporarily changed CUDA current device.

## Dense device storage

Add:

```text
DenseArray::CreateUninitialized(extents, resource, layout)
    -> Result<DenseArray>
```

It accepts every valid Core memory space and allocates the exact unique,
exhaustive owner span without touching elements. Existing `Create` remains
host-only and value-initializing. Device/managed views are descriptors and do
not become host-dereferenceable through `At`; pinned host remains explicitly
host-accessible.

`Clone(destination_resource, context)` remains a synchronous named deep copy:
allocate, enqueue `CopyBytes`, wait for its event, then publish. Any failure
publishes no owner. No implicit copy occurs in construction, view creation,
resize, evaluation, or algebra.

## Public Dense CUDA API

Public files:

```text
include/asc/dense/providers/cuda.h
include/asc/dense/providers/cuda_export.h
```

Provider context:

```text
DenseCudaContext::Create(ExecutionContext)
    -> Result<DenseCudaContext>

execution_context() -> const ExecutionContext&
```

It is move-only, requires a CUDA context, owns one cuBLAS handle bound to that
stream, uses host scalar pointer mode, disallows atomic reduction algorithms,
and uses pedantic/default-precision math for deterministic requests. One
instance is not concurrently mutable; independent instances/streams may run
concurrently. Operations allocate no ASC workspace.

### Built-in pointwise evaluator

`CudaEvaluate` returns `Result<CompletionEvent>` for exactly:

- a Dense terminal copied into a distinct destination;
- a rank-zero scalar fill;
- one-level `Negate` of a Dense terminal; and
- one-level `Add`, `Subtract`, or `Multiply` with Dense-terminal or rank-zero
  scalar operands.

Supported elements are exactly unqualified `float` and `double`, ranks zero
through eight, and unique nonnegative-stride device views. Nested nodes and
arbitrary external adapters are unsupported, not host-evaluated.

Validate rank, shape, placement, device, span, and alias before launch. Exact
terminal self-evaluation is a no-op event; other destination overlap fails.
Logical order is dimension-zero-fastest. No packing, transfer, workspace,
allocation, fallback, or hidden wait occurs.

### Dense CUDA algebra

Asynchronous `Result<CompletionEvent>` operations:

```text
CudaCopy
CudaScal
CudaAxpy
CudaGemv
CudaGemm
```

Copy/Scal/Axpy support float/double rank-one and rank-two unique
nonnegative-stride device views through project-owned kernels.

Gemv/Gemm use typed float/double cuBLAS operations and
`MatrixOperation::{kNone,kTranspose}`. Matrices must be cuBLAS-compatible
column-major LayoutLeft/leading-dimension mappings. LayoutRight and arbitrary
strided matrices fail unsupported before enqueue. All dimensions, leading
dimensions, and vector increments are checked before narrowing to provider
integers. Output overlap with any input is rejected. `beta == 0` does not
require initialized destination values.

Dot/Nrm2 are deferred because their host-scalar result contract would require
hidden synchronization or a new asynchronous scalar owner.

## Execution, error, and evidence contract

Provider failures use existing `Status`/`Result`, stable ASC codes, provider
name, and signed native code. No production exception API is added.

Operations are asynchronous unless explicitly stated otherwise (`Clone` is
synchronous). Contexts, resources, views/owners, provider objects, and caller
storage outlive completion. No operation silently waits, changes devices,
transfers, packs, allocates workspace, changes precision, or falls back.

Milestone 6 requires these exact evidence labels:

```text
core_cuda:  configure-tested, compile-tested, runtime-tested
dense_cuda: configure-tested, compile-tested, runtime-tested, parity-tested
```

Evidence records toolkit/compiler/runtime/cuBLAS, driver, device, compute
capability, architecture code, build mode, operations, layouts, sizes,
tolerances, allocation/copy/event behavior, and every skip.

## Required verification

- CUDA-disabled CPU configure/build/test/install with no CUDA discovery,
  provider target, or runtime link;
- intentional CUDA-request failure with unavailable compiler/toolkit;
- CUDA 12.9 configure/compile for architecture 86;
- real-hardware device/resource/copy/stream/event/lifetime/error coverage;
- uninitialized device owner, host/device clones, failure transactions, and
  no host dereference;
- exact bounded evaluator oracles across rank zero, zero extent, left/right/
  padded stride, aliases, unsupported nodes/ranks, independent streams, and
  CPU parity;
- float/double Copy/Scal/Axpy/Gemv/Gemm parity, transpose/degenerate/
  leading-dimension/beta-zero/narrowing/layout/alias cases;
- proof of no hidden device-wide synchronization;
- strict provider-neutral and provider header/ODR/negative compile contracts;
- exact build/install imported-target and package closures;
- copied-build-tree, install, relocation/path-with-spaces, static/shared,
  provider-only, provider-free-against-enabled-package, and subproject use;
- applicable ASan/UBSan host/provider-management coverage;
- Compute Sanitizer memcheck, racecheck, initcheck, and synccheck, each
  classified separately; and
- a project-owned benchmark separating transfers from operations, with
  warmup/checksums/hardware details and no unstable speed gate.

## Provenance

Implementation/tests/docs are project-owned clean-room work from this
contract, current project APIs, and official CMake/CUDA Runtime/cuBLAS
documentation. MdeCpp/deleted asc-cpp CUDA source, tests, literals, names,
macros, and prose are not inspected or copied. No third-party source, data, or
benchmark framework is imported.

## Explicit exclusions

- No production change to Utilities, Expression, Sparse, Random, or either
  Random storage facet.
- No Sparse CUDA or Random CUDA target/header/source/component/test claim.
- No provider edge in `ASC::cpp`.
- No cuSOLVER, cuSPARSE, cuRAND, Dot, Nrm2, reduction, factorization, solver,
  mixed precision, complex, Tensor Core/fast-math, batch, native handle/
  stream adoption, explicit cuBLAS workspace, general broadcasting, arbitrary
  external GPU expression, negative stride, runtime-rank owner, managed-memory
  host dereference, peer copy, graph capture, prefetch, pool allocator, or
  fallback.
- No OpenMP, Eigen, BLAS/LAPACK, oneMKL, TBB, SYCL, HIP, or ROCm dependency.
- No third-party or MdeCpp/deleted asc-cpp source/test material.
- No Milestone 7 or 8 behavior.
- No commit, push, pull request, merge, tag, release, or branch deletion.

## Publication gate

The lead reviews the cumulative diff, resolves every specialist finding, runs
fresh CPU-only and CUDA compiler/runtime/parity/package matrices, records
exact pass/fail/skip evidence, and stops at Publication Checkpoint B.
