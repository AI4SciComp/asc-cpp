# Milestone 4 Production Self-Review

Status: Production implementation complete; independent integration review
continues

Date: 2026-07-26

Branch: `feature/asc-cpp-m4-sparse-cpu`

## Scope and provenance

The production implementation is confined to the frozen ownership scope:

```text
include/asc/sparse.h
include/asc/sparse/compressed.h
include/asc/sparse/coordinate.h
include/asc/sparse/evaluate.h
include/asc/sparse/export.h
include/asc/sparse/linalg.h
src/sparse/reference_linalg.cc
include/asc/expression/expression.h
include/asc/expression/writable.h
include/asc/expression.h
include/asc/dense/view.h
```

This report is the only production-agent documentation change. No CMake,
test, package, general documentation, manifest, provider, or shared root file
was edited by the production agent.

The implementation was derived from the frozen Milestone 4 contract, accepted
ADRs, current core/expression/dense contracts, and independently reported
review findings. No MdeCpp source or test, deleted asc-cpp implementation or
test, upstream implementation, numerical table, or prose was inspected or
copied. There is no new dependency or third-party source.

## Neutral writable and placement protocol

Expression now provides the approved non-intrusive customization surface:

```text
ExpressionPlacementAdapter<T>
PlacedReadableExpression<T>
WritableExpressionAdapter<T>
WritableExpression<T>
ExpressionSpace
WritableExpressionShape
WritableExpressionAlias
WriteExpression
```

Placement reports an explicit `MemorySpace`. Top-level-volatile descriptor
objects are rejected at the `PlacedReadableExpression` boundary; const
readable descriptors remain supported. A writable adapter reports the
same value type and compile-time rank as its independent readable adapter,
exact shape, conservative alias identity, and a non-failing write for an
already validated coordinate. A write is not allowed to allocate, transfer,
synchronize, choose a provider, or fail. Top-level-const descriptor objects
are rejected at the `WritableExpression` concept boundary.

`AliasToken::FromAddressSpan` adds an optional validated half-open byte span,
and `AliasTokensMayOverlap` performs symmetric conservative comparison.
Span/span comparison uses overflow-checked endpoints. A span compared with an
identity-only token treats the identity as an address point. Zero-byte spans
never overlap. Identity-only pairs retain their historical equality behavior,
and `operator==` continues to compare identity only.

Mutable dense views opt in to writable placement without a sparse include or
behavior change. Const dense views remain placed/readable but not writable.
The specialization preserves the existing non-owning view, layout, memory
space, and alias contract. Each root dense view publishes its validated
physical byte span. A subview retains the complete parent/root span, which can
conservatively reject a physically disjoint sibling but cannot miss an
overlap. Expression continues to own no storage or evaluation.

## Coordinate storage

`SparseElement` accepts non-Boolean arithmetic element types that are neither
volatile nor nontrivially copyable/destructible. Owners require unqualified,
mutable elements. `SparseExtents` accepts exactly an unqualified core
`Extents<...>` specialization.

`CoordinateBuilder<Element, ExtentsType>` is move-only. `Create` validates
extents and non-negative capacity, requires an explicit host
`MemoryResource`, and allocates separate fixed-capacity coordinate and value
buffers. `Add` checks a complete coordinate and capacity before mutation and
preserves insertion order. There is no growth, reallocation, default
resource, or hidden workspace.

`Finalize` requires an explicit serial context, `DuplicatePolicy`, and
`ExplicitZeroPolicy`. Before any mutation it performs a no-workspace
pairwise duplicate scan, rejects duplicates under `kReject`, and proves every
integral `kSum` group can be combined without overflow. It then performs a
stable in-place insertion sort and in-place compaction. Floating sums follow
insertion order and ordinary IEEE arithmetic. Explicit-zero dropping happens
after duplicate handling, so NaN is retained. Success transfers the declared
buffers to a move-only `CoordinateArray`; failure leaves the builder's
logical entries and insertion order unchanged.

Finalized coordinate structure is sorted, unique, lexicographic in dimension
order, and immutable. Values remain mutable through mutable-element views.
Rank zero is a scalar domain with at most one finalized entry. Any zero extent
requires zero entries.

`CoordinateView<Element, Rank>` is a trivially copyable non-owning descriptor.
It exposes shape, NNZ, raw coordinate/value addresses, memory space,
conservative value alias identity, checked stored-entry access, and checked
coordinate lookup. A missing coordinate reads as additive zero through
`ExpressionAdapter`. Mutable-to-const conversion is one-way.

## Compressed storage and conversion

`CompressedSparseView<Element, Format>` and
`CompressedSparseArray<Element, Format>` support compile-time CSR or CSC,
with the aliases:

```text
CsrView, CscView, CsrArray, CscArray
```

Validated creation enforces a non-negative rank-two shape, equal index/value
lengths, exactly `outer_extent + 1` offsets, first offset zero,
nondecreasing bounded offsets, final offset NNZ, in-range inner indices, and
strictly increasing inner indices per segment. Empty storage still owns the
complete all-zero outer-offset array. Compressed owners are move-only and
copy validated input into three explicit resource allocations. Partial
allocation failure releases completed buffers and publishes no owner.

