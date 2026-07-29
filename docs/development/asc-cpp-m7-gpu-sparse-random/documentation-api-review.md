# Milestone 7 Documentation and API Review

Status: documentation and public API review complete; integration/runtime
evidence remains owned by the lead and independent verifier

Date: 2026-07-28

Role: independent documentation and API review

## Scope and authority

This review reads the frozen Milestone 7 contract and ownership ledger, the
approved architecture package, ADRs 0008, 0012, 0014, 0015, 0017, and 0018,
the Milestone 6 checkpoint and module guides, and the implemented Milestone 7
public provider headers. The public headers, rather than an implementation
detail or provider SDK, are the source of truth for the documented API.

The review changes only:

```text
docs/modules/sparse.md
docs/modules/random.md
docs/development/asc-cpp-m7-gpu-sparse-random/documentation-api-review.md
```

It does not inspect or copy MdeCpp or deleted asc-cpp provider code, and does
not edit production, CMake, tests, manifests, package integration, or any
Milestone 8 path.

## Public API review

### Sparse CUDA

`<asc/sparse/providers/cuda.h>` exposes no CUDA or cuSPARSE SDK type. Its
public surface is bounded to:

```text
SparseCudaContext
CudaStridedVectorView<Element>
CudaCsrArray<Element>
CudaCsrClone<Element>
CudaCloneCsr
CudaCsrSpmvWorkspaceSize
CudaCsrSpmv
CudaEvaluate
```

`SparseCudaContext` is move-only and retains the provider-neutral
`ExecutionContext`. `CudaStridedVectorView` is a non-owning, device-declared
float/double descriptor with checked signed extent and positive stride. It
does not introduce a Dense dependency.

`CudaCloneCsr` accepts only canonical host CSR and an explicit device resource.
The result owns three visible buffers plus completion. `CudaCsrArray::view()`
is the provider-controlled route that publishes a trusted canonical device
view. Ordinary public `CoordinateView::Create` and
`CompressedSparseView::Create` cannot inspect non-host structure and mark it
untrusted; CUDA operations reject it.

The public Sparse views now expose `canonical_structure_trusted()` and
`RebindValues`. Rebinding preserves the original structure, shape, NNZ,
memory-space declaration, and trust state while replacing only the value
pointer. It does not allocate, copy, retain storage, or elevate untrusted
structure. The guide explicitly assigns truthful placement, sufficient
allocation size, alignment, and lifetime to the caller; the provider
revalidates actual device placement before enqueue.

Unit-stride CSR SpMV has a separate explicit workspace query and uses
deterministic `CUSPARSE_SPMV_CSR_ALG2`. Positive nonunit strides use the
project kernel and exactly zero workspace. The guide records the distinct
workspace rules, `beta == 0`, overlap checks, owner lifetimes, and lack of
hidden allocation, transfer, packing, or wait.

Sparse `CudaEvaluate` supports existing trusted coordinate/CSR/CSC structure
with exact float/double values. The documented closed surface matches the
header: terminal copy, one-level negate, and one-level add/subtract/multiply
with terminals or same-type scalars. Add/subtract permits a scalar only when
it is exact zero; multiply admits any same-type scalar. Scalar-only, nested,
external-operation, structure-changing, distinct-structure, conversion, and
densifying forms are rejected.

### Random CUDA facets

The public provider headers are SDK-free and expose exactly:

```text
<asc/random/providers/cuda.h>
  CudaRandomWordGeneration
  CudaFillPhilox4x32

<asc/random/providers/dense_cuda.h>
  CudaDenseUniform01Generation
  CudaFillDenseUniform01

<asc/random/providers/sparse_cuda.h>
  CudaCoordinateArray
  CudaSparseUniform01Generation
  CudaGenerateSparseUniform01
```

The owner-approved raw-word API accepts a Core `MutableMemoryView`. This makes
byte capacity and declared memory space explicit without exposing an SDK type
or falsely claiming ownership. The guide states the truthful external-view
preconditions: device space on the context device, at least
`word_count * sizeof(uint32_t)` bytes, `uint32_t` alignment, and lifetime
through completion. A zero count is an exact no-op and the first unused offset
is returned explicitly.

The Dense provider accepts only unqualified float/double mutable device views,
ranks zero through eight, and valid unique layouts. Its logical
dimension-zero-fastest mapping, word consumption, padding behavior, and
first-unused offset match the provider-free contract.

The Sparse provider returns a move-only owner for exactly two visible device
allocations, completion, and both first-unused offsets. It preserves the
provider-free priority, tie, word-domain, canonical-order, and value mapping.
The guide calls out the bounded reference algorithm's
`O(logical_size * exact_count + exact_count^2 + rank * exact_count)` time,
including single-thread priority selection and insertion ordering, and zero
hidden workspace; it makes no speedup claim.

All result events are move-only through their `CompletionEvent` members.
Events retain provider completion state, not user buffers, views, resources,
or workspaces. The documentation consistently requires all of those objects
to remain alive and unmodified through completion and states that event
destruction does not complete work.

## Configuration and component review

The module guides use the exact default-off option and provider requirements:

```text
ASC_CPP_ENABLE_CUDA=ON
CUDAToolkit 12 or newer
caller-owned CMAKE_CUDA_ARCHITECTURES
```

The documented component targets and complete installed closures are:

