# Milestone 5 Production Self-Review

Status: Production implementation complete; independent reviews accepted

Date: 2026-07-26

Branch: `feature/asc-cpp-m5-random-storage-generation`

## Scope and provenance

The production implementation is confined to the frozen ownership scope:

```text
include/asc/random/dense.h
include/asc/random/sparse.h
```

This report is the only production-agent documentation change. The production
agent did not edit CMake, tests, package files, general module documentation,
the random base, another module, architecture manifests, or provider paths.

The implementation was independently written from the frozen Milestone 5
contract, accepted ADRs, and current public core, random, dense, and sparse
contracts. No MdeCpp or deleted asc-cpp source, test, vector, table, or prose
was inspected or copied. No third-party source or dependency was introduced.

## Public API and dependency boundaries

The dense facet adds:

```text
FillDenseUniform01(
    context, mutable_dense_view, stream, subsequence, offset)
    -> Result<RandomOffset>
```

The sparse facet adds:

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

Both operation templates accept exactly unqualified `float` or `double`.
Neither header changes or is included by the storage modules or the
storage-neutral random base umbrella.

`dense.h` directly includes only standard library, core, random, and dense
public headers. `sparse.h` directly includes only standard library, core,
random, and sparse public headers. There is no dense/sparse cross-edge,
storage-to-random edge, utility edge, provider header, or third-party include.
All public declarations remain in flat `namespace asc`; implementation helpers
are confined to names containing `internal`.

## Dense generation

`FillDenseUniform01` requires an explicit serial context and mutable host
`DenseView`. It validates execution, placement, logical-size conversion, word
count, and returned-offset arithmetic before the first destination write.
Rejected calls leave the destination unchanged.

Logical ordinals are decoded with dimension zero varying fastest. The decoded
coordinate is mapped through the view's validated strides, so `LayoutLeft`,
`LayoutRight`, and unique non-negative `LayoutStride` destinations receive the
same logical sequence. Physical holes and padding are never visited.

Word addressing is:

```text
float ordinal i:  offset + i
double ordinal i: offset + 2*i, then offset + 2*i + 1
```

Rank zero has logical size one and writes one scalar value. Any zero extent has
logical size zero, reads and writes no word, and returns the input offset.
Success allocates no storage, workspace, temporary, packing buffer, or
operation state.

## Sparse generation

`GenerateSparseUniform01` validates before output allocation:

- serial execution and a host destination resource;
- non-negative `exact_count` no greater than the extents' checked logical size;
- distinct structure and value `(stream, subsequence)` pairs;
- structure and value word counts; and
- both returned offsets.

Count zero consumes no structure or value word and preserves both offsets.
For nonzero count, structure consumes exactly two words for every logical
coordinate, independently of the selected count.

The project-owned reference selector repeatedly scans all canonical ordinals.
An ordinal maps to coordinates with the last dimension varying fastest. Its
priority combines the first structure word as the high 32 bits and the second
as the low 32 bits. Repeated threshold scans select the exact number of
strictly increasing `(priority, ordinal)` pairs without replacement and
without workspace.

Selected coordinates enter an exact-capacity `CoordinateBuilder` with
placeholder zeros. Finalization explicitly uses `DuplicatePolicy::kReject`
and `ExplicitZeroPolicy::kKeep`, producing canonical lexicographically sorted,
unique structure. Values are then written in finalized stored-entry order
using:

```text
float position p:  value_offset + p
double position p: value_offset + 2*p, then value_offset + 2*p + 1
```

The value domain therefore cannot affect structure. For equal shape, count,
and value address, changing only the structure address changes coordinate
attachment but not the canonical stored value sequence. Explicit floating
zero remains stored.

Rank zero has one logical coordinate and permits count zero or one. A
zero-extent domain permits only count zero. Static and dynamic extents use the
same template path.

## Ownership, failures, and concurrency

Sparse success returns one move-only `CoordinateArray`; copying the result is
ill-formed through the existing move-only owner contract. The caller-supplied
host `MemoryResource` must outlive the returned array and all views. Output
allocation consists only of the coordinate builder's coordinate and value
buffers. Selection uses `O(ExtentsType::kRank)` automatic storage and performs
no auxiliary allocation.

If either builder allocation fails, completed buffers roll back through
existing move-only `Buffer` ownership and no result owner is published. Any
subsequent internal status also destroys the local builder or array before
return. No input random offset is mutable, and a failure publishes no advanced
offset.