External coordinate and compressed views validate length, alignment, byte and
address arithmetic. Coordinate structure/value spans and all compressed
structure/value spans must be disjoint. For host memory, canonical structure
is validated before publication. A descriptor for a non-host space exposes
raw addresses and metadata only; checked `CoordinateAt`, `ValueAt`, and `Find`
reject host dereference. As with `DenseView::data`, raw sparse pointers do not
authorize host dereference, prove allocation length/provenance/lifetime, or
extend storage lifetime.

Named conversions are:

```text
ToCsr(context, coordinate, resource)
ToCsc(context, coordinate, resource)
ToCoordinate(context, csr-or-csc, resource)
ToCsc(context, csr, resource)
ToCsr(context, csc, resource)
```

Every conversion requires serial host execution and a destination host
resource. It allocates only the destination buffers, preserves explicit zeros
and value bit patterns through ordinary copying, and never mutates the
source. Coordinate/compressed conversion uses deterministic repeated scans.
CSR/CSC conversion writes the opposite format directly and does not route
through a coordinate owner. Compressed-to-coordinate construction uses a
builder whose buffers become the requested coordinate result; it does not
create another coordinate temporary.

## Sparse expression evaluation

Coordinate and compressed views participate as placed, terminal,
structure-preserving readable expressions. Mutable variants also participate
in the writable protocol.

Sparse `Evaluate` accepts only:

- serial execution and a host destination;
- an exact destination rank, shape, and value type;
- a structure-preserving source expression;
- the caller-provided canonical destination structure;
- no possible destination value overlap.

All validation completes before mutation. Exact direct self-assignment is a
no-op. ASC coordinate/compressed terminals and built-in unary/binary nodes
receive recursive physical value-byte-span overlap checks, including
independently created offset views with different identity tokens. External
adapters retain responsibility for truthful placement and conservative alias
metadata.

The evaluator visits only destination stored coordinates and writes only
values; it never inserts, erases, converts, or densifies structure. Rank-zero
scalar expansion and every non-preserving sparsity effect instantiate
successfully but return a status before mutation. Successful evaluation
creates no computational storage, temporary, workspace, packing, conversion,
transfer, synchronization, dispatch, or fallback.

## Serial CSR SpMV

The only algebra operation is:

```text
Spmv(context, alpha, csr_matrix, input, beta, output)
```

The matrix scalar is exactly `float` or `double`. Input is any placed readable
rank-one expression of the same scalar. Output is an independently readable,
placed, writable rank-one expression of the same scalar. External types need
no ASC inheritance, storage type, contiguity, or overloaded address-of
behavior; type erasure uses `std::addressof`.

Before mutation SpMV validates serial execution, host placement, matrix/vector
shape, consistent readable/writable output shape, and conservative output
overlap with matrix values and input. ASC coordinate-vector outputs must
store every logical coordinate exactly once; an incomplete sparse output is
rejected unchanged. Generic external writable adapters promise the equivalent
unique total-writable contract. ASC coordinate matrix/input/output
combinations receive physical value-byte-span checks; generic external types
use their conservative alias tokens.

The compiled float/double reference kernel traverses CSR rows and increasing
canonical column indices. Each product uses ordinary left-to-right IEEE
multiply/add. `beta == 0` branches before output reading. Empty shapes and
zero NNZ are valid. Successful execution allocates no storage or workspace,
packs and converts nothing, performs no transfer or synchronization, and
does not dispatch or fall back.

## Complexity and observable costs

| Operation | Time | Successful computational storage |
| --- | --- | --- |
| builder creation | `O(1)` metadata | at most two declared-capacity buffers |
| `Add` | `O(Rank)` | none |
| `Finalize` | `O(N^2 * Rank)` | none; reuses declared buffers |
| coordinate lookup | `O(log N * Rank)` | none |
| compressed owner creation | `O(outer + N)` | exactly three destination buffers |
| coordinate to CSR/CSC | `O(outer * N)` | destination buffers only |
| CSR to CSC / CSC to CSR | `O(destination_outer * (source_outer + N))` | destination buffers only |
| CSR to coordinate | `O(N + N^2)` including finalization | destination coordinate buffers only |
| CSC to coordinate | `O(rows * columns + N^2)` including finalization | destination coordinate buffers only |
| sparse evaluation | `O(destination_N * source lookup/expression cost)` | none |
| CSR SpMV | `O(rows + N)` | none |

The fixed-capacity coordinate owner retains its original allocated capacity
after compaction; NNZ reports the published logical entries. Compressed
conversion destinations allocate exactly outer offsets, inner indices, and
values. No performance baseline or provider speed claim is made here.

Successful computational paths described as allocation-free exclude
diagnostic `Status` string storage on rejected calls. M4 makes no general
no-heap error-path claim.

## Review findings resolved

- **M4-DOC-01:** `WritableExpression<const T>` now fails at the public concept
  boundary; const descriptors cannot reach a mutable adapter body.
