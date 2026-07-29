# Milestone 7 Contract: Sparse CUDA and Random CUDA Facets

Status: Frozen after owner instruction

Date: 2026-07-28

Owner instruction: continue the approved roadmap with Milestone 7; no
corrections supplied

Exact milestone: **Milestone 7 — sparse CUDA and random CUDA facets**

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

Baseline commit and unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the exact, intentionally uncommitted Milestones 0--6
Publication Checkpoint B candidate is retained in place. Milestone 7 does not
restore, reset, discard, commit, or reclassify that work.

## Purpose

Implement only the four remaining approved CUDA provider facets:

```text
ASC::sparse_cuda
ASC::random_cuda
ASC::random_dense_cuda
ASC::random_sparse_cuda
```

The six provider-free modules, `random_dense`, `random_sparse`, `ASC::cpp`,
`core_cuda`, and `dense_cuda` retain their existing contracts. No provider
edge enters `ASC::cpp`, and provider-free configuration or package lookup
must not discover CUDA.

The unreleased package candidate advances to 0.7.0. Milestone 8 hardening,
additional sparse operations/distributions/providers, publication, and
release work are excluded.

## Authorities and clean-room boundary

The approved architecture blueprint, ADRs 0001--0018, dependency/capability
manifests, backend matrix, roadmap, and Milestone 6 checkpoint govern this
contract. ADRs 0008, 0012, 0014, 0015, 0017, and 0018 are especially
controlling.

No MdeCpp or deleted asc-cpp implementation, test, literal vector, table, or
generated data may be copied or mechanically translated. Project CUDA kernels
are original work. CUDA Runtime and cuSPARSE are the only approved provider
libraries. cuRAND, Thrust/CUB as an API dependency, cuSOLVER, and any new
third-party package are forbidden.

## Targets and direct dependency graph

```text
asc_sparse_cuda / ASC::sparse_cuda
  direct ASC:     ASC::sparse;ASC::core_cuda
  private system: CUDA::cusparse

asc_random_cuda / ASC::random_cuda
  direct ASC:     ASC::random;ASC::core_cuda
  private system: CUDA Runtime through ASC::core_cuda

asc_random_dense_cuda / ASC::random_dense_cuda
  direct ASC:     ASC::random_dense;ASC::random_cuda;ASC::core_cuda
  private system: CUDA Runtime through ASC::core_cuda

asc_random_sparse_cuda / ASC::random_sparse_cuda
  direct ASC:     ASC::random_sparse;ASC::random_cuda;ASC::core_cuda
  private system: CUDA Runtime through ASC::core_cuda
```

All four targets exist only when the existing default-off
`ASC_CPP_ENABLE_CUDA=ON` option is enabled. CUDA architecture remains
caller-owned. Installed CUDAToolkit discovery runs only for a requested CUDA
closure. No M7 target links `ASC::cpp`, `ASC::dense_cuda`, or an unlisted
sibling/provider.

## Sparse CUDA API and behavior

The only new public sparse provider headers are:

```text
include/asc/sparse/providers/cuda.h
include/asc/sparse/providers/cuda_export.h
```

The public surface is bounded to:

```text
SparseCudaContext::Create(ExecutionContext)
    -> Result<SparseCudaContext>

SparseCudaContext::execution_context()
    -> const ExecutionContext& noexcept

CudaCloneCsr(context, canonical host CSR view, device resource)
    -> Result<CudaCsrClone<Element>>

CudaCsrSpmv(context, alpha, device CSR view,
            provider-neutral strided device input,
            beta, provider-neutral strided device output)
    -> Result<CompletionEvent>

CudaEvaluate(context, device coordinate/CSR/CSC destination,
             bounded structure-preserving expression)
    -> Result<CompletionEvent>
```

A provider-neutral strided vector descriptor may live in the sparse provider
header. It contains no SDK type, supports float/double and checked signed
64-bit extent/stride metadata, and accepts non-ASC storage. Sparse must never
include or link Dense.

`CudaCloneCsr` is the sole explicit host-to-device CSR staging path. It accepts
canonical host CSR, allocates exactly the CSR owner buffers from the caller's
device resource, and enqueues explicit copies on the supplied context. It
does not convert formats. The returned owner/result must make completion and
lifetime requirements explicit.

The selected cuSPARSE operation is deterministic float/double zero-based CSR
SpMV:

```text
y = alpha * A * x + beta * y
```

ASC offsets/indices remain signed 64-bit. Unit-stride input/output uses the
approved deterministic `CUSPARSE_SPMV_CSR_ALG2` path. A positive nonunit-
stride path may use an original project kernel with zero workspace. Provider
index/dimension/algorithm/workspace requirements are validated before
enqueue. Workspace is explicit and observable; no undocumented allocation is
allowed. `beta == 0` does not read prior output.

The bounded evaluator supports float/double values and existing canonical
coordinate, CSR, or CSC structure. It may implement only same-structure
terminal copy, negate, add, subtract, and multiply with terminals or rank-zero
scalars when the sparsity effect remains structure-preserving. General lookup,
union/intersection, densification, format conversion, arbitrary nesting, and
structure mutation are rejected.

Every sparse operation validates backend/device, placement, span, overlap,
shape, format, canonical structure, index width, algorithm, narrowing, and
workspace before mutation. It never hides transfer, conversion, packing,
densification, synchronization, precision change, allocation, or fallback.

## Random CUDA public surface and behavior

