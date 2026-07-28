# Milestone 4 Sparse CPU documentation and API review

Status: complete; no unresolved documentation or public-API blocker

Date: 2026-07-28

## Scope and authority

This independent review covers only the frozen Milestone 4 Sparse CPU
contract. Its authority is the M4 contract, ownership ledger, and provenance
record; ADRs 0003, 0004, 0007--0010, 0012, 0014, and 0018; the current Core and
Expression contracts; and the exact public headers in the approved M4
inventory.

The review did not use MdeCpp, deleted asc-cpp implementation material,
third-party sparse code, or any later milestone artifact. It does not treat a
Random storage facet, optimized provider, GPU facet, solver, or aggregate as
implemented.

## Reviewed public inventory and dependencies

The public Sparse inventory is exactly:

```text
include/asc/sparse.h
include/asc/sparse/export.h
include/asc/sparse/coordinate.h
include/asc/sparse/compressed.h
include/asc/sparse/evaluate.h
include/asc/sparse/linalg.h
```

The reviewed compatibility surface is:

```text
include/asc/expression/writable.h
include/asc/expression.h
include/asc/expression/expression.h
include/asc/dense/view.h
```

`asc_sparse` is a compiled static/shared target exported as `ASC::sparse`. Its
direct ASC link interface is exactly `ASC::core;ASC::expression`.
`ASC::expression` remains a Core-only interface component. Dense gains only
non-owning placement/writable adapters; it has no Sparse include or dependency.
No external dependency was added.

The public API comprises:

- `SparseElement`, `SparseViewElement`, `SparseExtents`, the two construction
  policies, and the CSR/CSC format enum;
- move-only `CoordinateBuilder`, `CoordinateArray`, and trivially copyable
  `CoordinateView`;
- move-only `CompressedSparseArray`, trivially copyable
  `CompressedSparseView`, and the CSR/CSC aliases;
- the six named coordinate/CSR/CSC conversions;
- structure-preserving `Evaluate` overloads for existing coordinate and
  compressed destinations;
- serial CSR `Spmv` for exactly `float` and `double`; and
- the storage-neutral readable-placement and writable Expression protocols,
  including optional byte-span alias metadata and writable uniqueness.

## API and semantic review

Coordinate construction has explicit extents, capacity, and host resource.
`Add` has no hidden growth. Rvalue-qualified `Finalize` performs stable
lexicographic canonicalization, insertion-order duplicate summation, checked
integral addition, and post-sum explicit-zero handling. Its failure paths run
before mutation; success consumes the builder and reuses its declared buffers.
Rank-zero scalar storage and zero-extent empty storage are represented without
null-pointer arithmetic.

Coordinate and compressed direct-owner factories validate canonical caller
arrays before copying. Owners are move-only; views own nothing, propagate
value constness in one direction, expose only const structure, and are
trivially copyable. Host external-view construction validates canonical
content. Non-host view construction can validate metadata/address arithmetic
only; the external owner remains responsible for content, provenance,
allocation length, alignment, and lifetime.

CSR/CSC formats enforce zero base, exact offset count, first/final offset,
nondecreasing offsets, in-range inner indices, and strictly increasing
per-segment order. Invalid compile-time format values are rejected. Empty
matrices retain `outer_extent + 1` offsets.

All six conversions require serial execution and an explicit destination host
resource. Same-format `FromCompressed` is constrained out. The implementations
write direct destination buffers, preserve explicit zeros and canonical
coordinates, and do not route opposite compressed conversions through COO.

Sparse evaluation preflights context, access, placement where known, exact
rank/shape/value type, structure-preserving effect, and conservative aliasing
before mutation. Exact direct self-assignment is the sole no-op overlap.
Successful evaluation changes values only and allocates no workspace.

SpMV preflights serial/host access, exact rank-one vector shapes, writable
uniqueness, and output overlap with matrix values or input. The compiled
reference traversal is deterministic CSR row order/increasing column order.
It performs ordinary left-to-right multiply/add and does not read old output
when `beta == 0`.

