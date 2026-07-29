# Milestone 4 production self-review

Role: production implementation engineer (`/root/m4_production`)

Scope reviewed: the frozen Sparse CPU production inventory and its four
authorized predecessor compatibility headers only.

## Implemented surface

- `CoordinateBuilder<Element, ExtentsType>` is a move-only, fixed-capacity,
  host-resource builder. `Add` validates a complete coordinate and capacity
  before mutation.
- Consuming `Finalize` validates its serial context and both policy enums
  before mutation. It preflights duplicate rejection and checked integral
  duplicate sums, performs a stable allocation-free in-place
  lexicographical sort, combines duplicates in insertion order, applies the
  explicit-zero policy, and publishes a move-only canonical
  `CoordinateArray`. A failure leaves the builder's logical entries intact.
- `CoordinateView<Element, Rank>` is a constrained, trivially copyable
  non-owning view with immutable structure, element const propagation,
  checked stored access, coordinate lookup, placement, and conservative
  value-span alias metadata. Rank zero and empty domains are handled without
  null-pointer arithmetic.
- `CompressedSparseArray<Element, Format>` and
  `CompressedSparseView<Element, Format>` implement exactly rank-two canonical
  CSR/CSC. External and copied-owner construction validate shape, offset
  count, zero base, monotonic offsets, final NNZ, inner bounds, strict inner
  ordering, byte counts, and address-range arithmetic. Empty owners retain one
  zero offset per empty outer descriptor.
- The six named `ConvertToCsr`, `ConvertToCsc`, and
  `ConvertToCoordinate` overloads cover COO-to-CSR/CSC, CSR/CSC-to-COO, and
  CSR-to-CSC/CSC-to-CSR. Each uses explicit serial execution and a destination
  host resource. Opposite compressed-format conversions use deterministic
  repeated scans and do not route through COO.
- Sparse `Evaluate` accepts only an exact-rank, exact-shape,
  structure-preserving expression and an existing mutable COO/CSR/CSC
  structure. Context, access, effect, shape, exact self-assignment, and
  conservative value overlap are resolved before mutation. Successful
  traversal writes only stored values and allocates nothing.
- `Spmv` accepts canonical CSR and placed rank-one readable/writable operands
  for exactly `float` and `double`. It validates context, access, exact vector
  lengths, output uniqueness, and output overlap before entering the compiled
  deterministic serial kernel. `beta == 0` does not read the old output.
- `ExpressionPlacementAdapter`, `PlacedReadableExpression`,
  `WritableExpressionAdapter`, and `WritableExpression` are storage-neutral.
  Public queries expose space, shape, conservative alias metadata, write
  placement, and output uniqueness. Dense views opt in without acquiring a
  Sparse include or dependency.

## Dependency and scope audit

Sparse production includes only standard-library, Core, and Expression
headers. It has no Dense, Utilities, Random, aggregate, provider, or SDK
include. Dense contains only the authorized neutral placement/writable
specializations and does not include Sparse. No external dependency, hidden
allocation, transfer, packing, synchronization, provider selection, or
fallback was introduced.

No BSR/SELL storage, structural mutation, hidden growth, transpose SpMV, SpMM,
solver, preconditioner, optimized provider, GPU provider, random facet, later
milestone API, or compatibility layer was implemented.

## Findings resolved during implementation

1. The first writable protocol draft could not prove unique output mappings.
   The lead accepted the verifier's clarification. The protocol now requires
   `WritableExpressionAdapter<T>::IsUnique`, exposes
   `WritableExpressionIsUnique`, and SpMV rejects a nonunique output before
   mutation. Dense and Sparse adapters report their validated uniqueness.
2. `CoordinateArray::Create` initially copied caller storage before enforcing
   canonical structure. It now validates a const external coordinate view
   before any allocation or publication.
3. Initial rank-zero paths formed `nullptr + 0`. Coordinate access, builder
   sorting, compaction, and owner shape/copy paths now branch at compile time
   and never perform null-pointer arithmetic.
4. Coordinate views initially admitted unsupported element types. Both
   coordinate and compressed views now require mutable or const forms of a
   valid `SparseElement`; invalid compressed format template values are also
   rejected.
5. Unknown duplicate and explicit-zero enum values initially followed an
   ordinary branch. Finalization now rejects both before any mutation.
6. Same-format `FromCompressed` could expose an unapproved clone operation.
   The helper is constrained to the opposite format, leaving only CSR-to-CSC
   and CSC-to-CSR.
7. Public headers initially relied on transitive standard/Core includes.
   Direct `<concepts>`, memory, result, and writable protocol includes were
   added where their names are used.
8. Sparse evaluation originally instantiated an invalid `ExpressionRead` for
   wrong-rank and rank-zero sources even though validation returned a shape
   error. Stored traversal is now compile-time guarded by equal rank, so all
   rank mismatches compile and fail transactionally with `kShape`.
9. External-view validation initially checked byte counts but not the complete
   integer address range. COO/CSR/CSC constructors now reject a value,
   coordinate, index, or offset span whose end would overflow `uintptr_t`.

## Production validation

The following production-owned checks passed:

```text
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -fsyntax-only -x c++
    -Iinclude <each new/modified public header>
PASS (8 headers)

g++ -std=c++20 -fno-exceptions -Wall -Wextra -Werror -pedantic
    -fsyntax-only -x c++ -Iinclude <each new/modified public header>
PASS (8 headers)

g++ -std=c++20 -fno-exceptions -Wall -Wextra -Werror -pedantic
    -Iinclude -c src/sparse/reference_linalg.cc
PASS

cmake --build /tmp/asc-cpp-m4-production-smoke
    --target asc_sparse --parallel 4
PASS

clang-format-19 --dry-run --Werror <production inventory>
PASS

git diff --check -- <production inventory>
PASS
```

Two direct linked smoke programs also passed:

- unsorted COO construction/finalization, COO-to-CSR, Dense-view double SpMV,
  `beta == 0`, numerical result, and overlap rejection;
- rank-zero duplicate sum/zero drop, all six conversions, separate-destination
  structure-preserving evaluation, and stored-value numerics.

These smokes are production checks only. The independent verification role
owns the milestone test verdict and sanitizer evidence.

## Remaining production risks

- External view adapters remain responsible for pointer provenance,
  allocation length, alignment, lifetime, and truthful alias/uniqueness
  metadata; those properties cannot be established from raw pointers.
- Device-memory descriptors retain their declared placement but Milestone 4
  operations reject non-host access. GPU evidence is **skipped**.
- Conversion complexity is intentionally deterministic repeated scanning for
  CSC construction and CSR/CSC transposition. It is correct for the reference
  milestone but is not an optimized provider path.
- Native MSVC/AppleClang and hosted sanitizer/package matrices remain lead and
  independent-review evidence rather than production-role claims.