```text
sparse_cuda / ASC::sparse_cuda:
  core;expression;sparse;core_cuda;sparse_cuda

random_cuda / ASC::random_cuda:
  core;random;core_cuda;random_cuda

random_dense_cuda / ASC::random_dense_cuda:
  core;expression;dense;random;random_dense;core_cuda;random_cuda;
  random_dense_cuda

random_sparse_cuda / ASC::random_sparse_cuda:
  core;expression;sparse;random;random_sparse;core_cuda;random_cuda;
  random_sparse_cuda
```

Their direct target edges match the frozen ledger:

```text
ASC::sparse_cuda:
  ASC::sparse;ASC::core_cuda
  private CUDA::cusparse

ASC::random_cuda:
  ASC::random;ASC::core_cuda

ASC::random_dense_cuda:
  ASC::random_dense;ASC::random_cuda;ASC::core_cuda

ASC::random_sparse_cuda:
  ASC::random_sparse;ASC::random_cuda;ASC::core_cuda
```

The guides explicitly state that provider-free and no-component lookup does
not discover CUDA and that missing required compiler/toolkit/provider inputs
are configuration failures, not fallback.

## Findings and resolutions

### Resolved: raw-word external storage contract

An early raw API accepted a bare `uint32_t*` plus count. The owner approved the
Core `MutableMemoryView` boundary. The final signature exposes pointer, byte
size, and memory-space declaration together, enabling span and placement
validation while retaining external ownership and keeping SDK types private.
The final documentation and compile example use only this approved signature.

### Resolved: truthful non-host canonical structure

Public Sparse view construction cannot inspect device-resident indices.
Treating such a view as canonical would have made provider validation depend
on an unchecked caller assertion. The final views carry a
`canonical_structure_trusted` state: host construction validates and trusts,
ordinary non-host construction remains untrusted, and provider-controlled
owners publish trusted views. `RebindValues` preserves rather than elevates
that state. The guide no longer describes arbitrary external device structure
as directly provider-consumable.

### Resolved: external values without structural forgery

The approved `RebindValues` APIs permit a caller to reuse trusted
provider-owned structure with separate values. Final review verified that they
preserve format/shape/NNZ/space/trust and reject structure/value span overlap.
The guide documents the validation limits and both owners' lifetimes instead
of implying that the rebound pointer is retained or fully proven at view
construction.

### Resolved: volatile vector constraint

Review instantiated `CudaStridedVectorView<volatile float>` and found that the
initial support predicate removed all cv-qualification. The class was
therefore admitted even though its descriptor conversion later attempted to
discard volatile qualification. Production changed the public support
predicate to reject volatile while preserving const input vectors. A GCC
negative compile now fails directly with an unsatisfied
`kSupportedElement<Element>` constraint.

### No remaining API defect

After those corrections, review found no unresolved signature, ownership,
dependency, SDK-isolation, reproducibility, or documentation defect in the
bounded Milestone 7 public surface.

## Example and header validation

The four new guide examples were compiled together as C++20 functions. They
exercise:

- explicit Sparse CSR workspace query/allocation and completion;
- raw word fill through `Buffer::mutable_view()`;
- Dense Uniform01 completion; and
- move-only Sparse exact-count generation.

Equivalent commands were:

```sh
g++ -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only -

/usr/bin/clang++-19 -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only -
```

Each compiler was also run with:

```text
-fno-exceptions -fno-rtti
```

Result: pass, 4/4 combined example translation checks.

Each of the four public provider headers was then compiled alone with the same
strict diagnostics under both compilers, normally and without exceptions or
RTTI. Result: pass, 16/16 header self-containment checks.

The volatile-vector negative check was:

```sh
printf '#include <asc/sparse/providers/cuda.h>\n\
using V = asc::CudaStridedVectorView<volatile float>;\n' |
  g++ -std=c++20 -Iinclude -x c++ -fsyntax-only -
```

Result: expected compile failure at the public constraint. These are
documentation/API compile checks, not package, runtime, numerical, bit-parity,
or performance evidence.

GCC and Clang positive trait checks also passed for move-only Sparse context
and CUDA owners, const input vector construction, and mutable/const
`RebindValues` results. Result: pass, 2/2 trait translation checks.

## Evidence labels and remaining risks

This documentation review does not establish GPU evidence by itself. The
integration report must classify each facet independently using only:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

The lead supplied the final primary-facet classification after direct,
independent local hardware runs:

```text
sparse_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested

random_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested

random_dense_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested

random_sparse_cuda:
  configure-tested
  compile-tested
  runtime-tested
  parity-tested
```

Trusted device CSC success is `skipped` because Milestone 7 has no approved
device CSC producer; CSR evidence is not generalized to that evaluator path.
The guides point to Publication Checkpoint B for exact commands, counts,
versions, hardware, package, relocation, and sanitizer results. This review's
successful header and guide compiles do not themselves establish
`runtime-tested` or `parity-tested`.

Remaining user-facing risks are documented:

- asynchronous lifetime enforcement remains caller-owned;
- views and events do not retain external storage;
- `MutableMemoryView` and `RebindValues` declarations must be truthful;
- one Sparse provider context is not concurrently mutable;
- cuSPARSE workspace is version/device dependent and must be queried;
- the sparse random reference selection is intentionally low-workspace but
  can be slow for large domains/counts;
- native codes and diagnostic strings are not stable control-flow APIs; and
- pre-1.0 source/ABI compatibility follows the 0.7 release-line policy.

## Provenance and boundary

The documentation is original project-owned text derived from the frozen
contract, approved ADRs, current project-owned public APIs, and the declared
CUDA/cuSPARSE capability boundary. It imports no third-party sample, source,
table, literal, benchmark framework, or prose.