`ExpressionAliasMetadata` preserves identity-only adapters and optionally
describes a value byte span. `ExpressionMayOverlap` combines identity, span,
and legacy `MayAlias` evidence. `WritableExpressionAdapter` supplies `Shape`,
`Alias`, `IsUnique`, `Write`, and an optional access-validation hook.
`WritableExpressionIsUnique` exposes the accepted M4 clarification required
for generic SpMV output preflight.

## Findings and resolutions

| Finding | Resolution |
| --- | --- |
| Dense defined placement/writable specializations without directly including their declarations. | `dense/view.h` now directly includes `expression/writable.h`; strict standalone parsing passes. |
| `CoordinateArray::Create` initially published caller arrays without actually invoking canonical validation. | The factory now validates through a const coordinate view before allocation/publication. |
| Rank-zero coordinate code initially formed null-plus-zero pointers. | Rank-zero-safe helpers/branches now cover access, lookup, validation, add, sort, and compaction. |
| Coordinate views initially admitted Boolean/non-arithmetic elements. | `SparseViewElement` now permits only mutable/const nonvolatile forms of `SparseElement`. |
| The writable protocol initially lacked the unique-mapping property required by generic output. | `IsUnique` and `WritableExpressionIsUnique` were added; Dense/Sparse adapters and SpMV implement the contract. |
| Compressed/evaluation headers relied on transitive standard/Core declarations. | Direct `<concepts>`, Core memory, and Core result includes were added as applicable. |
| Invalid compressed enum values and same-format compressed cloning exceeded the bounded format/conversion surface. | A format assertion rejects invalid values; `FromCompressed` requires the opposite format. |

No reviewed finding remains open.

## Documentation and example validation

`docs/modules/sparse.md` documents exact construction, ownership, lifetime,
const propagation, invariants, lookup, all six conversions, evaluation
preflight/no-densification behavior, SpMV, complexity, dependencies, examples,
and deliberate omissions. `docs/modules/expression.md` now documents the
placed-readable/writable protocol, optional alias spans, uniqueness, lifetime,
and the 0.4 component surface.

Commands and results:

```text
for header in asc/sparse.h asc/sparse/export.h asc/sparse/coordinate.h
  asc/sparse/compressed.h asc/sparse/evaluate.h asc/sparse/linalg.h
  asc/expression/writable.h asc/expression.h asc/dense/view.h; do
  printf '#include <%s>\n' "$header" |
    g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions
      -Iinclude -x c++ -fsyntax-only -
done
PASS (9/9)

Expression readable/placement/writable documentation example:
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions ...
PASS; linked and ran with exit 0

Coordinate builder/finalize/lookup documentation example:
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions ...
PASS; linked and ran with exit 0; lookup oracle 5.0

CSR SpMV documentation example:
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -fno-exceptions ...
PASS; linked and ran with exit 0; output oracle {13.0, 20.0}

clang++ strict documentation-example compile:
SKIPPED; clang++ is not installed in this local environment
```

Independent verification owns the full runtime, sanitizer, package,
relocation, and installed-consumer matrix. This documentation review makes no
substitute claim for those results.

## Complexity, omissions, and remaining risks

The documentation records current complexity rather than implying optimized
behavior: coordinate finalization is quadratic; coordinate-to-compressed and
opposite compressed conversions use deterministic repeated scans; compressed
evaluation currently reconstructs coordinates with repeated outer-offset
scans. There is no performance threshold in this milestone.

External adapter/view metadata remains a caller truth boundary. Its provenance,
allocation length, alignment, lifetime, alias accuracy, and thread-safety
cannot be proved by these non-owning protocols. Those are documented risks,
not hidden ownership.

No structural evaluation, hidden coordinate temporary, optimized provider,
CSC/transpose SpMV, SpMM, solver, factorization, preconditioner, Dense
dependency, or later-milestone surface is documented as present.

GPU evidence is exactly **skipped**: Milestone 4 authorizes no GPU configure,
compile, runtime, or parity claim.
