# Milestone 7 Independent Verification Design

Status: Frozen before production inspection

Date: 2026-07-27

Scope: Milestone 7 — sparse CUDA and random CUDA facets

## Independence boundary

This design was written after reading only the frozen Milestone 7 contract,
ownership ledger, approved ADRs 0008, 0012, 0014, 0015, 0017, and 0018, and
the already-approved provider-free random sequence contract. No Milestone 7
production header or source was inspected before this design was frozen. No
MdeCpp or deleted asc-cpp source, test, literal vector, or table is an oracle.

The verifier will inspect the public Milestone 7 API only after this document
exists. API spelling may change test call sites, but must not weaken these
oracles. Production implementation details, helper functions, launch geometry,
and provider output are never used to compute expected results.

## Planned verification artifacts

The verifier-owned sources are:

```text
tests/sparse_cuda/sparse_cuda_test.cc
tests/random_cuda/philox_oracle.h
tests/random_cuda/random_cuda_test.cc
tests/random_dense_cuda/random_dense_cuda_test.cc
tests/random_sparse_cuda/random_sparse_cuda_test.cc
tests/compile/m7_sparse_cuda_header.cc
tests/compile/m7_random_cuda_header.cc
tests/compile/m7_random_dense_cuda_header.cc
tests/compile/m7_random_sparse_cuda_header.cc
tests/compile/m7_cuda_contracts.cc
tests/compile/m7_cuda_negative_copy.cc
tests/consumer/sparse_cuda/main.cc
tests/consumer/random_cuda/main.cc
tests/consumer/random_dense_cuda/main.cc
tests/consumer/random_sparse_cuda/main.cc
benchmarks/sparse_cuda/sparse_cuda_benchmark.cc
benchmarks/random_cuda/random_cuda_benchmark.cc
```

All CMake creation and registration remains lead-owned. Tests must be guarded
by `ASC_CPP_ENABLE_CUDA`; runtime tests and benchmarks skip cleanly when no
usable CUDA device is present.

## Independent Philox oracle

The verifier implements a small test-only Philox4x32-10 oracle directly from
the approved round equations:

```text
p0 = uint64(0xD2511F53) * c0
p1 = uint64(0xCD9E8D57) * c2

next = [
  high32(p1) xor c1 xor k0,
  low32(p1),
  high32(p0) xor c3 xor k1,
  low32(p0)
]
```

Round zero uses the supplied key. Before each of rounds one through nine the
key is incremented modulo 2^32 by `(0x9E3779B9, 0xBB67AE85)`. All arithmetic
uses explicit unsigned fixed-width values.

Address decoding is independently restated:

```text
key       = [low32(stream), high32(stream)]
counter   = [low32(offset / 4), high32(offset / 4),
             low32(subsequence), high32(subsequence)]
word lane = offset % 4
```

The oracle calculates words rather than importing provider-free expected
literals. Tests compare device output independently to this oracle and also to
the public provider-free `Philox4x32Word`; agreement with both is required.
Inputs include zero, all-one and mixed stream/subsequence values, offsets on
both sides of block boundaries, a high block bit, and a non-four-multiple
count.

The test-only Uniform01 oracle uses integer bit selection and exactly
representable powers of two:

```text
float  = (word >> 8) * 2^-24
double = (((uint64(high) << 32 | low) >> 11)) * 2^-53
```

Bit equality is checked with `std::bit_cast` rather than a tolerance.

## Raw random CUDA oracles

For a caller-owned device word buffer:

1. every logical output word equals the independent Philox oracle at
   `offset + i`;
2. the first unused offset equals `offset + count`;
3. count zero returns the input offset, enqueues no observable work, and does
   not dereference or modify storage;
4. counts `1`, `3`, `4`, `5`, and a multi-block non-launch-multiple size catch
   lane, block, tail, and partition errors;
5. two adjacent calls with explicit adjacent offsets concatenate to exactly
   the result of one whole call;
6. repeated calls and independent concurrent contexts produce identical bits;
7. distinct explicit stream or subsequence addresses are evaluated rather
   than merely assumed different.

Overflow at `offset + count`, misalignment, a byte span too small for count,
wrong memory space, device mismatch, and invalid address span must fail before
mutation. A sentinel-filled destination is copied back after failures to prove
transactionality when the destination itself is valid. Failure status must
carry an ASC code, CUDA provider name where provider-specific, and a signed
native code.

