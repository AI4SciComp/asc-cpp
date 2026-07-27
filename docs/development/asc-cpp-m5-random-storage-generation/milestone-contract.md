# Milestone 5 Contract: Random Storage Generation

Status: Frozen after owner approval

Date: 2026-07-26

Owner approval: Milestone 5 with no corrections

Exact milestone: **Milestone 5 — random storage generation**

Branch: `feature/asc-cpp-m5-random-storage-generation`

Baseline commit and unchanged cumulative `HEAD`:
`33b261ea33616a6395c4ad3b20646093103344f7`

Predecessor state: the intentionally uncommitted Milestones 0--4 publication
candidate and the owner's earlier deletion of the retired implementation are
retained in place. Milestone 5 does not restore, reset, discard, commit, or
reclassify that work.

## Purpose

Implement the two approved random-owned CPU storage-generation facets:

```text
ASC::random_dense  -> ASC::random;ASC::dense
ASC::random_sparse -> ASC::random;ASC::sparse
```

The facets use the existing explicit Philox4x32-10 address vocabulary and
exact `Uniform01<float>`/`Uniform01<double>` transforms. They add no new
engine, distribution, entropy source, mutable cursor, global state, provider,
third-party dependency, or storage-module dependency on random.

Because all six provider-free modules and both approved random facets become
implemented in this milestone, the approved convenience aggregate also
becomes real:

```text
ASC::cpp -> ASC::core;ASC::utilities;ASC::expression;ASC::dense;
            ASC::sparse;ASC::random;ASC::random_dense;ASC::random_sparse
```

The aggregate owns no production behavior and is never a dependency of a
narrower target.

## Product targets

```text
build target:    asc_random_dense
exported target: ASC::random_dense
kind:            functional C++20 interface facet
direct links:    ASC::random;ASC::dense

build target:    asc_random_sparse
exported target: ASC::random_sparse
kind:            functional C++20 interface facet
direct links:    ASC::random;ASC::sparse

build target:    asc_cpp
exported target: ASC::cpp
kind:            provider-free convenience interface aggregate
direct links:    all six modules and both random facets, exactly as above

release line:    unreleased 0.5.0 candidate
```

The random facets are legitimate interface libraries because generation is
type/rank-dependent template behavior in their installed headers. They are not
empty placeholder targets. No compiled provider target is added.

## Public production files

Random owns exactly two new public facet headers:

```text
include/asc/random/dense.h
include/asc/random/sparse.h
```

The base umbrella `include/asc/random.h` remains storage-neutral and must not
include either facet header. A consumer explicitly includes the facet header
and links the matching facet target.

No dense or sparse production file changes. The facets adapt the existing
public storage contracts without acquiring ownership of storage. No new
common fill header, engine behavior, distribution, source file, or provider
header is approved.

Every public header is a self-contained `.h` file with a full-path guard and
direct includes. All public names are in flat `namespace asc`; internal
namespace names contain `internal` and do not leak into public signatures.

## Supported scalar and execution contract

Both facets support exactly unqualified `float` and `double`, matching the
existing exact `Uniform01` transforms. Boolean, integral, cv-qualified,
user-defined, normal, rejection, and variable-consumption distributions are
outside Milestone 5.

Every operation takes:

- an explicit `ExecutionContext`;
- explicit `RandomStream`, `RandomSubsequence`, and `RandomOffset` values;
- caller-provided storage or an explicit destination resource; and
- no default state.

Milestone 5 accepts only serial execution and host memory. Unsupported backend,
memory-space, count, shape, or offset-domain input fails before destination
mutation or output publication. Successful operations perform no transfer,
synchronization, provider selection, or fallback.

The returned next offsets are checked. An input offset is never modified, and
overflow returns `ErrorCode::kOverflow` before generation.

## Dense generation

The exact public operation family is:

```text
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>
```

The destination is a mutable `DenseView<float, Rank>` or
`DenseView<double, Rank>`. The result is the first unused word offset in the
same stream/subsequence.

