# Dense module

`ASC::dense` owns compile-time-rank multidimensional arrays, non-owning views,
layout mappings, rank-preserving subviews, storage-neutral expression
evaluation, selected reductions, and deterministic reference dense linear
algebra. The optional `ASC::dense_cuda` facet adds a bounded CUDA evaluator and
float/double CUDA algebra in the unreleased ASCCpp `0.9.0` candidate.

The provider-free dense component remains the serial CPU correctness path.
It neither includes nor links CUDA, and it never dispatches implicitly to the
optional provider.

## Build and dependency contract

```text
build target:     asc_dense
build-tree alias: ASC::dense
installed target: ASC::dense
target kind:      static/shared compiled library
direct ASC deps:  ASC::core, ASC::expression
external deps:    none

build target:     asc_dense_cuda
build-tree alias: ASC::dense_cuda
installed target: ASC::dense_cuda
target kind:      static/shared compiled provider facet
direct ASC deps:  ASC::dense, ASC::core_cuda
private provider: CUDA::cublas
```

An installed provider-free consumer requests the base component:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)
target_link_libraries(my_target PRIVATE ASC::dense)
```

The core and expression exports are loaded as transitive requirements.
Utilities, sparse, random, random-storage facets, the aggregate, and provider
targets are not part of an isolated dense consumer.

A provider consumer requests the facet explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_cuda)
target_link_libraries(my_target PRIVATE ASC::dense_cuda)
```

Its exact component closure is
`core;expression;dense;core_cuda;dense_cuda`. This request discovers the
consumer's separately supplied CUDAToolkit 12 or newer. A provider-free
component or no-component `ASC::cpp` request does not discover CUDA.
`ASC::cpp` deliberately has no `ASC::dense_cuda` edge.

## Public headers

| Header | Contract |
| --- | --- |
| `<asc/dense.h>` | complete provider-neutral dense surface |
| `<asc/dense/layout.h>` | left, right, and explicit-stride mappings |
| `<asc/dense/view.h>` | mutable and const-element non-owning views and subviews |
| `<asc/dense/array.h>` | move-only owner, initialized host creation, explicit uninitialized allocation, clone, and discard-resize |
| `<asc/dense/evaluate.h>` | expression evaluation and selected reductions |
| `<asc/dense/linalg.h>` | serial reference vector and matrix operations |
| `<asc/dense/export.h>` | shared-library symbol visibility |
| `<asc/dense/providers/cuda.h>` | CUDA provider context, bounded pointwise evaluation, and CUDA dense algebra |
| `<asc/dense/providers/cuda_export.h>` | dense CUDA facet symbol visibility |

Every supported public declaration is directly in `namespace asc`. Public
headers are self-contained. The provider-neutral umbrella includes no optional
provider header. Even the CUDA-specific header exposes no CUDA/cuBLAS SDK
header, handle, stream, event, or enum type.

## Public API map

| Surface | Public names |
| --- | --- |
| element and extent constraints | `DenseElement`, `DenseExtents` |
| layouts | `LayoutLeft`, `LayoutRight`, `LayoutStride`, `DenseLayoutKind`, `DenseLayoutMapping` |
| views | `DenseView`, `At`, `Subview`, `MayOverlap`, `IsExactView` |
| owner | `DenseArray`, `Create`, `CreateUninitialized`, `view`, `Clone`, `ResizeDiscard` |
| expression evaluation | `Evaluate`, `ReduceSum`, `ReduceMin`, `ReduceMax` |
| algebra metadata | `DenseLinearAlgebraScalar`, `MatrixOperation` |
| algebra | `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, `Gemm` |
| CUDA context | `DenseCudaContext`, `Create`, `execution_context` |
| CUDA evaluation | `CudaEvaluate` |
| CUDA algebra | `CudaCopy`, `CudaScal`, `CudaAxpy`, `CudaGemv`, `CudaGemm` |

`DenseElement` accepts non-Boolean, non-volatile arithmetic types that are
trivially copyable and trivially destructible. A view may add element
`const`; an owner cannot. `DenseLinearAlgebraScalar` narrows the algebra
surface to exactly unqualified `float` and `double`.
`DenseExtents` accepts exactly an unqualified specialization of core
`Extents<...>`; structural substitutes and cv-qualified extents types are not
an extension point.

## Minimal owner and expression example

This example creates a column-major matrix, writes it through a mutable view,
evaluates a rank-zero-expanded expression into a second owner, and reduces the
result:

```cpp
#include <asc/dense.h>

