# Dense module

`ASC::dense` is the provider-free CPU foundation for fixed-rank dense storage,
views, expression evaluation, deterministic reductions, and a narrow serial
linear-algebra reference path. It is a compiled C++20 library with exactly two
direct ASC dependencies: `ASC::core` and `ASC::expression`. Milestone 6 adds
the separately requested `ASC::dense_cuda` provider facet without adding a
provider edge to the base module.

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense)
target_link_libraries(my_target PRIVATE ASC::dense)
```

```cpp
#include <asc/dense.h>
```

Dense does not depend on Utilities, Random, Sparse, a retired Array or Linalg
component, or an external provider. All public declarations are directly in
`namespace asc`.

## Metadata and layouts

Dense uses Core's signed 64-bit `extent_t`, `index_t`, and `stride_t`
vocabulary. Rank remains a `std::size_t` template argument. Rank zero is a
scalar descriptor with logical and required span size one. If any extent is
zero, both sizes are zero.

`DenseLayout<Rank>::Create` constructs a checked mapping. `LayoutLeft` is
column-major and is the named default. `LayoutRight` is row-major.
`LayoutStride<Rank>` accepts explicit non-negative strides. Mapping creation
validates every extent, stride, product, sum, offset, required span, and
integral conversion before publishing a mapping.

Mutable views require a mapping whose uniqueness has been proven. Owners
require mappings that are both unique and exhaustive. A valid padded
non-exhaustive mapping can therefore describe a view but not owning storage.
Negative strides and repeated-address mappings are not supported.

Logical coordinate traversal always increments dimension zero fastest,
independent of physical layout.

The mapping exposes `extents()`, `strides()`, `logical_size()`,
`required_span_size()`, `is_unique()`, `is_exhaustive()`, and `kind()`.
`Offset` bounds-checks a complete coordinate and returns a checked
`std::size_t` offset.

```cpp
const std::array<asc::extent_t, 2> shape{2, 3};
auto mapping = asc::DenseLayout<2>::Create(shape);  // LayoutLeft
if (!mapping.ok()) {
  return mapping.status();
}
// strides == [1, 2], so coordinate [1, 2] has offset 5.
```

## Views

`DenseView<Element, Rank>` is a trivially copyable, non-owning descriptor. It
retains an element pointer, validated mapping metadata, and an explicit
`MemorySpace`; it never retains or deallocates storage. A nonempty view cannot
have a null pointer.

Element constness controls mutation. A mutable-element view converts to the
corresponding const-element view, but the reverse conversion is unavailable.
Bounds-checked element access reports a `Result` containing an element pointer.
Host dereference is valid only for host-accessible storage. Host and
pinned-host views may be accessed through `At`; device and managed views
remain descriptors and reject host dereference.

Rank-preserving subviews take an offset and extent in every dimension. They
allocate no storage, preserve strides and memory space, and validate the
complete requested region before publishing a result. Failure leaves the
source descriptor unchanged. A subview does not extend either its source view
or the referenced storage lifetime.

Dense views participate in the storage-neutral Expression protocol through
`ExpressionAdapter`. The adapter exposes exact shape, rank, value type,
`ExpressionOperation::kTerminal`, structure-preserving sparsity, scalar reads,
and conservative alias identity. Expression itself does not include a Dense
header or gain ownership of referenced storage.

## Owning arrays

`DenseArray<Element, ExtentsType>` is a move-only owner backed by a Core
`Buffer` allocated from an explicit `MemoryResource`. It supports non-cv
arithmetic elements other than `bool`; they are trivially copyable and
trivially destructible. Existing `Create` remains host-only and
value-initializes every element.

`CreateUninitialized(extents, resource, layout)` accepts every valid Core
memory space and allocates the exact unique, exhaustive owner span without
touching its elements. Use it when a provider operation or an explicit copy
will initialize storage. Reading an uninitialized element is a caller error.
Layout-left and layout-right owners are available; arbitrary-stride and padded
mappings remain view-only.

The extents type may mix static and dynamic Core `Extents`. Owning mappings
are contiguous layout-left or layout-right mappings; arbitrary-stride and
padded mappings remain view-only. Mutable owners produce mutable-element
views, while const owners produce only const-element views.

The memory resource is non-owning state: it must outlive the array and the
array's eventual deallocation. Every view is also non-owning and becomes
invalid when its storage is released.

A named deep clone takes an explicit destination resource and execution
context. It allocates an uninitialized destination, enqueues `CopyBytes`,
waits for that event, and publishes the new owner only after successful
completion. Clone is therefore synchronous even for CUDA. It performs no
implicit fallback or extra staging.

Discard-resize uses the owner's resource and is transactional: allocation or
validation failure leaves the array and every existing view unchanged;
success discards prior values and invalidates every prior view.

```cpp
using MatrixExtents =
    asc::Extents<asc::kDynamicExtent, asc::kDynamicExtent>;

