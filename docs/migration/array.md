# Array migration

> [!WARNING]
> **Superseded historical document.** The body below records the deleted
> five-component implementation at commit
> `33b261ea33616a6395c4ad3b20646093103344f7`. It is retained only for audit
> history and does not describe the active API or package. See the
> [current documentation][current-docs] and the
> [approved Stage A architecture][stage-a].

[current-docs]: ../README.md
[stage-a]: ../development/asc-cpp-architecture/architecture-blueprint.md

Array M1 adds a new dense descriptor/view/owner path without rewriting the
MdeCpp-derived arrays. It establishes the long-term ownership and dependency
boundary while preserving broad legacy Linalg, Random, expression, sparse, and
device behavior as compatibility surface. Linalg M1 is the first
higher module to consume the canonical view path directly.

## Milestone status

| Milestone | Status | Scope |
| --- | --- | --- |
| Array M1 | Implemented | Mixed 64-bit extents, checked mappings, typed views, move-only host owner, explicit serial clone, structural concepts, canonical umbrella |
| Array M2 | Planned | Migrate dense compatibility storage inward and add reviewed view transforms |
| Array M3 | Planned | Shape algebra, broadcasting, alias analysis, safe pointwise expressions, and context-driven assignment |
| Array M4 | Planned | Separate mutable COO builder from finalized rank-two CSR/CSC owners and views |
| Linalg M1 | Implemented | Seven context-first serial-reference operations over canonical rank-one/rank-two views; no legacy-owner adapter |
| Random M1 | Implemented | Context-first deterministic unit-uniform fill over canonical writable views; inherited samplers remain aggregate compatibility |
| Provider milestones | Planned in Core first | Device accessors/resources, explicit transfers, and asynchronous operations |

M1 is additive and declares no removal release for a legacy Array API.

## API mapping

| Inherited API or mechanism | Canonical direction | M1 compatibility disposition |
| --- | --- | --- |
| `MShape<...>` and `DShape<Rank>` | `Extents<...>` and `DynamicTensorExtents<Rank>` with signed 64-bit metadata and mixed extents | Legacy descriptors remain unchanged |
| integer `kDynamicExtent` | canonical `dynamic_extent` | Both remain distinct; do not substitute the canonical value into legacy templates |
| `LayoutLeft` / `LayoutRight` | `LayoutLeftMapping` / `LayoutRightMapping` | Legacy mappings remain unchanged |
| `LayoutStride` and padding policies | checked non-negative `LayoutStrideMapping` without M1 padding | Legacy padding and mapping behavior remain unchanged |
| macro-selected `DefaultLayout` | fixed canonical left/column-major default | Legacy default remains build/consumer dependent |
| `MIndex` and `MIterator` | signed coordinates plus mapping offset/access operations | Canonical iterator replacement is deferred |
| `UArray` | canonical owner/view separation for new code | `UArray` remains compatibility-only; no M1 adapter |
| `DenseMArray` and `DVector`/`DMatrix`/static aliases | `Tensor<T, ExtentsType, Mapping>` | Legacy aliases remain available and keep copy semantics |
| `MArrayView`, `MakeRef`, and legacy `View()` | `TensorView<Element, ExtentsType, Mapping, Accessor>` | Existing aliases retain characterized lifetime and constness behavior |
| `Memory<T>`, `Read`/`Write`, `UseDevice` | `Buffer<T>`, host-accessible views, and explicit `ExecutionContext` | No conversion, shared registry, mirror, or hidden transfer exists |
| implicit owner copy | deleted copy plus explicit `Clone(context, destination)` | Legacy deep copy remains unchanged |
| raw-pointer conversions and ownership flags | explicit data observation and single RAII ownership | Legacy conversions/flags are not adapted in M1 |
| `MObject` and `Expression` | future lifetime-safe expressions and context-driven assignment | Legacy expression stack remains compatibility-only |
| `SparseMArray` COO/finalized modes | future COO builder and finalized rank-two CSR/CSC owners/views | All legacy sparse APIs remain unchanged in M1 |
| sparse scalar/vector/rank-N aliases | exact-rank future sparse surface | Legacy names remain but are not canonical design claims |
| `CArray` pointer-tree helpers | no canonical M1 replacement | Compatibility-only |
| nominal `<asc/array/concepts.h>` | structural `<asc/array/tensor_concepts.h>` | Both headers remain distinct in M1 |

