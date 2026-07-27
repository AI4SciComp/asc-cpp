# Milestone 7 Contract — sparse CUDA and random CUDA facets

Status: Frozen

Date: 2026-07-27

Owner approval: Milestone 7, no corrections

Branch: `feature/asc-cpp-m7-gpu-sparse-random`

Baseline: cumulative, unpublished Milestones 0--6 working tree at unchanged
`HEAD` `33b261ea33616a6395c4ad3b20646093103344f7`

## Purpose

Milestone 7 implements only the four CUDA facets already named by the approved
architecture:

```text
ASC::sparse_cuda
ASC::random_cuda
ASC::random_dense_cuda
ASC::random_sparse_cuda
```

It preserves the six provider-free modules, the two provider-free random
storage facets, `ASC::cpp`, and the Milestone 6 `core_cuda` and `dense_cuda`
facets. It does not add a module, add a provider to `ASC::cpp`, or change a
provider-free package request into a CUDA request.

This contract freezes the complete implementation boundary. Later hardening,
additional sparse operations, additional distributions, other GPU backends,
and release work are excluded.

## Accepted architecture

This milestone is governed by the approved six-module blueprint and ADRs
0001--0018, especially:

- ADR 0008: explicit memory, contexts, events, and no fallback;
- ADR 0012: canonical coordinate/CSR/CSC invariants;
- ADR 0014: sparse-owned algebra with storage-neutral vector operands;
- ADR 0015: counter-based reproducibility and separately consumable random
  storage facets;
- ADR 0017: clean-room implementation and third-party provenance; and
- ADR 0018: provider evidence and the `0.7.x` release boundary.

No MdeCpp or deleted asc-cpp source, test, literal vector, or table may be
copied or mechanically translated. CUDA project kernels are original
implementations. CUDA Runtime and cuSPARSE are the only approved external
provider dependencies; cuRAND, Thrust/CUB as an API dependency, cuSOLVER, and
new third-party packages are not approved.

## Frozen targets and dependency closures

| Build/export target | Kind | Exact direct ASC dependencies | External implementation dependency |
| --- | --- | --- | --- |
| `asc_sparse_cuda` / `ASC::sparse_cuda` | compiled sparse provider facet | `ASC::sparse`, `ASC::core_cuda` | private `CUDA::cusparse` |
| `asc_random_cuda` / `ASC::random_cuda` | compiled random provider facet | `ASC::random`, `ASC::core_cuda` | private CUDA Runtime through `core_cuda` |
| `asc_random_dense_cuda` / `ASC::random_dense_cuda` | compiled random/dense integration facet | `ASC::random_dense`, `ASC::random_cuda`, `ASC::core_cuda` | private CUDA Runtime through `core_cuda` |
| `asc_random_sparse_cuda` / `ASC::random_sparse_cuda` | compiled random/sparse integration facet | `ASC::random_sparse`, `ASC::random_cuda`, `ASC::core_cuda` | private CUDA Runtime through `core_cuda` |

All four targets are available only when `ASC_CPP_ENABLE_CUDA=ON`. The existing
option remains default off. CUDA language and toolkit discovery remain absent
when CUDA is disabled. Installed package lookup discovers CUDAToolkit only
when a requested component closure contains a CUDA facet.

`ASC::cpp` remains provider-free. No M7 facet links `ASC::dense_cuda` or a
sibling facet outside the exact closure above.

## Frozen sparse CUDA surface

`<asc/sparse/providers/cuda.h>` and
`<asc/sparse/providers/cuda_export.h>` are the only new public sparse provider
headers. Common sparse headers contain no CUDA or cuSPARSE SDK type.

The public surface contains:

```text
SparseCudaContext::Create(ExecutionContext)
    -> Result<SparseCudaContext>

SparseCudaContext::execution_context()
    -> const ExecutionContext& noexcept

CudaCloneCsr(SparseCudaContext, host CSR view, device resource)
    -> Result<CudaCsrClone<Element>>

CudaCsrSpmv(SparseCudaContext, alpha, device CSR view,
            provider-neutral strided device input,
            beta, provider-neutral strided device output)
    -> Result<CompletionEvent>

CudaEvaluate(SparseCudaContext, device coordinate/CSR/CSC destination,
             bounded structure-preserving expression)
    -> Result<CompletionEvent>
```