auto extents = MatrixExtents::Create(2, 3);
if (!extents.ok()) {
  return extents.status();
}

asc::HostMemoryResource resource;
auto matrix =
    asc::DenseArray<double, MatrixExtents>::Create(resource, *extents);
if (!matrix.ok()) {
  return matrix.status();
}

auto view = matrix->view();
if (!view.ok()) {
  return view.status();
}
const std::array<asc::index_t, 2> coordinate{1, 2};
auto element = view->At(coordinate);
if (!element.ok()) {
  return element.status();
}
**element = 4.0;
```

## Expression evaluation and reductions

Dense evaluates a `ReadableExpression` into a caller-provided mutable view.
Evaluation accepts an explicit `ExecutionContext` and supports only the serial
backend and ordinary `MemorySpace::kHost` storage. Pinned-host storage is
dereferenceable through `At`, but the serial evaluator does not treat that as
permission to execute there. Ranked expressions require an exact rank and
extent match, and the expression value type must exactly match the destination
element type. A rank-zero expression expands to every coordinate of a ranked
destination.

Backend, memory, shape, and alias validation completes before any destination
element changes. Direct exact-view self-assignment is a permitted no-op. Every
other possible destination overlap is rejected conservatively. Evaluation
uses deterministic logical-coordinate order and performs no allocation,
packing, transfer, synchronization, or fallback.

`ReduceSum`, `ReduceMin`, and `ReduceMax` use the same deterministic logical
order. Empty sum returns zero. Empty minimum and maximum report
`kInvalidArgument`; they do not manufacture an identity value. Integral
`ReduceSum` checks every accumulation step and reports `kOverflow` instead of
returning a wrapped result. Floating reductions retain ordinary IEEE
addition in the documented logical order.

```cpp
const asc::ExecutionContext context = asc::ExecutionContext::Serial();
asc::Status evaluation = asc::Evaluate(context, 2.0, *view);
if (!evaluation.ok()) {
  return evaluation;
}
auto sum = asc::ReduceSum(context, *view);
if (!sum.ok()) {
  return sum.status();
}
// *sum == 12.0
```

## Serial reference BLAS

The `<asc/dense/blas.h>` header declares the compiled reference operations
`Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and `Gemm`. They accept explicit
serial execution, operate on rank-one or rank-two ordinary host views, and
support only `float` and `double`.
`DenseBlasTranspose` has `kNone` and `kTranspose`; conjugate transpose is
unavailable.

Every operation validates backend, memory, complete shape, checked arithmetic,
and forbidden output overlap before mutation. `Copy` permits exact
source/destination identity as a no-op. `Scal` and `Axpy` permit exact identity
when the same-index mathematical operation is well-defined. Other possible
output overlap is rejected.

`Dot` accumulates in logical index order. `Nrm2` uses scaled sum-of-squares so
intermediate squares do not needlessly overflow or underflow. A zero `beta` in
`Gemv` or `Gemm` guarantees that the prior destination is not read. NaN and
infinity otherwise follow ordinary IEEE arithmetic without normalization.

## Optional CUDA Dense facet

