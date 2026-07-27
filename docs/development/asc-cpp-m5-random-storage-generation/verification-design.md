# Milestone 5 Independent Verification Design

Status: Frozen before production inspection

Date: 2026-07-26

Scope: **Milestone 5 — random storage generation**

## Independence record

This design was derived from the frozen milestone contract, accepted ADRs, the
architecture verification strategy, and the existing public core, random-base,
dense, and sparse contracts. Before freezing this file, the verifier did not
inspect:

- `include/asc/random/dense.h`;
- `include/asc/random/sparse.h`;
- another Milestone 5 agent report;
- MdeCpp random implementation or tests;
- the deleted asc-cpp random implementation or tests; or
- copied external vectors.

Expected facet behavior is calculated from public `Philox4x32Word` and
`Uniform01` calls plus independently written ordinal and priority logic. This
tests the storage adapters' address mapping without duplicating their
implementation source. Existing Milestone 2 engine-vector tests remain the
independent evidence for the raw engine and scalar transforms.

## Frozen invariants and oracles

### Dense logical ordinal

For shape `e[0], ..., e[Rank-1]`, the dense ordinal is:

```text
ordinal = i[0] + e[0] * (i[1] + e[1] * (...))
```

Dimension zero changes fastest. The oracle asks the view mapping for each
physical offset but computes the random address only from the logical ordinal:

```text
float  expected = Uniform01<float>(
    Philox4x32Word(stream, subsequence, offset + ordinal))

double expected = Uniform01<double>(
    Philox4x32Word(stream, subsequence, offset + 2*ordinal),
    Philox4x32Word(stream, subsequence, offset + 2*ordinal + 1))
```

The same expected logical values must appear in `LayoutLeft`, `LayoutRight`,
and a unique padded `LayoutStride`. Padding is initialized to sentinels and
must remain unchanged.

For shape `{2, 3}`, whole-fill equivalence is tested by three `{2, 1}`
subviews at offsets `{0, column}` with starting offsets
`base + 2*column` for `float` and `base + 4*column` for `double`. This is a
partition of consecutive logical ordinals, not of physical addresses.

Rank zero consumes one logical value. Any zero extent consumes no words,
writes no elements or holes, and returns the input offset.

### Sparse canonical ordinal and priority

For shape `e[0], ..., e[Rank-1]`, the sparse candidate ordinal is:

```text
ordinal = (...((i[0] * e[1] + i[1]) * e[2] + i[2])...)
```

The last dimension changes fastest. For each ordinal:

```text
priority =
    uint64(Philox4x32Word(structure_stream, structure_subsequence,
                          structure_offset + 2*ordinal)) << 32
  | uint64(Philox4x32Word(structure_stream, structure_subsequence,
                          structure_offset + 2*ordinal + 1))
```

The oracle sorts `(priority, ordinal)`, selects the first `exact_count`,
converts selected ordinals to coordinates, then lexicographically sorts those
coordinates. The output must have exactly that unique canonical structure.
Tied priorities are resolved by ascending ordinal.

Values are independent of structure and use canonical stored position:

```text
float  expected[p] = Uniform01<float>(
    Philox4x32Word(value_stream, value_subsequence, value_offset + p))

double expected[p] = Uniform01<double>(
    Philox4x32Word(value_stream, value_subsequence, value_offset + 2*p),
    Philox4x32Word(value_stream, value_subsequence, value_offset + 2*p + 1))
```

Changing only the value domain must preserve coordinates. Changing only the
structure domain must preserve this canonical value sequence. The tests never
require changed addresses to produce different bits or coordinates, because
that is not a valid deterministic guarantee.

### Exact next offsets

Dense success returns `offset + logical_size` for `float` and
`offset + 2*logical_size` for `double`.

Sparse count zero returns both input offsets unchanged. Sparse nonzero success
returns `structure_offset + 2*logical_size` and returns
`value_offset + exact_count` for `float` or
`value_offset + 2*exact_count` for `double`.

Every addition and multiplication boundary is exercised. Overflow must return
`ErrorCode::kOverflow` before a write or allocation.

### Transaction and allocation oracle

A test `CountingMemoryResource` records allocation calls, live allocations,
deallocations, bytes, and configurable failure ordinal.

- Dense success performs zero general allocations during the measured
  operation. Dense validation failures leave all destination storage,
  including padding, byte-for-byte unchanged.
- Sparse metadata, count, domain-separation, and offset failures make zero
  resource allocation calls.
- Nonempty successful sparse generation makes only the coordinate builder's
  coordinate and value allocation calls; there is no third workspace
  allocation.