Operations are synchronous. Every random word is a pure function of explicit
stream, subsequence, and offset addresses. There is no global, thread-local,
static mutable, cached, or caller-hidden state. Separate calls may run
concurrently when their destination objects and resources permit it. The same
mutable dense destination or shared non-thread-safe resource requires caller
serialization.

## Overflow and undefined-behavior audit

- Dense and sparse signed logical sizes and counts are checked before unsigned
  conversion.
- Word-count multiplication uses `CheckedMultiply`.
- Returned offsets use `AdvanceRandomOffset` before any mutation or allocation.
- Every later word address is within the previously checked half-open address
  range.
- Dense coordinate-to-physical arithmetic is bounded by the already validated
  unique mapping and its checked required span.
- Sparse ordinal decoding divides only when a nonzero count proves every shape
  extent is nonzero.
- The priority combination uses unsigned fixed-width shifts and bitwise
  operations.
- No pointer is dereferenced before the explicit host-placement checks.
- No exceptions, signed overflow assumptions, invalid narrowing, unspecified
  iteration order, or data race is part of the implementation contract.

## Complexity and observable costs

| Operation | Time | Successful computational storage |
| --- | --- | --- |
| dense fill | `O(logical_size * Rank)` | none |
| sparse metadata validation | `O(1)` after validated extents | none |
| sparse priority selection | `O(exact_count * logical_size)` | `O(Rank)` automatic storage |
| sparse builder finalization | existing `O(exact_count^2 * Rank)` reference cost | reuses output buffers |
| sparse value generation | `O(exact_count)` | none |

There is no provider dispatch, fallback, transfer, synchronization, packing,
format conversion, densification, or hidden workspace. Diagnostic `Status`
message storage on rejected calls is not described as an allocation-free error
path.

## Focused production checks

The following checks passed before lead integration:

```text
clang-format-19 on both assigned headers

GCC 11.4:
  strict header-alone parse for both headers
  strict instantiated templates with -fno-exceptions
  dense strided-padding and sparse canonical runtime smoke

Clang 19:
  strict header-alone parse for both headers
  strict rank-zero instantiated templates with -fno-exceptions
  dense and sparse runtime suites under ASan+UBSan
```

Strict flags were:

```text
-std=c++20 -pedantic-errors -Wall -Wextra -Wconversion
-Wsign-conversion -Werror
```

The linked GCC runtime smoke used the current core and random sources. It
verified exact dense values and next offset on a padded stride mapping,
unchanged padding, sparse next offsets, canonical unique output order, and
exact double values in canonical stored-entry order.

The independent verifier's current dense and sparse runtime sources also
compiled and passed directly with strict GCC 11 flags. The same two runtime
suites passed a direct Clang 19 ASan+UBSan build with no finding. The
verifier-owned positive API contract and multi-TU executable compiled, linked,
and ran after the ODR correction.

After lead CMake integration, a fresh GCC 11 Debug, static, install-enabled
configuration in `/tmp/asc-cpp-m5-production-cmake.44Midd` built both runtime
tests, the positive API contract, multi-TU executable, and benchmark. Its
focused `asc_cpp.random_dense`, `asc_cpp.random_sparse`, and
`asc_cpp.random_storage` selection passed 11/11, including all six negative
compile cases. This focused matrix does not replace the lead's final clean
whole-project validation.

These are focused implementer checks, not substitutes for the independent
verification, sanitizer, package, relocation, isolated-consumer, or clean lead
matrices.

## Review finding resolved

- **M5-DOC-01:** The initial sparse facet defined the ordinary non-template
  `internal_random_sparse::StructurePriority` function in its public header
  without `inline`. It is now explicitly `inline`, preventing a multi-TU ODR
  violation while retaining its non-`constexpr` engine calls. Header-alone,
  instantiated, runtime, and verifier multi-TU checks cover the corrected
  definition.

The completed independent documentation/API and
portability/GPU/performance reviews found no other production or public-API
defect. The latter independently passed GCC/Clang static/shared, package,
consumer, no-exception, sanitizer, allocation, concurrency, and performance
smoke evidence within the locally available toolchains. Native MSVC and
AppleClang remain untested risks. GPU evidence is exactly `skipped`.

## Deferred and skipped scope

GPU evidence is exactly **skipped**. The two headers contain no CUDA, HIP,
SYCL, GPU provider, device compilation, runtime path, or CPU/GPU identity
claim.

Mutable cursors, entropy, new engines or distributions, normal/rejection
sampling, serialization, density/Bernoulli sparse generation, compressed
output, optimized sampling, parallel execution, and every Milestone 6 or 7
capability remain deferred.