Word consumption is exact:

```text
float:  one Philox word per logical element
double: two consecutive Philox words per logical element, high then low
```

Traversal is logical dimension-zero-fastest order, independent of
`LayoutLeft`, `LayoutRight`, and unique non-negative `LayoutStride` physical
storage. Rank zero consumes one value. Any zero extent consumes no words,
writes nothing, and returns the input offset.

The word address of logical ordinal `i` is:

```text
float:  offset + i
double: offset + 2*i, offset + 2*i + 1
```

Equivalent logical partitions reproduce a whole fill when each partition is
given the corresponding checked starting offset. The operation has no hidden
partition, thread, or physical-layout state.

All context, placement, logical-size, word-count, and offset-overflow checks
complete before the first write. The successful operation allocates no
storage or workspace. It does not pack, materialize, traverse physical holes,
or mutate padding.

## Sparse generation

The exact public result and operation family are:

```text
SparseUniform01Generation<Element, ExtentsType>
  array
  next_structure_offset
  next_value_offset

GenerateSparseUniform01<Element>(
    context, extents, exact_count, resource,
    structure_stream, structure_subsequence, structure_offset,
    value_stream, value_subsequence, value_offset)
    -> Result<SparseUniform01Generation<Element, ExtentsType>>
```

The result owns one canonical `CoordinateArray<Element, ExtentsType>`.
`Element` is exactly unqualified `float` or `double`.

The structure and value address domains must use distinct
`(stream, subsequence)` pairs. Equal pairs are rejected even when offsets do
not overlap. This makes the independence requirement explicit and prevents an
accidental shared random domain.

`exact_count` is a non-negative `nnz_t` no greater than the logical shape
size. A zero extent therefore permits only count zero. Rank zero has one
logical coordinate and permits count zero or one.

### Structure algorithm

Milestone 5 freezes a deterministic project-owned serial reference algorithm,
not a statistical-uniformity claim:

1. Map each logical coordinate to its canonical lexicographic ordinal, with
   the last dimension varying fastest.
2. For every logical ordinal, read two structure words and form one unsigned
   64-bit priority in high-word/low-word order.
3. Order candidates by `(priority, ordinal)`.
4. Select the `exact_count` smallest pairs without replacement.
5. Insert their coordinates into a sparse builder and finalize with
   `DuplicatePolicy::kReject` and `ExplicitZeroPolicy::kKeep`.

The ordinal tie-break is part of the sequence contract. The output is sorted
in the sparse module's canonical coordinate order and is unique. The contract
claims a deterministic pseudorandom exact-count structure, not mathematically
perfect uniform sampling over all subsets.

For a nonzero count, the structure domain consumes exactly two words for every
logical coordinate:

```text
next_structure_offset = structure_offset + 2*logical_size
```

For count zero, no structure word is read and the structure offset is
unchanged.

The serial reference selection uses repeated scans and no hidden workspace:

```text
time:  O(exact_count * logical_size)
extra computational storage: O(rank)
```

The coordinate builder's declared output allocation and canonicalization cost
remain visible. An optimized sampling algorithm is deferred.

### Value algorithm

After canonical structure finalization, values are generated in canonical
stored-entry order from only the value address domain:

```text
float:  value_offset + stored_position
double: value_offset + 2*stored_position, then the next word
```

Changing only the value address leaves structure unchanged. Changing only the
structure address leaves the canonical stored value sequence unchanged for the
same shape/count/value address, although those values may be attached to
different coordinates.

Explicit zero is retained. No duplicate combination, density/Bernoulli mode,
rejection sampling, hidden COO conversion, compressed output, densification,
transfer, synchronization, provider dispatch, or fallback occurs.

### Sparse ownership and failure

The caller supplies an explicit host `MemoryResource` that outlives the
returned owner. Successful generation makes exactly the coordinate builder's
declared coordinate/value allocations and no workspace allocation. Metadata,
count, address-domain, and offset validation complete before allocation.
Allocation failure publishes no owner and releases partial storage exactly
once.