- Failure of either declared builder allocation publishes no owner. Any
  earlier successful allocation is released exactly once, and the live count
  returns to zero.
- Destroying a successful result releases every owned allocation exactly once.

Diagnostic `Status` storage is excluded from the successful operation
allocation claim. A dedicated allocation counter measures successful dense
calls after all test setup is complete.

### Explicit-zero retention oracle

For `float`, the test deterministically scans public Philox words for the first
address whose public `Uniform01<float>` result is exactly zero, records that
address on failure, and generates a one-entry rank-zero sparse result from it.
The entry must remain stored with value `0.0F`. The search is bounded and its
bound is reported; failure to find such an address is a test failure rather
than a skip.

### Concurrency oracle

Independent destinations and explicit address tuples are filled concurrently
without sharing a mutable ASC object. Results and next offsets must equal
serial runs with the same tuples. Sparse generation uses separate independent
resources per worker. No claim is made that concurrent mutation of one view or
resource is supported.

## Test inventory

### Dense facet runtime

`tests/random_dense/random_dense_test.cc` covers:

- rank-zero `float` and `double`;
- zero extent and unchanged sentinels;
- left/right/unique padded-stride logical equivalence;
- physical padding preservation;
- independently calculated word-to-coordinate mapping;
- whole/partition equivalence;
- deterministic rerun;
- exact next offsets;
- near-maximum offset overflow;
- unsupported execution and non-host placement rejection;
- unchanged destination on every validation failure;
- zero successful-operation allocations; and
- independent-object concurrency.

### Sparse facet runtime

`tests/random_sparse/random_sparse_test.cc` covers:

- rank zero with count zero and one;
- zero-extent count zero and invalid nonzero count;
- dynamic and static rank;
- count zero, partial, and full logical-domain generation;
- negative and too-large counts;
- independently derived `(priority, ordinal)` selections;
- canonical lexicographic order and uniqueness;
- exact `float` and `double` value sequences;
- structure/value `(stream, subsequence)` separation;
- deterministic rerun and exact next offsets;
- structure invariance under value-domain change;
- value-sequence invariance under structure-domain change;
- explicit-zero retention;
- structure and value offset overflow;
- serial/host rejection before allocation;
- first/second allocation failure rollback and exactly-once release;
- exact successful builder allocation count with no workspace; and
- independent-object concurrency.

### Compile contracts

The `tests/compile/m5_*` sources cover:

- each facet header as the only project include;
- exceptions-enabled and exceptions-disabled strict compilation;
- positive operation/result type contracts;
- move-only sparse result ownership;
- representative multi-TU template instantiation;
- negative const dense destination;
- negative integral/cv-qualified scalar participation;
- negative sparse result copying; and
- negative use when only the base random umbrella is included.

### Isolated consumers

- `tests/consumer/random_dense`: includes the dense facet and exercises one
  fill while requiring no sparse target/header.
- `tests/consumer/random_sparse`: includes the sparse facet and exercises one
  generation while requiring no dense target/header.
- `tests/consumer/cpp`: consumes the aggregate and exercises both facets.

The lead-owned package harness must verify build tree, copied build tree,
install, relocation, a path containing spaces, static/shared, subproject,
component closures, no-component lookup, and missing facet target behavior.

### Benchmark

`benchmarks/random_storage/random_storage_benchmark.cc` is a smoke harness,
not a speed gate. It records:

- compiler and configuration;
- serial-reference backend;
- dense shape/layout/repetitions, operation allocation count, elapsed time,
  next offset, and checksum;
- sparse shape/count/repetitions, resource allocation count, elapsed time,
  next offsets, and coordinate/value checksum.

It warms up, prevents dead-code elimination with printed checksums, and makes
no performance guarantee.

## Dependency and provider falsification

Source scans must prove:

```text
random base     -> core only
random_dense    -> random + dense only
random_sparse   -> random + sparse only
dense/sparse    -> no random include or link
```

Facet public headers must contain no provider SDK header or type. CPU-only
configuration must not discover a provider. Milestone 5 adds no provider
target, option, source, transfer, synchronization, fallback, or execution
dispatch.

GPU evidence is exactly **skipped**. Toolkit and hardware inventory are not
Milestone 5 configure, compile, runtime, or parity evidence.

## Pass gate

The verification role accepts the implementation only when:

- all runtime and compile-contract cases above pass under strict GCC and Clang;
- applicable CPU runtime tests pass under ASan and UBSan;
- isolated consumers and lead-owned package matrices pass;
- allocation and rollback counters match the frozen oracle;
- no forbidden dependency or provider leak is found; and
- every defect is either fixed and regression-tested or recorded as an
  unresolved release blocker.
