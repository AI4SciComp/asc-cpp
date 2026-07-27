# Sparse module

`ASC::sparse` is the serial CPU sparse-storage component in the unreleased
ASCCpp `0.9.0` candidate. It owns compile-time-rank coordinate storage,
canonical rank-two CSR and CSC storage, non-owning views, explicit format
conversion, structure-preserving expression evaluation, and deterministic
reference CSR sparse matrix-vector multiplication.

Milestone 7 adds an opt-in `ASC::sparse_cuda` provider facet for explicit
host-to-device CSR cloning, bounded device sparse evaluation, and
float/double device CSR SpMV. It does not change the provider-free base target
or add sparse CUDA to `ASC::cpp`.

Sparse is independent of dense. It uses storage-neutral expression protocols
for readable and writable operands, so an application may use a dense view or
its own vector adapter without adding a dense include or target dependency to
the sparse component.

The M4 base contains no optimized provider, hidden densification, hidden
conversion, transfer, packing, workspace, or fallback. The M7 CUDA facet keeps
transfer and workspace explicit and never links dense.

## Build and dependency contract

```text
build target:     asc_sparse
build-tree alias: ASC::sparse
installed target: ASC::sparse
target kind:      static/shared compiled library
direct ASC deps:  ASC::core, ASC::expression
external deps:    none

CUDA build target:     asc_sparse_cuda
CUDA build-tree alias: ASC::sparse_cuda
CUDA installed target: ASC::sparse_cuda
CUDA target kind:      static/shared compiled library
CUDA direct ASC deps:  ASC::sparse, ASC::core_cuda
CUDA private provider: CUDA::cusparse
```

An installed consumer requests the component explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse)
target_link_libraries(my_target PRIVATE ASC::sparse)
```

The core and expression exports are loaded as transitive requirements.
Utilities, dense, random, random-storage facets, the aggregate, and provider
targets are absent from an isolated sparse consumer. An application that needs
both storage modules requests `COMPONENTS dense sparse` and links both targets
independently.

The CUDA facet exists only with `ASC_CPP_ENABLE_CUDA=ON`. An installed
consumer requests it explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS sparse_cuda)
target_link_libraries(my_target PRIVATE ASC::sparse_cuda)
```

That component closure imports sparse and core CUDA plus CUDAToolkit Runtime
and cuSPARSE. It imports no dense or random target. A provider-free sparse
request from the same installation does not discover CUDAToolkit.

## Public headers

| Header | Contract |
| --- | --- |
| `<asc/sparse.h>` | complete Milestone 4 sparse surface |
| `<asc/sparse/coordinate.h>` | coordinate builder, finalized owner, and view |
| `<asc/sparse/compressed.h>` | canonical CSR/CSC owners, views, and conversions |
| `<asc/sparse/evaluate.h>` | structure-preserving sparse expression evaluation |
| `<asc/sparse/linalg.h>` | serial reference CSR SpMV |
| `<asc/sparse/export.h>` | shared-library symbol visibility |
| `<asc/sparse/providers/cuda.h>` | sparse CUDA context, device vector descriptor, CSR clone, workspace query, CSR SpMV, and bounded evaluator |
| `<asc/sparse/providers/cuda_export.h>` | sparse CUDA shared-library symbol visibility |

The sparse umbrella includes the storage-neutral readable and writable
expression protocols it uses. Every supported public declaration is directly
in `namespace asc`. Public headers are self-contained and contain no optional
provider SDK header or type.

## Public API map

| Surface | Public names |
| --- | --- |
| element, extent, and algebra constraints | `SparseElement`, `SparseExtents`, `SparseLinearAlgebraScalar` |
| construction and format policies | `DuplicatePolicy`, `ExplicitZeroPolicy`, `SparseCompressedFormat` |
| coordinate storage | `CoordinateBuilder`, `CoordinateArray`, `CoordinateView` |
| compressed storage | `CompressedSparseArray`, `CompressedSparseView`, `CsrArray`, `CscArray`, `CsrView`, `CscView` |
| conversions | `ToCsr`, `ToCsc`, `ToCoordinate` |
| expression evaluation | `Evaluate` |
| algebra | `Spmv` |