## Deliberate source and semantic differences

Canonical code intentionally differs from inherited behavior:

- shape, coordinate, stride, and size metadata use signed 64-bit aliases;
- a descriptor may mix static and dynamic dimensions;
- checked products, spans, and offsets report overflow instead of narrowing;
- rank zero is a scalar descriptor with logical size one;
- the default mapping is always left/column-major;
- canonical stride mappings reject negative strides and do not expose legacy
  padding policies;
- a mutable-element view requires a proven unique mapping;
- mutable-to-const view conversion is one-way, and a const owner never produces
  a mutable-element view;
- an owner is move-only and deep copy is an explicit context-bearing operation;
- canonical types have no implicit pointer conversion, ownership Boolean,
  manual deletion, mirroring, or device preference;
- Array M1 is host-accessible and synchronous serial only;
- canonical M1 does not expose transforms, expressions, broadcasting, or
  sparse ownership merely because similarly named legacy behavior exists.

These are architecture corrections, not accidental regressions to be restored
through compatibility overloads.

## Descriptor migration

Choose a canonical descriptor type according to which dimensions belong in the
type:

```cpp
using OldRuntimeMatrix = asc::DShape<2>;
using NewRuntimeMatrix = asc::DynamicTensorExtents<2>;

using OldFixedMatrix = asc::MShape<3, 4>;
using NewFixedMatrix = asc::Extents<3, 4>;

using NewMixedTensor =
    asc::Extents<3, asc::dynamic_extent, 5>;
```

The old and new descriptors are unrelated types. M1 provides no implicit
conversion. Rebuild a canonical descriptor from validated logical extents and
handle the returned `Result`.

Do not cast a checked canonical size back to `int` unless the receiving legacy
API has a separately validated range. Descriptor validation can succeed for a
shape too large to allocate; allocation remains an independent result.

## Layout and indexing migration

Select the layout explicitly at module boundaries. Code that relied on
`DefaultLayout` changing with `ASC_USE_ROW_MAJOR` must choose either
`LayoutLeftMapping` or `LayoutRightMapping` in its canonical type.

For external storage, construct a checked mapping and then a checked view with
the full available backing span and memory space. Logical size is not a
substitute for backing span when a stride mapping contains holes. A zero-stride
dimension may be used only with a const-element view unless the mapping can
otherwise prove unique addressing.

Replace integer index aggregation with one of the canonical mapping paths:

- `TryOffset(...)` for untrusted coordinates;
- contract-checked `operator()` after program inputs are established;
- `UncheckedOffset(...)` only inside a hot path with a preceding proof.

There is no canonical M1 iterator adapter. Algorithms that require inherited
`MIterator` behavior remain on the compatibility path until their owning
module is migrated.

## Dense owner and view migration

A canonical `Tensor` is not a drop-in typedef for `DenseMArray`:

1. create and validate `Extents`;
2. construct the owner with an explicit host resource or serial context and
   host space;
3. obtain a status-bearing mutable view for writes;
4. expose a const-element view from const owners to readers;
5. call `Clone` when an independent owner is required;
6. keep the owner/allocation alive for the complete lifetime of every view.

The tested canonical sequence is shown in the
[Array module guide](../modules/array.md). The installed Array-only consumer
uses the same mixed-extents, mutable-view, const-view, and clone path while
linking only `ASC::array`.

Do not migrate `MakeRef` by constructing a second owner around the same
pointer. Use a checked `TensorView` for borrowing. M1 has no adopted-ownership
factory and no lifetime extension. A function returning a view must receive or
otherwise reference storage whose lifetime the caller controls.

Code that currently mutates through a view obtained from a const legacy owner
must change its API: make ownership/mutation explicit, or accept a
const-element view and stop mutating. Canonical const correctness will not
preserve that legacy loophole.

## Copy, execution, and memory migration

Copying a legacy dense owner creates independent data. Copying a canonical
owner does not compile. Spell the operation `Clone`, pass an explicit
`ExecutionContext` and destination resource or space, and handle failure.

