# Milestone 4 independent verification design

Status: Frozen before production inspection

Date: 2026-07-28

Authority: Milestone 4 contract, ownership ledger, provenance record, ADRs
0001--0004, 0007--0010, 0012, 0014, 0016--0018, and repository `AGENTS.md`

This design was derived without inspecting Milestone 4 production code or any
MdeCpp, deleted asc-cpp, third-party, provider, or later-milestone
implementation or test. Expected structures and numerical values below were
calculated independently from elementary sparse definitions.

## Verification boundary

The suite covers only the CPU Sparse foundation approved for Milestone 4:

- general-rank coordinate assembly, finalization, ownership, and views;
- canonical rank-two CSR and CSC ownership and views;
- every approved COO/CSR/CSC conversion;
- structure-preserving destination-owned sparse evaluation;
- serial reference CSR SpMV for `float` and `double`;
- the storage-neutral placement/writable Expression protocol needed by those
  operations; and
- build, component, consumer, allocation, and portability evidence.

It must not exercise or imply structural expression evaluation, implicit
growth, densification, CSC SpMV, transpose, SpMM, solves, provider dispatch,
GPU execution, or any later milestone.

## Independently derived canonical structures

### Coordinate finalization oracle

For shape `(3, 4)`, insert in this order:

```text
(2, 1) = 5
(0, 3) = 2
(2, 1) = -1
(1, 0) = 0
(0, 1) = 7
```

With duplicate summation and zero retention, the canonical output is:

```text
coordinates: (0, 1), (0, 3), (1, 0), (2, 1)
values:       7,      2,      0,      4
```

The duplicate value is `5 + (-1) = 4` in insertion order. With zero dropping,
the `(1, 0)` entry is absent. With duplicate rejection, finalization fails and
the original five logical entries remain available for a later successful
finalization. A separate `max(Element) + 1` integral duplicate pair must
produce overflow and the same rollback guarantee.

Rank-three lexicographic coverage uses shape `(2, 3, 2)` and insertion order
`(1,2,0)`, `(0,2,1)`, `(1,0,1)`, `(0,0,0)`. Expected order is
`(0,0,0)`, `(0,2,1)`, `(1,0,1)`, `(1,2,0)`. Rank zero has the sole empty
coordinate and logical size one. Any zero extent rejects every added entry.
Floating NaN is retained under zero dropping because it is not equal to zero.

### Compressed and conversion oracle

The canonical `3 x 4` matrix is:

```text
row 0: (column 1, 2), (column 3, -1)
row 1: (column 0, 4), (column 2, 5)
row 2: (column 1, 3)
```

Its CSR representation is:

```text
outer offsets: [0, 2, 4, 5]
inner indices: [1, 3, 0, 2, 1]
values:        [2, -1, 4, 5, 3]
```

Its CSC representation, obtained by column-major canonical traversal, is:

```text
outer offsets: [0, 1, 3, 4, 5]
inner indices: [1, 0, 2, 1, 0]
values:        [4, 2, 3, 5, -1]
```

All six named conversions must produce these exact structures as applicable.
Round trips must preserve shape, explicit zeros, coordinates, and values.
Rectangular `0 x 4`, `3 x 0`, and `0 x 0` matrices must retain the correct
`outer_extent + 1` zero-offset array for each destination format.

Malformed compressed inputs independently cover a negative extent, negative
NNZ, wrong offset count, nonzero first offset, decreasing offsets, negative
offset, final-offset/NNZ mismatch, out-of-range inner index, unsorted inner
indices, duplicate inner indices, null nonempty buffers, byte/address
overflow, and both CSR and CSC outer/inner interpretations. Failure must
publish no owner and must release any partial allocations once.

### SpMV numerical oracle

Using the CSR matrix above, input `x = [2, -1, 3, 4]`, old output
`y = [7, 11, 13]`, `alpha = 2`, and `beta = -0.5`:

```text
A*x = [-6, 23, -3]
y   = [-15.5, 40.5, -12.5]
```

The arithmetic is independently expanded row by row:

```text
row 0: 2*(-1) + (-1)*4 = -6
row 1: 4*2 + 5*3       = 23
row 2: 3*(-1)           = -3
```

The oracle is run for both `float` and `double`. Additional cases cover zero
NNZ, zero rows, zero columns, `alpha == 0`, and `beta == 0` with old output
initialized to NaN so any forbidden read is visible.

## Transaction and failure matrix

Every failing operation snapshots logical entries, destination values, or
resource counters before invocation and compares them afterward.

