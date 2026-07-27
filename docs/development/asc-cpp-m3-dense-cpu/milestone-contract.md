# Milestone 3 Contract: Dense CPU

Status: Frozen after owner approval

Date: 2026-07-26

Owner approval: Milestone 3 with no corrections

Branch: `feature/asc-cpp-m3-dense-cpu`

Baseline commit:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the intentionally uncommitted Milestones 0--2 publication
candidate and the user's prior deletion of the retired implementation are
retained in place. This milestone does not restore, reset, discard, or
reclassify that work.

## Purpose

Implement the approved CPU foundation of the `dense` module. The component
owns multidimensional dense storage, views, layout mappings, dense expression
evaluation, selected reductions, and the narrow deterministic serial dense
linear-algebra baseline.

The module is C++20, uses the flat `asc` namespace, and has exactly these
direct production dependencies:

```text
ASC::dense -> ASC::core
ASC::dense -> ASC::expression
```

`dense` must not include or link `utilities`, `sparse`, `random`, a retired
`array` or `linalg` component, or any provider SDK.

## Product target

```text
build target:    asc_dense
exported target: ASC::dense
kind:            static/shared compiled library
direct links:    ASC::core;ASC::expression
release line:    unreleased 0.3.0 candidate
```

The target contains genuine compiled serial reference linear-algebra behavior.
No provider or aggregate target is created.

## Public files

```text
include/asc/dense.h
include/asc/dense/array.h
include/asc/dense/evaluate.h
include/asc/dense/export.h
include/asc/dense/layout.h
include/asc/dense/linalg.h
include/asc/dense/view.h
```

Compiled implementation is confined to `src/dense/`. Public headers are
self-contained `.h` files. Compiled C++ sources are `.cc` files.

## Storage and layout contract

- Rank is a `std::size_t` template argument. Shapes use core `extent_t`;
  coordinates use `index_t`; strides use `stride_t`.
- Rank zero has logical and required span size one. Any zero extent has logical
  and required span size zero.
- `LayoutLeft` is column-major and is the named default. `LayoutRight` is
  row-major. `LayoutStride` uses caller-supplied non-negative strides.
- Mapping construction checks negative extents/strides and every product, sum,
  offset, span, and conversion overflow before publishing a mapping.
- Mutable views and owners require a mapping whose uniqueness is proven.
  M3 may conservatively reject a mathematically unique arbitrary-stride
  mapping when the proof is not available.
- Owners require unique and exhaustive storage. Padded/non-exhaustive mappings
  are view-only in M3.
- Negative strides, repeated-address views, shared ownership, runtime rank,
  external adoption/deleters, and hidden packing are deferred.
- Logical coordinate traversal increments dimension zero fastest and is
  independent of physical layout.

## View contract

`DenseView<Element, Rank>` is a trivially copyable, non-owning descriptor of:

- an element pointer;
- a validated layout mapping;
- an explicit `MemorySpace`.

A nonempty view cannot have a null pointer. A const-element view cannot become
mutable. Mutable-to-const conversion is one-way. Bounds-checked element access
returns `Result<Element*>` or its const equivalent; unchecked access is
reserved for validated internal traversal.

Rank-preserving subviews accept per-dimension offsets and extents, perform no
allocation, preserve strides and memory space, and reject out-of-range or
overflowing regions transactionally. Rank-reducing slices are deferred.

A view participates in the storage-neutral expression protocol through
`ExpressionAdapter`; the expression target does not include a dense header.
The adapter reports shape, value type, rank, terminal category,
structure-preserving sparsity, value reads, and conservative alias identity.

## Owner contract

`DenseArray<Element, ExtentsType>`:

- uses a core `Buffer` from an explicit `MemoryResource`;
- is move-only;
- owns host-accessible, unique, exhaustive storage in M3;
- accepts mixed static/dynamic core `Extents`;
- supports left and right contiguous mappings;
- value-initializes supported scalar elements;
- yields mutable views only from a mutable owner and const-element views from
  a const owner;
- exposes a named deep `Clone` using an explicit destination resource and
  serial execution context;