- **M4-DOC-02:** external sparse view creation rejects coordinate/value span
  overlap and every pairwise compressed structure/value overlap, preventing
  mutable values from corrupting immutable structure.
- **M4-DOC-03:** iterable spans were removed for potentially non-host storage.
  Raw pointers remain non-dereferencing descriptors with the same caller
  precondition as dense raw data; checked sparse access rejects non-host
  memory.
- **M4-DOC-04:** SpMV rejects incomplete coordinate-vector output structure
  before any read or write.
- **M4-DOC-05:** generic operand type erasure uses `std::addressof`, so an
  external type's overloaded unary address-of operator is irrelevant.
- **M4-LEAD-ALIAS:** SpMV performs physical value-byte-span checks for ASC
  sparse operands instead of relying only on token equality. Sparse
  evaluation already recursively applies the same rule.
- **M4-VERIFY-RANK:** coordinate and compressed evaluation mutation bodies are
  compile-time gated by exact rank; rank-zero and other mismatches compile and
  return pre-mutation validation failures.
- **M4-PORT-01:** root dense and sparse views now publish validated
  span-aware alias tokens, and their adapters use symmetric byte-span overlap.
  Independently created offset views can no longer evade SpMV overlap
  validation. Dense subviews retain root-span conservatism.
- **M4-PORT-02:** placed-readable and therefore writable concepts reject
  top-level-volatile descriptors at the concept boundary. Const readable
  placement remains supported.
- **M4-DOC-06:** base `ReadableExpression` now rejects top-level volatile and
  const-volatile descriptors at its own concept boundary. Expression shape,
  read, alias, sparse evaluation, and derived placement/writable APIs therefore
  fail by constraints instead of inside a function body. Top-level const
  readable expressions remain supported.

## Production validation

Formatting and whitespace checks passed:

```bash
clang-format-19 --dry-run --Werror \
  include/asc/expression/expression.h \
  include/asc/expression/writable.h include/asc/expression.h \
  include/asc/dense/view.h include/asc/sparse.h include/asc/sparse/*.h \
  src/sparse/reference_linalg.cc

git diff --check -- \
  include/asc/expression/expression.h \
  include/asc/expression/writable.h include/asc/expression.h \
  include/asc/dense/view.h include/asc/sparse.h include/asc/sparse \
  src/sparse/reference_linalg.cc
```

For both `g++-11` 11.4.0 and `clang++-19` 19.0.0, every new/modified public
header passed individually with exceptions enabled and disabled:

```bash
<compiler> -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only <header>

<compiler> -std=c++20 -fno-exceptions -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  -x c++ -fsyntax-only <header>
```

The compiled source passed both compilers with both exception configurations:

```bash
<compiler> -std=c++20 [-fno-exceptions] -pedantic-errors \
  -Wall -Wextra -Wconversion -Wsign-conversion -Werror \
  -DASC_SPARSE_BUILDING_LIBRARY -Iinclude \
  -c src/sparse/reference_linalg.cc -o <temporary-object>
```

A standalone strict probe instantiated and ran validated alias-token spans,
builder/finalization,
coordinate-to-CSR-to-CSC-to-coordinate conversion, structure-preserving
evaluation, rank-zero evaluation rejection, dense-view writable
interoperability, and compiled double SpMV under GCC and Clang. It linked
against the previously validated M3 core archive; both executions returned
zero:

```bash
<compiler> -std=c++20 -pedantic-errors -Wall -Wextra \
  -Wconversion -Wsign-conversion -Werror -Iinclude \
  /tmp/m4_sparse_production_probe.cc \
  src/sparse/reference_linalg.cc \
  /tmp/asc-cpp-m3-production-gcc-lib/src/core/libasc_core.a \
  -o <temporary-probe>

<temporary-probe>
```

The probe statically verified readable/writable concepts, top-level-const
writable rejection, top-level-volatile and const-volatile base-readable
rejection, top-level-volatile placement/writable rejection, const readable and
placed-readable support, and move-only/result types. Runtime checks verified
overlapping/disjoint/zero-span alias behavior, an identity point inside a
span, preserved identity equality, rejection without mutation for partially
overlapping independently created dense SpMV vectors, the negated stored
value, scalar rejection, and SpMV result `(0, 12)`.

The lead owns actual target/package integration and the fresh full validation
matrix. The production role did not claim a CMake, package, sanitizer,
provider, benchmark, or GPU result. GPU evidence for M4 remains exactly
`skipped`.

## Residual assumptions for independent review

- Raw external sparse pointers require truthful provenance, allocation length,
  lifetime, and non-host accessibility metadata; C++ cannot prove them.
- A non-host external structural descriptor cannot be dereferenced to prove
  canonical ordering in M4. Algorithms reject it before access; the caller
  remains responsible for truthful structural metadata.
- External expression adapters are responsible for truthful readable,
  placement, total-writable, and conservative alias contracts.
- Stable floating duplicate summation is deterministic but remains
  order-sensitive and follows ordinary IEEE behavior.
- Reference algorithms deliberately use repeated scans and scalar arithmetic;
  optimized CPU and all GPU providers are deferred.
