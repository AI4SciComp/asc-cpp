# Dense module

`ASC::dense` is the provider-free CPU foundation for fixed-rank dense storage,
views, expression evaluation, deterministic reductions, and a portable serial
BLAS reference path. It is a compiled C++20 library with exactly two
direct ASC dependencies: `ASC::core` and `ASC::expression`. CUDA Core and Dense adds
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
arithmetic elements other than `bool`, plus `std::complex<float>` and
`std::complex<double>`; accepted elements remain trivially copyable and
trivially destructible. Existing `Create` remains host-only and
value-initializes every element. Complex storage starts typed array and element
lifetimes using C++20 non-allocating placement array construction.

For arithmetic elements, `CreateUninitialized(extents, resource, layout)`
accepts every valid Core memory space and allocates the exact unique,
exhaustive owner span without touching its elements. Use it when a provider
operation or an explicit copy will initialize storage. Reading an
uninitialized arithmetic element is a caller error. Complex owners support
host/pinned-host storage only: their typed default construction initializes
complex values to zero even in this named factory. Device/managed complex
creation is rejected before allocation or host access.
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
context. For arithmetic elements it allocates an uninitialized destination, enqueues `CopyBytes`,
waits for that event, and publishes the new owner only after successful
completion. Clone is therefore synchronous even for CUDA. It performs no
implicit fallback or extra staging.

Complex cloning requires the existing serial host execution contract and
host/pinned-host resources, copying typed live elements without a transfer.
`ReduceSum` supports complex algebra; ordered `ReduceMin` and `ReduceMax`
remain constrained to arithmetic elements. Exact scalar matching still
rejects implicit real/complex or complex precision conversions.

## Bounded value display and LAPACK foundations

Include `<asc/dense/print.h>` explicitly for `PrintArray`. It writes a bounded
logical-value preview of an owner or view to a supplied Core `ByteSink`, using
caller-owned scratch, options and progress report. It does not evaluate lazy
expressions, allocate a formatting buffer, transfer device values or select a
provider. Rank-zero, empty and higher-rank output and truncation are defined
by the [display contract](../contracts/array-display-v1.md). This display is
not a serialization format.

The narrow `<asc/dense/lapack/{types,workspace,report,factor_view,structured_view}.h>` headers
define provider-neutral copied identities, caller workspace plans, mandatory
failure-surviving reports, and family-tagged raw pivots/factors. They reuse
the checked Dense BLAS full-matrix descriptor. A raw singular factor cannot
be promoted into a successful reusable factor view. These foundations alone
provide no external LAPACK dispatch or numerical capability; the
[source inventory](../contracts/lapack-upstream-inventory.json) and
[coverage ledger](../contracts/lapack-coverage.yaml) retain required,
implemented and verified states separately.

Distinct LU-factor band, positive-definite band, tridiagonal, square
bidiagonal and RFP storage descriptors expose their actual packed arrays and
validate full reachable backing spans without densifying. Successful LU,
Cholesky and Householder QR views borrow factors and retain their originating
family; these are representations, not factorization implementations.
Indefinite variant-specific factor arrays, blocked reflectors and rectangular
bidiagonal storage remain pending with their associated routine contracts.

Include `<asc/dense/lapack/lu.h>` for native `Getrf` and `Getrs` overloads
taking an explicit serial `ExecutionContext`. They support all four LAPACK
real/complex scalar types, rectangular partial-pivot factors, reusable square
solves with multiple RHS and N/T/C operations, and both physical layouts.
Callers provide the matrix, one-based signed 64-bit pivots, RHS and report;
the native route uses no scratch allocation or external provider. Exact-zero
singularity is a numerical failure with inspectable partial factors, not a
successful reusable factor. See the [general LU contract](../contracts/lapack-general-lu.md)
for reconstruction, provenance, aliasing, mutation and evidence boundaries.

## Native text and binary archives