Exact names may use the repository's established `Cuda*` naming order, but no
additional capability may be introduced. Any provider-neutral strided vector
descriptor lives in the sparse CUDA header, contains no SDK type, supports
float/double, signed-64 extent/stride metadata, and can be constructed from
non-ASC storage. Sparse never includes or links dense.

`CudaCloneCsr` is the named, explicit staging path. It allocates exactly the
CSR owner buffers from the caller's device memory resource, enqueues explicit
host-to-device copies on the supplied context, and returns the owner plus its
completion event. The caller keeps source spans, destination resource, owner,
and context alive through completion. It accepts only canonical host CSR and
does not convert formats.

The selected cuSPARSE operation is deterministic float/double CSR SpMV:

```text
y = alpha * A * x + beta * y
```

Supported inputs are canonical zero-based CSR with signed 64-bit ASC offsets
and indices, device memory, nonnegative positive vector stride, and a
cuSPARSE algorithm whose exact name is recorded in evidence. Index,
dimension, and workspace requirements are checked before enqueue. Any required
workspace is explicit and caller-owned or retained by the returned completion
state; its allocation and size must be observable and documented. `beta == 0`
does not require initialized output values.

The bounded evaluator supports only float/double stored values and existing
canonical coordinate, CSR, or CSC structure. It may implement exact-structure
copy, negate, add, subtract, and multiply with same-structure terminals and
rank-zero scalars where the expression sparsity effect remains
structure-preserving. General sparse lookup, union/intersection, densification,
format conversion, arbitrary nesting, and structure mutation are rejected.

Every sparse operation validates backend/device, memory space, shape, format,
index width, address spans, overlap, supported expression/algorithm, provider
narrowing, and workspace before destination mutation. It never silently
transfers, converts, packs, densifies, synchronizes, changes precision,
allocates undocumented workspace, or falls back.

## Frozen random CUDA surface

The public provider headers are:

```text
<asc/random/providers/cuda.h>
<asc/random/providers/cuda_export.h>
<asc/random/providers/dense_cuda.h>
<asc/random/providers/dense_cuda_export.h>
<asc/random/providers/sparse_cuda.h>
<asc/random/providers/sparse_cuda_export.h>
```

Common random, dense, and sparse headers contain no CUDA SDK type.

### Raw-bit facet

`ASC::random_cuda` exposes one asynchronous project-kernel operation that fills
caller-provided device storage with Philox4x32-10 words from explicit
`RandomStream`, `RandomSubsequence`, and `RandomOffset`. Success returns a
move-only result containing the completion event and first unused offset.

The word at logical position `i` is bit-for-bit identical to:

```text
Philox4x32Word(stream, subsequence, offset + i)
```

Validation covers device placement, byte/type alignment, address span, count,
offset overflow, context/device compatibility, and exact no-op behavior before
enqueue. No cuRAND state, entropy, default engine, mutable pool, transfer,
workspace, or fallback exists.

### Dense random facet

`ASC::random_dense_cuda` exposes asynchronous float/double Uniform01 fill of a
caller-provided unique device `DenseView`, returning a completion event and
first unused offset. Rank zero through eight, zero extents, `LayoutLeft`,
`LayoutRight`, and valid unique padded nonnegative strides are supported.

Logical dimension zero varies fastest exactly as in Milestone 5. Float consumes
one word and double consumes two adjacent words per logical coordinate.
Results are bit-for-bit equal to the provider-free Philox and Uniform01
contracts independent of physical layout and launch partition.

### Sparse random facet

`ASC::random_sparse_cuda` exposes asynchronous exact-count float/double
coordinate generation for compile-time rank. It returns a move-only generated
coordinate owner, completion event, and first unused structure/value offsets.
The caller supplies the device memory resource; result allocation is explicit
and limited to the canonical coordinate and value buffers. No hidden
computational workspace is permitted.

Candidate priorities, tie breaking, word consumption, independent
structure/value domains, canonical coordinate order, and value assignment are
bit-for-bit identical to `GenerateSparseUniform01`. Rank zero, zero extent,
empty, and full-count cases are included. The implementation may use a bounded
low-workspace algorithm; its complexity and performance limitations must be
documented. Density/Bernoulli generation and variable-consumption
distributions remain excluded.