`SparseElement` accepts an unqualified, non-Boolean arithmetic type that is
trivially copyable and trivially destructible. A view may add element `const`;
an owner cannot. `Spmv` is narrower and supports exactly unqualified `float`
and `double`.

Logical coordinates and compressed inner indices use zero-based, signed
64-bit `index_t`. Shapes use `extent_t`; capacities and stored-entry counts
use `nnz_t`; byte counts use `std::size_t`. Rank is fixed at compile time.
Sparse validates sums, products, byte counts, span sizes, positions, casts,
and address ranges before publishing storage or mutating a destination.

## Sparse-only construction and SpMV example

This installed-target example builds a rectangular coordinate matrix,
finalizes it with explicit policies, converts it to CSR, and runs SpMV with a
project-external vector type. It includes no dense header and links no dense
target.

```cpp
#include <asc/sparse.h>

#include <array>
#include <cstddef>
#include <span>
#include <utility>

struct ExternalVector {
  double* data;
  std::array<asc::extent_t, 1> shape;
  asc::AliasToken alias;
};

namespace asc {

template <>
struct ExpressionAdapter<::ExternalVector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;
  static constexpr ExpressionOperationCategory kOperationCategory =
      ExpressionOperationCategory::kTerminal;
  static constexpr SparsityEffect kSparsityEffect =
      SparsityEffect::kStructurePreserving;

  static std::array<extent_t, 1> Shape(const ::ExternalVector& vector) {
    return vector.shape;
  }
  static double Read(const ::ExternalVector& vector,
                     std::span<const index_t, 1> coordinate) {
    return vector.data[static_cast<std::size_t>(coordinate[0])];
  }
  static bool MayAlias(const ::ExternalVector& vector, AliasToken token) {
    return AliasTokensMayOverlap(vector.alias, token);
  }
};

template <>
struct ExpressionPlacementAdapter<::ExternalVector> {
  static MemorySpace Space(const ::ExternalVector&) {
    return MemorySpace::kHost;
  }
};

template <>
struct WritableExpressionAdapter<::ExternalVector> {
  using value_type = double;
  static constexpr rank_t kRank = 1;

  static std::array<extent_t, 1> Shape(const ::ExternalVector& vector) {
    return vector.shape;
  }
  static AliasToken Alias(const ::ExternalVector& vector) {
    return vector.alias;
  }
  static void Write(::ExternalVector& vector,
                    std::span<const index_t, 1> coordinate, double value) {
    vector.data[static_cast<std::size_t>(coordinate[0])] = value;
  }
};

}  // namespace asc

int main() {
  using Extents = asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

  asc::HostMemoryResource resource;
  auto extents = Extents::Create(2, 3);
  if (!extents.ok()) {
    return 1;
  }

  auto builder =
      asc::CoordinateBuilder<double, Extents>::Create(*extents, 3, resource);
  if (!builder.ok()) {
    return 2;
  }

  constexpr std::array<asc::index_t, 2> kEntry0 = {0, 0};
  constexpr std::array<asc::index_t, 2> kEntry1 = {0, 2};
  constexpr std::array<asc::index_t, 2> kEntry2 = {1, 1};
  if (!builder->Add(kEntry0, 2.0).ok() || !builder->Add(kEntry1, 1.0).ok() ||
      !builder->Add(kEntry2, 3.0).ok()) {
    return 3;
  }

  const asc::ExecutionContext context = asc::ExecutionContext::Serial();
  auto coordinate = builder->Finalize(context, asc::DuplicatePolicy::kReject,
                                      asc::ExplicitZeroPolicy::kKeep);
  if (!coordinate.ok()) {
    return 4;
  }
  auto coordinate_view = coordinate->view();
  if (!coordinate_view.ok()) {
    return 5;
  }

  auto csr = asc::ToCsr(context, *coordinate_view, resource);
  if (!csr.ok()) {
    return 6;
  }
  auto matrix = csr->view();
  if (!matrix.ok()) {
    return 7;
  }

  std::array<double, 3> input_values = {1.0, 2.0, 3.0};
  std::array<double, 2> output_values = {-1.0, -1.0};
  auto input_alias = asc::AliasToken::FromAddressSpan(
      input_values.data(), input_values.size() * sizeof(double));
  auto output_alias = asc::AliasToken::FromAddressSpan(
      output_values.data(), output_values.size() * sizeof(double));
  if (!input_alias.ok() || !output_alias.ok()) {
    return 8;
  }
  ExternalVector input{input_values.data(), {3}, *input_alias};
  ExternalVector output{output_values.data(), {2}, *output_alias};

  const asc::Status status =
      asc::Spmv(context, 1.0, *matrix, input, 0.0, output);
  return status.ok() && output_values[0] == 5.0 && output_values[1] == 6.0 ? 0
                                                                           : 9;
}
```