#include <array>
#include <utility>

int main() {
  using MatrixExtents = asc::Extents<2, 2>;
  using Matrix = asc::DenseArray<double, MatrixExtents>;

  const auto extents = MatrixExtents::Create();
  if (!extents.ok()) {
    return 1;
  }

  asc::HostMemoryResource resource;
  auto source_result = Matrix::Create(*extents, resource);
  auto destination_result = Matrix::Create(*extents, resource);
  if (!source_result.ok() || !destination_result.ok()) {
    return 1;
  }
  Matrix source = std::move(*source_result);
  Matrix destination = std::move(*destination_result);

  auto source_view = source.view();
  auto destination_view = destination.view();
  if (!source_view.ok() || !destination_view.ok()) {
    return 1;
  }

  double value = 1.0;
  for (asc::index_t column = 0; column < 2; ++column) {
    for (asc::index_t row = 0; row < 2; ++row) {
      const std::array<asc::index_t, 2> index = {row, column};
      auto element = source_view->At(index);
      if (!element.ok()) {
        return 1;
      }
      **element = value++;
    }
  }

  auto expression = asc::MakeAdd(*source_view, 2.0);
  if (!expression.ok()) {
    return 1;
  }
  const asc::Status evaluated = asc::Evaluate(asc::ExecutionContext::Serial(),
                                              *destination_view, *expression);
  if (!evaluated.ok()) {
    return 1;
  }

  const asc::DenseView<const double, 2> result_view(*destination_view);
  const auto sum = asc::ReduceSum(asc::ExecutionContext::Serial(), result_view);
  return sum.ok() && *sum == 18.0 ? 0 : 1;
}
```

The declaration order makes `resource` outlive both arrays. The expression
borrows `source_view`, so that descriptor and the source allocation remain
alive until evaluation completes. Successful evaluation allocates no result
storage, temporary, workspace, or packing buffer.

## Shape, layout, and traversal

Rank is a `std::size_t` template argument. Shapes use core `extent_t`,
coordinates use `index_t`, and strides use `stride_t`. Metadata is signed so a
negative caller input can be diagnosed before conversion to an allocation or
pointer offset.

Rank zero describes one scalar element. If any extent is zero, the logical
size and required span size are zero.

The supported layouts are:

| Layout | Meaning |
| --- | --- |
| `LayoutLeft` | contiguous column-major; dimension zero varies fastest |
| `LayoutRight` | contiguous row-major; the last dimension varies fastest |
| `LayoutStride` | caller-supplied non-negative strides |

`LayoutLeft` is the named default for an owner. Layout is always part of the
type or mapping metadata; no build option changes it.

Mapping creation is fallible and transactional. It rejects negative extents or
strides and checks every extent product, offset product, offset sum, span
calculation, and integer conversion before returning a mapping. Mutable views
require proven uniqueness. M3 may reject an arbitrary-stride mapping when its
uniqueness cannot be established conservatively. Owners require a unique,
exhaustive mapping, so padded or otherwise non-exhaustive storage is view-only.

Logical traversal always increments dimension zero fastest, independent of the
physical mapping. Evaluation, reduction, and reference linear algebra use this
documented logical order where an operation defines a serial accumulation
order.

Negative strides, repeated-address mappings, a runtime-rank owner, and hidden
packing are deferred.

## Views and subviews

A dense view is a trivially copyable, non-owning descriptor containing:

- an element pointer whose pointee constness defines mutability;
- one validated compile-time-rank layout mapping; and
- an explicit `MemorySpace`.

A nonempty view cannot have a null pointer. A mutable view converts to a
const-element view, never the reverse. Constructing or copying a view does not
allocate dense element storage and does not extend storage lifetime.

`Create` cannot prove the provenance, allocation length, alignment, or actual
memory-space placement of an external pointer. The caller must supply a pointer
to a suitably aligned live element span covering `required_span_size()`
elements in the declared space. Violating that precondition is not converted
into a recoverable status.

The owner of the referenced storage must outlive every view access and every
operation using the view. Moving or destroying the descriptor does not affect
the storage. A successful owner resize invalidates all prior views; a failed
resize preserves both the owner and its existing views.

Bounds-checked access returns `Result<Element*>` or the const-element
equivalent. An invalid coordinate returns `kIndex` without dereferencing the
pointer. Host access additionally requires a host-accessible memory space.
The returned pointer borrows the same storage and has the same invalidation
rules as its view.

A const-qualified view descriptor preserves the mutability expressed by
`Element`; it does not make the elements const. Use
`DenseView<const Element, Rank>` for a read-only element view.

Rank-preserving subviews take one offset and extent per dimension. A successful
subview keeps the parent strides and memory space and allocates no dense element
storage. Invalid offsets, extents, ranges, or offset arithmetic fail before a
descriptor is published. Rank-reducing slices are not part of Milestone 3.

`MayOverlap` compares conservative byte-span bounds. It may report overlap for
disjoint logical elements separated by padding, but it does not authorize a
possibly overlapping write. `IsExactView` additionally requires identical
data address, mapping, and memory space.

## Owning arrays

A dense array is a move-only owner backed by core `Buffer` and an explicit
`MemoryResource`. The resource is non-owned and must outlive the array,
including its final deallocation.

`Create` retains the host behavior established in Milestone 3:

- use mixed static/dynamic core `Extents`;
- support contiguous `LayoutLeft` and `LayoutRight`;
- require ordinary `MemorySpace::kHost` storage;
- value-initialize arithmetic, trivially copyable, trivially destructible
  elements;
- expose mutable views only from mutable owners and const-element views from
  const owners; and
- do not expose shallow copying or external adoption.

`CreateUninitialized` accepts any valid core memory space, supports the same
two exhaustive owner layouts, allocates exactly the required owner span, and
does not read, write, or construct the trivial arithmetic element storage.
It is the explicit entry point for device and managed owners. A successful
zero-sized owner has no allocation.

The exact overloads are:

```text
DenseArray<Element, ExtentsType>::CreateUninitialized(
    const ExtentsType&, MemoryResource&, LayoutLeft = {})
    -> Result<DenseArray>