Include `<asc/dense/io.h>` explicitly for ASC text/binary read and write APIs.
`WriteDenseArrayText` and `WriteDenseArrayBinary` traverse logical values in
dimension-zero-fastest order, independent of physical layout and padding.
Wire scalar identity is exact; these formats never implicitly narrow, promote
real data to complex, or evaluate an expression. They are distinct from a
bounded `PrintArray` preview.

`DenseArrayReader::PrepareText`/`PrepareBinary` validate one header using
caller metadata and scratch. A prepared reader retains the current source
position and is consumed once. It borrows the source, metadata, scratch and
report: keep those objects live and unchanged until completion. Failure does
not rewind or resynchronize the source. `ReadDenseArray<T, ExtentsType>`
creates a new owner through an explicit resource and left/right layout;
`ReadDenseArrayInto` uses explicit disjoint typed staging and commits to the
existing view only after full payload/trailer validation. Source-wrapper
`ReadDenseArrayTextInto`/`ReadDenseArrayBinaryInto` also reject destination
aliases before header parsing.

`ArrayIoLimits` bounds bytes, metadata, extents/products, decoded/staging
storage, allocation requests and scratch. Zero is a real zero limit. One
empty-owner resource request still counts. Whole-file EOF checks require
spare input budget for the nonseekable probe; framed reads need not consume
the next frame. `ArrayIoReport` retains actual progress and commit state.

`LoadDenseArrayText`/`LoadDenseArrayBinary` compose checked Core File reads;
`SaveDenseArrayText`/`SaveDenseArrayBinary` require explicit
`ArrayFileOverwrite::kTruncate` and checked write/flush/close. They do not
promise atomic replacement or durability. Borrowed stream codecs allocate no
hidden parser buffers, while path/File and caller stream conveniences retain
their own documented allocation behavior. Source/sink ErrorCode and native
code survive without copying unbounded diagnostic strings.

See the [text](../contracts/array-text-v1.md) and
[binary](../contracts/array-binary-v1.md) contracts and the
[installed read–solve–archive example](../../examples/lapack_array_io/README.md).
Array I/O has no LAPACK dependency; Matrix Market is a separate required
interchange package and is not supplied by these native format APIs.

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

The `<asc/dense/blas.h>` header declares the complete frozen classic Level 1,
Level 2, and Level 3 surfaces. Level 1 contains `Rotg`, `Rotmg`, `Rot`, `Rotm`, `Swap`,
`Scal`, `Copy`, `Axpy`, `Dot`, `Dotu`, `Dotc`, `Nrm2`, `Asum`, and `Iamax`.
The real families support `float` and `double`; applicable complex families
support `std::complex<float>` and `std::complex<double>`. The `Dot` overloads
also cover the frozen `sdsdot` bias form, whose products and bias are
accumulated in `double` before conversion to `float`, and the
float-input/double-result `dsdot` form.

Level 1 operations use `DenseBlasVectorView<Element>`. It stores a pointer to
logical element zero, a signed 64-bit size, a nonzero signed increment, and a
caller-supplied backing `ConstMemoryView`. `Create` proves the complete
reachable byte span, including negative increments, before publishing the
descriptor. It never owns, extends, or deallocates the backing storage.
Ordinary `DenseView` layout, arrays, expressions, and the existing overloads
are unchanged.

```cpp
std::array<double, 5> x_storage{1.0, 0.0, 2.0, 0.0, 3.0};
std::array<double, 5> y_storage{4.0, 0.0, 5.0, 0.0, 6.0};
std::array<double, 1> result_storage{};

auto x = asc::DenseBlasVectorView<const double>::Create(
    x_storage.data() + 4, 3, -2,
    asc::ConstMemoryView(x_storage.data(), sizeof(x_storage),
                         asc::MemorySpace::kHost));
auto y = asc::DenseBlasVectorView<const double>::Create(
    y_storage.data(), 3, 2,
    asc::ConstMemoryView(y_storage.data(), sizeof(y_storage),
                         asc::MemorySpace::kHost));
auto result = asc::DenseBlasVectorView<double>::Create(
    result_storage.data(), 1, 1,
    asc::ConstMemoryView(result_storage.data(), sizeof(result_storage),
                         asc::MemorySpace::kHost));
if (!x.ok() || !y.ok() || !result.ok()) {
  return asc::Status(asc::ErrorCode::kInvalidArgument,
                     "invalid BLAS descriptor");
}
asc::Status status =
    asc::Dot(asc::ExecutionContext::Serial(), *x, *y, *result);
// result_storage[0] == 28.0: 3*4 + 2*5 + 1*6.
```