The resource is declared before every sparse owner, so it outlives their
deallocation. The adapters borrow the two `std::array` objects, which outlive
the synchronous call. A writable adapter promises that every in-shape logical
coordinate names a unique writable scalar. The precomputed span token covers
each vector's complete accessible byte range, so `MayAlias` remains
conservative even for offset views whose starting addresses differ. Violating
either semantic promise is an adapter defect that the concepts cannot infer.

## Coordinate construction and finalization

`CoordinateBuilder<Element, ExtentsType>` is a move-only assembly object.
`ExtentsType` must be an unqualified specialization of core `Extents<...>`.
Creation requires validated extents, an explicit non-negative capacity, and
an explicit host `MemoryResource`.

The capacity is fixed. Creation makes its allocation visible, and `Add` never
grows or reallocates the builder. `Add` requires one complete logical
coordinate and a value. It validates rank, bounds, and remaining capacity
before mutation, then retains insertion order.

Rank zero describes one scalar coordinate. Any zero extent makes the domain
empty, so no coordinate can be added.

Finalization consumes the builder on success and requires all policies to be
explicit:

```text
ExecutionContext::Serial()
DuplicatePolicy::kReject or DuplicatePolicy::kSum
ExplicitZeroPolicy::kKeep or ExplicitZeroPolicy::kDrop
```

Finalization stably sorts coordinates in lexicographic dimension order.
`kReject` reports the first duplicate without publishing an owner. `kSum`
combines equal coordinates in insertion order. Integral addition is checked
for overflow; floating addition follows ordinary left-to-right IEEE
arithmetic. Zero dropping happens after duplicate handling. A value equal to
`Element{}` is an explicit zero; NaN is not.

On success, the returned move-only `CoordinateArray` owns sorted, unique,
structurally immutable coordinates and mutable values. On failure, the
builder retains its original logical entries and can be inspected, extended
within capacity, or finalized again. Finalization uses the builder's declared
storage and acquires no hidden workspace. The serial reference implementation
uses duplicate prevalidation and stable insertion sorting, with worst-case
`O(rank * nnz^2)` time and `O(rank)` fixed local metadata.

## Coordinate owners and views

`CoordinateArray` owns its allocation through the non-owning
`MemoryResource` supplied at builder creation. The resource must outlive the
builder, the finalized owner, and final deallocation. Copy construction and
copy assignment are deleted. Moving transfers the allocation and its resource
lifetime obligation.

`CoordinateView<Element, Rank>` is trivially copyable and never owns storage.
The structure is always exposed as const; element constness controls only
whether stored values may be modified. A mutable-element view converts to the
corresponding const-element view, never the reverse. A const owner yields only
a const-element view.

A view exposes shape, rank, stored-entry count, canonical coordinates, values,
memory space, checked stored-entry access, logical coordinate lookup, and a
conservative value-span alias identity. `coordinate_data()` and `value_data()`
are raw address descriptors. Like `DenseView::data()`, they do not make the
referenced address host-dereferenceable. A caller may dereference those
pointers on the host only after establishing
`space() == MemorySpace::kHost`. `CoordinateAt`, `ValueAt`, and `Find` are the
checked host-access operations. Lookup of an unstored logical coordinate
returns a successful null pointer without insertion or allocation.

As with dense views, const-qualifying the descriptor object does not change the
mutability expressed by `Element`. Use `CoordinateView<const T, Rank>` when
stored values must be read-only. Direct `WriteExpression` and SpMV's generic
output protocol additionally require a non-const destination descriptor.

The owner must outlive every view access and every operation using a view.
Owner destruction and structural replacement invalidate its views. Moving an
owner transfers the allocation; existing views still refer to that allocation,
now owned by the destination object. The caller synchronizes access involving
overlapping mutable values.