DenseArray<Element, ExtentsType>::CreateUninitialized(
    const ExtentsType&, MemoryResource&, LayoutRight)
    -> Result<DenseArray>
```

Every owner can expose its normal `DenseView` descriptor. Creating a view does
not dereference the pointer. `DenseView::At` remains the explicit checked host
access operation and accepts only `MemorySpace::kHost` in this milestone;
device, managed, and pinned-host descriptors are manipulated through explicit
copy/provider operations rather than silently migrated or reclassified.

Deep copying is a named clone operation with an explicit destination resource
and execution context. It allocates one uninitialized destination buffer,
enqueues `CopyBytes`, waits for the event, and publishes the owner only after
completion. The resource spaces must be accessible to the explicit context.
The source remains unchanged, and allocation, copy, or wait failure publishes
no partially initialized clone. Clone is therefore synchronous even when it
uses a CUDA stream.

Discard-resize allocates and value-initializes replacement storage through the
owner's resource. On success it discards old values and invalidates every
previous view. On failure the original shape, values, allocation, and view
validity remain unchanged. Resize is not a reshape and preserves no element
values. Because this operation retains the initialized-creation contract, it
supports host owners only; a device, managed, or pinned-host owner receives
`kUnsupported` without mutation.

Moving an owner transfers the allocation and its resource-lifetime obligation.
Views of the moved allocation continue to refer to that allocation, now owned
by the destination object. Move assignment invalidates views of the
destination's previous allocation. A moved-from owner cannot create a view,
clone, or resize until assigned another valid owner.

## Expression participation and evaluation

Dense views participate in the storage-neutral expression protocol through
`ExpressionAdapter`. The adapter reports:

- the element value type and compile-time rank;
- the exact shape;
- terminal operation category;
- structure-preserving sparsity metadata;
- indexed scalar reads; and
- conservative alias identity.

Expression does not include dense headers. A view captured by an expression
node is still non-owning, so the underlying storage and compatible
shape/indexing contract must outlive every later expression read.

The dense adapter's scalar read assumes valid indices and host-accessible live
storage. Direct `ReadExpression` calls do not accept an execution context or
return a memory-access status. Use checked view access or a dense evaluator
when metadata or placement is not already validated.

Dense evaluation writes a `ReadableExpression` into a caller-provided mutable
view using an explicit `ExecutionContext`. Milestone 3 accepts only serial
execution and host memory.

Before writing any element, evaluation validates:

- backend and memory accessibility;
- exact rank and shape, except expression-owned rank-zero scalar expansion;
- destination metadata and required offset arithmetic; and
- aliasing.

Direct exact-view self-assignment is a no-op. Other possible overlap is
conservatively rejected before mutation. The successful computational path
allocates no result storage, temporary, or workspace; packs and transfers
nothing; performs no hidden synchronization; and does not fall back to another
backend.

The selected reductions are `ReduceSum`, `ReduceMin`, and `ReduceMax`.
Reduction uses deterministic dimension-zero-fastest logical order. Empty sum
returns zero. Empty minimum and maximum return `kInvalidArgument`.
Boolean-valued expressions are excluded. Integral sum overflow returns
`kOverflow` rather than executing signed overflow. For floating minimum and
maximum, the first logical value initializes the result and each later value
replaces it only when the ordinary `<` or `>` comparison succeeds; this makes
NaN and signed-zero behavior order-dependent in the documented traversal.

## Serial reference linear algebra

Milestone 3 provides:

```text
Copy
Scal
Axpy
Dot
Nrm2
Gemv
Gemm
```

The operations accept `float` or `double` views, an explicit serial execution
context, arbitrary validated non-negative strides, and caller-provided
destinations where applicable. Matrix operations accept `kNone` and
`kTranspose`; conjugation is deferred.

`Copy`, `Scal`, and `Axpy` accept rank-one or rank-two views. `Dot` and `Nrm2`
accept rank-one views. `Gemv` and `Gemm` use rank-two matrices whose shape is
`{rows, columns}`; transpose changes the logical operand dimensions without
changing its mapping.

All operations validate the requested backend, host accessibility, shape,
validated mapping bounds, and forbidden output overlap before destination
mutation. They allocate no storage or workspace, perform no packing or
transfer, and do not select or fall back to a provider on the successful
computational path. Floating numerical overflow during a valid computation
follows the native scalar behavior.

`Copy` permits exact source/destination identity as a no-op. `Scal` and `Axpy`
permit the mathematically valid same-index cases described by their
operations. `Gemv` and `Gemm` require output not to overlap any input. Other
possible output overlap is rejected conservatively.

`Dot` accumulates in deterministic logical-index order. `Nrm2` uses scaled
sum-of-squares so finite values do not overflow merely because a naive
square-and-sum would overflow. `Gemv` and `Gemm` do not read the prior
destination when `beta == 0`.

The scalar operations and reductions use the operation scalar type; there is
no hidden promotion or precision change. NaN, infinity, rounding, signed zero,
and contraction follow ordinary native floating-point arithmetic and the
active compiler mode. No reproducible-math or floating exception-status layer
is provided.

`Nrm2` returns NaN if any input is NaN; otherwise it returns infinity if any
input is infinite. An empty or all-zero vector has norm zero.

## Dense CUDA context

`DenseCudaContext::Create` takes a CUDA `ExecutionContext` by value, verifies
its provider state, creates one cuBLAS handle bound to that context's stream,
uses host scalar pointer mode, and disables atomic reduction algorithms.
Deterministic execution requests select pedantic/default-precision math;
backend-default requests retain the approved default-precision provider mode.

The dense context is move-only and owns its cuBLAS state. Creation may allocate
provider state. A moved-from object supports destruction, move assignment, and
inspection of its retained `execution_context()`; provider operations return
`kInvalidState`. `execution_context()` returns a non-owning reference valid
until that particular dense context object is destroyed.

Destroying or replacing a live dense context destroys its cuBLAS handle.
cuBLAS 12.9 documents that handle destruction implicitly performs a
device-wide synchronization, so context teardown is a synchronization and
performance boundary even though successful submitted operations are
asynchronous and event-tracked.

Despite accepting `const DenseCudaContext&`, a provider operation submits work
through its handle and stream. One instance must not be used concurrently.
Separate dense contexts and streams may submit concurrently. The dense
context, its execution context, every operand/resource, and any caller-owned
state must outlive the returned event's completion.

## CUDA pointwise evaluation

`CudaEvaluate` supports exactly `float` and `double`, compile-time rank zero
through eight, and unique non-negative-stride device views. Its bounded
expression set is:

- a dense terminal copied into a distinct destination;
- a rank-zero scalar fill;
- one `MakeNegate` node whose operand is a dense terminal; and
- one `MakeAdd`, `MakeSubtract`, or `MakeMultiply` node whose operands are
  dense terminals or exact-type rank-zero scalars.

Nested nodes, other operations, arbitrary external expression adapters,
different scalar types, ranks above eight, non-device placement, repeated
addresses, and shape mismatch return an explicit error before launch. They are
never evaluated through the serial host path.

Logical traversal is dimension-zero-fastest and does not depend on left,
right, or padded-stride physical storage. Exact terminal self-evaluation is a
no-op event. Every other possible destination overlap is rejected. Success
allocates no result, packing buffer, or ASC workspace; performs no host/device
transfer or fallback; launches on the explicit stream; and returns a
completion event without waiting.

## CUDA dense algebra

Every CUDA operation returns `Result<CompletionEvent>`:

```text
CudaCopy  CudaScal  CudaAxpy  CudaGemv  CudaGemm
```

`CudaCopy`, `CudaScal`, and `CudaAxpy` use project-owned kernels for
`float`/`double` rank-one and rank-two unique non-negative-stride device views.
They preserve logical behavior across validated left, right, and padded-stride
mappings. Exact copy identity is a no-op. Any accepted in-place `Scal` or
`Axpy` case follows the operation's explicit same-index contract; unsafe
overlap is rejected before enqueue.

`CudaGemv` and `CudaGemm` call typed float/double cuBLAS operations. Matrix
operands must have a cuBLAS-compatible column-major mapping and checked leading
dimension; row-major and arbitrary strided matrix mappings return
`kUnsupported`. `kNone` and `kTranspose` are the only matrix operations.
Vector increments, shapes, dimensions, and leading dimensions are checked
before conversion to the provider's integer width.

GEMV output must not overlap its matrix or input; GEMM output must not overlap
either input matrix. When `beta == 0`, neither operation requires a prior
output value. Operations allocate no ASC workspace and perform no hidden
packing, transfer, synchronization, precision change, or provider fallback.
The cuBLAS handle may own provider-internal state established at context
creation.

`Dot` and `Nrm2` remain serial because their current host-scalar return type
would require an implicit wait. CUDA reductions, factorization, solvers,
complex and mixed precision, fast-math/Tensor Core modes, batching, native
handles, external streams, explicit workspace, and cuSOLVER are not part of
this milestone.

## Explicit CUDA dense example

This example performs named synchronous host/device clones around one
asynchronous device operation:

```cpp
#include <asc/core/providers/cuda.h>
#include <asc/dense.h>
#include <asc/dense/providers/cuda.h>