- Builder creation/Add: negative capacity, invalid extent, incomplete or
  negative/out-of-range coordinate, capacity exhaustion, arithmetic/byte
  overflow, and inaccessible context.
- Finalize: non-serial or inaccessible context, duplicate rejection,
  duplicate integral overflow, and allocation failure.
- Compressed construction: every malformed invariant above and allocation
  failure at each buffer boundary.
- Conversion: incompatible context/space and deterministic allocation failure
  at each destination buffer boundary, with unchanged source.
- Evaluation: rank/value/shape mismatch, non-host placement, non-preserving
  sparsity effect, exact and partial alias cases, and source access failure.
  Exact self-assignment is the only accepted overlap and is a no-op.
- SpMV: wrong backend, matrix/input/output placement, wrong vector ranks or
  lengths, scalar/value disagreement, nonunique output, matrix-value/output
  overlap, input/output exact or partial overlap, and source access failure.
  All checks complete before the first output write.

Custom counting and fail-after memory resources provide exact allocation and
release evidence. External views can validate metadata and address arithmetic
only; tests do not claim provenance, allocation-length, alignment, or lifetime
proof.

## API, ownership, and expression coverage

- Static traits verify builders and owners are move-only and finalized views
  are trivially copyable.
- Const owners yield const-element views; mutable-to-const view conversion is
  accepted and the reverse is rejected.
- Structure is immutable through all views; mutable value updates are visible.
- Stored-entry checked access, logical lookup, shape/rank/NNZ/space, and alias
  spans are validated.
- Moving an owner preserves its allocation/resource association; destruction
  and partial construction release every allocation exactly once.
- Expression metadata is checked for value type, compile-time rank, exact
  shape, serial placement, alias, terminal operation, and
  structure-preserving sparsity.
- A non-ASC rank-one external vector supplies independent readable, writable,
  and placement adapters. It drives SpMV in a Sparse-only translation unit.
- Dense interoperability is a separate positive translation unit that
  explicitly includes both Dense and Sparse. Neither Sparse headers nor
  `ASC::sparse` may acquire a Dense dependency.

## Compile-only plan

Positive probes:

- each of the six public Sparse headers and new
  `asc/expression/writable.h` compiled alone;
- generic self-containment and exceptions-disabled probes for the modified
  `asc/expression.h`, `asc/expression/expression.h`, and `asc/dense/view.h`;
- each header with exceptions disabled;
- multi-translation-unit use of templates and the compiled SpMV symbols;
- concepts for coordinate/compressed views and the external vector adapter;
- Sparse-only external-vector SpMV;
- explicit Dense-view interoperability.

Expected compile failures:

- copying a coordinate builder;
- copying finalized coordinate, CSR, or CSC owners;
- mutating values through a const-element view;
- mutating compressed structure through any view;
- unsupported `bool` and non-arithmetic Sparse elements;
- unsupported integer SpMV;
- a non-`Extents` coordinate owner descriptor;
- wrong-rank or nonunique writable SpMV output; and
- absent Writable/Placement opt-in for an otherwise unrelated type.

The lead owns all CMake registration and decides how many negative probes are
separate CMake targets.

## Allocation, complexity, and no-densification evidence

After owner construction, coordinate/compressed reads, structure-preserving
evaluation, and SpMV must report zero dynamic allocations through both the
explicit resource counters and a process allocation probe. The benchmark uses
a deterministic rectangular CSR pattern with fixed NNZ per row and records:

```text
compiler, build configuration, rows, columns, NNZ, iterations, format,
operation, allocation count, elapsed time, and checksum
```

It measures structure-preserving evaluation and CSR SpMV without a speed
threshold. Matrix sizes are chosen so accidental densification would exceed a
strict allocation budget; no dense buffer is constructed by the test.
Conversions separately require only explicit destination buffers and are
checked against documented output-buffer counts.

## Sanitizer, portability, package, and consumer plan

- GCC Debug static and Clang Release shared builds with warnings as errors;
- ASan plus UBSan over every compatible Sparse runtime test;
- standalone LSan and TSan where the local toolchain supports them;
- minimum CMake 3.25;
- build-tree, copied-build-tree, install/relocation, path-with-spaces,
  static/shared, and subproject consumers;
- installed Sparse-only consumer links exactly `ASC::sparse` and uses only the
  external vector adapter;
- component graph inspection requires exactly Core, Expression, and Sparse;
- public-header formatting, self-containment, exceptions-disabled, direct
  includes, target links, and forbidden sibling/provider scans.

No CUDA or other GPU provider is part of Milestone 4. GPU evidence is exactly
**skipped**: no GPU configure, compile, runtime, or parity claim is authorized.