External view creation checks metadata, index ranges, span arithmetic, null
addresses for nonempty spans, and memory-space vocabulary. It cannot prove
pointer provenance, allocation length, alignment, declared placement, or
lifetime. The caller must provide live, suitably aligned spans of the required
length and keep them alive.

Host element access is available only for `MemorySpace::kHost` in Milestone 4.
Pinned, device, and managed enum values do not make their addresses
host-dereferenceable.

## Canonical CSR and CSC

`CompressedSparseArray<Element, Format>` and
`CompressedSparseView<Element, Format>` have rank exactly two. The format is a
compile-time value:

```text
SparseCompressedFormat::kCsr
SparseCompressedFormat::kCsc
```

The aliases `CsrArray`, `CscArray`, `CsrView`, and `CscView` give the common
spellings. Shape is always `{rows, columns}`.

Canonical compressed storage has these invariants:

- internal index base is zero;
- offsets contain `outer_extent + 1` entries;
- the first offset is zero;
- offsets are nondecreasing;
- the final offset equals the number of stored entries;
- every inner index is in range;
- inner indices are strictly increasing within each outer segment;
- duplicate inner indices are absent; and
- structure is immutable after publication.

For CSR, rows are outer segments and columns are inner indices. For CSC,
columns are outer segments and rows are inner indices. Empty storage still
contains `outer_extent + 1` zero offsets, including shapes with a zero outer or
inner extent.

Owner construction validates all metadata and copies caller-provided offsets,
indices, and values through an explicit host resource. Destination allocations
are visible. Failure publishes no partial owner and releases any partial
allocation exactly once.

Compressed owners are move-only. Their views follow the coordinate view's
const propagation, non-ownership, external-storage preconditions, placement,
aliasing, lifetime, invalidation, and concurrency rules. Logical lookup uses
the canonical segment and returns additive zero for an unstored coordinate.
`outer_offset_data()`, `inner_index_data()`, and `value_data()` are raw address
descriptors, not permission to host-dereference a non-host address. `ValueAt`
and `Find` enforce host accessibility and return explicit status.

## Explicit conversions

Named conversions create a new owner using an explicit serial context and
destination host resource:

```text
canonical coordinate -> CSR
canonical coordinate -> CSC
CSR -> canonical coordinate
CSC -> canonical coordinate
CSR -> CSC
CSC -> CSR
```

Conversion never mutates its source. It preserves shape, values, explicit
zeros, and canonical coordinate/value order. It does not change index base or
width, combine duplicates, route through an undisclosed coordinate temporary,
densify, transfer, synchronize, dispatch, or fall back.

Each destination owns newly allocated offsets, indices, coordinates, and
values as applicable. Allocation failure or invalid source metadata publishes
no partial owner and releases partial destination storage. The serial
reference implementation may use deterministic repeated scans instead of
hidden workspace:

| Conversion | Time complexity | Destination allocation |
| --- | --- | --- |
| coordinate to CSR/CSC | `O(destination_outer_extent * nnz)` | offsets, indices, values |
| CSR to coordinate | `O(rows + rank * nnz^2)` | coordinates, values |
| CSC to coordinate | `O(rows * (columns + nnz) + rank * nnz^2)` | coordinates, values |
| CSR to CSC | `O(columns * (rows + nnz))` | offsets, indices, values |
| CSC to CSR | `O(rows * (columns + nnz))` | offsets, indices, values |

These are correctness-path bounds, not optimized-provider performance claims.
No operation allocates storage proportional to the dense logical shape.

## Expression participation and evaluation

Coordinate and compressed views participate as terminal,
structure-preserving `ReadableExpression` values. A captured view descriptor
remains non-owning. The source storage and its compatible structure must
outlive expression reads and evaluation.

Sparse evaluation writes into an existing mutable coordinate or compressed
destination. It requires:

- explicit serial execution;
- exact destination rank, shape, and value type;
- an existing caller-provided canonical destination structure;
- host-accessible source and destination where placement is known; and
- `SparsityEffect::kStructurePreserving`.

The evaluator traverses only the destination's stored canonical coordinates
and changes only values. It never inserts, erases, allocates a result, or
visits the dense logical domain.

Before the first write, evaluation validates context, shape, placement,
structure, and conservative alias metadata. Exact direct self-assignment is a
no-op. Other possible value overlap is rejected before mutation.