All random validation is transactional: invalid count/shape/domain/placement,
offset overflow, allocation failure, provider failure, or unsupported rank is
reported before a usable result is returned. The caller keeps the context,
resource, result storage, and referenced input state alive through completion.

## Error, ownership, and asynchronous contract

All public production failures use existing `Status`/`Result`, stable ASC error
codes, provider name, and signed native code. Diagnostic text is not stable.
No production exception is introduced.

Provider contexts and generated owners are move-only where they own mutable
provider state or storage. Execution contexts are immutable shared-state
owners. Completion events are move-only. Event destruction does not
device-wide synchronize.

M7 runtime operations are asynchronous except an explicitly documented staging
helper whose name says it clones and whose contract states any wait. The caller
keeps contexts, resources, arrays/views, descriptors, and user storage alive
until completion. No operation silently changes CUDA device or stream.

## Required verification

- clean CPU-only static and shared configure/build/test/install with CUDA
  disabled, no CUDA discovery, and no M7 imported target;
- requested-CUDA failure with an intentionally unavailable compiler/toolkit;
- CUDA configure and compile for the explicit local architecture;
- public-header self-containment, C++20, exceptions-disabled, multi-TU/ODR,
  move/copy ownership, positive and negative compile contracts;
- exact direct-target, component-closure, SDK-isolation, and dependency
  manifest checks;
- sparse device clone, canonical CSR, empty/rectangular/padded vectors,
  float/double SpMV, `alpha`/`beta`, `beta == 0`, invalid metadata, overlap,
  width limits, provider failure, event/lifetime, concurrency, and independent
  CPU reference parity;
- bounded sparse evaluator structure/shape/effect/alias/type/format rejection
  and numerical parity for every supported operation;
- raw Philox independent vectors, offsets/overflow, zero count, alignment,
  placement, launch partition, concurrency, and CPU bit parity;
- dense random rank/layout/stride/zero/overflow/alias and CPU bit parity;
- sparse random rank-zero/zero/full/exact-count, uniqueness, canonical order,
  stream independence, allocation failure, offset consumption, and CPU bit
  parity;
- build-tree, installed, relocated, path-with-spaces, static/shared, component,
  unavailable-component, registry-disabled, and isolated-consumer matrices;
- ASan+UBSan on provider-free code and every host-testable M7 validation path;
- smoke benchmarks with warmup, synchronization outside timed regions where
  appropriate, checksums, exact hardware/toolchain, and no unsupported speedup
  claim.

## Evidence labels

Evidence is classified exactly as:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

No label is inferred from another. Toolkit detection alone is
`configure-tested`; compilation alone is `compile-tested`; real GPU execution
without an independent oracle is `runtime-tested`; comparison with an
independent CPU/reference or bit oracle is `parity-tested`; absent hardware,
toolchain, platform, operation, or matrix is `skipped`.

The final evidence records CUDA compiler/toolkit, Runtime, cuSPARSE, driver,
device, compute capability, architecture code, host compiler, static/shared
mode, exact operations/layouts/sizes/tolerances, allocation/workspace/transfer/
event behavior, benchmark method, and every skip.

## Explicit exclusions

- SpMM, sparse triangular solve, preconditioners, solvers, arbitrary sparse
  conversion, BSR/SELL, or general sparse GPU evaluation;
- cuRAND or any distribution beyond raw Philox4x32-10 and Uniform01
  float/double;
- Sobol, direction tables, normal/rejection/variable-consumption
  distributions, entropy, serialization, or a default random state;
- hidden transfer, packing, conversion, densification, allocation,
  synchronization, narrowing, precision change, or fallback;
- OpenMP, Eigen, MKL, BLAS/LAPACK CPU facets, HIP, ROCm, SYCL, or another GPU
  provider;
- Milestone 8 packaging/API/performance/downstream hardening;
- commit, push, pull request, merge, tag, release, or branch deletion.

## Publication Checkpoint B

Milestone 7 stops at Publication Checkpoint B after:

1. complete integrated diff and provenance review;
2. clean CPU and CUDA validation proportional to the exact local environment;
3. static/shared, sanitizer, package, relocation, and isolated-consumer
   evidence;
4. independent verification, documentation/API review, and
   portability/GPU/performance review with findings resolved or recorded;
5. exact GPU evidence labels and explicit skips; and
6. a proposed, not executed, remote publication and branch-cleanup sequence.