Level 2 contains `Gemv`, `Gbmv`, `Hemv`, `Hbmv`, `Hpmv`, `Symv`, `Sbmv`,
`Spmv`, `Trmv`, `Tbmv`, `Tpmv`, `Trsv`, `Tbsv`, `Tpsv`, `Ger`, `Geru`,
`Gerc`, `Her`, `Hpr`, `Her2`, `Hpr2`, `Syr`, `Spr`, `Syr2`, and `Spr2` for
every mathematically applicable S/D/C/Z row. Its checked non-owning matrix
descriptors are `DenseBlasMatrixView`, `DenseBlasBandMatrixView`,
`DenseBlasTriangularBandView`, and `DenseBlasPackedMatrixView`. Each retains a
caller-owned backing span and explicit row- or column-major layout. Bandwidth,
leading dimension, packed span, alignment, overflow, and reachability are
proved by `Create` before a descriptor is published.

```cpp
std::array<double, 6> matrix_storage{1.0, 4.0, 2.0, 5.0, 3.0, 6.0};
std::array<double, 3> input_storage{1.0, 2.0, -1.0};
std::array<double, 2> output_storage{};

auto matrix = asc::DenseBlasMatrixView<const double>::Create(
    matrix_storage.data(), 2, 3, asc::DenseBlasLayout::kColumnMajor, 2,
    asc::ConstMemoryView(matrix_storage.data(), sizeof(matrix_storage),
                         asc::MemorySpace::kHost));
auto input = asc::DenseBlasVectorView<const double>::Create(
    input_storage.data(), 3, 1,
    asc::ConstMemoryView(input_storage.data(), sizeof(input_storage),
                         asc::MemorySpace::kHost));
auto output = asc::DenseBlasVectorView<double>::Create(
    output_storage.data(), 2, 1,
    asc::ConstMemoryView(output_storage.data(), sizeof(output_storage),
                         asc::MemorySpace::kHost));
if (!matrix.ok() || !input.ok() || !output.ok()) {
  return asc::Status(asc::ErrorCode::kInvalidArgument,
                     "invalid Level 2 descriptor");
}
asc::Status level2_status = asc::Gemv(
    asc::ExecutionContext::Serial(), asc::DenseBlasTranspose::kNone, 1.0,
    *matrix, *input, 0.0, *output);
// output_storage == {2.0, 8.0}.
```

`DenseBlasTranspose` includes none, transpose, and conjugate transpose;
triangular and structured operations take explicit upper/lower and
unit/nonunit controls. General, symmetric, and Hermitian matrix-vector calls
reject output overlap with a read operand. Triangular multiply and solve
update their vector in place but reject overlap with matrix storage. Rank
updates reject matrix overlap with input vectors. The referenced triangle is
the only stored triangle modified, and Hermitian diagonal imaginary parts are
zeroed as required by BLAS.

For `alpha == 0`, Level 2 does not read multiplicative matrix or vector
operands. For `beta == 0`, it does not read the prior output. Empty operations
preserve the same checked no-work contract. The serial path accepts host and
pinned-host descriptors and executes synchronously in deterministic logical
index order without allocation, packing, transfer, synchronization, or
fallback.