## Dense random CUDA oracles

For rank-zero through rank-eight destinations, the independent logical
ordinal is dimension-zero-fastest:

```text
ordinal = i[0] + e[0] * (i[1] + e[1] * (...))
```

The expected `float` consumes oracle word `offset + ordinal`; `double`
consumes `offset + 2*ordinal` as high word and the next word as low word.
The returned first unused offset is checked exactly.

The matrix covers:

- rank zero;
- a zero extent in a nonzero rank;
- `LayoutLeft` and `LayoutRight`;
- a unique padded nonnegative stride mapping whose holes contain sentinels;
- dynamic and static extents;
- shapes that cross likely launch boundaries;
- whole-fill versus logically consecutive partition fills; and
- reruns and two independent concurrent contexts.

Every logical value must be bit-identical across layouts and partitions.
Padding and holes must retain their original bytes. Zero extent consumes no
words and mutates no byte. Offset overflow, aliased/non-unique layout, negative
or invalid stride, wrong placement, invalid span, and context/device mismatch
must fail before mutation or usable completion publication.

## Sparse random CUDA oracles

For extents `e[0] ... e[Rank-1]`, candidate ordinal uses canonical
last-dimension-fastest order:

```text
ordinal = (...((i[0] * e[1] + i[1]) * e[2] + i[2])...)
```

For every candidate, the verifier forms:

```text
priority =
    uint64(oracle_word(structure_offset + 2*ordinal)) << 32
  | uint64(oracle_word(structure_offset + 2*ordinal + 1))
```

The oracle sorts test-owned `(priority, ordinal)` records, selects the exact
smallest count using ordinal as the deterministic tie break, decodes
coordinates independently, and sorts selected coordinates lexicographically.
It never calls the provider-free sparse generator to determine structure.

Stored position `p` selects values only after canonical sorting. Float uses
one independent oracle word and double uses two high-word-first oracle words.
Thus the tests prove exact count, uniqueness, canonical order, priority
selection, domain separation, and bit-exact stored-position value assignment.
They also compare the completed device result with a separately invoked
provider-free `GenerateSparseUniform01` result as a second parity check.

Cases include rank zero at counts zero and one, zero extent/count zero, small
rank-one and rectangular rank-two shapes, rank three, empty, partial, and full
count, float and double, changed structure address with fixed value address,
and changed value address with fixed structure address. For nonzero count:

```text
next_structure_offset = structure_offset + 2*logical_size
next_value_offset     = value_offset + count       // float
next_value_offset     = value_offset + 2*count     // double
```

Count zero leaves both offsets unchanged. Invalid count, equal structure/value
domains, structure or value offset overflow, unsupported rank, wrong memory
resource space/device, and allocation failure must return no usable owner.
An instrumented failing resource checks rollback and exactly-once release.
Successful allocation is limited to canonical coordinate and value buffers;
no hidden computational workspace allocation is permitted.

## Sparse CUDA staging and SpMV oracles

The host fixtures are independently constructed canonical zero-based signed
64-bit CSR structures. Clone verification copies each device owner buffer back
and compares offsets, indices, values, shape, and empty-CSR
`outer_extent + 1` zero offsets. The resource's allocation log must show
exactly the owner buffers and no conversion/workspace allocation.

SpMV expected values are calculated by a verifier-owned scalar loop:

```text
expected[row] =
    alpha * sum(A.value[k] * x[col[k]]) + beta * original_y[row]
```

The loop uses `long double` accumulation before conversion solely as an
independent numerical oracle; tolerances are recorded per precision and size.
Fixtures cover empty and rectangular matrices, float and double, positive
non-unit input/output strides with untouched padding, `alpha` values `0`, `1`,
and a nontrivial negative value, `beta` values `0`, `1`, and a nontrivial
value. A `beta == 0` case initializes output bytes to a quiet-NaN pattern and
requires finite correct results, proving the prior value was not consumed.

The selected cuSPARSE algorithm name and workspace size/allocation behavior
are recorded by the lead's evidence. The test accepts only deterministic
repeat output for identical inputs. It does not infer determinism merely from
algorithm naming.