- exposes transactional discard-resize using its resource; successful resize
  invalidates all prior views and failed resize leaves the owner unchanged.

M3 owner elements are arithmetic, trivially copyable, and trivially
destructible. Linear algebra is restricted further to `float` and `double`.

## Evaluation and reduction contract

Dense owns evaluation of a `ReadableExpression` into a caller-provided mutable
destination:

- explicit `ExecutionContext`;
- serial backend and host memory only;
- exact rank and shape, except expression-owned rank-zero scalar expansion;
- all shape, memory, and alias validation before mutation;
- no allocation, packing, transfer, synchronization, or fallback;
- deterministic logical-coordinate traversal;
- direct exact-view self-assignment is a permitted no-op;
- other possible destination overlap is conservatively rejected before
  mutation in M3.

M3 selected reductions are deterministic logical-order `ReduceSum`,
`ReduceMin`, and `ReduceMax`. Empty sum returns zero; empty minimum/maximum
return `kInvalidArgument`. No parallel reduction or order-independent result is
claimed.

## Serial dense linear-algebra contract

The public operations are:

```text
Copy
Scal
Axpy
Dot
Nrm2
Gemv
Gemm
```

They support `float` and `double`, explicit serial `ExecutionContext`, rank-one
or rank-two dense views, arbitrary validated non-negative strides, and
caller-provided destinations. `Gemv` and `Gemm` support `kNone` and
`kTranspose`; conjugation is deferred.

Rules:

- validate backend, memory, shape, overflow, and forbidden output overlap
  before mutation;
- exact source/destination identity is a no-op for `Copy` and is safe for
  `Scal`/`Axpy` where the mathematical same-index contract permits it;
- `Dot` uses deterministic logical index order;
- `Nrm2` uses scaled sum-of-squares rather than a naive sum of squares;
- `beta == 0` in `Gemv` and `Gemm` must not read the previous destination;
- no operation allocates, packs, transfers, changes precision, synchronizes,
  or falls back;
- NaN and infinity follow ordinary IEEE arithmetic; no undocumented
  normalization is performed.

Factorizations, solvers, complex or mixed precision, batched operations,
tensor contraction, optimized CPU providers, provider selection, and
workspace-bearing algorithms are deferred.

## Required evidence

- self-contained public headers with exceptions enabled and disabled;
- exact target/include/import dependency audits;
- compile traits for owner move-only behavior and const-view propagation;
- rank-zero, zero-extent, negative, boundary, and overflow cases;
- left/right/padded unique-stride offset properties;
- subview, clone, resize, invalidation documentation, and failure rollback;
- expression evaluation shape/alias transaction tests and allocation counts;
- independently calculated numerical cases for every linalg operation,
  including transposes, degenerate shapes, extreme finite values, and
  `beta == 0` no-read;
- scaled `Nrm2` stability evidence;
- ASan/UBSan;
- build-tree, installed, relocated, path-with-spaces, static/shared,
  subproject, and dense-only consumer evidence;
- an allocation-free serial reference benchmark with recorded compiler,
  configuration, shape, iteration count, and timing, but no unstable
  pass/fail speed threshold.

GPU evidence for this milestone is exactly `skipped`.

## Explicit exclusions

- No production change to `core`, `utilities`, `expression`, or `random`
  except a release-blocking compatibility correction discovered by review and
  recorded explicitly by the lead.
- No `sparse` source or target.
- No random dense/sparse generation facet.
- No CUDA, HIP, SYCL, OpenMP, TBB, Eigen, BLAS/LAPACK, oneMKL, or other
  optional provider integration.
- No third-party dependency.
- No compatibility layer for deleted `array` or `linalg` APIs.
- No MdeCpp source, test, table, vector, or documentation copying.
- No umbrella `ASC::cpp`, tag, release, publication, or branch deletion.

## Publication gate

The lead stops at Publication Checkpoint B after integrating every scoped
change, resolving release-blocking reviews, and recording exact
pass/fail/skip results. Remote writes require a separate owner approval.
