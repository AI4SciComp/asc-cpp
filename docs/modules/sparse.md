# Sparse module

`ASC::sparse` is the provider-free sparse-storage and reference-algebra
component. It is a compiled static or shared library with exactly two direct
ASC dependencies: `ASC::core` and `ASC::expression`. It has no Dense, Random,
provider, or third-party dependency. CUDA Sparse and Random adds the separately requested
`ASC::sparse_cuda` provider without changing that base dependency graph.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse)
target_link_libraries(my_target PRIVATE ASC::sparse)
```

```cpp
#include <asc/sparse.h>
```

The umbrella includes coordinate, compressed, evaluation, and Sparse BLAS
APIs. The narrow `<asc/sparse/blas.h>` header declares the standardized
operations and the retained pre-standardization `Spmv` overload, and
`<asc/sparse/export.h>` supplies the compiled-library visibility macro.

## Common vocabulary

`SparseElement<T>` accepts an unqualified, non-Boolean arithmetic type or
`std::complex<float>`/`std::complex<double>` that is trivially copyable and
trivially destructible. Views additionally permit `const T`; constness applies
to values, while structure is always immutable. `SparseBlasScalar` is exactly
`float`, `double`, `std::complex<float>`, or `std::complex<double>`.

The public policy and format enums are:

- `DuplicatePolicy::{kReject, kSum}`;
- `ExplicitZeroPolicy::{kKeep, kDrop}`; and
- `SparseCompressedFormat::{kCsr, kCsc}`.

Shapes, coordinates, inner indices, outer offsets, capacity, and NNZ use the
checked Core integer vocabulary. Coordinates and inner indices are zero-based.
Sparse CPU operations require an explicit serial `ExecutionContext` and host
storage. There is no implicit transfer or fallback.

## Coordinate construction and storage

The coordinate family is:

```text
CoordinateBuilder<Element, ExtentsType>
CoordinateArray<Element, ExtentsType>
CoordinateView<Element, Rank>
```

`ExtentsType` must be an unqualified Core `Extents<...>` specialization. Its
rank is the compile-time coordinate rank. Rank zero is a scalar domain with one
empty coordinate. A shape containing a zero extent is empty and cannot contain
an entry.

Create a move-only builder with an explicit host resource, validated extents,
and nonnegative capacity:

```cpp
asc::HostMemoryResource resource;
using Shape = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

auto shape = Shape::Create(3, 4);
if (!shape.ok()) {
  return 1;
}
auto builder =
    asc::CoordinateBuilder<double, Shape>::Create(resource, *shape, 5);
if (!builder.ok()) {
  return 1;
}

const std::array<asc::index_t, 2> coordinate{2, 1};
asc::Status add_status = builder->Add(coordinate, 5.0);
if (!add_status.ok()) {
  return 1;
}

auto matrix = std::move(*builder).Finalize(
    asc::ExecutionContext::Serial(), asc::DuplicatePolicy::kSum,
    asc::ExplicitZeroPolicy::kKeep);