Structure-filtering, structure-union, structure-intersection, value-dependent,
densifying, destination-required, and rank-zero scalar-expansion expressions
are rejected in Milestone 4. A caller needing changed structure must assemble
an explicit builder or select an explicitly named conversion; sparse
evaluation never chooses dense result storage.

Successful evaluation allocates no computational storage, temporary,
workspace, or packing buffer. It performs no format conversion, densification,
transfer, synchronization, provider dispatch, or fallback. A failed
diagnostic `Status` owns a `std::string` and may allocate; the component makes
no general heap-free error-path claim.

## Storage-neutral vector operands

`ASC::expression` supplies independent readable, placement, and writable
protocols. A vector used as an SpMV input must be a rank-one
`PlacedReadableExpression`. The output must independently be a rank-one
`ReadableExpression` and `WritableExpression`.

Alias metadata can describe either an identity or one half-open byte span:

- `AliasToken::FromIdentity(pointer)` creates an identity-only token and cannot
  describe the pointer's accessible range;
- `AliasToken::FromAddressSpan(pointer, bytes)` validates
  `[pointer, pointer + bytes)` and returns `kMemoryAccess` for a null nonempty
  range or `kOverflow` when the endpoint is not representable;
- `AliasTokensMayOverlap(left, right)` compares two spans symmetrically, treats
  an identity-only token as one address point when compared with a span, and
  compares two identity-only tokens by identity; and
- a zero-byte span is valid, including at a null address, and never overlaps
  another token.

`AliasToken::operator==` intentionally remains identity-only. A span-aware
adapter must use `AliasTokensMayOverlap` in its `MayAlias` implementation and
return its complete span token from `WritableExpressionAlias`; equality alone
does not compare byte ranges. Identity-only metadata is sufficient only when
exact identity fully captures the adapter's alias domain or partial overlap is
otherwise impossible by construction.

This distinction matters when an external span begins before an overlapping
ASC value span. Its starting identity lies outside the ASC span, so an
identity-point comparison would miss the overlap even though later external
bytes intersect it. Publishing the external span's full checked range makes
the comparison symmetric and conservative. For strided storage, the token
must cover the complete accessible physical range, including holes. If one
token cannot describe disjoint regions, the adapter's `MayAlias` must retain
enough independent metadata to answer conservatively.

ASC coordinate and compressed views publish their complete validated value
byte spans. Structural arrays are immutable and use separate validated,
disjoint storage. Empty value storage publishes a zero-byte token. A token is
metadata only: it owns no storage and does not extend pointer or view lifetime.

An external adapter declares exact shape, memory space, conservative alias
metadata, scalar reads, and writes. Its write operation is called only after
validation and must not allocate, transfer, synchronize, select a provider, or
fail. The adapter is responsible for truthful metadata, valid indexing, and
storage lifetime.

A `DenseView` opts into the same protocol when `<asc/dense.h>` is explicitly
included by the application. Sparse neither includes nor links dense, and a
sparse-only application can use its own adapter.

A mutable rank-one `CoordinateView` can also be an SpMV output, but only when
its canonical structure stores every logical coordinate. SpMV validates
`nnz == extent` and the exact coordinate sequence `0..extent-1` before
mutation. An incomplete sparse vector is rejected rather than silently
dropping output values. An external writable adapter makes the corresponding
total-coverage and unique-destination promise itself.

Top-level-volatile descriptor objects do not satisfy `ReadableExpression`,
`PlacedReadableExpression`, or `WritableExpression`. Const descriptors may
remain readable/placed when their adapter supports reads, but a
top-level-const descriptor is not writable. Sparse element concepts
independently reject volatile element types.

## Serial CSR SpMV

Milestone 4 provides:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

The matrix is canonical CSR with shape `{rows, columns}`. The input has shape
`{columns}` and output has shape `{rows}`. Matrix, vectors, `alpha`, and
`beta` use exactly the same `float` or `double` scalar type.

The operation accepts only an explicit serial context and host placement. It
validates context, placement, shapes, canonical matrix metadata, scalar
agreement, output uniqueness, and possible output overlap with matrix values
or input before mutation.

Traversal is deterministic CSR row order with increasing column indices.
Each row uses ordinary left-to-right native floating multiply and add:

```text
output[row] = alpha * sum(matrix[row, column] * input[column])
              + beta * output[row]
```

When `beta == 0`, SpMV does not read the prior output. Empty shapes and zero
stored entries are valid. NaN, infinity, signed zero, rounding, and contraction
follow ordinary native floating arithmetic and the active compiler mode.
There is no higher-precision accumulator, reproducible-math mode, numerical
exception status, or normalization.

Successful SpMV allocates no storage or workspace, packs nothing, performs no
format conversion or densification, transfers and synchronizes nothing, and
does not dispatch or fall back. The reference kernel performs
`O(rows + matrix_nnz)` adapter calls and scalar arithmetic. Total cost also
includes the adapter-defined scalar-read/write costs; for example, coordinate
view lookup is logarithmic in its stored-entry count. The no-allocation claim
covers ASC-owned computational storage and workspace. An external readable
adapter remains responsible for its own declared read behavior.

CSC SpMV, transpose, SpMM, triangular solve, preconditioners, iterative or
direct solvers, factorization, mixed precision, optimized CPU providers, and
additional GPU operations are deferred.

## Sparse CUDA context and canonical device ownership

`SparseCudaContext::Create` consumes a CUDA `ExecutionContext` by value,
creates one move-only cuSPARSE handle, and binds it to that context's stream.
`execution_context()` returns the immutable shared-state context owner.
Copying the sparse context is disabled; moving transfers the handle.
Destroying or replacing a live context releases CPU-side cuSPARSE resources
and does not device-wide synchronize.

One sparse CUDA context must not be used concurrently from multiple host
threads because it owns one mutable provider handle. Independent contexts may
execute concurrently on disjoint mutable storage.

Device sparse views carry internal canonical-provenance metadata. A host view
is trusted only after its structure has passed host validation. A raw device
view created from external pointers is untrusted because its contents cannot
be inspected transactionally without synchronization. M7 provider factories
preserve trusted provenance when they create a device owner. Sparse CUDA
rejects an untrusted device structure before provider access or enqueue; the
caller cannot opt out through a supported public flag.

The explicit CSR staging operation is:

```text
CudaCloneCsr(sparse_context, canonical_host_csr_view, device_resource)
  -> Result<CudaCsrClone<Element>>

CudaCsrClone
  array
  completion
```

`Element` is exactly `float` or `double`. The call requests exactly three
owner buffers from the supplied device resource: signed-64 outer offsets,
signed-64 inner indices, and values. It enqueues explicit host-to-device
copies on the sparse context and returns the trusted device CSR owner plus the
final completion event. It neither converts format nor changes index base,
width, structure, values, or precision.

The host source spans, device resource, returned owner, and context remain
alive through completion. The resource must then outlive final owner
deallocation. A borrowed device view does not extend the clone owner's
lifetime. Pageable host source storage is valid, but CUDA Runtime submission
need not be host-nonblocking for every pageable transfer.

```cpp
#include <asc/core/providers/cuda.h>
#include <asc/sparse/providers/cuda.h>

#include <array>
#include <utility>

int main() {
  constexpr std::array<asc::extent_t, 2> kShape = {2, 3};
  constexpr std::array<asc::nnz_t, 3> kOuter = {0, 2, 3};
  constexpr std::array<asc::index_t, 3> kInner = {0, 2, 1};
  constexpr std::array<double, 3> kValues = {2.0, 1.0, 3.0};

  asc::HostMemoryResource host_resource;
  auto host_csr = asc::CsrArray<double>::Create(kShape, kOuter, kInner, kValues,
                                                host_resource);
  if (!host_csr.ok()) {
    return 1;
  }
  auto host_view = std::as_const(*host_csr).view();
  if (!host_view.ok()) {
    return 2;
  }

  const asc::Device device{asc::Backend::kCuda, 0};
  auto execution = asc::CreateCudaExecutionContext(device);
  auto device_resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  if (!execution.ok() || !device_resource.ok()) {
    return 3;
  }
  auto sparse_context = asc::SparseCudaContext::Create(*execution);
  if (!sparse_context.ok()) {
    return 4;
  }
  auto clone =
      asc::CudaCloneCsr(*sparse_context, *host_view, **device_resource);
  if (!clone.ok()) {
    return 5;
  }
  return clone->completion.Wait().ok() ? 0 : 6;
}
```

