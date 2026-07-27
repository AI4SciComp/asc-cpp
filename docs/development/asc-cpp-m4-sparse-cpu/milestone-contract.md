# Milestone 4 Contract: Sparse CPU

Status: Frozen after owner approval

Date: 2026-07-26

Owner approval: Milestone 4 with no corrections

Exact milestone: **Milestone 4 — sparse CPU**

Branch: `feature/asc-cpp-m4-sparse-cpu`

Baseline commit and current uncommitted `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the intentionally uncommitted Milestones 0--3 publication
candidate and the user's earlier deletion of the retired implementation are
retained in place. Milestone 4 does not restore, reset, discard, commit, or
reclassify that work.

## Purpose

Implement the approved CPU foundation of the `sparse` module:

- general-rank coordinate construction, finalized ownership, and views;
- canonical rank-two CSR and CSC ownership and views;
- explicit coordinate/compressed conversions;
- destination-owned structure-preserving sparse expression evaluation;
- deterministic serial CSR sparse matrix-vector multiplication;
- independent package, numerical, allocation, and no-densification evidence.

The module uses strict C++20 and flat `namespace asc`. Its exact direct
production dependencies are:

```text
ASC::sparse -> ASC::core
ASC::sparse -> ASC::expression
```

Sparse must not include or link utilities, dense, random, a retired array or
linalg component, an aggregate, an optional provider, or a provider SDK.

## Product target

```text
build target:    asc_sparse
exported target: ASC::sparse
kind:            static/shared compiled library
direct links:    ASC::core;ASC::expression
release line:    unreleased 0.4.0 candidate
```

The target contains genuine compiled float/double serial CSR SpMV behavior.
No optimized CPU provider, GPU provider, provider option, provider discovery,
or aggregate target is created.

## Public production files

Sparse owns exactly:

```text
include/asc/sparse.h
include/asc/sparse/export.h
include/asc/sparse/coordinate.h
include/asc/sparse/compressed.h
include/asc/sparse/evaluate.h
include/asc/sparse/linalg.h
src/sparse/reference_linalg.cc
```

Milestone 4 may add the previously deferred, storage-neutral writable/placement
protocol needed by ADR 0014:

```text
include/asc/expression/writable.h
include/asc/expression.h
include/asc/dense/view.h
```

`writable.h` remains owned by expression and depends only on core. The
`dense/view.h` change is limited to non-owning protocol specialization so a
dense view can be an SpMV vector when a consumer includes both components.
It adds no sparse include, link edge, behavior, storage, or provider. This is
the only approved predecessor-module compatibility correction.

All public headers are self-contained `.h` files with full-path guards and
direct includes. Ordinary compiled implementation is `.cc`. Internal
namespace names contain `internal` and do not leak into signatures.

## Common sparse vocabulary

The public surface includes:

```text
SparseElement
DuplicatePolicy::{kReject,kSum}
ExplicitZeroPolicy::{kKeep,kDrop}
SparseCompressedFormat::{kCsr,kCsc}
```

Sparse storage elements are unqualified, non-Boolean arithmetic values that
are trivially copyable and trivially destructible. Finalized storage supports
that set; SpMV is restricted to exactly unqualified `float` and `double`.

All shapes, coordinates, inner indices, outer offsets, capacities, and NNZ
values use checked core vocabulary. Logical coordinates and inner indices are
zero-based signed 64-bit values. Products, sums, byte counts, positions, span
sizes, casts, and address ranges are checked before publication or mutation.

## Coordinate construction and finalized storage

The public family is:

```text
CoordinateBuilder<Element, ExtentsType>
CoordinateArray<Element, ExtentsType>
CoordinateView<Element, Rank>
```

`ExtentsType` is an unqualified specialization of core `Extents<...>`.
Compile-time rank is inherited from that type. Rank zero is a scalar domain;
any zero extent is empty and cannot contain an entry.

The builder is move-only and is created with:

- validated extents;
- an explicit non-negative capacity;
- an explicit host `MemoryResource`.

Creation makes the capacity allocation visible. `Add` accepts a complete
coordinate plus value, validates every index and capacity before mutation,
and preserves insertion order. The builder contains no hidden growth,
reallocation, entropy, or default resource.

Consuming `Finalize` requires:

- an explicit serial `ExecutionContext`;
- an explicit `DuplicatePolicy`;
- an explicit `ExplicitZeroPolicy`.

Finalization is transactional with respect to published output. It performs a
stable lexicographic canonicalization in dimension order, combines equal
coordinates in insertion order only when `kSum` is requested, and rejects an
equal coordinate before publishing when `kReject` is requested. Integral
duplicate sums use checked addition; floating sums follow deterministic
ordinary IEEE arithmetic. `kDrop` removes values equal to `Element{}` after
duplicate handling; NaN is not an explicit zero.

Finalization may reuse the builder's declared capacity storage and performs no
undisclosed allocation or workspace acquisition. On success it consumes the
builder and returns a move-only `CoordinateArray` with sorted, unique,
structurally immutable coordinates and mutable values. On failure the builder
remains valid with its pre-finalization logical entries; tests may require an
internal rollback strategy, but no partially finalized owner is exposed.

The finalized coordinate view is trivially copyable and non-owning. Structure
is always const. Element constness controls only value mutation.
Mutable-to-const conversion is one-way. The owner/resource outlives its views.
Structural replacement or owner destruction invalidates them.

Views expose shape, rank, NNZ, coordinates, values, memory space, checked
stored-entry access, coordinate lookup, and conservative value-span alias
identity. Host dereference rejects non-host memory in Milestone 4. External
view creation validates metadata and address arithmetic but cannot prove
pointer provenance, allocation length, alignment, or lifetime; these remain
documented caller obligations.

Coordinate views participate as storage-neutral readable expressions with
terminal category and structure-preserving sparsity. A missing logical
coordinate reads as additive zero without allocation.

## CSR and CSC

The public family is:

```text
CompressedSparseView<Element, Format>
CompressedSparseArray<Element, Format>
CsrView<Element>
CscView<Element>
CsrArray<Element>
CscArray<Element>
```

`Format` is fixed at compile time to `kCsr` or `kCsc`. Rank is exactly two.
Shapes contain non-negative row and column extents.

Canonical compressed invariants:

- internal index base is zero;
- outer offsets have length `outer_extent + 1`;
- the first offset is zero and offsets are nondecreasing;
- the final offset equals NNZ;
- every inner index is in range;
- inner indices are strictly increasing within an outer segment;
- duplicates are absent;
- structure is immutable after publication;
- values may be mutable through a mutable-element view;
- an empty object still has `outer_extent + 1` zero offsets.

Validated owner construction copies caller-provided offsets, indices, and
values through an explicit host `MemoryResource`. Every allocation and byte
count is observable. Failure returns before publishing an owner and releases
partial allocations exactly once.

Compressed owners are move-only. Const propagation, resource/view lifetime,
structural invalidation, host accessibility, external-view preconditions, and
alias identity follow the coordinate contract.

Compressed views participate as structure-preserving readable expressions.
Coordinate lookup uses the canonical segment structure and returns zero for an
unstored coordinate.

## Conversions

Named CPU conversions cover:

```text
canonical coordinate -> CSR
canonical coordinate -> CSC
CSR -> canonical coordinate
CSC -> canonical coordinate
CSR -> CSC
CSC -> CSR
```

Each conversion takes an explicit serial context and destination host resource
and returns a new move-only owner. Output buffers are explicit allocations.
The reference conversion may use deterministic repeated scans instead of
hidden workspace; its documented complexity must match the implementation.

No conversion:

- mutates its source;
- silently changes index base or width;
- drops explicit zeros;
- combines duplicates that should not exist in a finalized source;
- routes through an undisclosed COO temporary;
- packs, transfers, densifies, synchronizes, dispatches, or falls back.

Failure is transactional and releases partial destination allocations.
Round trips preserve shape, canonical coordinate/value order, explicit zeros,
and ordinary floating values including NaN payload behavior to the degree
allowed by value copying.

## Storage-neutral writable and placement protocol

Expression adds a non-intrusive writable/placement customization contract
without storage ownership or evaluation:

```text
ExpressionPlacementAdapter<T>
PlacedReadableExpression<T>
WritableExpressionAdapter<T>
WritableExpression<T>
ExpressionSpace(...)
WritableExpressionShape(...)
WritableExpressionAlias(...)
WriteExpression(...)
```

The exact spellings may be refined only to meet existing expression naming
consistency; the semantics are frozen.

A writable adapter supplies value type, compile-time rank, exact shape,
explicit memory space, conservative alias identity, and a write operation for
an already validated logical coordinate. The write operation cannot allocate,
transfer, synchronize, select a provider, or fail after validation. A type
used as a read/write destination must independently satisfy
`ReadableExpression`.

Expression nodes remain storage-neutral and do not gain evaluation. Existing
readable adapters remain source-compatible; the new placement/writable
protocol is opt-in. Dense and sparse views specialize it without acquiring a
sibling dependency. An external non-ASC vector type can specialize it.

## Sparse expression evaluation

Sparse owns evaluation into caller-provided mutable coordinate or compressed
destinations.

Milestone 4 accepts only:

- exact destination rank and shape;
- exact destination value type;
- explicit serial execution;
- host-accessible source and destination where placement is known;
- expression sparsity effect exactly `kStructurePreserving`;
- an existing caller-provided destination structure.

The evaluator visits the destination's canonical stored coordinates and writes
only its values. It never inserts or erases structure. Structure-filtering,
union, intersection, value-dependent, densifying, destination-required, and
rank-zero scalar-expansion expressions are rejected before mutation in M4.

All shape, placement, structure, and conservative alias checks complete before
mutation. Exact direct self-assignment is a no-op. Other possible destination
value overlap is rejected. External adapters remain responsible for truthful
placement, indexing, alias, and sparsity metadata.

Successful evaluation creates no computational storage, temporary, workspace,
packing, conversion, transfer, synchronization, provider dispatch, or
fallback. Failed diagnostic `Status` construction may allocate through its
string representation; no general no-heap error-path claim is made.

## Serial CSR SpMV

The only Milestone 4 algebra operation is:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

It supports:

- canonical CSR only;
- exactly `float` and `double`;
- explicit serial `ExecutionContext`;
- host memory;
- a placed storage-neutral readable rank-one input;
- a placed storage-neutral readable and writable rank-one output;
- arbitrary non-owning vector implementations that truthfully satisfy the
  expression placement/writable protocols.

The matrix shape is `(rows, columns)`, input length is `columns`, and output
length is `rows`. The implementation validates context, placement, shape,
structure, scalar agreement, and possible output overlap with matrix values or
input before mutation. The output must be a unique logical destination.

Traversal is deterministic CSR row order with increasing column indices.
Each row uses ordinary left-to-right IEEE multiply/add. `beta == 0` must not
read prior output values. Empty shapes and zero NNZ are valid. NaN and
infinity follow ordinary IEEE behavior; no undocumented normalization occurs.

Successful SpMV allocates no storage or workspace, packs nothing, performs no
format conversion or densification, transfers and synchronizes nothing, and
does not dispatch or fall back. CSC SpMV, transpose, SpMM, triangular solve,
preconditioners, iterative/direct solvers, and optimized/GPU providers are
deferred.

## Packaging and dependency contract

The cumulative available component set becomes:

```text
core utilities expression random dense sparse
```

Unavailable and unexported:

```text
random_dense random_sparse cpp
every provider facet
```

Required component `sparse` expands only to `core`, `expression`, and
`sparse`, in that order. It must not import dense even though an application
may independently request both. A sparse-only installed consumer must compile,
link, and run with an external vector adapter while every forbidden target is
absent.

Minimum CMake is 3.25. Root integration continues to require exact
ASCCMake 0.1.0 and uses only the verified functions:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

Standard CMake owns conditional per-component export/install logic.

## Required evidence

- every new or modified public header alone with exceptions enabled and
  disabled under strict GCC and Clang;
- multi-TU ODR and positive/negative concept contracts;
- direct include and target graph audits;
- rank-zero, zero-extent, empty-capacity, negative, boundary, overflow, and
  failure-rollback cases;
- stable duplicate summation/rejection and explicit-zero keep/drop behavior;
- const propagation, move-only owners, external view validation, resource
  lifetime, and structural invalidation;
- malformed compressed offsets/indices/lengths/order/duplicates;
- empty outer-offset invariants;
- coordinate/CSR/CSC round trips, including rectangular and empty matrices;
- structure-preserving expression evaluation, alias transaction, allocation
  counts, and rejection of every non-preserving sparsity class;
- independently calculated float/double SpMV values, empty/degenerate shapes,
  arbitrary external vector adapters, dense-vector interoperability when both
  headers are explicitly included, alias failures, and `beta == 0` no-read;
- ASan and UBSan;
- build-tree, copied-build-tree, install, relocation, path-with-spaces,
  static/shared, subproject, and sparse-only consumers;
- an allocation/no-densification serial reference benchmark with recorded
  compiler, configuration, shape, NNZ, iterations, layout/format, operation,
  allocation count, elapsed time, and checksum, but no unstable speed gate.

GPU evidence for Milestone 4 is exactly **skipped**.

## Explicit exclusions

- No production change to core, utilities, random, or dense beyond the exact
  neutral writable-protocol specialization named above.
- No random storage facet.
- No dense storage, dense result, dense temporary, dense include, or dense
  target edge in sparse production.
- No BSR, SELL, general-rank compressed storage, arbitrary finalized
  structural mutation, hidden builder growth, or external deleter/adoption.
- No SpMM, transpose SpMV, solver, preconditioner, factorization, or optimized
  provider.
- No CUDA, HIP, SYCL, OpenMP, TBB, Eigen, BLAS/LAPACK, oneMKL, cuSPARSE, or
  other provider integration.
- No third-party dependency.
- No compatibility layer for deleted array/linalg/sparse APIs.
- No MdeCpp source, test, vector, table, or prose copying.
- No `ASC::cpp`, tag, release, publication, or branch deletion.

## Publication gate

The lead reviews the complete cumulative diff, reconciles every specialist
finding, runs fresh local matrices, records exact pass/fail/skip evidence, and
stops at Publication Checkpoint B. Remote writes and history mutation require
separate owner approval.