Level 3 contains `Gemm`, `Symm`, `Hemm`, `Syrk`, `Herk`, `Syr2k`, `Her2k`,
`Trmm`, and `Trsm` for all 30 mathematically applicable S/D/C/Z rows.
Every matrix in one call has the same explicit row- or column-major layout.
Structured rank updates modify only the selected triangle; Hermitian updates
ignore stored diagonal imaginary inputs and publish a real diagonal. `Trmm`
and `Trsm` update their general matrix operand in place and accept left/right,
upper/lower, none/transpose/conjugate-transpose, and unit/nonunit controls.

```cpp
std::array<double, 4> left_storage{1.0, 3.0, 2.0, 4.0};
std::array<double, 4> right_storage{2.0, 1.0, 0.0, 2.0};
std::array<double, 4> output_storage{};
auto left = asc::DenseBlasMatrixView<const double>::Create(
    left_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 2,
    asc::ConstMemoryView(left_storage.data(), sizeof(left_storage),
                         asc::MemorySpace::kHost));
auto right = asc::DenseBlasMatrixView<const double>::Create(
    right_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 2,
    asc::ConstMemoryView(right_storage.data(), sizeof(right_storage),
                         asc::MemorySpace::kHost));
auto output = asc::DenseBlasMatrixView<double>::Create(
    output_storage.data(), 2, 2, asc::DenseBlasLayout::kColumnMajor, 2,
    asc::ConstMemoryView(output_storage.data(), sizeof(output_storage),
                         asc::MemorySpace::kHost));
asc::Status level3_status = asc::Gemm(
    asc::ExecutionContext::Serial(), asc::DenseBlasTranspose::kNone,
    asc::DenseBlasTranspose::kNone, 1.0, *left, *right, 0.0, *output);
// output_storage == {4.0, 10.0, 4.0, 8.0}.
```

Level 3 has the same single CPU/GPU semantic contract: validate the complete
call before mutation or submission; do not read multiplicative operands when
`alpha == 0`; do not read the destination when `beta == 0`; accept empty and
rectangular valid shapes; reject forbidden overlap; and never allocate,
transfer, pack, synchronize, or select another backend. The serial path is
synchronous. The CUDA path returns explicit asynchronous completion.

Scalar inputs and outputs for `Rotg`, `Rotmg`, reductions, and mixed dot
products are size-one caller-owned descriptors. Modified-rotation parameter
arrays have size five and unit increment. `Rot` applies
`x' = c*x + s*y`, `y' = c*y - s*x`; complex rotations use real `c` and `s`.
`Dotu` is unconjugated and `Dotc` conjugates the left operand. Complex `Asum`
and `Iamax` use `abs(real) + abs(imag)`. `Iamax` returns the first maximum as
a zero-based logical index and returns `-1` for an empty operand.

Every operation validates execution backend, host placement, shape, checked
descriptor arithmetic, and forbidden overlap before mutation. `Copy` and
`Swap` allow exact identity; `Axpy` allows exact identity and does not read its
source when `alpha` is zero. Partial overlap is rejected conservatively.
Rotations reject overlap between their independently mutable operands.
Reduction results cannot overlap inputs. Empty dot, norm, and absolute-sum
operations publish zero.

Dot products use explicit serial logical-index order. `Nrm2` uses scaled
sum-of-squares so intermediate squares do not needlessly overflow or
underflow. NaN, infinity, signed zero, and subnormal values otherwise follow
the documented operation formula and ordinary IEEE behavior.

The pre-existing ordinary-view `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`,
and `Gemm` overloads remain available for `float` and `double`. Conjugate
transpose is identical to transpose for those real-valued matrix overloads. A
zero `beta` in `Gemv` or `Gemm` guarantees that the prior destination is not
read.

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

One `DenseCudaContext` serializes access to its mutable cuBLAS handle state,
including temporary scalar pointer-mode changes, so independent callers may
submit concurrently when their storage does not race. Separate provider
contexts retain independent handles and streams. All successful CUDA
evaluation and BLAS operations return `Result<CompletionEvent>` and do not
wait. Until the event completes, keep the
Dense provider context, its execution context, every resource and owner, all
views and expression nodes, and the referenced storage alive. Do not mutate,
move, resize, reset, or destroy an operand or destination while work using it
is pending. Destroying the event does not complete the work. If a provider or
kernel failure occurs after work may have entered the stream, the established
recovery path drains that stream before returning an error so no untracked
asynchronous use survives the failed call.

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