This installed-target example uses the `sparse_cuda` component shown above.
The device owner remains in `clone` until its event has completed.

## CUDA CSR SpMV

The storage-neutral vector descriptor is:

```text
CudaStridedVectorView<Element>::Create(
    device_pointer, extent, positive_stride, MemorySpace::kDevice)
```

It is a trivially small non-owning descriptor with signed-64 extent and stride
metadata, no CUDA SDK type, and exact `float`, `const float`, `double`, or
`const double` element type. It can describe project-external device storage.
The operation validates scalar alignment, representable physical span,
placement, and device identity before compute. For storage allocated by
`CudaMemoryResource`, it also validates the exact registered allocation
subspan. CUDA Runtime 12.9 exposes no allocation-range query for arbitrary
external pointers, so the declared span of external or suballocated storage
remains a caller lifetime/size contract.

SpMV uses two entry points:

```text
CudaCsrSpmvWorkspaceSize(
    sparse_context, alpha, trusted_device_csr,
    input, beta, output)
  -> Result<size_t>

CudaCsrSpmv(
    sparse_context, alpha, trusted_device_csr,
    input, beta, output, caller_workspace)
  -> Result<CompletionEvent>
```

Both implement only non-transpose
`y = alpha * A * x + beta * y` with exact matching `float` or `double`
storage and compute type. The matrix uses canonical zero-based signed-64 CSR.
Input extent equals columns and output extent equals rows. `beta == 0` does
not read prior output values.

The provider path is explicit:

- unit-stride input and output use deterministic
  `CUSPARSE_SPMV_CSR_ALG2`; the size query returns the exact provider
  workspace requirement, and the caller supplies a device
  `MutableMemoryView` of at least that size; and
- positive nonunit stride uses an original deterministic project kernel; its
  query returns zero and `CudaCsrSpmv` requires an empty workspace.

This is a declared capability split, not runtime fallback. Neither path packs
vectors or silently selects the other. Workspace placement, registered
allocation range when ASC-owned, alignment, size, and overlap are validated
before enqueue. Matrix/vector
storage, workspace, and context remain alive through completion.

The operation rejects wrong shape/type/placement/device, nonpositive stride,
untrusted CSR provenance, span and width overflow, insufficient workspace,
output overlap with any input or workspace, moved context, and provider
failure before a usable event is returned. It performs no format conversion,
transfer, densification, precision change, hidden allocation, or hidden
synchronization.

## Bounded CUDA sparse evaluation

`CudaEvaluate` has coordinate and compressed-view overloads and returns a
completion event. It supports exact `float` or `double`, rank zero through
eight, and trusted canonical device storage. The destination structure is
immutable; only its stored values are written.

The bounded shallow expression set is:

```text
copy(a)
-a
a + b
a - b
a * b
a + 0
a - 0
a * scalar
0 + a
0 - a
scalar * a
```

Terminals must have the exact value type, shape, format, stored-entry count,
and structure-pointer identity as the destination. Two distinct owners with
equal coordinate or index contents are not treated as the same structure.
Only the exact zero scalar is accepted for addition or subtraction, because a
nonzero scalar would change implicit zero entries and densify the logical
result. Multiplication accepts any exact-type scalar.
`RebindValues` creates a checked view over the same immutable trusted
structure and an exact-length distinct value span. Direct exact copy to the
same value span is an already-complete no-op; other exact in-place
elementwise operands are supported because each stored entry is independent.
Partial value overlap is rejected. Unsupported nesting, operations, types,
ranks, formats, shapes, untrusted structure, and overlap fail before value
mutation.

CSR evaluation is reachable from `CudaCloneCsr`. Coordinate evaluation is
reachable when an application also requests `random_sparse_cuda` and uses its
generated trusted device owner; this does not add a random dependency to
`ASC::sparse_cuda`. Although the compressed template accepts CSC, M7 has no
approved producer for trusted device CSC. Raw device CSC is untrusted and
rejected. The CSC success path is therefore explicitly **skipped** and remains
a disclosed M7 capability gap rather than an invented clone/token API.

Evaluation performs one project-kernel operation per stored entry and uses no
computational allocation or workspace. It does not inspect the dense logical
domain, mutate structure, convert format, densify, transfer, pack,
synchronize, or fall back.