if (!matrix.ok()) {
  return 1;
}
```

`Add` validates the complete coordinate and capacity before mutation. It never
grows or reallocates and preserves insertion order. The caller-provided
resource must outlive the builder and every owner constructed from it.

`Finalize` is rvalue-qualified. It stably sorts coordinates
lexicographically, combines duplicates in insertion order only under `kSum`,
and applies the explicit-zero policy after duplicate handling. Integral sums
are checked; floating sums use deterministic ordinary IEEE arithmetic. NaN is
not equal to zero and is retained by `kDrop`.

Success consumes the builder and reuses its declared buffers. Failure,
including duplicate rejection or integral overflow, leaves the builder valid
with the original logical entries. Finalization acquires no additional
resource storage.

`CoordinateArray::Create(resource, extents, coordinates, values)` is the
direct-owner factory for already canonical flattened coordinates. It validates
the structure, copies coordinates and values through the explicit host
resource, and publishes nothing on failure. Owners are move-only.

`CoordinateView::Create` constructs a non-owning view over external storage.
Views expose `rank`, `shape`/`extents`, `nnz`, `coordinates`, `values`,
`memory_space`, checked `Coordinate` and `AtStored` access, `Lookup`,
`ValueAlias`, and `SameDescriptor`. Host construction validates strict
lexicographic order, uniqueness, and bounds. Public construction for
inaccessible non-host storage can validate metadata and spans but cannot
inspect structure bytes. Such a view reports
`canonical_structure_trusted() == false`; provider operations reject it rather
than trusting a caller assertion. A provider-owned array publishes a trusted
device view after producing canonical structure.

`RebindValues(new_values)` creates another non-owning view that retains the
same structure pointers, shape, NNZ, memory-space declaration, and canonical
trust state while replacing only the value pointer. It checks the value span
and rejects overlap with structure, but cannot prove the allocation's device,
size beyond that span, or lifetime. The caller must supply correctly aligned,
sufficient same-space storage, and a provider validates actual placement
before enqueue. Rebinding neither allocates nor copies and does not extend any
lifetime.

`Lookup` returns additive zero for a missing logical coordinate. Structure
pointers are const. A mutable view can convert to its const-element form, but
the reverse conversion is unavailable. `CoordinateArray::view()` preserves
that const propagation.

Views are trivially copyable and own nothing. Moving an owner transfers its
allocation/resource association without relocating the buffers; the moved-from
owner cannot publish another view. Destroying the allocation-owning object,
replacing its structure, destroying external storage, or violating the
resource-outlives-owner rule invalidates affected views. `ValueAlias` describes
only the value byte span; it does not retain it.

For `N` entries and rank `R`, `Add` is `O(R)`, current stable finalization is
`O(N^2 * R)` time and `O(R)` local storage, and `Lookup` is
`O(log(N) * R)`. These operations do not densify.

## CSR and CSC

The compressed family and aliases are:

```text
CompressedSparseView<Element, Format>
CompressedSparseArray<Element, Format>
CsrView<Element>
CscView<Element>
CsrArray<Element>
CscArray<Element>
```

Compressed storage has rank two. For both formats:

- the index base is zero;
- the offset count is `outer_extent + 1`;
- the first offset is zero and offsets are nondecreasing;
- the final offset equals NNZ;
- inner indices are in range and strictly increasing in every segment;
- duplicate inner indices are invalid; and
- an empty matrix still has its required zero-offset array.

`CompressedSparseView::Create` has pointer/NNZ and span overloads. Host
construction validates the complete canonical structure.
`CompressedSparseArray::Create` accepts either a two-element shape or
`rows, columns`, validates the caller arrays, and copies offsets, indices, and
values through an explicit host resource. The owner uses three visible
allocations and is move-only.

Compressed views expose `format`, `shape`/`extents`, `rows`, `columns`, `nnz`,
the three storage pointers, `memory_space`, checked `OuterOffset`,
`InnerIndex`, and `AtStored` access, `Lookup`, `ValueAlias`, and
`SameDescriptor`. Missing-coordinate lookup is zero and uses binary search
within the canonical outer segment.

Compressed structure, ownership, const propagation, external-storage
responsibility, resource lifetime, and alias lifetime follow the coordinate
rules. Lookup in a segment containing `S` entries is `O(log(S))`.

## Explicit conversions

The six approved conversion directions are:

| Source | Destination | Call |
| --- | --- | --- |
| coordinate | CSR | `ConvertToCsr<Element>(context, source, resource)` |
| coordinate | CSC | `ConvertToCsc<Element>(context, source, resource)` |
| CSR | coordinate | `ConvertToCoordinate(context, source, resource)` |
| CSC | coordinate | `ConvertToCoordinate(context, source, resource)` |
| CSC | CSR | `ConvertToCsr<Element>(context, source, resource)` |
| CSR | CSC | `ConvertToCsc<Element>(context, source, resource)` |

The corresponding
`CompressedSparseArray<Element, Format>::FromCoordinate` and
opposite-format `FromCompressed` factories are also public. Same-format
`FromCompressed` is constrained out.

Every conversion requires a serial context and explicit destination host
resource and returns a new move-only owner. It preserves shape, values,
explicit zeros, zero-based index width, and canonical coordinates. It does not
mutate the source, combine entries, drop zeros, create a dense buffer, transfer
data, or route CSR-to-CSC/CSC-to-CSR through a coordinate temporary.

Coordinate-to-compressed conversion takes
`O(destination_outer_extent * N)` time and three destination-buffer
allocations. Opposite compressed conversion takes
`O(destination_outer_extent * (source_outer_extent + N))` time and three
destination-buffer allocations. Compressed-to-coordinate traversal is
`O(source_outer_extent + N)` before the coordinate builder's
`O(N^2)` canonical finalization and uses the two coordinate destination
buffers. The algorithms use no undisclosed workspace.

## Structure-preserving evaluation

`Evaluate(context, expression, destination)` has overloads for mutable
coordinate and compressed views. It writes only values at the destination's
existing canonical stored coordinates.

Preflight requires:

- serial execution and a host destination;
- accessible source storage and host placement when the source exposes it;
- exact destination rank, shape, and value type; and
- `SparsityEffect::kStructurePreserving`.

All other sparsity effects and rank-zero scalar expansion into a ranked
destination are rejected. Exact direct self-assignment is a no-op. Every other
possible overlap with destination values is rejected using identity and
byte-span metadata before the first write. External adapters are responsible
for truthful access and alias metadata.

Sparse terminal `MayAlias` checks values and every coordinate, outer-offset,
and inner-index span that a read can inspect. Its single-span placement alias
remains the exact values span because the structure buffers may be disjoint;
it does not claim that unrelated address gaps are storage. CSR SpMV separately
checks the complete output byte span against the values, offsets, and indices
before executing.

Successful evaluation allocates no storage or workspace and never changes
structure. It performs no packing, conversion, densification, transfer,
synchronization, provider dispatch, or fallback. Coordinate destination
traversal is linear in destination NNZ apart from source reads. The current
compressed coordinate reconstruction additionally scans outer offsets for
each stored entry.

## Standardized Sparse BLAS

The allocation-free serial reference surface implements every applicable
BLAS Technical Forum Sparse BLAS compute family for S, D, C, and Z:

| Level | API | Contract |
| --- | --- | --- |
| 1 | `SparseDot` | selected unconjugated/conjugated indexed dot |
| 1 | `SparseAxpy` | `y[indx] += alpha * x` |
| 1 | `SparseGather` | `x = y[indx]` |
| 1 | `SparseGatherZero` | gather, then set selected `y` entries to zero |
| 1 | `SparseScatter` | `y[indx] = x` |
| 2 | `Spmv` | `y += alpha * op(A) * x` |
| 2 | `SparseTriangularSolve` | `x = alpha * inv(op(T)) * x` |
| 3 | `Spmm` | `C += alpha * op(A) * B` |
| 3 | `SparseTriangularSolveMultiple` | `B = alpha * inv(op(T)) * B` |

`SparseBlasVectorView`, `SparseBlasIndexedVectorView`, and
`SparseBlasMatrixView` describe caller-owned storage with an explicit backing
span. Vector increments may be positive or negative but not zero; dense
matrices may be row- or column-major. Indexed vectors use public signed 64-bit,
zero-based indices and must be sorted, unique, and in range. Host construction
validates those invariants. Device construction cannot inspect indices and is
therefore untrusted; CUDA operations accept only a trusted provider clone.

`SparseBlasTriangularView::Create` is the explicit analysis step. It requires a
square canonical host CSR or CSC matrix, validates that stored entries stay in
the declared upper or lower triangle, and rejects a missing or zero non-unit
diagonal. Unit diagonals are implicit. A successful descriptor therefore
cannot become singular unless the caller violates the documented immutable
matrix lifetime.

Matrix operations accept canonical CSR and CSC on the serial CPU. Coordinate
storage participates through the explicit `ConvertToCsr`/`ConvertToCsc`
operations; execution never converts implicitly. Transpose and conjugate
transpose are explicit. All output shapes, placements, writable spans, and
overlap rules are checked before mutation, and invalid enum values return
`kInvalidArgument`. Where `alpha` is present, zero prevents reads of its
multiplicative operands; SpMV/SpMM leave their additive destination unchanged,
while triangular solves publish zero. Calls do not allocate, transfer, pack,
densify, synchronize, dispatch to another backend, or fall back.

Sparse matrix addition and sparse matrix multiplication are not admitted by
ADR 0019 and are not supplied. BSR/VBR/SELL, one-based descriptors, unsorted
finalized storage, and finalized duplicate entries remain unsupported rather
than being represented inaccurately. Duplicate summation and explicit-zero
keep/drop behavior remain explicit construction policies.

This Sparse-only indexed-vector example exercises the standardized Level 1
surface:

```cpp
constexpr std::array<asc::index_t, 2> indices{0, 2};
const std::array<double, 2> sparse_values{2.0, -1.0};
std::array<double, 3> dense_values{3.0, 4.0, 5.0};

