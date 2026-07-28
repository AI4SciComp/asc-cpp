# Milestone 6 Production Self-Review

Status: Complete; no open production blocker

Date: 2026-07-28

Scope: frozen Milestone 6 Core CUDA and Dense CUDA production ledger only

## Implemented contract

Core now keeps `ExecutionContext` and `CompletionEvent` provider-neutral while
their private state supports compiled providers. The CUDA Core facet provides:

- CUDA device enumeration;
- stable, noncopyable resources for pinned-host, device, and managed memory;
- one owned nonblocking stream per immutable copyable execution context;
- checked asynchronous copies on that stream;
- move-only queryable completion events that retain execution state but not
  caller arrays; and
- stable ASC statuses with `cuda` provider/native error detail.

CUDA copies validate sizes, nullability, declared spaces, pointer attributes,
device identity, and overlap before enqueue. Exact self-copy returns a
stream-ordered no-op event. Partial overlap is rejected. `Wait()` synchronizes
only its event; production contains no device-wide or stream-wide
synchronization call.

Dense now provides extents-first
`CreateUninitialized(const ExtentsType&, MemoryResource&, layout)` for every
valid Core memory space. Existing `Create` remains host-only and
value-initializing. `Clone` allocates an unpublished destination, enqueues an
explicit copy, waits for that event, and publishes only on success. Pinned
host views are directly host-accessible; device and managed views remain
non-dereferenceable through `At`.

The CUDA Dense facet provides a move-only context with one stream-bound cuBLAS
handle, host scalar pointer mode, atomics disabled, and pedantic math for
deterministic execution. Its bounded evaluator accepts only the approved
terminal, scalar, one-level negate, and one-level binary shapes; it supports
exactly unqualified float/double destinations and ranks zero through eight.
Project-owned grid-stride kernels implement logical dimension-zero-fastest
evaluation plus Copy, Scal, and Axpy for rank-one/rank-two views.

Typed float/double cuBLAS Gemv and Gemm accept only checked column-major
leading-dimension mappings and `MatrixOperation::{kNone,kTranspose}`. All
provider integer narrowing occurs after complete shape/layout/alias
validation. Degenerate inner dimensions are handled asynchronously with
project-owned fill/scale kernels so `beta == 0` never reads uninitialized
output.

## Files

Public/evolved Core:

- `include/asc/core/execution.h`
- `include/asc/core/providers/cuda.h`
- `include/asc/core/providers/cuda_export.h`
- `src/core/execution.cc`
- `src/core/execution_internal.h`
- `src/core/cuda/cuda_internal.h`
- `src/core/cuda/runtime.cc`

Public/evolved Dense:

- `include/asc/dense/array.h`
- `include/asc/dense/view.h`
- `include/asc/dense/providers/cuda.h`
- `include/asc/dense/providers/cuda_export.h`
- `src/dense/cuda/context.cc`
- `src/dense/cuda/context_internal.h`
- `src/dense/cuda/kernels.cu`
- `src/dense/cuda/kernels_internal.h`
- `src/dense/cuda/operations.cc`
- `src/dense/cuda/validation_internal.h`

No production Utilities, Expression, Sparse, Random, M7, or M8 file changed.

## Review findings and resolutions

1. The initial `CreateUninitialized` draft followed the older resource-first
   factory convention. It was changed to the frozen extents-first API after
   owner review.
2. `DenseView::At` originally admitted only ordinary host memory. The lead
   amended the production ledger to include `view.h`; pinned host is now
   explicitly host-accessible while serial expression execution keeps its
   existing ordinary-host policy.
3. The first `CudaEvaluate` constraint admitted cv-qualified destinations
   during descriptor selection. It now participates only for exactly
   unqualified float/double destinations; volatile and other element types are
   compile-time negatives.
4. An initial conditional-expression `std::initializer_list` in erased
   evaluator overlap validation could leave a dangling backing array. It was
   replaced with explicit left/right validation. Fresh optimized consumers
   and the complete evaluator suite pass.
5. The first shared build exposed hidden private state-constructor symbols
   used by the Core provider bridge. The two private constructors now carry
   `ASC_CORE_EXPORT`; clean shared libraries and an optimized shared consumer
   link and run.
6. Exact CUDA self-copy initially recorded an event before native pointer and
   device validation. Validation now completes first.
7. Device Dense descriptors now reject misaligned nonempty pointers before
   enqueue.
8. Portability review found that default Dense context move-assignment would
   replace the old execution context before destroying its stream-bound cuBLAS
   handle. Custom move-assignment now destroys the old handle state first,
   then replaces the execution context and adopts the new handle state, with a
   self-assignment guard.
9. Degenerate LayoutRight matrices can share strides with a column-major
   mapping. The erased descriptor now retains layout kind so every
   LayoutRight matrix is rejected as required, while LayoutLeft and compatible
   padded leading-dimension mappings remain supported.

No unresolved API, ownership, provider isolation, lifetime, or numerical
finding remains in production scope.

## Production validation

The following production-owned checks passed:

```text
CUDA 12.9.86 / nvcc 12.9.86 / architecture 86
GCC 11.4 Debug static configure and build: pass
GCC 11.4 Release static configure and build: pass
GCC 11.4 Release shared configure and build: pass
shared libasc_core_cuda/libasc_dense_cuda optimized consumer: pass
Core CUDA runtime/validation/native-state verifier executables: 3/3 pass
Dense owner/evaluator/linalg/concurrency/move/Release-regression: 6/6 pass
focused registered M6 compile/runtime tests: 13/13 pass
optimized Release evaluator and linalg executables: 2/2 pass
project-owned CUDA benchmark: pass (observation only, no speed gate)
provider header self-containment and no-exceptions syntax: pass
provider multi-TU/ODR executable: pass
copy/move/volatile negative compile contracts: 4/4 expected failure
clang-format-19 dry run: pass
git diff --check for production scope: pass
forbidden provider/dependency/synchronization source scan: pass
```

The runtime tests used the available CUDA device and covered resource
allocation/deallocation, current-device restoration, host/device/managed
copies, event query/wait/move/lifetime, owner cloning, ranks zero through
eight, zero extents, padded and right layouts, exact/partial aliases,
unsupported expressions, independent streams, Copy/Scal/Axpy, all Gemm
transpose pairs, Gemv transpose, `beta == 0`, degenerate inner dimensions,
float/double parity, and allocation-free operations.

Final evidence classification and whole-repository/package/sanitizer/Compute
Sanitizer matrices remain lead-owned Publication Checkpoint B work.

## Exclusions and provenance

No provider registry, default device/context, native-handle adoption,
workspace allocation, packing, transfer, fallback, Dot/Nrm2, reduction,
solver, sparse/random CUDA, mixed precision, complex, Tensor Core/fast-math,
batching, or later-milestone behavior was added. Only CUDA Runtime and cuBLAS
interfaces approved by the frozen contract are used. No MdeCpp/deleted
asc-cpp or third-party source, test, literal, macro, data, or prose was
inspected or imported.
