# Milestone 4 Independent Verification Design

Status: Frozen before production inspection

Date: 2026-07-26

Branch: `feature/asc-cpp-m4-sparse-cpu`

## Independence record

This design was derived from the frozen Milestone 4 contract, ownership
ledger, accepted ADRs 0001, 0004, 0007--0010, 0012, 0014, 0016--0018, the
architecture testing strategy, and the public Milestone 1--3 core,
expression, and dense contracts. It was written before inspecting Milestone 4
production source, production self-review, documentation conclusions,
portability conclusions, MdeCpp sparse source or tests, deleted asc-cpp sparse
source or tests, or an upstream sparse test corpus.

Expected structures and numerical results below are independent calculations
from the frozen semantics. They are not differential results from another
sparse implementation.

## Verification boundary

Writable files are limited to:

```text
tests/sparse/**
tests/compile/m4_*.cc
tests/compile/m4_*.h
tests/consumer/sparse/**
benchmarks/sparse/**
docs/development/asc-cpp-m4-sparse-cpu/verification-design.md
docs/development/asc-cpp-m4-sparse-cpu/verification-review.md
```

The verifier will not edit production, CMake/package integration, shared
architecture tests, manifests, or general documentation. Test registration
and package integration are lead-owned.

## Independently derived structural oracles

### General-rank coordinate oracle

For shape `(2, 3, 2)`, insert in this order:

```text
(1, 0, 1) ->  5
(0, 2, 1) -> -2
(1, 0, 1) ->  7
(0, 0, 0) ->  0
(0, 2, 0) ->  4
```

Stable lexicographic finalization with duplicate summation and explicit-zero
keep must produce:

```text
(0, 0, 0) ->  0
(0, 2, 0) ->  4
(0, 2, 1) -> -2
(1, 0, 1) -> 12
```

The drop policy removes only the first row. A reject-duplicates finalization
must fail without publishing output or changing the builder's logical
entries. A subsequent sum finalization of the same builder must still produce
the oracle above.

Integral duplicate values `INT64_MAX` and `1` must return `kOverflow`.
Floating duplicates `1.0e20`, `-1.0e20`, and `3.0`, inserted in that order,
must sum to `3.0`; this distinguishes stable insertion order from an
unspecified reorder. NaN survives explicit-zero dropping.

Rank zero is the scalar coordinate domain. One empty coordinate is valid and
canonical; duplicate empty coordinates follow the selected duplicate policy.
Any zero extent is an empty domain, so no entry can be added.

### Rectangular COO/CSR/CSC oracle

For shape `(4, 5)`, canonical row-coordinate entries are:

| row | column | value |
| ---: | ---: | ---: |
| 0 | 0 | 2 |
| 0 | 3 | -1 |
| 1 | 1 | 4 |
| 1 | 4 | 0 |
| 2 | 0 | 7 |
| 2 | 2 | 3 |
| 2 | 4 | 5 |
| 3 | 1 | -2 |
| 3 | 4 | 6 |

The exact CSR structure is:

```text
outer offsets: [0, 2, 4, 7, 9]
inner indices: [0, 3, 1, 4, 0, 2, 4, 1, 4]
values:        [2, -1, 4, 0, 7, 3, 5, -2, 6]
```

The exact CSC structure is:

```text
outer offsets: [0, 2, 4, 5, 6, 9]
inner indices: [0, 2, 1, 3, 2, 0, 1, 2, 3]
values:        [2, 7, 4, -2, 3, -1, 0, 5, 6]
```

Every named conversion pair must preserve shape, NNZ, canonical coordinate
order, the explicit zero, and copied value bits. A quiet NaN with a chosen
payload will be checked by `bit_cast` after a round trip rather than by
floating equality.

For an empty compressed object the offsets length is always
`outer_extent + 1` and every offset is zero. This includes `(0, 5)` CSR,
`(4, 0)` CSC, and nonzero shapes with zero NNZ.

## Independently derived SpMV oracle

Dropping the explicit zero from the rectangular matrix and using

```text
x = [1, -2, 3, 4, -1]
```

gives:

```text
A*x = [-2, -8, 11, -2]
```

With `alpha = 2`, `beta = -0.5`, and old output
`[10, 20, 30, 40]`, the exact new output is:

```text
[-9, -26, 7, -24]
```

The same oracle is exercised for `float` and `double`, with exact comparison
because these values are exactly representable. A separate non-integral case
uses a magnitude-aware tolerance.

An instrumented external vector adapter counts reads and writes. With
`beta == 0`, output read count must remain zero even when its read operation
returns poison. With nonzero beta, each logical output is read once. Possible
input/output or matrix-values/output alias must fail before any write.

Empty and degenerate cases include:

- `(0, 3)` times a length-three vector writes no output;
- `(3, 0)` times a length-zero vector with zero NNZ writes three results;
- a nonempty all-zero matrix applies only beta;
- zero-row and zero-column mismatches fail before mutation.

NaN and infinity cases check ordinary IEEE propagation, not normalization or
an invented numerical status. Positive and negative zero are checked only
where the documented operation order determines them.

## Test layers and falsification plan

### Coordinate construction, ownership, and views

- valid static, dynamic, rank-zero, and zero-extent extents;
- negative and overflowing extents rejected before allocation;
- negative capacity, zero capacity, exact capacity, and capacity-plus-one;
- Add validates complete coordinate rank and every negative/out-of-range index
  before mutation;