## Provider-free failure, mutation, cost, and concurrency

Recoverable failures return `Status` or `Result<T>`; sparse exposes no public
exception API. Public input and operation metadata is checked before a
destination changes.

Successful numerical operations mutate documented values directly. They are
not transactional against NaN or infinity generated by valid floating
arithmetic.

| Surface | Ownership and lifetime | Mutation on failure | Cost and hidden work |
| --- | --- | --- | --- |
| builder creation | move-only capacity storage; resource outlives it | no builder published | two declared allocations sized by capacity |
| `Add` | builder retains entries | logical entries unchanged | rank-linear validation and one insertion |
| finalization | success transfers storage to owner | builder retains pre-call entries | worst-case `O(rank * nnz^2)`; no hidden workspace |
| coordinate view creation/access | non-owning descriptor | no view/value published | host validation `O(rank * nnz)`; lookup `O(rank * log(nnz))` |
| compressed view creation/access | non-owning descriptor | no view/value published | host validation `O(outer_extent + nnz)`; lookup `O(log(segment_nnz))` |
| compressed owner creation | move-only buffers; resource outlives them | no owner published; partial buffers released | three declared allocations plus structure/value copies |
| conversion | new move-only destination owner | source unchanged; no destination published | documented scan plus explicit destination buffers |
| sparse evaluation | caller-owned values and structure | unchanged after validation failure | one expression read/write per stored destination coordinate |
| `Spmv` | caller-owned matrix and vectors | unchanged after validation failure | `O(rows + matrix_nnz)` adapter calls plus adapter-defined scalar-access costs |

Owners and views contain no hidden mutable cache. Separate owners are
independent. Concurrent const access is allowed when referenced storage and
element types permit it. Concurrent operations involving overlapping mutable
values require caller synchronization. Finalized structure is immutable.

The serial context is synchronous. An operation retains no context, view,
expression, resource, or storage after return, creates no completion event,
and performs no hidden synchronization.

## Provider, GPU, and performance evidence boundary

The base sparse component is a deterministic correctness path, not an
optimized CPU provider. It has no Eigen, oneMKL, OpenMP, TBB, BLAS/LAPACK,
HIP, or SYCL integration. CUDA and cuSPARSE exist only behind the separate
`ASC::sparse_cuda` component.

GPU evidence uses exactly **configure-tested**, **compile-tested**,
**runtime-tested**, **parity-tested**, or **skipped**. Toolkit discovery alone
is configure-tested. Provider compilation alone is compile-tested. Real-device
execution without an independent oracle is runtime-tested. Only comparison
with an independent structural/numerical CPU oracle is parity-tested. Missing
toolchain, hardware, operation, or matrix is skipped. One label never implies
another.

The Milestone 7 Publication Checkpoint B report records the actual local label
for CSR clone, unit-stride cuSPARSE SpMV, strided project-kernel SpMV, each
reachable evaluator format/operation, workspace behavior, and performance
smoke. This guide does not infer hardware evidence from configuration or
compilation. The CSC evaluator success path is **skipped** because M7 has no
approved trusted device CSC producer.

Benchmark evidence belongs to the Milestone 4 Publication Checkpoint B report.
It records compiler, configuration, shape, stored-entry count, format,
iterations, allocation instrumentation, elapsed time, and checksum without an
unstable speed threshold or provider-level performance claim.

## Deferred work and provenance

Milestone 4 does not provide BSR, SELL, general-rank compressed storage,
arbitrary finalized structural mutation, hidden builder growth, external
adoption/deleters, structure-changing evaluation, sparse-to-dense
materialization, SpMM, transpose SpMV, solvers, preconditioners, optimized CPU
kernels, or random generation. M7 adds only the bounded CUDA surface above;
trusted CSC device staging, additional sparse algebra, general GPU
evaluation, another GPU backend, and optimized large-domain sparse-random
selection remain deferred.

The implementation is project-owned and follows the frozen
[Milestone 4 contract](../development/asc-cpp-m4-sparse-cpu/milestone-contract.md)
and approved sparse, expression, memory/execution, ownership, mixed-storage,
and provider ADRs. No MdeCpp, deleted asc-cpp, third-party sparse-library, or
provider production source, test, table, vector, or documentation material is
copied.
