# Array module

`ASC::array` owns multidimensional descriptors, storage mappings, non-owning
views, and array ownership. M1 provides a canonical dense host path beside the
MdeCpp-derived arrays retained for compatibility.

| Surface | M1 status | Intended use |
| --- | --- | --- |
| `Extents`, contiguous/stride mappings, and `DefaultAccessor` | Canonical M1 | New descriptor code |
| `TensorView` with element-typed constness | Canonical M1 | New borrowed array access |
| Move-only `Tensor` and explicit `Clone` | Canonical M1 | New owning dense arrays |
| Structural tensor concepts | Canonical M1 | Generic algorithms |
| Linalg M1 operands over canonical views | Canonical consumer | Seven context-first `float`/`double` BLAS operations |
| Random M1 destination views | Canonical consumer | Context-first deterministic unit-uniform fill for `float`/`double` |
| `MShape`, `DenseMArray`, `UArray`, legacy views and expressions | Compatibility, unchanged by M1 | Existing callers only |
| `SparseMArray` and COO/CSR/CSC layouts | Compatibility, unchanged by M1 | Existing sparse callers only |
| Canonical transforms, expressions, broadcasting, and sparse owners | Deferred to Array M2--M4 | Do not rely on them yet |
| Device-only storage, transfer, and asynchronous array operations | Deferred to provider milestones | Do not rely on them yet |

The canonical model is:

```text
Extents + LayoutMapping + Accessor = TensorView
Buffer + contiguous mapping        = Tensor
```

Canonical Array depends only on canonical Core. It does not use the legacy
memory manager, process-wide device policy, implicit mirroring, or provider SDK
headers.

## Include and component

Use the canonical umbrella and minimal installed component:

```cpp
#include <asc/array.h>
```

```cmake
find_package(ASCCpp REQUIRED COMPONENTS array)
target_link_libraries(my_target PRIVATE ASC::array)
```

The umbrella contains only the canonical M1 headers. It does not expose legacy
`DenseMArray`, `UArray`, expressions, or sparse arrays. During migration,
`<asc/cpp.h>` continues to include the canonical umbrella and the legacy dense
aggregate separately.

The installed Array-only consumer verifies the same operations as this
canonical example:

```cpp
#include <asc/array.h>

#include <utility>

int main() {
  using Shape = asc::Extents<2, asc::dynamic_extent, 2>;
  using Tensor = asc::Tensor<int, Shape>;

  auto shape_result = Shape::Create(3);
  if (!shape_result.ok()) return 1;

  auto tensor_result = Tensor::Create(shape_result.value());
  if (!tensor_result.ok()) return 1;

  Tensor tensor = std::move(tensor_result).value();
  auto view_result = tensor.View();
  if (!view_result.ok()) return 1;
  view_result.value()(1, 2, 1) = 7;

  const Tensor& read_only = tensor;
  auto const_view_result = read_only.View();
  if (!const_view_result.ok() || const_view_result.value()(1, 2, 1) != 7) {
    return 1;
  }

  auto clone_result = tensor.Clone(asc::ExecutionContext::Serial(),
                                   asc::MemorySpace::kHost);
  if (!clone_result.ok()) return 1;
  auto clone_view_result = clone_result.value().View();
  if (!clone_view_result.ok() ||
      clone_view_result.value().Data() == view_result.value().Data()) {
    return 1;
  }
  clone_view_result.value()(1, 2, 1) = 9;
  return const_view_result.value()(1, 2, 1) == 7 ? 0 : 1;
}
```

Canonical operations report recoverable validation, allocation, resource, and
capability failures with `Status` or `Result<T>`. Contract-checked element
access is for already-valid program inputs; use checked factories and checked
pointer access for untrusted metadata or coordinates.

## Extents

`Extents<StaticExtents...>` is a fixed-rank descriptor. Each template value is
either a non-negative static extent or `asc::dynamic_extent`. Static and
dynamic dimensions may be mixed in one type.

```cpp
using ScalarShape = asc::Extents<>;
using FixedShape = asc::Extents<3, 4>;
using MixedShape = asc::Extents<3, asc::dynamic_extent, 5>;
using DynamicRank3 = asc::DynamicTensorExtents<3>;
```