Build asc-cpp with `ASC_CPP_ENABLE_CUDA=ON` and an explicit
`CMAKE_CUDA_ARCHITECTURES` value as described in the
[Core CUDA configuration](core.md#optional-cuda-runtime-facet). CUDA is off by
default. An installed consumer requests the Dense provider explicitly:

```cmake
find_package(ASCCpp 0.9 CONFIG REQUIRED COMPONENTS dense_cuda)
target_link_libraries(my_target PRIVATE ASC::dense_cuda)
```

```cpp
#include <asc/core/providers/cuda.h>
#include <asc/dense.h>
#include <asc/dense/providers/cuda.h>
```

The component closure is exactly
`core;expression;dense;core_cuda;dense_cuda`. The provider target privately
uses cuBLAS, while public provider headers expose no CUDA or cuBLAS SDK type.
Requesting only `dense` or the provider-free `cpp` aggregate does not discover
CUDAToolkit or import either provider target.

### Context and asynchronous lifetime

`DenseCudaContext::Create(execution_context)` accepts only a CUDA execution
context. It is move-only, owns one cuBLAS handle bound to that context's
stream, and exposes the provider-neutral context through
`execution_context()`. Host scalar pointer mode, deterministic math policy,
and disabled atomic reductions are provider implementation details of the
frozen contract, not native-handle interoperability.

One `DenseCudaContext` is not concurrently mutable. Independent provider
contexts and streams may execute concurrently when their storage does not
race. All CUDA evaluation and algebra operations return
`Result<CompletionEvent>` and do not wait. Until the event completes, keep the
Dense provider context, its execution context, every resource and owner, all
views and expression nodes, and the referenced storage alive. Do not mutate,
move, resize, reset, or destroy an operand or destination while work using it
is pending. Destroying the event does not complete the work.

### Device owners and explicit transfer

Use `DenseArray::CreateUninitialized` with a CUDA resource to create pinned,
device, or managed storage without a host initialization pass. Device and
managed views cannot be dereferenced through `DenseView::At`. A named `Clone`
with the matching explicit execution context is the synchronous deep-copy
boundary between supported spaces; construction, view creation, resize,
evaluation, and algebra never transfer implicitly.

```cpp
constexpr std::int32_t kDeviceOrdinal = 0;

auto resource_result =
    asc::CudaMemoryResource::Create(kDeviceOrdinal, asc::MemorySpace::kDevice);
if (!resource_result.ok()) {
  return resource_result.status();
}
std::unique_ptr<asc::CudaMemoryResource> device_resource =
    std::move(*resource_result);

using VectorExtents = asc::Extents<asc::kDynamicExtent>;
auto extents = VectorExtents::Create(1024);
if (!extents.ok()) {
  return extents.status();
}
auto vector = asc::DenseArray<float, VectorExtents>::CreateUninitialized(
    *extents, *device_resource);
if (!vector.ok()) {
  return vector.status();
}

auto execution =
    asc::CreateCudaExecutionContext(kDeviceOrdinal);
if (!execution.ok()) {
  return execution.status();
}
auto dense_context = asc::DenseCudaContext::Create(*execution);
if (!dense_context.ok()) {
  return dense_context.status();
}
auto view = vector->view();
if (!view.ok()) {
  return view.status();
}
auto fill = asc::CudaEvaluate(*dense_context, 0.0F, *view);
if (!fill.ok()) {
  return fill.status();
}
asc::Status completed = fill->Wait();
if (!completed.ok()) {
  return completed;
}
// device_resource must outlive vector and its eventual deallocation.
```

### Bounded pointwise evaluation

`CudaEvaluate` supports exactly unqualified `float` and `double`, ranks zero
through eight, and unique nonnegative-stride device views. Its expression
surface is intentionally closed:

- a Dense terminal copied into a distinct destination;
- a rank-zero scalar fill;
- one `Negate` node whose operand is a Dense terminal; or
- one `Add`, `Subtract`, or `Multiply` node with Dense-terminal or rank-zero
  scalar operands.

Nested nodes and arbitrary external adapters return `kUnsupported`; they are
never read on the host. Rank, shape, placement, device, address span, and
alias checks complete before launch. Exact terminal self-evaluation returns a
no-op event; other destination overlap fails. Evaluation follows
dimension-zero-fastest logical order. It performs no allocation, workspace,
packing, transfer, fallback, or hidden wait.

### CUDA algebra subset

The asynchronous operations are `CudaCopy`, `CudaScal`, `CudaAxpy`,
`CudaGemv`, and `CudaGemm`.

Copy, Scal, and Axpy accept only `float` or `double` rank-one and rank-two
unique nonnegative-stride device views. Their project-owned kernels handle
layout-left, layout-right, and valid padded mappings without packing.

Gemv and Gemm use typed float/double cuBLAS calls and
`MatrixOperation::kNone` or `MatrixOperation::kTranspose`. Matrix operands
must have cuBLAS-compatible column-major layout-left/leading-dimension
mappings. Layout-right and arbitrary strided matrices return `kUnsupported`
before enqueue. Dimensions, leading dimensions, and vector increments are
checked before narrowing to provider integers. Output overlap with any input
is rejected. When `beta == 0`, the prior output value is not read.

No CUDA Dense operation allocates ASC workspace, transfers, packs, changes
precision, silently waits, or falls back. Dot and Nrm2 are absent because a
host-scalar return would require hidden synchronization or a new asynchronous
scalar owner. Factorizations, solvers, reductions, batching, complex or mixed
precision, native handles, and Tensor Core or fast-math modes are also outside
this facet.

### Errors, costs, and evidence

Unsupported rank, scalar, expression, layout, placement, alias, or
determinism fails through `Status` before enqueue. Provider failures preserve
an ASC code, provider name, and signed native code. Message text and native
codes are diagnostic rather than portable control-flow values.

The caller-visible costs are one explicitly created stream per Core CUDA
context, one cuBLAS handle per Dense CUDA context, explicit owner allocations,
explicit transfer or clone operations, at most one kernel or cuBLAS enqueue
per nontrivial operation, and one completion event. No-op and empty operations
need not launch a kernel. The pointwise and level-one operations use no
workspace. Gemv/Gemm use no ASC-managed workspace.

Milestone 6 evidence labels are independent. `core_cuda` requires
**configure-tested**, **compile-tested**, and **runtime-tested** evidence.
`dense_cuda` additionally requires **parity-tested** evidence. Documentation
or successful toolkit discovery alone establishes none of these labels; the
Publication Checkpoint B report records the exact toolkit, compiler, driver,
device, compute capability, architecture code, operations, layouts, sizes,
tolerances, and any skip.

## Allocation, transfer, and provider boundary

Dense supplies a deterministic serial correctness baseline, not an optimized
provider. Operations use caller-owned destinations and never hide allocation,
temporary storage, packing, transfer, precision conversion, synchronization,
or fallback. The optional CUDA facet follows the same explicit policy. An
unsupported context, memory space, scalar type, layout, expression, or
operation returns a status rather than selecting another implementation.

## Lifetime and concurrency

An array's resource outlives the array, and an array outlives every use of its
views and every expression node that captures those views. Capturing a view by
value copies only its non-owning descriptor. Moving an owner transfers
ownership but does not turn existing descriptors into owning references.
Destroying the owner or successfully resizing it invalidates prior views.

Dense adds no synchronization to caller-owned storage. Concurrent reads are
valid only while the referenced objects and bytes remain alive and no thread
mutates, moves, resizes, or destroys them. Any concurrent write requires
caller-provided synchronization and non-overlapping access under the C++
memory model. CUDA event dependencies and stream ordering additionally govern
device storage. One `DenseCudaContext` is not concurrently mutable;
independent contexts may submit independent work.

## Deliberately absent

Milestone 6 Dense provides no:

- Sparse storage or operation;
- provider edge in `ASC::dense` or `ASC::cpp`;
- runtime-rank owner or rank-reducing slice;
- negative-stride or repeated-address view;
- shared ownership, external adoption, or custom deleter;
- hidden temporary, packing, transfer, synchronization, or fallback;
- complex, mixed-precision, batched, or tensor operation;
- factorization, solver, or workspace-bearing algorithm;
- optimized CPU provider; or
- CUDA Dot, Nrm2, reduction, arbitrary external expression evaluation, general
  broadcasting, native handle/stream adoption, or hidden workspace;
- CUDA layout-right/arbitrary-stride Gemv or Gemm;
- Sparse CUDA, Random CUDA, HIP, or SYCL; or
- OpenMP, TBB, Eigen, BLAS/LAPACK, or oneMKL integration.

The [frozen Milestone 6 contract][contract] is authoritative for the CUDA
facet. The [Milestone 3 contract][dense-contract] remains the provider-free
Dense authority.

[contract]: ../development/asc-cpp-m6-gpu-core-dense/milestone-contract.md
[dense-contract]: ../development/asc-cpp-m3-dense-cpu/milestone-contract.md