- no hidden growth or allocation during Add;
- stable duplicate sum, duplicate reject rollback, zero keep/drop, NaN keep;
- checked integral duplicate overflow rollback;
- owner and builder are move-only; moved-from state is rejected;
- finalized order is sorted and unique and structure cannot be mutated;
- values mutate only through mutable-element views;
- mutable-to-const conversion is one-way and descriptors are trivially
  copyable;
- stored-entry access, missing-coordinate lookup, and host-access rejection;
- external view rejects inconsistent counts, null nonempty spans, invalid
  memory-space enumeration, byte/address overflow, and malformed shape;
- custom resources prove partial-construction rollback and exactly-once
  deallocation.

### Compressed storage and conversion

- CSR and CSC owner creation from exact external spans;
- negative shape or NNZ, wrong offset/index/value lengths, nonzero first
  offset, decreasing offsets, final-offset mismatch, out-of-range inner index,
  unsorted segment, duplicate inner index, and integer/address overflow;
- rectangular and empty canonical invariants;
- mutable value/const structure behavior and const propagation;
- move-only owner/resource lifetime and view invalidation obligations;
- all six named conversion directions;
- source immutability and failure-transaction behavior under an allocation
  resource that fails each destination allocation in turn;
- no allocation beyond explicit destination buffers and no undisclosed COO
  route, workspace, format change, dropping, combining, transfer, or
  densification.

### Expression participation and evaluation

- coordinate, CSR, and CSC views satisfy the readable terminal contract with
  structure-preserving sparsity;
- missing logical coordinates read as additive zero;
- mutable sparse destinations satisfy writable and placement protocols, while
  const destinations do not;
- an external non-ASC rank-one vector independently satisfies readable,
  placement, and writable protocols;
- exact-rank, exact-shape, exact-value, serial, host, and canonical-structure
  validation completes before mutation;
- negation of a ranked sparse terminal evaluates over the existing structure;
- direct exact self-assignment is a no-op;
- non-exact possible value overlap is rejected transactionally;
- custom expressions for filtering, union, intersection, value-dependent,
  densifying, and destination-required effects are each rejected before the
  first write;
- rank-zero scalar expansion is rejected, while a true rank-zero
  structure-preserving sparse terminal can use a rank-zero destination;
- a strict counting resource/global allocation scope demonstrates no
  computational allocation, temporary, workspace, conversion, or
  densification on success.

### CSR SpMV

- independently derived float/double oracles above;
- external vector adapter, plus dense-view interoperability only when both
  public headers are explicitly included;
- canonical CSR only, exact rank-one lengths, exact scalar type, serial
  context, and host placement;
- unsupported context and invalid/unknown memory space rejected before access;
- output uniqueness and conservative input/matrix overlap validation;
- `beta == 0` no-read, zero NNZ, empty and degenerate shapes;
- deterministic row/increasing-column accumulation;
- NaN, infinity, signed zero, and large finite inputs;
- zero allocations/wrapping/packing/conversion/densification/transfers/
  synchronization/provider dispatch/fallback.

### Compile and dependency contracts

- each sparse header and modified expression/dense header compiles alone under
  strict C++20 with exceptions enabled and disabled;
- one positive multi-TU instantiation links without ODR defects;
- compile traits cover trivially copyable views and move-only owners/builders;
- negatives cover Boolean/volatile/cv element misuse, non-Extents owner shape,
  mutable-from-const view conversion, owner copying, integral SpMV, wrong-rank
  vector, missing writable output, and malformed adapter signatures;
- a sparse-only consumer uses an external vector adapter and links only
  `ASC::sparse`;
- a second consumer requests sparse and dense independently and demonstrates
  dense-view SpMV without a sparse-to-dense dependency.

### Runtime safety and package expectations

The lead-owned integration must run focused and full tests under GCC and Clang,
static and shared builds, ASan and UBSan, minimum CMake, exceptions enabled
and disabled headers, build-tree/copied-tree/install/relocation/path-with-
spaces/subproject consumers, and installed imported-target audits.

GPU evidence is exactly **skipped**: Milestone 4 has no provider facet, provider
source, provider discovery, or device execution.

## Planned verification files

The exact split may be refined to match public spellings without weakening
coverage:

```text
tests/sparse/test_support.h
tests/sparse/allocation_counter.h
tests/sparse/allocation_counter.cc
tests/sparse/coordinate_test.cc
tests/sparse/compressed_test.cc
tests/sparse/conversion_test.cc
tests/sparse/evaluate_test.cc
tests/sparse/linalg_test.cc
tests/compile/m4_sparse_contract.cc
tests/compile/m4_sparse_multi_tu.h
tests/compile/m4_sparse_multi_tu_a.cc
tests/compile/m4_sparse_multi_tu_b.cc
tests/compile/m4_sparse_multi_tu_main.cc
tests/compile/m4_negative_*.cc
tests/consumer/sparse/CMakeLists.txt
tests/consumer/sparse/main.cc
benchmarks/sparse/allocation_free_benchmark.cc
```

The benchmark will use a deterministic canonical CSR matrix and external
vectors, warm up first, record compiler/configuration, matrix shape, NNZ,
format, operation, iterations, allocation count, elapsed time, and checksum,
and impose no unstable speed threshold.

## Acceptance rule

A failed contract, numerical, sanitizer, dependency, package, or
no-densification check is release-blocking until resolved or explicitly
removed from the frozen scope by the owner. A skipped GPU result is expected
and must not be reworded as passed. Hosted MSVC/AppleClang evidence and
clang-tidy evidence may remain CI-only when the local toolchain is absent, but
that limitation must be reported.
