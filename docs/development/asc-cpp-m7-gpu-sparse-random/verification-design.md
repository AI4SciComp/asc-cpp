# Milestone 7 Independent Verification Design

Status: Frozen before Milestone 7 production inspection

Date: 2026-07-28

Scope: Milestone 7 — sparse CUDA and random CUDA facets

Post-freeze clarification: owner adjudication confirmed that only the raw
`MutableMemoryView` carries declared byte capacity. Dense and sparse external
views retain their inherited truthful-valid-storage precondition and cannot
prove an allocation terminal bound. The failure-matrix wording below records
that limitation; no expected-value oracle or production-derived behavior was
changed.

## Independence boundary

This design was frozen after reading only the Milestone 7 contract and
ownership ledger, approved ADRs 0008, 0012, 0014, 0015, 0017, and 0018, and
the frozen Milestone 4, 5, and 6 predecessor contracts. No Milestone 7
production header or source was inspected before this file was created.

No MdeCpp or deleted asc-cpp implementation, test, expected literal, table, or
generated datum is an oracle. No Milestone 8 artifact is inspected. API
spelling discovered later may change test call sites but must not change the
independent expected-value rules below.

## Verifier-owned artifacts

The verifier may create only:

```text
tests/sparse_cuda/** except CMakeLists.txt
tests/random_cuda/** except CMakeLists.txt
tests/random_dense_cuda/** except CMakeLists.txt
tests/random_sparse_cuda/** except CMakeLists.txt
tests/compile/m7_*.cc and m7_*.h
tests/consumer/{sparse_cuda,random_cuda,random_dense_cuda,random_sparse_cuda}/**
  except CMakeLists.txt
benchmarks/{sparse_cuda,random_cuda}/** except CMakeLists.txt
docs/development/asc-cpp-m7-gpu-sparse-random/verification-design.md
docs/development/asc-cpp-m7-gpu-sparse-random/verification-review.md
```

The lead owns all CMake registration, target/package assertions, aggregate
validation, and integration. Runtime tests and benchmarks must use the
repository skip code when no usable CUDA device exists.

## Independent Philox4x32-10 word oracle

The verifier implements Philox directly from the approved equations using
only fixed-width unsigned arithmetic:

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

Round zero uses the input key. Before rounds one through nine, increment the
key modulo 2^32 by `(0x9E3779B9, 0xBB67AE85)`.

Address decoding is independently restated:

```text
key       = [low32(stream), high32(stream)]
counter   = [low32(offset / 4), high32(offset / 4),
             low32(subsequence), high32(subsequence)]
word lane = offset % 4
```

The oracle calculates words rather than copying vectors. Device results must
agree both with this oracle and with the existing provider-free
`Philox4x32Word` API.

Cases cover all four lanes, both sides of counter-block boundaries, non-four
multiples, a high counter bit, zero and mixed/all-one stream and subsequence
words, and multi-block counts.

## Independent Uniform01 transforms

The test-only transforms use exactly representable powers of two:

```text
float  = (word >> 8) * 2^-24
double = (((uint64(high) << 32) | low) >> 11) * 2^-53
```

Float and double comparisons use `std::bit_cast`, not tolerance. Double words
are consumed high then low. These oracles do not call a production transform.

## Raw random CUDA verification

For a caller-owned device `uint32_t` buffer:

1. output word `i` equals the independent Philox oracle at `offset + i`;
2. the returned first unused offset is exactly `offset + count`;
3. counts 1, 3, 4, 5, and a large non-launch-multiple catch lane, block,
   launch-partition, and tail errors;
4. zero count returns the input offset and does not dereference storage;
5. two adjacent explicit-offset calls concatenate to one whole call;
6. reruns and independent CUDA contexts yield identical words; and
7. changing stream or subsequence is checked against the oracle, not merely
   assumed to change output.

Offset overflow, byte-count overflow, null nonempty destination, misalignment,
host placement, wrong backend/device, representable-address failure, and a
word count exceeding the truthful `MutableMemoryView` byte capacity must fail
before mutation. A sentinel allocation is copied back after failures.
Move/copy contracts for the asynchronous result and completion event are
compile-checked.