Metadata uses signed 64-bit `extent_t`, `index_t`, and `stride_t` types.
`Create(...)` receives exactly the dynamic values in dimension order and
validates that they are non-negative. A default descriptor gives every dynamic
dimension extent zero.

- Rank zero describes one scalar value and has logical size one.
- Any zero extent makes logical size zero.
- `Rank()`, `DynamicRank()`, and `StaticExtent()` describe the type.
- `GetExtent()` describes a runtime dimension.
- `GetSize()` checks multiplication and returns `Result<extent_t>`.
- Descriptor and mapping construction do not allocate.

Metadata larger than `INT_MAX` is valid when its calculations fit the canonical
metadata type. A large descriptor does not imply that an owner can allocate
the corresponding storage.

## Layout mappings

Mappings turn logical coordinates into offsets. The canonical default is
left/column-major and never changes in response to `ASC_USE_ROW_MAJOR` or a
consumer definition.

| Mapping | Meaning |
| --- | --- |
| `LayoutLeftMapping<ExtentsType>` | First dimension has unit stride |
| `LayoutRightMapping<ExtentsType>` | Last dimension has unit stride |
| `LayoutStrideMapping<ExtentsType>` | Caller supplies non-negative strides |

For coordinate `i`, every mapping computes
`sum(i[d] * stride[d])`. Factories reject invalid metadata and checked
multiply/add overflow. `TryOffset(...)` is the status-bearing operation for
untrusted coordinates; `operator()` retains release-active rank and bounds
contracts; `UncheckedOffset(...)` is for hot paths whose caller already proved
the bounds.

A mapping exposes both logical size and required backing span. The required
span is zero for an empty shape, one for rank zero, and otherwise:

```text
1 + sum((extent[d] - 1) * stride[d])
```

Stride mappings may contain holes or a zero-stride broadcast dimension. A
mutable view requires proven unique addressing. A const-element view may be
non-unique. `IsUnique()` is a conservative allocation-free proof, so `false`
means only that uniqueness was not established. Negative strides, negative
indices, and padding policies are outside canonical M1.

Contiguous means exhaustive storage matching canonical left or right strides
after ignoring dimensions of extent one. Empty mappings are contiguous and
unique.

## Tensor views

`TensorView<Element, ExtentsType, Mapping, Accessor>` is a small non-owning
handle. With the default accessor and canonical mappings it is trivially
copyable. It stores a data handle, mapping, accessor, available backing span,
and memory space; it never owns or extends the lifetime of the allocation.

View creation validates all of the following before publishing a value:

- the available span is non-negative and covers the required span;
- nonempty storage has a non-null handle;
- the storage is host-accessible in M1;
- a mutable-element view has a mapping whose uniqueness is proven.

Element constness is part of the view type:

- `TensorView<T, ...>` permits element mutation;
- it converts to the corresponding `TensorView<const T, ...>`;
- the reverse conversion does not exist;
- a const `Tensor` returns only a const-element view.

Like `std::span`, a const view object does not by itself make the referenced
elements const. Use a const-element view when mutation must be excluded.

Views have no implicit pointer conversion and do not allocate, synchronize,
transfer, select a device, or change ownership. The complete backing allocation
must outlive every use of every derived view. Do not return a view into a local
owner or retain a view after its owner has been moved from or destroyed.

M1 has no canonical slice, transpose, permutation, or reshape operation.
Applications must not infer those operations from the presence of legacy view
methods.

## Tensor ownership

`Tensor<T, ExtentsType, Mapping>` owns one canonical `Buffer<T>` and its
descriptor. M1 owners accept only unique, exhaustive, contiguous mappings.
Arbitrary strided storage belongs in a view, not an M1 owner.

- A default owner is valid and has no descriptor or allocation.
- A zero-size descriptor is a distinct state and remains observable.
- Factories validate descriptor/span/resource compatibility before allocation.
- Only host-accessible resources and spaces are supported by Array M1.
- Copy construction and copy assignment are deleted.
- Move transfers exactly one ownership and resets the source to default-empty.
- Mutable and const `View()` overloads return mutable-element and const-element
  views respectively.
- There is no resize, reserve, ownership switch, implicit pointer conversion,
  mirror, manual release, or execution preference.

