# Milestone 4 contract: Sparse CPU

Status: Frozen from the owner-approved Milestone 4 contract

Original approval date: 2026-07-26

Continuation date: 2026-07-28

Corrections: None

Exact milestone: **Milestone 4 — sparse CPU**

Branch: `feature/asc-cpp-m4-sparse-cpu`

Baseline commit:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the intentionally uncommitted, locally validated
Milestones 0--3 Publication Checkpoint B candidate and the user's prior
deletion of the retired implementation are retained in place. This milestone
does not restore, reset, discard, commit, or reclassify that work.

## Authority and purpose

This contract is subordinate to the owner's current instruction, the supplied
runbook, the approved architecture package and ADRs, and the exact
owner-approved Milestone 4 contract retained as architecture evidence on the
completed hardening branch. No later implementation, test, or review artifact
is adopted by this continuation.

Implement only the approved CPU foundation of the `sparse` module:

- general-rank coordinate construction, finalized ownership, and views;
- canonical rank-two CSR and CSC ownership and views;
- explicit coordinate/compressed conversions;
- destination-owned structure-preserving sparse expression evaluation; and
- deterministic serial CSR sparse matrix-vector multiplication.

The module is strict C++20, uses flat `namespace asc`, and has exactly:

```text
ASC::sparse -> ASC::core
ASC::sparse -> ASC::expression
```

Sparse must not include or link Utilities, Dense, Random, a retired Array or
Linalg component, an aggregate, an optional provider, or a provider SDK.

## Product target and exact files

```text
build target:    asc_sparse
exported target: ASC::sparse
kind:            static/shared compiled library
direct links:    ASC::core;ASC::expression
release line:    unreleased 0.4.0 candidate
```

The exact new Sparse production inventory is:

```text
include/asc/sparse.h
include/asc/sparse/export.h
include/asc/sparse/coordinate.h
include/asc/sparse/compressed.h
include/asc/sparse/evaluate.h
include/asc/sparse/linalg.h
src/sparse/reference_linalg.cc
```

Milestone 4 may make only these predecessor compatibility changes:

```text
include/asc/expression/writable.h
include/asc/expression.h
include/asc/expression/expression.h
include/asc/dense/view.h
```

`writable.h` remains storage-neutral and owned by Expression. The Dense change
is limited to non-owning writable/placement protocol specialization so a
Dense view can be an SpMV vector when a consumer explicitly includes both
components. It creates no Sparse include or dependency.

Expression alias metadata may be extended with an optional byte span so
storage adapters can conservatively reject partial overlap. Existing
identity-token source behavior remains valid and source compatible; Expression
does not acquire storage ownership, evaluation, or a sibling dependency.

All public headers are self-contained `.h` files with full-path guards and
direct includes. Ordinary compiled implementation is `.cc`. Internal
namespace names contain `internal` and do not leak into signatures.

## Common Sparse vocabulary

The public surface includes:

```text
SparseElement
DuplicatePolicy::{kReject,kSum}
ExplicitZeroPolicy::{kKeep,kDrop}
SparseCompressedFormat::{kCsr,kCsc}
```

Sparse storage elements are unqualified, non-Boolean arithmetic values that
are trivially copyable and trivially destructible. SpMV is restricted further
to exactly unqualified `float` and `double`.

Shapes, coordinates, inner indices, outer offsets, capacity, and NNZ use the
checked Core vocabulary. Logical coordinates and inner indices are zero-based
signed 64-bit values. Products, sums, bytes, positions, casts, and address
ranges are checked before publication or mutation.

## Coordinate construction and finalized storage

The public family is:

```text
CoordinateBuilder<Element, ExtentsType>
CoordinateArray<Element, ExtentsType>
CoordinateView<Element, Rank>
```

`ExtentsType` is an unqualified Core `Extents<...>` specialization.
Compile-time rank comes from that type. Rank zero is a scalar domain; any zero
extent is empty and cannot contain an entry.

The builder is move-only and is created with validated extents, explicit
non-negative capacity, and an explicit host `MemoryResource`. Capacity
allocation is visible. `Add` accepts a complete coordinate and value, validates
indices/capacity before mutation, and preserves insertion order. There is no
hidden growth, reallocation, entropy, or default resource.