#include <array>

int main() {
  using Vector = asc::DenseArray<float, asc::Extents<4>>;

  auto extents = asc::Extents<4>::Create();
  if (!extents.ok()) {
    return 1;
  }

  asc::HostMemoryResource host_resource;
  auto source = Vector::Create(*extents, host_resource);
  if (!source.ok()) {
    return 1;
  }
  auto source_view = source->view();
  if (!source_view.ok()) {
    return 1;
  }
  for (asc::index_t index = 0; index < 4; ++index) {
    const std::array<asc::index_t, 1> coordinate = {index};
    auto element = source_view->At(coordinate);
    if (!element.ok()) {
      return 1;
    }
    **element = static_cast<float>(index + 1);
  }

  const asc::Device device = {.backend = asc::Backend::kCuda, .ordinal = 0};
  auto device_resource =
      asc::CudaMemoryResource::Create(device, asc::MemorySpace::kDevice);
  auto execution = asc::CreateCudaExecutionContext(device);
  if (!device_resource.ok() || !execution.ok()) {
    return 1;
  }
  auto dense_context = asc::DenseCudaContext::Create(*execution);
  if (!dense_context.ok()) {
    return 1;
  }

  auto device_array = source->Clone(**device_resource, *execution);
  if (!device_array.ok()) {
    return 1;
  }
  auto device_view = device_array->view();
  if (!device_view.ok()) {
    return 1;
  }

  auto scaled = asc::CudaScal(*dense_context, 2.0F, *device_view);
  if (!scaled.ok() || !scaled->Wait().ok()) {
    return 1;
  }

  auto result = device_array->Clone(host_resource, *execution);
  if (!result.ok()) {
    return 1;
  }
  auto result_view = result->view();
  if (!result_view.ok()) {
    return 1;
  }
  for (asc::index_t index = 0; index < 4; ++index) {
    const std::array<asc::index_t, 1> coordinate = {index};
    auto element = result_view->At(coordinate);
    if (!element.ok() || **element != 2.0F * static_cast<float>(index + 1)) {
      return 1;
    }
  }
  return 0;
}
```

The declaration order keeps both memory resources, the execution/dense
contexts, and all owners alive through completion. `Clone` waits internally;
`CudaScal` does not, so the example waits its event explicitly.

## Failure, mutation, and cost

Recoverable failures return `Status` or `Result<T>`; the dense public
production surface exposes no exception API. Invalid public inputs are checked
before destination mutation.

No-allocation statements describe dense computational storage, temporaries,
workspace, and packing on successful paths. A validation or operational
failure constructs a core `Status` diagnostic backed by `std::string` and may
therefore allocate heap memory. The component does not promise a generally
heap-free diagnostic path.

| Surface | Ownership and lifetime | Mutation on failure | Cost and hidden work |
| --- | --- | --- | --- |
| layout mapping | value metadata | no mapping published | rank-dependent validation; no dense storage allocation |
| view/subview | non-owning descriptor | no descriptor published | rank-dependent validation; no dense storage allocation |
| initialized array creation | move-only host buffer; resource must outlive it | no owner published | one allocation and value initialization |
| uninitialized array creation | move-only buffer in the resource's explicit space | no owner published | one allocation; no element access or initialization |
| clone | new move-only buffer | source unchanged; no clone published | one allocation, explicit byte copy, and event wait |
| discard-resize | existing move-only buffer | original owner unchanged | one replacement allocation and initialization |
| expression evaluation | caller-owned destination | unchanged after validation failure | one scalar expression read/write per logical coordinate |
| reductions | caller retains source | no source mutation | linear logical traversal and scalar accumulation |
| level-1 operations | caller-owned views | destination unchanged after validation failure | linear traversal; no computational storage allocation |
| `Gemv` | caller-owned operands | destination unchanged after validation failure | conventional quadratic matrix-vector work |
| `Gemm` | caller-owned operands | destination unchanged after validation failure | conventional cubic matrix-matrix work |
| CUDA context | move-only cuBLAS/provider owner | no context published after creation failure | provider state creation may allocate |
| CUDA evaluation/level-1 | caller-owned device views outlive completion | validation failure occurs before enqueue | linear device work plus one completion event; no ASC workspace/packing/transfer |
| CUDA GEMV/GEMM | caller-owned device views and dense context outlive completion | validation failure occurs before cuBLAS invocation | conventional algebra cost plus provider scheduling/event overhead; no ASC workspace/packing/transfer |

Successful numerical operations mutate their documented destination directly;
they are not transactional against arithmetic NaN or infinity produced during
the computation. A provider launch or asynchronous execution failure can occur
after enqueue; the returned event is the completion/error boundary when a
successful submission was published.

## Concurrency and execution

Views and arrays contain no hidden mutable cache. Separate owners are
independent. Concurrent const access is valid when the underlying storage and
element type permit it. The caller must synchronize any accesses involving
overlapping mutable storage.

The serial context is synchronous. A serial operation does not retain its
context, views, mappings, expression, workspace, or storage after return.

CUDA operations are stream-ordered and asynchronous under the event-completion
contract. The event retains core provider completion/execution state but not
the dense context, expressions, views, or storage. Those objects and their
resources outlive completion. Destroying an event does not make early storage
destruction safe and does not synchronize the device. One dense CUDA context
is not concurrently usable; independent contexts/streams can execute
concurrently when their mutable storage does not overlap.

## Provider, GPU, and performance evidence boundary

The base dense component remains a deterministic serial correctness path. It
has no BLAS/LAPACK, Eigen, oneMKL, OpenMP, TBB, CUDA, HIP, or SYCL edge.
`ASC::dense_cuda` is the only implemented optimized/provider facet in M6 and
privately uses CUDA Runtime through `ASC::core_cuda` plus cuBLAS.

GPU evidence is reported separately as **configure-tested**,
**compile-tested**, **runtime-tested**, **parity-tested**, or **skipped**.
Toolkit/device inventory or successful configuration alone is not
runtime/parity evidence. The M6 Publication Checkpoint B records exact
toolkit/compiler, driver, GPU, architecture, operation/layout/scalar coverage,
tolerances, and any skipped case. This guide does not generalize that evidence
to another CUDA version, GPU architecture, operating system, scalar, layout,
or deferred operation.

The project-owned benchmark separates transfers from operations, warms up the
provider, preserves checksums, and records compiler/configuration/hardware/
provider details. Timing is reviewed smoke evidence, not an unstable CI gate
or a general speed guarantee.

## Deferred work and provenance

The cumulative package does not provide rank-reducing slicing, negative strides,
repeated-address views, runtime-rank or shared owners, external adoption,
reshape, optimized CPU kernels, factorization, solvers, complex or mixed
precision, batching, tensor contraction, hidden materialization, or provider
fallback. Sparse and random CUDA belong to Milestone 7 and are not present.

The CUDA implementation is project-owned and follows the frozen
[Milestone 6 contract](../development/asc-cpp-m6-gpu-core-dense/milestone-contract.md)
and approved ADRs. No MdeCpp, deleted asc-cpp, third-party sample, production,
test, table, vector, benchmark, or documentation material is copied. CUDA and
cuBLAS are optional, separately supplied provider libraries rather than
vendored or redistributed project material.
