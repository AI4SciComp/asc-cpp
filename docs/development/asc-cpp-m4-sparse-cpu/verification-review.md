# Milestone 4 independent verification review

Status: **Approved for Publication Checkpoint B**

Date: 2026-07-28

Reviewer: independent verification engineer (`/root/m4_verification`)

GPU evidence: **skipped**

## Independence and scope

The verification design was frozen before inspecting Milestone 4 production.
Its COO/CSR/CSC structures, SpMV values, failure transactions, compile
negatives, allocation checks, and consumer plan were derived independently
from the frozen contract and applicable ADRs. No MdeCpp, deleted asc-cpp,
third-party, provider, or later-milestone implementation or test was read or
used.

The final review covers only:

- general-rank coordinate construction/finalization/owners/views;
- canonical rank-two CSR/CSC owners/views and the six approved conversions;
- structure-preserving Sparse evaluation;
- serial reference CSR SpMV for `float` and `double`;
- the new neutral placement/writable protocol and Dense-view opt-in;
- Sparse-only consumption, compile contracts, allocation behavior, and
  performance sanity.

No later sparse format, structural expression evaluation, solver, optimized
provider, or GPU behavior is tested or claimed.

## Verification inventory

Runtime and support:

```text
tests/sparse/allocation_probe.cc
tests/sparse/allocation_probe.h
tests/sparse/compressed_conversion_test.cc
tests/sparse/coordinate_test.cc
tests/sparse/evaluate_test.cc
tests/sparse/external_vector.h
tests/sparse/linalg_test.cc
tests/sparse/test_expression.h
tests/sparse/test_resources.h
tests/sparse/test_support.h
```

Positive compile/ODR/interoperability:

```text
tests/compile/m4_header_sparse.cc
tests/compile/m4_header_coordinate.cc
tests/compile/m4_header_compressed.cc
tests/compile/m4_header_evaluate.cc
tests/compile/m4_header_export.cc
tests/compile/m4_header_linalg.cc
tests/compile/m4_header_writable.cc
tests/compile/m4_exceptions_disabled.cc
tests/compile/m4_dense_interop.cc
tests/compile/m4_multi_tu.h
tests/compile/m4_multi_tu_a.cc
tests/compile/m4_multi_tu_b.cc
tests/compile/m4_multi_tu_main.cc
```

Expected compile failures:

```text
tests/compile/m4_negative_builder_copy.cc
tests/compile/m4_negative_const_value_mutation.cc
tests/compile/m4_negative_coordinate_owner_copy.cc
tests/compile/m4_negative_invalid_compressed_format.cc
tests/compile/m4_negative_non_extents_builder.cc
tests/compile/m4_negative_same_format_conversion.cc
tests/compile/m4_negative_structure_mutation.cc
tests/compile/m4_negative_unadapted_writable.cc
tests/compile/m4_negative_unsupported_element.cc
tests/compile/m4_negative_unsupported_spmv_scalar.cc
tests/compile/m4_negative_wrong_rank_spmv.cc
```

Consumer and performance:

```text
tests/consumer/sparse/main.cc
benchmarks/sparse/sparse_benchmark.cc
```

The lead owns all CMake registration and shared package fixtures.

## Functional and transactional evidence

### Coordinate storage

- Stable lexicographic finalization is checked at rank zero, two, and three.
- The independent `(3,4)` oracle verifies stable duplicate summation,
  duplicate rejection, explicit-zero keep/drop, insertion-order arithmetic,
  and exact canonical coordinate/value order.
- Floating NaN survives `kDrop`; integral duplicate overflow fails without
  changing the builder.
- Negative/out-of-range coordinates, exhausted/negative/overflowing capacity,
  invalid policy enumerators, zero extents, rank-zero scalar domains, and
  byte-count overflow are checked.
- Failed duplicate, overflow, policy, and allocation operations retain their
  documented pre-call state and publish no owner.
- Move-only ownership, moved-from rejection, const propagation, trivially
  copyable views, mutable values, immutable coordinates, checked stored
  access/lookup, placement, alias spans, and exact releases are covered.

### Compressed storage and conversions

- Independent CSR and CSC structures for the `(3,4)` oracle are compared
  offset by offset, index by index, and value by value.
- Both CSR and CSC reject wrong offset counts, invalid starts/finals,
  decreasing/negative offsets, negative/out-of-range inner indices,
  unsorted/duplicate segments, invalid shapes/NNZ, null nonempty buffers, and
  arithmetic overflow.