auto sparse = asc::SparseBlasIndexedVectorView<const double>::Create(
    indices.data(), sparse_values.data(), 2, 3,
    {indices.data(), sizeof(indices), asc::MemorySpace::kHost},
    {sparse_values.data(), sizeof(sparse_values), asc::MemorySpace::kHost});
auto dense = asc::SparseBlasVectorView<double>::Create(
    dense_values.data(), 3, 1,
    {dense_values.data(), sizeof(dense_values), asc::MemorySpace::kHost});
if (!sparse.ok() || !dense.ok() ||
    !asc::SparseAxpy(asc::ExecutionContext::Serial(), 2.0, *sparse, *dense)
         .ok()) {
  return 1;
}
// dense_values == {7.0, 4.0, 3.0}
```

## Compatibility CSR SpMV

The earlier expression-adapted overload remains source compatible:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

It accepts canonical CSR with `float` or `double`, a placed readable rank-one
input, and a placed readable/writable rank-one output of the same value type.
The output adapter must report a unique logical mapping. Matrix shape is
`(rows, columns)`, input length is `columns`, and output length is `rows`.

This Sparse-only example uses rank-one coordinate views as vectors:

```cpp
const std::array<asc::extent_t, 2> matrix_shape{2, 3};
const std::array<asc::nnz_t, 3> offsets{0, 2, 3};
const std::array<asc::index_t, 3> columns{0, 2, 1};
const std::array<double, 3> matrix_values{2.0, 1.0, 4.0};
auto matrix = asc::CsrView<const double>::Create(
    offsets, columns, matrix_values, matrix_shape, asc::MemorySpace::kHost);