## Dense random CUDA verification

For rank zero through eight, logical dimension zero varies fastest:

```text
ordinal = i[0] + e[0] * (i[1] + e[1] * (...))
```

Float consumes oracle word `offset + ordinal`; double consumes words
`offset + 2*ordinal` and the following word. The returned offset is exact.

The runtime matrix includes rank zero, zero extent, `LayoutLeft`,
`LayoutRight`, unique padded nonnegative stride, dynamic/static extents, and
a shape crossing likely launch boundaries. Every logical element is bit
checked. Physical holes retain sentinels.

Whole-fill and explicit consecutive partitions must match. Reruns and two
independent contexts must match. Offset overflow, const/integral destination
compile misuse, nonunique/aliased mapping, negative or invalid stride, wrong
placement/device/backend must reject without mutation or usable completion
publication. The inherited `DenseView` contract requires truthful valid
storage but carries no terminal allocation capacity, so external undersized
storage is neither fabricated nor claimed runtime-detectable.

## Sparse random CUDA verification

Candidate ordinal is canonical last-dimension-fastest order:

```text
ordinal = (...((i[0] * e[1] + i[1]) * e[2] + i[2])...)
```

The verifier independently forms:

```text
priority =
    uint64(oracle_word(structure_offset + 2*ordinal)) << 32
  | uint64(oracle_word(structure_offset + 2*ordinal + 1))
```

It sorts `(priority, ordinal)`, using ordinal as the deterministic tie break,
selects exactly the smallest requested count, independently decodes
coordinates, and sorts selected coordinates lexicographically. It never calls
the provider-free sparse generator to determine expected structure.

After canonical ordering, stored position `p` consumes one value word for
float and two high-word-first words for double. The completed GPU owner is
also compared with a separately invoked provider-free
`GenerateSparseUniform01` result as a second parity check.

Cases include rank zero at count zero and one, zero extent/count zero,
rank-one, rectangular rank-two, rank-three, empty, partial, and full count,
float/double, and changed structure or value addresses. Expected offsets are:

```text
count == 0:
  next_structure_offset = structure_offset
otherwise:
  next_structure_offset = structure_offset + 2*logical_size

next_value_offset = value_offset + count       // float
next_value_offset = value_offset + 2*count     // double
```

Negative/excess count, equal structure/value domains, either offset overflow,
wrong resource placement/device, and allocation failure publish no usable
owner. Rank is not artificially capped; a rank-nine case proves the
compile-time-rank path remains available. A failing resource verifies rollback
and exactly-once release. Successful generation may own only coordinate and
value buffers; no hidden computational workspace is accepted.

## CSR staging verification

The verifier constructs canonical host CSR fixtures from independent arrays.
After `CudaCloneCsr` completion, it copies every device buffer back and checks:

- shape, NNZ, signed-64 offsets and indices;
- all values;
- resource ownership;
- empty CSR with `outer_extent + 1` zero offsets; and
- float and double behavior.

Malformed host offsets/base/order/terminal NNZ, unsorted or out-of-range
columns, shape/byte overflow, wrong resource placement/device, and allocation
failure must publish no owner. Clone does not convert format or allocate
workspace beyond the explicit owner buffers.

## CSR SpMV numerical and workspace oracle

Expected values are calculated by a test-owned scalar loop:

```text
expected[row] =
    alpha * sum(A.value[k] * x[column[k]]) + beta * original_y[row]
```

The fixtures use exactly representable small values so float and double can be
checked exactly where possible; otherwise a separately recorded tolerance is
used. They cover rectangular and empty matrices, zero NNZ, unit and positive
nonunit input/output stride, untouched padding, nontrivial alpha/beta, and
`beta == 0` with NaN-initialized output to prove the old value is not read.

Unit stride must expose the selected deterministic cuSPARSE algorithm's
queried caller-owned workspace. Tests pass exact workspace and reject
undersized, host-space, wrong-device, and operand-overlapping workspace.
Positive nonunit stride is separately verified as the project-kernel path with
zero workspace. No runtime fallback between paths is accepted.