### CUDA BLAS

The provider supplies asynchronous `CudaRotg`, `CudaRotmg`, `CudaRot`,
`CudaRotm`, `CudaSwap`, `CudaScal`, `CudaCopy`, `CudaAxpy`, `CudaDot`,
`CudaDotu`, `CudaDotc`, `CudaNrm2`, `CudaAsum`, and `CudaIamax` overloads for
every applicable frozen Level 1 row. It also supplies `CudaGemv`, `CudaGbmv`,
`CudaHemv`, `CudaHbmv`, `CudaHpmv`, `CudaSymv`, `CudaSbmv`, `CudaSpmv`,
`CudaTrmv`, `CudaTbmv`, `CudaTpmv`, `CudaTrsv`, `CudaTbsv`, `CudaTpsv`,
`CudaGer`, `CudaGeru`, `CudaGerc`, `CudaHer`, `CudaHpr`, `CudaHer2`,
`CudaHpr2`, `CudaSyr`, `CudaSpr`, `CudaSyr2`, and `CudaSpr2` for every
applicable frozen Level 2 row. Level 3 adds `CudaGemm`, `CudaSymm`, `CudaHemm`,
`CudaSyrk`, `CudaHerk`, `CudaSyr2k`, `CudaHer2k`, `CudaTrmm`, and `CudaTrsm`
for all 30 applicable rows. The existing ordinary-view `CudaCopy`,
`CudaScal`, `CudaAxpy`, `CudaGemv`, and `CudaGemm` overloads remain available.

Level 1 vector, modified-rotation parameter, scalar-result, and Iamax-result
descriptors all reference caller-owned storage on the CUDA context device.
Vector increments may be positive or negative and preserve the same logical
element-zero convention as the CPU API. By-value coefficients remain host
values. Every modified scalar or reduction result remains in device storage;
the call returns without copying or reading it on the host.

`CudaIamax` additionally requires an aligned device `MutableMemoryView` of at
least `sizeof(index_t)` bytes. For positive increments, cuBLAS writes its
native one-based result there. For negative increments, where cuBLAS defines
no result, a project kernel scans in public logical order and writes the same
one-based intermediate while preserving the first-maximum tie rule. A final
project kernel converts that intermediate to the public zero-based logical
index. The workspace, public result, operand, context, and execution context
must all outlive asynchronous completion. Workspace cannot overlap either
operand or result.

The ordinary-view Copy, Scal, and Axpy overloads accept only `float` or
`double` rank-one and rank-two unique nonnegative-stride device views. Their
project-owned kernels handle layout-left, layout-right, and valid padded
mappings without packing.

The exact Level 2 API accepts the same checked full, band, packed, and
signed-stride descriptors as the serial API, with all nonempty storage on the
CUDA context device. Row- and column-major layouts, transpose and conjugate
transpose, upper and lower triangles, and unit and nonunit diagonals preserve
the serial semantic contract. Representable paths use typed 64-bit cuBLAS
entry points. Approved project kernels implement row-major complex cases that
cannot be represented by a cuBLAS flag transformation without conjugating the
wrong operand. These kernels use caller storage directly and add no workspace.

The ordinary-view Gemv and Gemm overloads use typed float/double cuBLAS calls and
`MatrixOperation::kNone` or `MatrixOperation::kTranspose`. Matrix operands
must have cuBLAS-compatible column-major layout-left/leading-dimension
mappings. Layout-right and arbitrary strided matrices return `kUnsupported`
before enqueue. Dimensions, leading dimensions, and vector increments are
checked before narrowing to provider integers. Output overlap with any input
is rejected. When `beta == 0`, the prior output value is not read.