if (!matrix.ok()) {
  return 1;
}

const std::array<asc::extent_t, 1> input_shape{3};
const std::array<asc::index_t, 3> input_coordinates{0, 1, 2};
const std::array<double, 3> input_values{3.0, 5.0, 7.0};
auto input = asc::CoordinateView<const double, 1>::Create(
    input_coordinates.data(), input_values.data(), input_shape, 3,
    asc::MemorySpace::kHost);
if (!input.ok()) {
  return 1;
}

const std::array<asc::extent_t, 1> output_shape{2};
const std::array<asc::index_t, 2> output_coordinates{0, 1};
std::array<double, 2> output_values{0.0, 0.0};
auto output = asc::CoordinateView<double, 1>::Create(
    output_coordinates.data(), output_values.data(), output_shape, 2,
    asc::MemorySpace::kHost);
if (!output.ok()) {
  return 1;
}

asc::Status status =
    asc::Spmv(asc::ExecutionContext::Serial(), 1.0, *matrix, *input, 0.0,
              *output);
if (!status.ok()) {
  return 1;
}
// output_values == {13.0, 20.0}
```

Before mutation, SpMV validates context, access, host placement, vector
uniqueness and lengths, and possible output overlap with the input or matrix
values. Arbitrary external vector types can participate through the
Expression readable, placement, and writable adapters; Sparse does not depend
on Dense.

Traversal is deterministic CSR row order with increasing columns. Every dot
product uses ordinary left-to-right multiply/add. When `beta == 0`, old output
is not read. Empty shapes and zero NNZ are valid; NaN and infinity follow
ordinary IEEE behavior.

SpMV is `O(rows + NNZ)` aside from adapter read/write costs and uses no
allocation or workspace. It performs no packing, conversion, densification,
transfer, synchronization, optimized-provider dispatch, or fallback.

## Optional CUDA Sparse facet

CUDA is default-off. Build asc-cpp with `ASC_CPP_ENABLE_CUDA=ON`, CUDAToolkit
12 or newer, and a caller-selected `CMAKE_CUDA_ARCHITECTURES` value. An
installed consumer requests the provider explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse_cuda)
target_link_libraries(my_target PRIVATE ASC::sparse_cuda)
```

```cpp
#include <asc/core/providers/cuda.h>
#include <asc/sparse.h>
#include <asc/sparse/providers/cuda.h>
```

The installed closure is exactly
`core;expression;sparse;core_cuda;sparse_cuda`. `ASC::sparse_cuda` directly
links `ASC::sparse` and `ASC::core_cuda` and privately links cuSPARSE.
Requesting only `sparse`, the provider-free `cpp` aggregate, or no component
does not discover CUDAToolkit or import a CUDA target. Enabling CUDA without a
usable compiler, toolkit, runtime, or cuSPARSE is a configuration failure, not
a silent provider disablement.

The public provider header contains no CUDA or cuSPARSE SDK type. In addition
to the retained CUDA Sparse and Random APIs, it exposes:

```text
SparseCudaContext
CudaStridedVectorView<Element>
CudaCsrArray<Element>
CudaCsrClone<Element>
CudaCloneCsr
CudaCsrSpmvWorkspaceSize
CudaCsrSpmv
CudaEvaluate
CudaIndexedVectorArray / CudaCloneIndexedVector
CudaTriangularCsrArray / CudaCloneTriangularCsr
CudaSparseDot / CudaSparseAxpy / CudaSparseGather
CudaSparseGatherZero / CudaSparseScatter
CudaSpmv / CudaSpmm
CudaSparseTriangularSolve / CudaSparseTriangularSolveMultiple
```

### Context, staging, and trusted structure

Create a Core CUDA `ExecutionContext`, then create the move-only
`SparseCudaContext` from it. The Sparse context owns one cuSPARSE handle bound
to the Core context's stream. It is not a native-handle adoption interface.
One Sparse context must not be used concurrently; independent contexts and
streams may execute concurrently only when their storage does not race.

`CudaCloneCsr(context, host_csr, device_resource)` stages canonical CSR with
any `SparseBlasScalar`. `CudaCloneIndexedVector` stages a validated canonical
indexed vector. `CudaCloneTriangularCsr` accepts only a successfully analyzed
host triangular descriptor and publishes only a const matrix view with the
validated triangle/diagonal metadata. The resource must be device storage on
the context device. `CudaCloneCsr` returns a move-only `CudaCsrClone`
containing:

- `array`, a device `CudaCsrArray` with exactly three caller-resource
  allocations for offsets, indices, and values; and
- `completion`, the event for the three explicit host-to-device copies.

The clone preserves signed 64-bit, zero-based structure and does not convert
format, pack, densify, or allocate hidden workspace. Its device view is
provider-created and therefore carries trusted canonical structure. A public
`CsrView::Create(..., MemorySpace::kDevice)` cannot validate device structure
and remains untrusted; Sparse CUDA rejects it. To use external value storage
with provider-owned structure, call `RebindValues` on a trusted view. The
rebound pointer must name a correctly aligned, sufficiently large allocation
in the inherited memory space on the context device. The call retains neither
the new values nor the original structure owner.

The device resource must outlive the array and its deallocation. The array
must outlive every view and operation using it. Do not read, rebind, move, or
destroy the clone owner while its copy is pending. Waiting for the clone event
is the simple boundary before using the data from another stream; work
submitted later to the same Core context follows its stream order.

### Deterministic CSR SpMV

`CudaStridedVectorView<Element>::Create(data, extent, stride)` is a
provider-neutral descriptor for external device storage. `Element` is
`float`, `double`, or its const form; extent is checked signed 64-bit and
stride must be positive. The descriptor owns no bytes and declares device
placement, so the caller must provide a truthful pointer on the context
device.

Call `CudaCsrSpmvWorkspaceSize` with the matrix and vectors before allocating
workspace. Unit-stride, nonempty operands use deterministic
`CUSPARSE_SPMV_CSR_ALG2`; pass a device `MutableMemoryView` whose size is at
least the returned byte count. Positive nonunit strides use the original ASC
project kernel and return a required size of zero; that path rejects a
nonempty workspace. Workspace and operand spans must not overlap.

This synchronous wrapper demonstrates the spelling and intentionally keeps
every borrowed object alive through `Wait()`:

```cpp
asc::Status RunCudaSpmv(asc::SparseCudaContext& context,
                        asc::CsrView<const float> matrix,
                        const float* input_data, float* output_data,
                        asc::MemoryResource& device_resource) {
  auto input = asc::CudaStridedVectorView<const float>::Create(
      input_data, matrix.columns(), 1);
  if (!input.ok()) {
    return input.status();
  }
  auto output = asc::CudaStridedVectorView<float>::Create(
      output_data, matrix.rows(), 1);
  if (!output.ok()) {
    return output.status();
  }
  auto workspace_size =
      asc::CudaCsrSpmvWorkspaceSize(context, matrix, *input, *output);
  if (!workspace_size.ok()) {
    return workspace_size.status();
  }
  auto workspace =
      asc::Buffer::Allocate(device_resource, *workspace_size);
  if (!workspace.ok()) {
    return workspace.status();
  }
  auto workspace_view = workspace->mutable_view();
  if (!workspace_view.ok()) {
    return workspace_view.status();
  }
  auto completion = asc::CudaCsrSpmv(context, 1.0F, matrix, *input, 0.0F,
                                     *output, *workspace_view);
  if (!completion.ok()) {
    return completion.status();
  }
  return completion->Wait();
}
```