Consuming `Finalize` takes:

- an explicit serial `ExecutionContext`;
- an explicit `DuplicatePolicy`; and
- an explicit `ExplicitZeroPolicy`.

Finalization is transactional. It performs stable lexicographic
canonicalization in dimension order, combines duplicates in insertion order
only for `kSum`, and rejects duplicates before publication for `kReject`.
Integral sums use checked addition. Floating sums follow deterministic ordinary
IEEE arithmetic. `kDrop` removes values equal to `Element{}` after duplicate
handling; NaN is not zero.

Finalization may reuse declared capacity storage but performs no undisclosed
allocation or workspace acquisition. Success consumes the builder and returns
a move-only sorted, unique, structurally immutable owner with mutable values.
Failure leaves the builder valid with its original logical entries and
publishes no partial owner.

The finalized view is trivially copyable and non-owning. Structure is always
const; element constness controls value mutation. Mutable-to-const conversion
is one way. Structure replacement or owner destruction invalidates views.

Views expose shape, rank, NNZ, coordinates, values, memory space, checked
stored-entry access, coordinate lookup, and conservative value-span alias
metadata. Host dereference rejects non-host memory. External view construction
validates metadata/address arithmetic but cannot prove pointer provenance,
allocation length, alignment, or lifetime.

Coordinate views are structure-preserving readable expression terminals.
Missing logical coordinates read as additive zero without allocation.

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

`Format` is compile-time `kCsr` or `kCsc`; rank is exactly two. Canonical
compressed invariants are:

- zero index base;
- `outer_extent + 1` offsets;
- first offset zero and nondecreasing offsets;
- final offset equal to NNZ;
- every inner index in range;
- strictly increasing inner indices per outer segment;
- no duplicates;
- immutable structure and mutable values;
- an empty object still owns `outer_extent + 1` zero offsets.

Validated owner construction copies caller offsets, indices, and values
through an explicit host resource. Every allocation/byte count is observable.
Failure publishes no owner and releases partial allocations exactly once.

Compressed owners are move-only. Const propagation, external-view validation,
resource/view lifetime, structure invalidation, host accessibility, and alias
metadata follow the coordinate contract.

Compressed views are structure-preserving readable expression terminals.
Coordinate lookup uses the canonical segment and returns zero for an unstored
coordinate.

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

Every conversion takes an explicit serial context and destination host
resource and returns a new move-only owner. Output buffers are explicit
allocations. Deterministic repeated scans are permitted only when documented
complexity matches the implementation.

No conversion mutates its source, changes index base/width, drops explicit
zeros, combines duplicates, routes through an undisclosed COO temporary,
packs, transfers, densifies, synchronizes, dispatches, or falls back. Failure
is transactional and releases partial destination allocations. Round trips
preserve shape, canonical coordinates/order, explicit zeros, and copied values.

## Storage-neutral writable and placement protocol

Expression adds an opt-in, non-intrusive contract:

```text
ExpressionPlacementAdapter<T>
PlacedReadableExpression<T>
WritableExpressionAdapter<T>
WritableExpression<T>
ExpressionSpace(...)
WritableExpressionShape(...)
WritableExpressionAlias(...)
WritableExpressionIsUnique(...)
WriteExpression(...)
```

Spellings may be refined only for consistency with the existing Expression
API. Semantics are frozen.

A writable adapter supplies value type, compile-time rank, exact shape,
explicit memory space, conservative alias metadata, a truthful destination
uniqueness query, and a write operation for an already validated logical
coordinate. Writing cannot allocate, transfer, synchronize, select a provider,
or fail after validation. A read/write destination independently satisfies
`ReadableExpression`.

Expression nodes remain storage-neutral and gain no evaluation. Existing
readable adapters remain source compatible. Dense, Sparse, and external
non-ASC vector types may opt in without a sibling dependency.

## Sparse expression evaluation

Sparse evaluates into caller-provided mutable coordinate or compressed
destinations. Milestone 4 accepts only:

- exact destination rank, shape, and value type;
- explicit serial execution;
- host-accessible source and destination where placement is known;
- sparsity effect exactly `kStructurePreserving`; and
- an existing caller-provided destination structure.

The evaluator visits canonical stored coordinates and writes values only.
Structure-filtering, union/intersection, value-dependent, densifying,
destination-required, and rank-zero expansion expressions are rejected before
mutation.

Context, placement, structure, shape, and conservative alias validation
complete before mutation. Exact direct self-assignment is a no-op; every other
possible destination-value overlap is rejected. External adapters remain
responsible for truthful metadata.

Successful evaluation allocates no computational storage or workspace, changes
no structure, and performs no packing, conversion, transfer, synchronization,
provider dispatch, or fallback.

## Serial CSR SpMV

The sole algebra operation is:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

It supports canonical CSR, exactly `float`/`double`, explicit serial execution,
host memory, a placed readable rank-one input, and a placed readable/writable
rank-one unique output. Arbitrary non-owning vector implementations may
participate through truthful Expression adapters.

Matrix shape is `(rows, columns)`, input length is `columns`, and output length
is `rows`. Context, placement, shape, structure, scalar agreement, and possible
output overlap with matrix values/input are checked before mutation.

Traversal is deterministic CSR row order with increasing columns. Each row
uses ordinary left-to-right IEEE multiply/add. `beta == 0` does not read old
output. Empty shapes and zero NNZ are valid. NaN/infinity follow ordinary IEEE
behavior.

Successful SpMV allocates no storage/workspace, packs nothing, performs no
conversion/densification/transfer/synchronization, and never dispatches or
falls back. CSC SpMV, transpose, SpMM, triangular solve, preconditioners,
solvers, and optimized/GPU providers are deferred.

## Packaging and dependencies

The cumulative available components become:

```text
core utilities expression dense sparse random
```

Unavailable and unexported:

```text
random_dense random_sparse cpp
every provider facet
```

Required `sparse` expands exactly to `core;expression;sparse`. A Sparse-only
installed consumer must compile/link/run with an external vector adapter and
without Dense or any other forbidden target.

Minimum CMake remains 3.25. Root integration requires exact ASCCMake 0.1.0 and
only its real APIs:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

Standard CMake owns conditional component export/install logic.

## Required evidence

- all new/modified public headers alone with exceptions enabled/disabled;
- multi-TU ODR and positive/negative concepts;
- direct include/target graph audits;
- rank-zero, zero-extent, empty capacity, negative, overflow, and rollback;
- duplicate sum/reject and zero keep/drop behavior;
- constness, move-only ownership, lifetime, invalidation, external views;
- malformed compressed metadata and empty offset invariants;
- rectangular/empty COO/CSR/CSC round trips;
- structure-preserving evaluation, alias transaction, allocation counts, and
  rejection of every non-preserving sparsity class;
- independent float/double SpMV oracles, empty/degenerate shapes, external
  vector adapters, Dense interoperability only when explicitly included,
  alias failures, and beta-zero no-read;
- ASan/UBSan;
- build-tree, copied-build-tree, install/relocation, paths with spaces,
  static/shared, subproject, and Sparse-only consumers; and
- allocation/no-densification benchmark recording compiler, configuration,
  shape, NNZ, iterations, format, operation, allocations, time, and checksum,
  with no unstable speed gate.

GPU evidence is exactly **skipped**.

## Explicit exclusions and publication gate

- No production Core, Utilities, or Random change.
- No Dense change beyond the neutral view protocol specialization.
- No Dense storage/result/temporary/include/target edge in Sparse.
- No random storage facet, BSR, SELL, arbitrary-rank compressed format,
  structural mutation, hidden builder growth, or external adoption.
- No SpMM, transpose SpMV, solver, factorization, preconditioner, optimized
  provider, or GPU.
- No third-party dependency or deleted API compatibility layer.
- No MdeCpp source, test, vector, table, or prose copying.
- No `ASC::cpp`, tag, release, publication, or branch deletion.

The lead reviews the cumulative diff, reconciles every finding, runs fresh
local matrices, records exact pass/fail/skip evidence, and stops at
Publication Checkpoint B. Remote writes require separate owner approval.