- Empty `(0,4)`, `(3,0)`, and `(0,0)` COO/CSR/CSC owners and all applicable
  round trips retain the required `outer_extent + 1` zero offsets.
- COO-to-CSR, COO-to-CSC, CSR-to-COO, CSC-to-COO, CSR-to-CSC, and CSC-to-CSR
  match the independent structures. Explicit zero and rectangular/empty
  round trips preserve shape, structure, NNZ, and values.
- Conversion leaves its source unchanged. Injected failure at each of the
  three compressed destination buffers releases partial storage exactly once.
  A successful compressed conversion performs exactly three explicit
  destination-resource allocations.
- Invalid compressed format instantiation and unapproved same-format public
  conversion are compile-negative contracts.

### Sparse evaluation

- Coordinate and CSR destinations are evaluated from a terminal, negated
  expression, and a non-ASC external expression.
- Only canonical stored coordinates are written; structure is unchanged.
- Exact direct self-assignment is a no-op.
- Shape, rank, rank-zero expansion, source/destination placement, source
  access, all six non-preserving sparsity effects, and partial value overlap
  fail before mutation.
- Successful structure-preserving evaluation performs zero process
  allocations and acquires no resource workspace.

### CSR SpMV

For the independent CSR matrix

```text
offsets = [0, 2, 4, 5]
indices = [1, 3, 0, 2, 1]
values  = [2, -1, 4, 5, 3]
```

with `x = [2,-1,3,4]`, old `y = [7,11,13]`, `alpha = 2`, and
`beta = -0.5`, both `float` and `double` produce:

```text
A*x = [-6, 23, -3]
y   = [-15.5, 40.5, -12.5]
```

Coverage also includes deterministic left-to-right cancellation, strided
external vectors, zero rows, zero columns, zero NNZ, beta scaling, and
`beta == 0` with NaN old output. The suite rejects wrong lengths, nonunique
output, matrix/output overlap, exact and partial input/output overlap,
non-host matrix/input/output, unsupported integer scalar, and wrong-rank
output before mutation. Successful calls allocate zero storage.

The installed Sparse-only consumer defines its own external vector adapters,
links only `ASC::sparse`, invokes the compiled SpMV symbol, and never includes
Dense. A separate positive compile probe explicitly includes both components
and verifies Dense-view SpMV interoperability.

## Findings and resolutions

| Finding | Resolution | Regression |
| --- | --- | --- |
| The initial writable protocol had no neutral output-uniqueness query, so generic SpMV could not validate a repeated-address external destination. | Added required adapter `IsUnique` and public `WritableExpressionIsUnique`; Dense, Sparse, and external test adapters implement it. | Nonunique stride-zero output fails transactionally; unadapted writable type is a compile negative. |
| `CoordinateView` initially accepted forbidden Boolean/non-arithmetic element types. | Constrained view elements through the Sparse storage element contract while preserving one-way const conversion. | `m4_negative_unsupported_element.cc`. |
| Rank-zero coordinate access could form `nullptr + 0`. | Rank-zero paths avoid pointer arithmetic. | Rank-zero duplicate finalization, coordinate retrieval, lookup, GCC, ASan, and UBSan. |
| Invalid duplicate/zero policy enumerators could be interpreted as ordinary policies. | Finalization now validates both enumerators before sorting or mutation. | Invalid-policy rollback cases. |
| Wrong-rank/rank-zero evaluation validated a shape error but still instantiated `ExpressionRead` with destination-rank coordinates. | Equal-rank traversal is guarded at compile time; mismatches return `kShape` before mutation. | Runtime rank-one/rank-zero sources into rank-two destination. |
| Public compressed templates did not explicitly reject invalid format values, and `FromCompressed` exposed an unapproved same-format copy. | Added valid-format enforcement, a direct `<concepts>` include, and cross-format-only `FromCompressed`. | Invalid-format and same-format compile negatives. |
| Initial lead full run reported 139/141: the coordinate overflow probe expected `kOverflow` for NNZ exceeding a `(3,4)` logical domain, and the cumulative M2 inventory audit rejected additive M4 files. | The test now uses valid shape `(extent_t::max,1)` with NNZ max to reach checked coordinate-byte overflow; the lead made the historical M2 audit additive-safe. | Corrected coordinate test passes independently under GCC and ASan/UBSan; stabilized lead full matrix is 141/141. |

No unresolved correctness, dependency, API, or verification blocker remains.