The operation computes `y = alpha * A * x + beta * y`. It validates trusted
CSR structure, zero-based signed 64-bit metadata, device and context
compatibility, vector shape/stride/span, output overlap, provider narrowing,
algorithm, and workspace before enqueue. `beta == 0` does not read prior
output. Both paths are `O(rows + NNZ)` apart from provider scheduling; the
unit-stride path has the explicit queried workspace, and the strided path has
zero workspace. There is no transfer, packing, allocation, conversion,
synchronization, precision change, or fallback.

### Standardized CUDA Sparse BLAS

The standardized CUDA calls use the same formulas, scalar families,
transpose/conjugation choices, dense layouts, signed strides, shape checks,
and overlap rules as the serial API. CUDA matrix execution accepts trusted CSR
only; callers convert coordinate/CSC storage explicitly on the host before an
explicit clone. Level 1, standardized SpMV/SpMM, and triangular solves use
project-owned CUDA kernels with zero operation workspace. The retained
unit-stride real compatibility SpMV continues to use the explicitly queried
`CUSPARSE_SPMV_CSR_ALG2` path.

Every standardized call returns a `CompletionEvent`. It validates metadata,
memory space, context device, and storage spans before enqueue. It neither
allocates nor transfers, and successful submission does not synchronize.
Indexed and triangular clone calls are visibly named setup operations; they
allocate only from the supplied device resource and return the event for their
explicit host-to-device copies. Keep descriptors, owners, and all borrowed
storage alive and unmodified until the returned event completes.

### Bounded device evaluation

`CudaEvaluate(context, expression, destination)` writes only the values of an
existing trusted device coordinate, CSR, or CSC view. It supports exactly
unqualified `float` and `double` and identical structure:

- a terminal copy;
- a one-level terminal negation;
- one-level terminal/scalar or terminal/terminal add, subtract, or multiply;
- scalar multiply with any same-type scalar; and
- scalar add/subtract only when the scalar is exact zero.

Two scalar operands, nested or external operation nodes, distinct structure
pointers, shape/format mismatch, general lookup, structural union or
intersection, densification, conversion, and partial value overlap are
rejected. Exact shared value pointers are allowed when the pointwise operation
is safe. Evaluation is asynchronous, `O(NNZ)`, and uses no allocation or
workspace.

### Asynchronous lifetime and failures

Clone, all Sparse BLAS calls, compatibility SpMV, and evaluation return
move-only completion state and do not wait on success. Until completion, keep
the Core and Sparse contexts, resources, owners, external storage,
views/expression nodes, and compatibility SpMV workspace alive and unmodified.
A completion event retains provider completion state, not user arrays or
workspace. Destroying it does not complete the work or synchronize the device.

Failures use `Status`/`Result` with stable ASC error codes plus provider/native
diagnostics. Diagnostic strings and native codes are not control-flow APIs.
Submission failure may synchronize already-enqueued work only to make cleanup
safe; successful calls never hide a wait or device-wide synchronization.

## Deliberate omissions

Sparse BLAS adds no hidden coordinate temporary, Dense dependency, provider
registry, device CSC/coordinate staging helper, preconditioner, iterative
solver, factorization, BSR/SELL, arbitrary sparse conversion, general sparse
device evaluation, mixed dense/sparse expression algebra, native-handle
adoption, or implicit transfer/workspace. Sparse addition and sparse
multiplication remain deferred because ADR 0019 does not approve them.

Final local Sparse BLAS evidence classifies the standardized Sparse BLAS CUDA
rows as **configure-tested**, **compile-tested**, **runtime-tested**, and
**parity-tested** on the recorded RTX 3060 environment. Trusted device CSC
success is **skipped** because CUDA Sparse and Random has no approved device CSC producer;
CSR evidence is not generalized to unimplemented device CSC execution. Exact
commands, versions, counts, sanitizer/package status, and hardware details are
recorded in the [release validation report](../../release/release-validation.md).
A documentation or compile-only example is not runtime or parity evidence.

The [frozen CUDA Sparse and Random contract][m7-contract] is authoritative for the
provider surface and deferred work. The [Sparse contract][m4-contract]
remains authoritative for the provider-free CPU surface.

[m4-contract]: ../architecture/decisions/0012-sparse-semantics.md
[m7-contract]: ../architecture/decisions/0014-sparse-linalg-providers.md
