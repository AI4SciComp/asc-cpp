# Milestone 5 Production Self-Review

Status: production implementation complete; independent verification pending

Date: 2026-07-28

Role: production implementation agent

Owned production files:

```text
include/asc/random/dense.h
include/asc/random/sparse.h
```

This review covers only the frozen Milestone 5 CPU random-storage facets. It
does not approve integration, packaging, documentation, tests, providers, or a
later milestone.

## Contract conformance

### Public API

`include/asc/random/dense.h` defines only the approved operation family:

```cpp
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>
```

The function participates in overload resolution only for unqualified
`float` and `double` destinations. A const-element view, integral element,
Boolean element, cv-qualified element, or user-defined element is rejected at
compile time.

`include/asc/random/sparse.h` defines only:

```cpp
SparseUniform01Generation<Element, ExtentsType>
GenerateSparseUniform01<Element>(...)
```

The result contains the approved `array`, `next_structure_offset`, and
`next_value_offset` members. Ownership remains move-only because
`CoordinateArray` is move-only. The operation and result support exactly
unqualified `float` and `double`.

Both headers are self-contained C++20 `.h` files with full-path guards. Public
declarations are in flat `namespace asc`; implementation helpers are in
`internal_random_dense` and `internal_random_sparse`. Neither header changes
or enters the storage-owned production files or the storage-neutral
`include/asc/random.h` umbrella.

### Dense sequence and traversal

Dense generation:

- rejects non-serial execution and non-host placement before mutation;
- converts the logical size and checks the complete word count and returned
  offset before the first write;
- consumes one word for `float` and two consecutive high/low words for
  `double`;
- decodes each logical ordinal with dimension zero varying fastest;
- maps the resulting logical coordinate through the destination strides;
- writes only mapped elements, not physical holes or padding;
- performs no allocation, transfer, synchronization, packing, provider
  selection, fallback, or mutable-state update; and
- leaves a zero-extent destination untouched and returns the input offset.

Rank zero naturally executes one ordinal at physical offset zero. The physical
offset arithmetic relies only on non-negative unique mappings already
validated by `DenseView` and `DenseLayout`; the layout contract has already
proved the maximum offset and span representable before a view can reach this
operation.

### Sparse structure and values

Sparse generation validates, before allocation:

- serial execution and host accessibility;
- host resource placement;
- non-negative exact count no greater than logical size;
- distinct structure and value `(stream, subsequence)` domains;
- structure word-count and returned-offset arithmetic; and
- value word-count and returned-offset arithmetic.

For each selected entry, the structure implementation repeatedly scans all
logical ordinals. It forms the exact high-word/low-word 64-bit priority and
selects the smallest pair strictly following the previous selected
`(priority, ordinal)` pair. The ordinal tie-break therefore remains frozen,
selection is without replacement, and no selection workspace is allocated.
Each selected ordinal is decoded with the last dimension varying fastest,
which is the sparse canonical lexicographic ordinal mapping.

The existing coordinate builder receives the selected coordinates with
placeholder zero values. Finalization uses exactly
`DuplicatePolicy::kReject` and `ExplicitZeroPolicy::kKeep`, producing unique
canonical coordinates. Only after canonical finalization are values generated
in stored-entry order. `float` consumes one value word per position and
`double` consumes two high/low words. Explicit zero remains stored.

Count zero reads no structure or value word and leaves both offsets unchanged.
Rank zero has one logical ordinal and admits count zero or one through the
existing `Extents<>` and coordinate-builder contracts.

The repeated reference selection has
`O(exact_count * logical_size)` time and `O(rank)` local computational storage.
The only dynamic allocations are the coordinate builder's declared coordinate
and value output buffers. If the value-buffer allocation fails after the
coordinate-buffer allocation, the existing move-only `Buffer` rollback
releases the coordinate buffer exactly once. No result owner is published on
failure.

## Dependency and provenance review

The dense facet directly includes only core, dense, and random public headers.
The sparse facet directly includes only core, sparse, and random public
headers. There is no dense/sparse cross-contamination and no base-random
storage dependency.

The implementation adds:

- no third-party include or dependency;
- no provider or SDK type;
- no entropy source, mutable cursor, global state, or thread-local state;
- no standard-library random distribution;
- no source or generated data table; and
- no MdeCpp/deleted asc-cpp code, test, vector, prose, or mechanical
  translation.

The algorithms were implemented from the frozen project-owned contract and the
current public Philox, `Uniform01`, dense-view, and sparse-builder APIs.

## Focused validation

The following checks passed in the working tree.

```text
g++ 11.4.0
  -std=c++20 -pedantic-errors -Wall -Wextra
  -Wconversion -Wsign-conversion -Werror
  -Iinclude -x c++ -fsyntax-only -
PASS: both headers instantiated for float/double dense and sparse APIs

g++ 11.4.0
  same flags plus -fno-exceptions -fno-rtti
PASS: both facets instantiated with exceptions and RTTI disabled

/opt/intel/oneapi/compiler/2024.2/bin/icpx
  -std=c++20 -pedantic-errors -Wall -Wextra
  -Wconversion -Wsign-conversion -Werror
  -Iinclude -x c++ -fsyntax-only -
PASS: both facets instantiated for the complementary float/double cases

/opt/intel/oneapi/compiler/2024.2/bin/icpx
  same flags plus -fno-exceptions -fno-rtti
PASS: both facets instantiated with exceptions and RTTI disabled

g++ 11.4.0, direct linked focused executable
PASS: LayoutRight dense dimension-zero-fastest mapping, exact float words,
      dense next offset, sparse canonical values, sparse structure offset,
      sparse value offset, and exact count

g++ 11.4.0, direct linked executable with
  -fsanitize=address,undefined -fno-omit-frame-pointer
  ASAN_OPTIONS=detect_leaks=1
PASS: padded LayoutStride dense fill preserved its hole; full-count sparse
      generation produced the expected next offsets and no sanitizer finding

git diff --no-index --check /dev/null include/asc/random/dense.h
git diff --no-index --check /dev/null include/asc/random/sparse.h
PASS: expected status 1 for new-file diffs, with no whitespace diagnostics
```

Separate concept checks passed for positive `float`/`double`, negative
`int`/`const float`, and non-copyability of
`SparseUniform01Generation<float, Extents<2, 3>>`.

The ordinary `clang-format` executable is not present on this environment's
`PATH`; formatting was therefore checked manually against the repository's
Google-derived 80-column policy and remains subject to the lead's clean-tree
format validation.

## Self-review findings

No architecture or public-API defect requiring an owner decision was found.
No out-of-scope file change is requested.

Residual items intentionally left to the assigned independent roles and lead
integrator are:

- independently derived dense and sparse sequence/priority oracles;
- failure-resource allocation and exactly-once rollback instrumentation;
- negative compile, multi-translation-unit, and isolated-consumer coverage;
- aggregate/package/relocation and exact target-edge validation;
- full GCC/Clang, static/shared, sanitizer, and clean-build matrices; and
- benchmark methodology and the required GPU classification of `skipped`.