Malformed offsets, nonzero base, decreasing offsets, terminal-offset/nnz
mismatch, out-of-range or unsorted columns, negative extent/stride,
incompatible shapes, overflow/narrowing, invalid span, wrong placement/device,
overlap between output and matrix/vector storage, unsupported scalar, and
provider failure are rejected before output mutation. No implicit transfer,
format conversion, packing, device switch, synchronization, or fallback is
accepted.

## Bounded sparse evaluator oracles

For coordinate, CSR, and CSC owners with identical canonical structure, the
verifier calculates each stored result independently for:

```text
copy(a)
-a
a + b
a - b
a * b
a + scalar
a - scalar
a * scalar
scalar + a
scalar - a
scalar * a
```

Only operations actually included in the frozen bounded public expression
surface are invoked; absent commuted forms are recorded rather than invented.
Every supported operation is checked for float and double, retained structure,
unchanged indices/offsets/coordinates, correct values, deterministic rerun,
and legal documented in-place aliasing.

The rejection matrix covers mismatched structure, shape, format, scalar/type,
sparsity-changing effect, general nested expression, destination overlap that
is not explicitly legal, and unsupported rank/format. Rejection must precede
destination mutation. General lookup, union/intersection, densification,
format conversion, and structure mutation are never exercised as success
paths.

## Ownership, lifetime, concurrency, and events

Compile-time assertions require owning CUDA contexts, completion events, raw
fill results, and generated sparse owners to be noncopyable and movable where
the frozen contract assigns ownership. Accessors and neutral descriptors are
checked for the promised exception specification and absence of CUDA SDK
types in common headers.

Runtime tests retain all caller-owned storage and resources until an explicit
event wait completes, then verify output. They exercise moving a result/event
before wait and destroying a completed event without a device-wide
synchronization claim. No test intentionally violates a documented caller
lifetime precondition.

Two execution contexts enqueue independent operations on disjoint storage,
wait in reverse order, and compare both results to independent oracles.
Repeated work on a shared immutable context but disjoint storage checks the
absence of mutable global random/provider state. Concurrency correctness is
based on results and event behavior, not wall-clock overlap.

## Compile and isolated-consumer contracts

Each new public provider header is compiled alone as C++20. Strict builds cover
exceptions enabled and disabled. A multi-header contract TU verifies no SDK
type or macro is required at the public boundary and checks move/copy traits,
return/result shapes, supported scalar invocability, signed-64 descriptor
metadata, and const access.

Negative compile checks require failure when:

- a move-only context, completion event, fill result, or generated owner is
  copied;
- a const destination is supplied;
- an integral or cv-qualified random element is requested;
- an unsupported sparse scalar/index/format is used; or
- a CUDA facet declaration is used without its facet header.

Four isolated consumers each request one exact component and link only its
matching imported target. They include only the matching public header and
perform a compile/link smoke call or trait check without requiring execution
on the build host. The package harness must test build-tree, install,
relocation, path-with-spaces, static/shared, unavailable component,
registry-disabled, and no accidental sibling facet in the component closure.

## Smoke benchmark oracles

Benchmarks are evidence tools, not speed gates. They print the CUDA device,
toolchain/build mode supplied by the harness, operation, scalar type, shape,
nnz/count, layout/strides, warmup count, measured repetitions, explicit
allocation/workspace facts, elapsed time, and a copied-back checksum.

Warmup completes before timing. Allocation, host-device staging, and result
verification are outside the timed region unless the benchmark name explicitly
states otherwise. Each timed batch is explicitly synchronized after enqueue.
Sparse CUDA covers clone-excluded CSR SpMV for float and double. Random CUDA
covers raw words plus dense and sparse generation with separate operation
records. Checksums are compared with an untimed independent oracle. No
performance ratio or unsupported speedup claim is a pass condition.

## Evidence classification

Every executed row receives exactly one applicable evidence label:

- `configure-tested`: successful CUDA/toolkit/provider configuration only;
- `compile-tested`: the CUDA test/consumer source compiled and linked;
- `runtime-tested`: execution on a real GPU without an independent result
  oracle;
- `parity-tested`: real-GPU output passed the independent CPU/bit oracle; or
- `skipped`: unavailable compiler, toolkit, runtime, device, operation, or
  matrix.

One label never implies another. Runtime and parity evidence records compiler,
toolkit, Runtime, cuSPARSE, driver, device, compute capability, architecture,
host compiler, linkage, exact operation/layout/size/tolerance, allocation,
workspace, transfer, event, and benchmark methodology.