The exact Level 3 API uses typed 64-bit cuBLAS entry points and direct caller
device storage. Row-major calls use algebraically equivalent flag, side,
triangle, dimension, and operand transformations; no transpose buffer is
created. By-value coefficients remain host values. Matrices and the context
must outlive the returned completion event.

No successful CUDA Dense operation allocates, transfers, packs, silently
waits, or falls back. Extended `sdsdot` and mixed `dsdot` use approved project
kernels because the frozen CUDA provider does not supply their exact
accumulation/result contracts; both still write caller-owned device results
asynchronously. Factorizations, batching, native handles, and Tensor Core or
fast-math modes remain outside this facet.

### Errors, costs, and evidence

Unsupported rank, scalar, expression, layout, placement, alias, or
determinism fails through `Status` before enqueue. Provider failures preserve
an ASC code, provider name, and signed native code. Message text and native
codes are diagnostic rather than portable control-flow values.

The caller-visible costs are one explicitly created stream per Core CUDA
context, one cuBLAS handle per Dense CUDA context, explicit owner allocations,
explicit transfer or clone operations, a bounded provider/kernel enqueue
sequence, and one completion event. `sdsdot` performs extended accumulation
and bias addition in one project kernel; nonempty `CudaIamax` converts the
provider index with a second project kernel. Empty scalar-producing operations
launch a scalar write.
Level 1 uses no workspace except the explicit caller-owned `CudaIamax`
workspace. Levels 2 and 3 use no ASC-managed workspace.

Dense BLAS Level 3 evidence labels are independent. `dense_cuda` requires
**configure-tested**, **compile-tested**, **runtime-tested**, and
**parity-tested** evidence. Documentation or successful toolkit discovery
alone establishes none of these labels; the release verification report records the
exact toolkit, compiler, driver, device, compute capability, architecture
code, operations, layouts, sizes, tolerances, and every skip.

## Allocation, transfer, and provider boundary

Dense supplies a deterministic serial correctness baseline, not an optimized
provider. Successful operations use caller-owned destinations and never hide
allocation, temporary storage, packing, transfer, precision conversion,
synchronization, or fallback. The optional CUDA facet follows the same
explicit policy; its post-enqueue failure drain is documented above. An
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
device storage. A `DenseCudaContext` serializes its mutable provider handle;
independent contexts may submit independent work without that serialization.

## Matrix Market interchange

`asc/dense/matrix_market.h` adds rank-two Matrix Market array reading and writing
without a Sparse or provider dependency. Owning reads use an explicit resource;
view reads use supplied typed staging and commit only after complete validation.
See the [interchange guide](../matrix-market.md),
[normative profile](../contracts/matrix-market-profile.md) and
[standalone example](../../examples/dense_matrix_market/README.md).

## Deliberately absent

The current Dense surface provides no:

- Sparse storage or operation;
- provider edge in `ASC::dense` or `ASC::cpp`;
- runtime-rank owner or rank-reducing slice;
- negative-stride or repeated-address ordinary `DenseView`;
- shared ownership, external adoption, or custom deleter;
- hidden temporary, packing, transfer, synchronization, or fallback;
- mixed-precision, batched, or tensor operation;
- full Reference-LAPACK routine coverage (the explicit facet is incremental);
- optimized CPU provider; or
- CUDA arbitrary external expression evaluation, general broadcasting,
  native handle/stream adoption, or hidden workspace;
- CUDA arbitrary-stride ordinary-view Gemv or Gemm;
- Sparse CUDA, Random CUDA, HIP, or SYCL; or
- OpenMP, TBB, Eigen, optimized CPU BLAS, or oneMKL integration.

The [frozen CUDA Core and Dense contract][contract] is authoritative for the CUDA
facet. The [Dense contract][dense-contract] remains the provider-free
Dense authority.

[contract]: ../architecture/decisions/0013-dense-linalg-providers.md
[dense-contract]: ../architecture/decisions/0011-dense-semantics.md