Array M1 clone is a synchronous serial host operation. It does not activate
legacy OpenMP/CUDA behavior, follow `UseDevice`, copy a mirror, or synchronize
the process-wide `Device`. An application needing current device-backed legacy
arrays must remain on the compatibility path. Do not combine `Memory<T>` and
`Buffer<T>` ownership for the same allocation.

## Linalg consumer migration

Linalg M1 accepts canonical `TensorView`-like rank-one and rank-two
descriptors for exactly `Copy`, `Scal`, `Axpy`, `Dot`, `Nrm2`, `Gemv`, and
`Gemm`. It does not accept canonical owners directly: obtain a checked mutable
or const-element view first and keep the owner alive for the synchronous call.

The caller preallocates every destination with exact extents. Linalg uses the
view's actual strides, preserves padding holes, validates accessible span and
mapping uniqueness, and rejects forbidden overlap before writing. It does not
bridge a canonical view to `DenseMArray`, `Memory<T>`, a mirror, or the global
device policy.

Only the seven-operation subset migrates in Linalg M1. Pointwise/general
reductions in legacy `blas.h` still wait for Array M3 shape algebra and
evaluation. Contractions and convenience matrix operations wait for reviewed
shape contracts, while factors and solvers remain future Linalg work. See
[Linalg migration](linalg.md) for the complete classification.

## Random consumer migration

Random M1 accepts writable structural canonical views of any fixed rank for
exactly `float` or `double` unit-uniform output. Obtain the view explicitly,
pass a serial `ExecutionContext`, `Uniform01<T>`, and literal
`RandomKey`/`RandomCounter`, then propagate `Status`.

The operation validates actual extents, non-negative strides, required and
available spans, host accessibility, uniqueness, data, and counter arithmetic.
Logical traversal is independent of physical layout and preserves padded
holes. It never resizes an owner, creates a view, transfers a mirror, or
consults `DefaultLayout` or `UseDevice`.

This does not migrate legacy `PseudoSampler`, low-discrepancy, normal, or
spherical APIs. Those continue to consume `DenseMArray` and related
compatibility behavior through `ASC::cpp`. See
[Random migration](random.md) for the complete family mapping.

## Expressions and transformations

M1 has no canonical expression assignment, broadcasting, slicing, transpose,
permutation, or reshape API. Existing code using `MObject`, `Expression`, or
legacy dense view transformations should keep using the compatibility types as
one coherent subsystem.

Array M3 will define expression shape metadata, trailing-axis broadcasting,
alias handling, borrowed-lvalue rules, temporary-owner rejection, and explicit
context-driven assignment together. Migrating only expression syntax before
those semantics exist would recreate the current architectural ambiguity.

## Sparse migration

M1 does not provide a canonical sparse type. Existing COO insertion,
`Finalize()`, CSR/CSC conversion, iteration, and Eigen mapping continue through
`SparseMArray` and its legacy layouts.

Array M4 will use separate types for mutable rank-two COO construction and
finalized CSR/CSC ownership/views. Finalization will validate, sort, and
coalesce structure before publishing a compressed value. Until that reviewed
surface exists, do not advertise legacy sparse scalar, vector, or rank-N names
as the canonical sparse model.

## Compatibility window

Legacy Array APIs remain available during the bounded pre-1.0 transition.
Removal requires:

1. a canonical equivalent for the ordinary use case;
2. completion of broad Linalg, Random, and known downstream caller migration;
3. focused canonical and legacy regression coverage;
4. package, backend, dependency, sanitizer, and configuration gates;
5. a documented breaking release and removal table.

Array M1 itself neither deprecates a legacy header nor declares a removal
release. Mixing the two systems at an ownership boundary is unsupported even
while both are installed.

## Provenance

The inherited Array sources were selected and adapted from MdeCpp as recorded
in [the migration inventory](inventory.md). Canonical M1 is a new asc-cpp
architecture derived from
[`architecture_blueprint_v1.md`](../design/architecture_blueprint_v1.md) and
the approved [`array_design.md`](../design/array_design.md); it is not a renamed
MdeCpp array hierarchy. Linalg's canonical view consumption is
governed separately by
[`linalg_design.md`](../design/linalg_design.md).
