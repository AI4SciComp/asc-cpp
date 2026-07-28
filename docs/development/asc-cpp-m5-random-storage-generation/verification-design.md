# Milestone 5 Independent Verification Design

Status: Frozen before production-source inspection

Date: 2026-07-28

Role: Independent verification agent

## Independence boundary

This design was frozen after reading the approved Milestone 5 contract,
ownership ledger, provenance record, architecture/ADR/manifests, and the
already-public Milestones 1--4 Core, Dense, Sparse, and Random APIs. The
Milestone 5 production headers were not inspected before this file was
written.

No MdeCpp/deleted asc-cpp implementation, test, vector, table, prose, or
mechanical translation was inspected or copied. Expected data below is
derived directly from the frozen contract and existing project-owned public
Philox/Uniform01 behavior.

## Test ownership and boundaries

Verification owns only:

```text
tests/random_dense/**
tests/random_sparse/**
tests/compile/m5_*.cc
tests/compile/m5_*.h
tests/consumer/random_dense/**
tests/consumer/random_sparse/**
tests/consumer/cpp/**
benchmarks/random_storage/**
verification-design.md
verification-review.md
```

The lead owns all CMake registration, package scripts, architecture scans, and
shared integration. Tests will therefore be standalone C++ sources with no
write to an integration file. Production defects and missing integration are
reported rather than worked around in tests.

## Dense independent oracle

For a destination with extents `e[0] ... e[Rank-1]`, logical ordinal `i`
increments dimension zero first:

```text
coordinate[d] = floor(i / product(e[0 ... d-1])) mod e[d]
```

Rank zero has the one empty coordinate and logical size one. If any extent is
zero, logical size is zero.

The oracle computes physical storage offsets only through the already-public
`DenseLayout::Offset` mapping. It never assumes that logical and physical
orders match. For each logical ordinal:

```text
float expected =
    Uniform01<float>(GeneratePhilox4x32Word(stream, subsequence, offset + i))

double expected = Uniform01<double>(
    GeneratePhilox4x32Word(stream, subsequence, offset + 2*i),
    GeneratePhilox4x32Word(stream, subsequence, offset + 2*i + 1))
```

The expected next offset is computed with the already-public checked
`AdvanceRandomOffset`:

```text
float  -> AdvanceRandomOffset(offset, logical_size)
double -> AdvanceRandomOffset(offset, 2 * logical_size)
```

Tests compare values exactly, not statistically. A padded `LayoutStride`
buffer starts with a distinct sentinel in every physical slot; all mapped
logical slots must receive the oracle value and every hole must retain the
sentinel.

Whole/partition equivalence uses dimension-zero slabs. Partition `p` begins
at the checked word offset corresponding to the whole-fill logical ordinal of
the partition's first element. Independent buffers and explicit addresses are
used for thread/partition tests; no shared mutable random state exists.

Dense failure transactions freeze the complete physical span, invoke an
invalid operation, and require bit-for-bit equality afterward. Cases include
offset overflow, non-serial context, non-host placement, and invalid mapping
metadata constructible through the public APIs. Zero-extent success must not
dereference a null destination and returns the unchanged offset.

## Sparse independent oracle

For extents `e[0] ... e[Rank-1]`, the contract's canonical ordinal has the
last dimension varying fastest. Coordinate decoding is:

```text
remaining = ordinal
for d = Rank-1 down to 0:
  coordinate[d] = remaining mod e[d]
  remaining = floor(remaining / e[d])
```

Rank zero has logical size one and the one empty coordinate. Any zero extent
has logical size zero.

For every logical ordinal `i`, the independent priority is:

```text
high = GeneratePhilox4x32Word(
    structure_stream, structure_subsequence, structure_offset + 2*i)
low = GeneratePhilox4x32Word(
    structure_stream, structure_subsequence, structure_offset + 2*i + 1)
priority = (uint64_t(high) << 32) | uint64_t(low)
```

The oracle materializes `(priority, ordinal)` pairs in the test only, sorts by
priority and then ordinal, takes the first `exact_count`, decodes coordinates,
and finally sorts selected coordinates lexicographically. This is
intentionally structurally different from the contract's required repeated
scan reference implementation and independently checks tie handling and final
canonical order.

Values are computed solely from canonical stored position `j`:

```text
float expected =
    Uniform01<float>(GeneratePhilox4x32Word(
        value_stream, value_subsequence, value_offset + j))

double expected = Uniform01<double>(
    GeneratePhilox4x32Word(
        value_stream, value_subsequence, value_offset + 2*j),
    GeneratePhilox4x32Word(
        value_stream, value_subsequence, value_offset + 2*j + 1))
```

For count zero, both next offsets equal the respective input offsets. For
nonzero count:

```text
next_structure_offset =
    AdvanceRandomOffset(structure_offset, 2 * logical_size)
next_value_offset =
    AdvanceRandomOffset(value_offset, count)       // float
next_value_offset =
    AdvanceRandomOffset(value_offset, 2 * count)   // double
```

Changing only the value address must preserve coordinates. Changing only the
structure address must preserve the canonical value sequence for an unchanged
shape/count/value address. Structure and value `(stream, subsequence)` equality
must fail before allocation.

## Allocation and rollback oracle

A test `MemoryResource` records requested byte count/alignment, successful
allocations, failed calls, live pointers, deallocations, and duplicate
deallocation attempts. It can fail a selected allocation call.

Successful nonempty sparse generation is expected to make exactly the
coordinate builder's two declared allocations:

```text
coordinates: exact_count * Rank * sizeof(index_t), alignof(index_t)
values:      exact_count * sizeof(Element), alignof(Element)
```

For rank zero the coordinate allocation request has zero bytes but remains
part of the builder's two-allocation contract. For count zero, the same
builder allocation interface may make two zero-byte allocation calls; tests
record actual calls and compare them to the existing builder contract rather
than invent a new allocator convention.

Failure of the first allocation publishes no result and leaves zero live
allocations. Failure of the second releases the first exactly once and
publishes no result. Destruction of a successful result releases each
successfully allocated buffer exactly once. Metadata, count, address-domain,
and offset failures must make zero allocation calls.

The dense operation is tested with a counting resource backing a previously
created owner and by direct external views. Allocation counters are sampled
before/after the fill and must not change.

## Compile contracts

Standalone compile sources will cover:

- each facet header as the sole ASC project include with exceptions enabled
  and disabled under strict GCC and Clang;
- positive float/double rank-zero and ranked invocability;
- multi-translation-unit instantiation/ODR;
- result and owner move-only traits;
- useful negative sources for a const dense destination, integral/boolean or
  cv-qualified scalars, and copying the sparse generation result;
- inclusion without the matching facet header and linking without the
  matching facet target, registered by the lead as expected failures.

No negative test relies only on diagnostic wording.

## Runtime cases

Dense:

- rank zero and zero extent;
- `LayoutLeft`, `LayoutRight`, and padded unique `LayoutStride`;
- exact logical values, untouched padding, deterministic rerun;
- whole/partition equivalence and independent-thread objects;
- float and double state advancement;
- offset overflow before mutation;
- backend and placement rejection before mutation;
- zero operation allocation.

Sparse:

- rank zero count zero/one;
- zero extent count zero and invalid nonzero count;
- fully static, fully dynamic, and mixed extents;
- count zero, one, partial, and full;
- exact independent priority oracle, uniqueness, and canonical order;
- float and double value sequences and explicit-zero retention;
- equal address-domain rejection;
- deterministic rerun and independent-thread objects;
- structure-only/value-only address changes;
- structure and value offset overflow;
- invalid negative/excess count before allocation;
- first/second allocation failure rollback and exactly-once release;
- exact successful allocation call/byte/alignment evidence.

## Consumers and performance observation

Owned isolated consumer sources use only:

- `find_package(ASCCpp REQUIRED COMPONENTS random_dense)`;
- `find_package(ASCCpp REQUIRED COMPONENTS random_sparse)`;
- no-component `find_package(ASCCpp REQUIRED)` for `ASC::cpp`.

The lead registers build-tree, copied-build-tree, install, relocation,
path-with-spaces, static/shared, and subproject executions and audits forbidden
targets.

The benchmark records compiler, build configuration, scalar, shape/count,
layouts, repetitions, allocation counts, elapsed time, offsets, and stable
checksums. It is an observation with no speed threshold. GPU evidence for
Milestone 5 is exactly `skipped`.