Wrong shape, negative/zero stride, invalid span, untrusted/malformed device
CSR, wrong placement/device/backend, output overlap with matrix/input, and
provider narrowing fail before output mutation.

## Bounded sparse evaluator oracle

For trusted coordinate, CSR, and reachable CSC device structures, the verifier
computes each stored result independently for approved shallow forms:

```text
copy(a)
-a
a + b
a - b
a * b
a + 0
0 + a
a - 0
0 - a
a * scalar
scalar * a
```

Nonzero scalar addition/subtraction is rejected because it changes implicit
zeros. Scalar-minus-sparse is accepted only for exact zero, which is negate.
General nesting, structure mismatch, untrusted provenance, rank/type/shape/
format mismatch, densifying sparsity effect, and unsupported scalar are
rejected.

Every successful form checks float/double values and unchanged immutable
structure. Checked value rebinding must preserve trusted structure only for an
exact NNZ-length value span disjoint from coordinate/offset/index structure.
Exact full value-span in-place evaluation is legal; partial overlap and
structure/value overlap reject before mutation.

If no approved M7 producer can create trusted device CSC, the CSC success path
is classified `skipped`, never inferred from CSR/coordinate behavior.

## Lifetime, events, concurrency, and failure

Static contracts verify move-only provider contexts, events, generated owners,
and results. Runtime tests retain every context/resource/owner/view/workspace
through completion and exercise moved result/event objects.

Independent contexts and streams run concurrent explicit-address operations.
Correctness is based on results and event query/wait behavior, not timing
overlap. Context/device restoration is tested where local hardware permits;
multi-GPU evidence is `skipped` on a single-device host.

Zero-work operations return usable already-complete events. Provider failures
must not publish a completion that permits premature lifetime release.
Destruction is checked for absence of device-wide synchronization only where a
stable observable test exists.

## Compile, header, and consumer verification

Verifier-owned translation units cover:

- each provider header standalone under C++20;
- all four APIs in one translation unit;
- move/copy ownership traits;
- exceptions-disabled parsing through lead registration;
- rejection of copied result/context objects;
- rejection of const dense destinations; and
- rejection of integral dense/sparse random elements.

Four isolated consumers include only the requested public provider header and
link only the matching installed component. They instantiate the public API
without provider SDK types. The lead must register build-tree and
installed/relocated/path-with-spaces modes and audit exact component closure.

## Performance probes

Smoke benchmarks are correctness probes, not speed gates:

- CSR SpMV float and double, 1024x1024, five sorted unique columns per row;
- raw Philox, 2^20 words;
- dense float Uniform01, 1024x1024;
- sparse float Uniform01, 128x128, exact count 128, allocation included.

Each benchmark warms up, waits each asynchronous completion, excludes
setup/transfer/oracle work from timing except where allocation is explicitly
named, and performs untimed independent checksums/oracles. Sparse-random checks
every coordinate and value bit against the independent priority/Philox oracle.
The harness must record compiler, toolkit, driver, GPU, compute capability,
architecture, configuration, warmups, repetitions, workspace/allocation
behavior, elapsed observation, and checksum. No comparative speedup or
cross-system conclusion is permitted.

## Evidence classification

Every final facet is classified independently using exactly:

```text
configure-tested
compile-tested
runtime-tested
parity-tested
skipped
```

Configure evidence is not compile evidence; compile evidence is not runtime
evidence; runtime evidence is not parity evidence without an independent
oracle. Unavailable toolchains, hardware, providers, or approved producers are
reported as `skipped`.

## Final review contents

`verification-review.md` records:

- exact files and independent oracle roles;
- exact commands and pass/fail/skip results;
- CPU/GPU parity by facet;
- allocation/workspace and lifetime evidence;
- consumer/package evidence supplied by lead-owned registration;
- benchmark observations and checks;
- findings, production-owner resolutions, and reruns;
- remaining risks and skipped matrices; and
- confirmation that no remote/history operation occurred.