The only new public random provider headers are:

```text
include/asc/random/providers/cuda.h
include/asc/random/providers/cuda_export.h
include/asc/random/providers/dense_cuda.h
include/asc/random/providers/dense_cuda_export.h
include/asc/random/providers/sparse_cuda.h
include/asc/random/providers/sparse_cuda_export.h
```

### Raw words

`ASC::random_cuda` supplies one asynchronous project-kernel operation that
fills caller-provided device storage with Philox4x32-10 words from explicit
`RandomStream`, `RandomSubsequence`, and `RandomOffset`. Success returns a
move-only completion result and the first unused offset.

The destination is the existing Core `MutableMemoryView` plus an explicit
word count. The implementation validates the requested byte count against the
view's declared byte capacity before enqueue. As with every non-owning view,
the caller must describe external storage truthfully; no allocation registry
or CUDA Driver API dependency is introduced.

For every logical position `i`, the result is bit-identical to:

```text
Philox4x32Word(stream, subsequence, offset + i)
```

Validate placement, device, span, alignment, count, offset overflow, and
context compatibility before enqueue. Zero count is an exact no-op. There is
no cuRAND, entropy, default state, mutable pool, transfer, workspace, or
fallback.

Owner amendment approved 2026-07-28: this capacity-bearing signature resolves
the previously ambiguous raw destination-span contract. A pointer-plus-count
signature without declared byte capacity is not approved.

### Dense Uniform01

`ASC::random_dense_cuda` asynchronously fills a caller-provided unique device
`DenseView` with exactly unqualified float/double Uniform01 values and returns
completion plus the first unused offset.

Ranks zero through eight, zero extents, `LayoutLeft`, `LayoutRight`, and valid
unique padded nonnegative strides are supported. Logical dimension zero varies
fastest. Float consumes one word and double consumes two adjacent words per
logical coordinate. Results are bit-identical to the provider-free contract,
independent of layout and launch partition.

### Sparse exact-count generation

`ASC::random_sparse_cuda` asynchronously generates exact-count float/double
coordinate storage for compile-time rank. The move-only result owns the
canonical coordinate/value buffers, completion event, and first unused
structure/value offsets. The caller supplies the device resource.

Candidate priorities, tie breaking, word consumption, separate structure and
value domains, canonical order, and value assignment are bit-identical to
`GenerateSparseUniform01`. Rank zero, zero extent, empty, and full-count cases
are included. Only the canonical coordinate/value output allocations are
allowed; hidden computational workspace is forbidden. A bounded low-workspace
algorithm is allowed only with documented complexity/performance limits.

Density/Bernoulli generation, compressed output, Sobol, direction tables,
normal/rejection/variable-consumption distributions, entropy, serialization,
and default random state are excluded.

## Errors, ownership, and asynchronous lifetime

Public failures use existing `Status`/`Result`, stable ASC codes, provider
name, and signed native code. No production exception is introduced.

Provider contexts and result owners are move-only where they own mutable
state/storage. Execution contexts are immutable shared-state owners and
completion events are move-only. Event destruction does not synchronize the
device.

All runtime operations are asynchronous except a staging helper whose name and
documentation explicitly state any wait. Callers keep contexts, resources,
owners, views/descriptors, and referenced bytes alive through completion. No
operation silently changes CUDA device or stream.

## Required validation and evidence

- clean CPU-only static/shared configure, build, test, install, and consume
  with no CUDA discovery or M7 target;
- intentionally unavailable CUDA compiler/toolkit/component failure;
- CUDA configure/compile for the explicit local architecture;
- strict C++20 public headers, exceptions-disabled, multi-TU ODR, ownership,
  and positive/negative compile contracts;
- exact direct targets, installed closures, SDK isolation, and dependency
  manifest checks;
- sparse clone, CSR SpMV, bounded evaluator, validation, lifetime,
  concurrency, workspace/allocation, and independent CPU parity;
- raw-word, dense-fill, and sparse-generation exact CPU/GPU bit parity,
  offsets/overflow, rank/layout/stride/count/domain, failure, allocation, and
  concurrency coverage;
- build-tree, installed, relocated/path-with-spaces, static/shared, component,
  unavailable-component, registry-disabled, and isolated consumers;
- ASan+UBSan for provider-free and host-testable validation paths plus separate
  CUDA runtime tools where available;
- performance smoke with warmups, event-based completion, checksums, exact
  environment, observable allocations/workspace, and no unsupported speedup
  claim.

GPU evidence uses exactly:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

Each final facet is classified independently. No label is inferred from
another, and unavailable environments/hardware are `skipped`, never passed.

## Explicit exclusions

- SpMM, sparse triangular solve, preconditioners, solvers, BSR/SELL, arbitrary
  sparse conversion, or general sparse GPU evaluation;
- any distribution beyond raw Philox4x32-10 and Uniform01 float/double;
- hidden transfer, packing, conversion, densification, allocation,
  synchronization, narrowing, precision change, or fallback;
- OpenMP/Eigen/MKL/BLAS/LAPACK CPU facets, HIP/ROCm/SYCL, or another GPU
  provider;
- Milestone 8 hardening or downstream work;
- commit, push, pull request, merge, tag, release, or branch deletion.

Milestone 7 stops at Publication Checkpoint B with the complete local diff,
clean validation, separate reviews, exact evidence labels/skips, remaining
risks, and proposed—but unexecuted—publication and cleanup actions.