## Packaging and dependency contract

The cumulative available component set becomes:

```text
core utilities expression random dense sparse
random_dense random_sparse cpp
```

Required component closures are exact:

```text
random_dense  -> core;expression;random;dense;random_dense
random_sparse -> core;expression;random;sparse;random_sparse
cpp           -> core;utilities;expression;random;dense;sparse;
                 random_dense;random_sparse;cpp
```

The closure ordering may be topologically refined in package implementation,
but no sibling facet may enter the other facet's closure. In particular:

- a base `random` consumer imports no storage target;
- a `random_dense` consumer imports no sparse or random-sparse target;
- a `random_sparse` consumer imports no dense or random-dense target;
- neither storage module imports random;
- no provider or SDK is discovered for any provider-free component.

No-component `find_package(ASCCpp)` now succeeds by requesting `cpp`.

Minimum CMake remains 3.25. Root integration consumes exact ASCCMake 0.1.0 and
uses only the verified functions:

```text
asc_target_enable_cxx20
asc_target_enable_warnings
asc_target_enable_sanitizers
asc_register_test
```

Standard CMake owns conditional component export/install logic.

## Required evidence

- both new headers alone under strict GCC and Clang, with exceptions enabled
  and disabled;
- positive concepts/API contracts, multi-TU ODR, and useful negative compile
  cases for const destinations, unsupported scalars, invalid result copying,
  and missing facet headers/targets;
- dense rank-zero, zero-extent, left/right/strided layout, physical padding,
  whole/partition equivalence, deterministic rerun, state advancement,
  offset overflow, backend/placement rejection, unchanged failure, and zero
  operation allocations;
- sparse rank-zero, zero-extent, dynamic/static rank, count zero/full/invalid,
  uniqueness, canonical order, independently derived priority oracles,
  structure/value stream separation, reproducibility, changed-stream
  independence, explicit-zero retention, offset overflow, allocation failure
  rollback, exactly-once release, and declared allocation counts;
- deterministic thread/partition tests using independent objects and explicit
  addresses; no shared mutable state;
- source and target graph audits proving exact facet edges and unchanged base
  module ceilings;
- build-tree, copied-build-tree, install, relocation, path-with-spaces,
  static/shared, subproject, random-base-only, dense-facet-only,
  sparse-facet-only, aggregate, and no-component consumers;
- successful allocation/state-advance benchmark with compiler, configuration,
  shape/count, layouts, repetitions, allocation counts, elapsed time, and
  checksums, but no unstable speed gate;
- ASan and UBSan on applicable CPU runtime tests.

GPU evidence for Milestone 5 is exactly **skipped**.

## Explicit exclusions

- No production change to core, utilities, expression, dense, sparse, or the
  random base engine/distribution/umbrella.
- No new random engine, mutable generator, entropy, seed expansion, normal or
  rejection distribution, serialization, Sobol, permutation, shuffle, Latin,
  spherical, density/Bernoulli sparse mode, or compressed sparse generation.
- No OpenMP, TBB, Eigen, BLAS/LAPACK, oneMKL, CUDA, cuRAND, cuBLAS, cuSOLVER,
  cuSPARSE, HIP, SYCL, or other provider integration.
- No GPU target, option, discovery, source, compilation, runtime, or parity
  operation.
- No third-party dependency or copied MdeCpp/deleted asc-cpp source, test,
  vector, table, or prose.
- No compatibility layer for deleted array/linalg/random fill APIs.
- No Milestone 6 memory/device/dense CUDA work.
- No commit, push, pull request, merge, tag, release, or branch deletion.

## Publication gate

The lead reviews the complete cumulative diff, reconciles every specialist
finding, runs fresh local compiler/sanitizer/package/consumer matrices,
records exact pass/fail/skip evidence, and stops at Publication Checkpoint B.
Remote writes and history mutation require separate owner approval.