## Exact independent commands and results

Environment:

```text
CMake 4.1.2
GCC 11.4.0
Clang 19.0.0
ASCCMake 0.1.0 from:
/home/yicai/AI4SciComp/asc-cmake/build/install
```

GCC Debug/static, warnings as errors:

```sh
cmake -S . -B /tmp/asc-cpp-m4-verification-gcc \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m4-verification-gcc --parallel 4
ctest --test-dir /tmp/asc-cpp-m4-verification-gcc \
  -L milestone-4 --output-on-failure -j4
```

Result: configure/build **passed**; Milestone 4 plus its component/package
closure **49/49 passed**. The final focused Sparse selection also **6/6
passed**.

Clang ASan plus UBSan focused verification:

```sh
CC=clang-19 CXX=clang++-19 \
cmake -S . -B /tmp/asc-cpp-m4-verification-asan \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DASC_CPP_BUILD_TESTING=ON \
  -DASC_CPP_WARNINGS_AS_ERRORS=ON \
  -DASC_CPP_ENABLE_ADDRESS_SANITIZER=ON \
  -DASC_CPP_ENABLE_UNDEFINED_SANITIZER=ON \
  -DASCCMake_DIR=/home/yicai/AI4SciComp/asc-cmake/build/install
cmake --build /tmp/asc-cpp-m4-verification-asan --parallel 4 \
  --target asc_sparse_coordinate_test \
           asc_sparse_compressed_conversion_test \
           asc_sparse_evaluate_test \
           asc_sparse_linalg_test \
           asc_sparse_multi_tu \
           asc_sparse_benchmark
ctest --test-dir /tmp/asc-cpp-m4-verification-asan \
  -R '^asc_cpp\.sparse\.' --output-on-failure -j4
```

Result: **6/6 passed**, with no ASan or UBSan diagnostic.

Expected-failure and source-policy checks:

```sh
for f in tests/compile/m4_negative_*.cc; do
  g++ -std=c++20 -Iinclude -Itests/compile -fsyntax-only "$f"
done
clang-format-19 --dry-run --Werror \
  tests/sparse/*.h tests/sparse/*.cc \
  tests/compile/m4_*.h tests/compile/m4_*.cc \
  tests/consumer/sparse/main.cc \
  benchmarks/sparse/sparse_benchmark.cc
git diff --check -- \
  tests/sparse tests/compile/m4_* \
  tests/consumer/sparse/main.cc benchmarks/sparse \
  docs/development/asc-cpp-m4-sparse-cpu/verification-design.md
```

Result: all **11/11** negative sources failed compilation as intended;
format and whitespace checks **passed**.

The lead's stabilized clean GCC Debug/static full suite result is **141/141
passed**, including build-tree, copied/build/install component closure,
installed relocation, Sparse-only consumers, subproject use, paths with
spaces, and registry-unchanged package checks.

## Performance and allocation evidence

Final GCC Debug/static benchmark:

```text
compiler=gcc configuration=debug-like rows=128 columns=256 nnz=512
format=csr operation=structure_preserving_evaluate iterations=64
total_ns=572462989 per_iteration_ns=8944730 checksum=-574.125 allocations=0

compiler=gcc configuration=debug-like rows=128 columns=256 nnz=512
format=csr operation=spmv iterations=256
total_ns=78492069 per_iteration_ns=306610 checksum=977.375 allocations=0
```

The values are informational and have no unstable speed gate. Both measured
operations allocate zero storage. The benchmark holds only CSR-sized buffers;
it constructs no dense matrix or undisclosed conversion workspace.

## Remaining risks and disposition

- Native MSVC and AppleClang compilation remain hosted-CI evidence rather than
  evidence from this Linux workstation.
- External adapters are responsible for truthful shape, space, alias,
  uniqueness, pointer provenance, alignment, and lifetime metadata; the type
  system cannot verify those claims.
- Coordinate writable lookup and reference structure-preserving evaluation
  favor clarity over optimized asymptotic traversal. Timings are recorded,
  allocations are zero, and optimized/provider work remains explicitly
  deferred.
- Standalone LSan/TSan and broader portability matrices belong to the separate
  portability review.
- GPU implementation and providers are outside Milestone 4. GPU evidence is
  exactly **skipped**; there is no configure, compile, runtime, or parity
  claim.

Within the frozen Milestone 4 boundary, independent verification approves the
integrated candidate for Publication Checkpoint B.