`Clone` is the only canonical M1 deep-copy operation. It takes an explicit
`ExecutionContext` and destination resource or space, performs synchronous
serial host work, preserves extents and mapping order, and returns a distinct
owner. A failure does not modify the source or expose a partial destination.

`Tensor` is not safe for concurrent mutation. Independent owners and read-only
views share no Array-owned mutable global state.

## Structural concepts

`<asc/array/tensor_concepts.h>` defines operation-oriented concepts for
descriptor/mapping observation, readable or writable access, contiguous
tensors, and exact tensor rank. They inspect expressions and associated types;
they do not require inheritance from an ASC base class.

`Vector` means exactly rank one and `Matrix` exactly rank two. A scalar is not a
vector and a vector is not a matrix. The nominal concepts in
`<asc/array/concepts.h>` remain legacy compatibility declarations.

## Linalg consumption of Array views

Linalg M1 builds narrower readable/writable vector and matrix
concepts on this structural surface. Its seven operations receive views, never
owners, so callers must obtain `Tensor::View()` explicitly. Linalg repeats the
operation-boundary span, host-access, mapping-uniqueness, metadata-overflow, and
alias validation before compiled provider dispatch.

Linalg uses logical coordinates and the actual left, right, or unique stride
mapping; it does not consult Array's legacy `DefaultLayout`. It never resizes an
owner, fills a missing output allocation, transfers a view, or extends view
lifetime. The owner or external allocation must outlive the synchronous call.

This narrow use does not add canonical Array transforms, expressions,
broadcasting, pointwise evaluation, or sparse storage. Pointwise and general
reduction operations currently located in legacy Linalg `blas.h` remain on the
compatibility path until Array M3 defines the complete evaluation contract. See
the [Linalg module](linalg.md) and
[Linalg migration](../migration/linalg.md).

## Random consumption of Array views

Random M1 accepts writable structural canonical descriptors of any fixed rank,
including rank zero. The destination scalar is exactly `float` or `double` and
must match `Uniform01<T>`. As with Linalg, owners are not operation arguments;
callers obtain a checked writable view explicitly and keep its allocation alive
for the synchronous call.

`FillRandom` repeats descriptor, span, host-access, uniqueness, and overflow
validation before writing. It enumerates rightmost-axis-fastest logical
coordinates and uses actual strides, so left, right, and unique padded-stride
views receive the same values at equal logical coordinates while holes remain
untouched. This does not add slice/subview construction or global-coordinate
identity to Array M1; callers assign counter offsets to partitions explicitly.

See the [Random module](random.md) and
[Random migration](../migration/random.md).

## Compatibility boundary

M1 does not change the copy, layout, integer-metadata, host/device, expression,
or sparse behavior of the inherited array stack. Existing callers may continue
to include its narrow headers, including `<asc/array/marray.h>`, but those types
do not implement the canonical owner/view contract.

In particular:

- `DenseMArray` and `UArray` still use compatibility `Memory<T>`;
- `MakeRef` and legacy view aliases retain their characterized lifetime and
  constness behavior;
- the legacy default layout may still depend on `ASC_USE_ROW_MAJOR`;
- legacy expressions retain their existing shape, alias, execution, and
  lifetime rules;
- `SparseMArray` still combines COO construction and compressed storage in one
  family and still advertises its inherited aliases.

Do not wrap one allocation in both canonical `Buffer<T>` ownership and legacy
`Memory<T>` ownership. M1 provides no ownership or synchronization bridge
between the two systems.

## Deferred work

Array M2 is expected to migrate dense compatibility storage and add reviewed
zero-copy transforms. Array M3 owns centralized shape algebra, broadcasting,
alias analysis, lifetime-safe expressions, and context-driven assignment.
Array M4 owns separate rank-two sparse builders and finalized CSR/CSC
owners/views. Device accessors, transfers, and asynchronous operations wait for
explicit Core provider contracts.

See [Array migration](../migration/array.md) for old-to-new guidance and
[`array_design.md`](../design/array_design.md) for the frozen Array M1 contract.
The separate frozen Linalg consumer contract is in
[`linalg_design.md`](../design/linalg_design.md).
